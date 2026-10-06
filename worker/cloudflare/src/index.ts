interface Env {
  APP_ENV: string;
  WEB_ORIGIN: string;
  GOOGLE_CLIENT_ID: string;
  GOOGLE_CLIENT_SECRET: string;
  GOOGLE_REDIRECT_URI: string;
}

interface PendingAuthorization {
  deviceId: string;
  codeVerifier: string;
  createdAt: number;
}

interface GoogleTokens {
  access_token: string;
  refresh_token?: string;
  expires_in?: number;
  token_type: string;
  scope: string;
}

interface Connection {
  deviceId: string;
  connectionId: string;
  email: string;
  tokens: GoogleTokens;
  accessTokenExpiresAt: number;
  connectedAt: number;
  selectedCalendarId?: string;
  selectedCalendarTimeZone?: string;
}

interface BrowserSession {
  deviceId: string;
  connectionId: string;
  createdAt: number;
}

// Development-only storage. Cloudflare may evict/restart isolates at any time.
const pendingAuthorizations = new Map<string, PendingAuthorization>();
const connections = new Map<string, Connection>();
const browserSessions = new Map<string, BrowserSession>();

const STATE_TTL_MS = 10 * 60 * 1000;
const SESSION_TTL_MS = 7 * 24 * 60 * 60 * 1000;
const COOKIE_NAME = 'desk_buddy_session';
const CALENDAR_PAGE_SIZE = 250;
const CALENDAR_WINDOW_MS = 7 * 24 * 60 * 60 * 1000;
const jsonHeaders = { 'content-type': 'application/json; charset=utf-8' };

class CalendarError extends Error {
  constructor(message: string, readonly status: number) {
    super(message);
  }
}

function randomToken(byteLength = 32): string {
  const bytes = crypto.getRandomValues(new Uint8Array(byteLength));
  return base64Url(bytes);
}

function base64Url(bytes: Uint8Array): string {
  let binary = '';
  for (const byte of bytes) binary += String.fromCharCode(byte);
  return btoa(binary).replaceAll('+', '-').replaceAll('/', '_').replaceAll('=', '');
}

function cookieValue(request: Request, name: string): string | undefined {
  const cookieHeader = request.headers.get('cookie') ?? '';
  for (const part of cookieHeader.split(';')) {
    const [key, ...value] = part.trim().split('=');
    if (key === name) return value.join('=');
  }
  return undefined;
}

function corsHeaders(request: Request, env: Env): Headers {
  const headers = new Headers({ vary: 'Origin' });
  if (request.headers.get('origin') === env.WEB_ORIGIN) {
    headers.set('access-control-allow-origin', env.WEB_ORIGIN);
    headers.set('access-control-allow-credentials', 'true');
    headers.set('access-control-allow-methods', 'GET, POST, OPTIONS');
    headers.set('access-control-allow-headers', 'content-type');
  }
  return headers;
}

function jsonResponse(request: Request, env: Env, body: unknown, status = 200): Response {
  const headers = corsHeaders(request, env);
  headers.set('content-type', jsonHeaders['content-type']);
  headers.set('cache-control', 'no-store');
  return Response.json(body, { status, headers });
}

function frontendRedirect(env: Env, result: 'connected' | 'denied' | 'error'): Response {
  const destination = new URL('/', env.WEB_ORIGIN);
  destination.searchParams.set('oauth', result);
  return new Response(null, { status: 303, headers: { location: destination.toString(), 'cache-control': 'no-store' } });
}

function validDeviceId(deviceId: string | null): deviceId is string {
  return deviceId !== null && /^[A-Za-z0-9_-]{1,64}$/.test(deviceId);
}

function pruneExpiredEntries(): void {
  const now = Date.now();
  for (const [state, item] of pendingAuthorizations) {
    if (now - item.createdAt > STATE_TTL_MS) pendingAuthorizations.delete(state);
  }
  for (const [sessionId, session] of browserSessions) {
    if (now - session.createdAt > SESSION_TTL_MS) browserSessions.delete(sessionId);
  }
}

async function handleOAuthStart(request: Request, env: Env, url: URL): Promise<Response> {
  const deviceId = url.searchParams.get('device_id');
  if (!validDeviceId(deviceId)) return jsonResponse(request, env, { success: false, error: 'Invalid device_id' }, 400);
  if (!env.GOOGLE_CLIENT_ID || !env.GOOGLE_CLIENT_SECRET || !env.GOOGLE_REDIRECT_URI || !env.WEB_ORIGIN) {
    return jsonResponse(request, env, { success: false, error: 'OAuth is not configured' }, 503);
  }

  pruneExpiredEntries();
  const state = randomToken();
  const codeVerifier = randomToken(48);
  const challenge = base64Url(new Uint8Array(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(codeVerifier))));
  pendingAuthorizations.set(state, { deviceId, codeVerifier, createdAt: Date.now() });

  const authorizeUrl = new URL('https://accounts.google.com/o/oauth2/v2/auth');
  authorizeUrl.search = new URLSearchParams({
    client_id: env.GOOGLE_CLIENT_ID,
    redirect_uri: env.GOOGLE_REDIRECT_URI,
    response_type: 'code',
    scope: 'openid email https://www.googleapis.com/auth/calendar.readonly',
    state,
    code_challenge: challenge,
    code_challenge_method: 'S256',
    access_type: 'offline',
    prompt: 'consent',
  }).toString();
  return Response.redirect(authorizeUrl.toString(), 302);
}

async function handleOAuthCallback(request: Request, env: Env, url: URL): Promise<Response> {
  pruneExpiredEntries();
  const state = url.searchParams.get('state');
  const pending = state ? pendingAuthorizations.get(state) : undefined;
  if (state) pendingAuthorizations.delete(state); // OAuth state is one-time, even on error.
  if (!pending || Date.now() - pending.createdAt > STATE_TTL_MS) return frontendRedirect(env, 'error');

  if (url.searchParams.has('error')) {
    return frontendRedirect(env, url.searchParams.get('error') === 'access_denied' ? 'denied' : 'error');
  }

  const code = url.searchParams.get('code');
  if (!code) return frontendRedirect(env, 'error');

  try {
    const tokenResponse = await fetch('https://oauth2.googleapis.com/token', {
      method: 'POST',
      headers: { 'content-type': 'application/x-www-form-urlencoded' },
      body: new URLSearchParams({
        code,
        client_id: env.GOOGLE_CLIENT_ID,
        client_secret: env.GOOGLE_CLIENT_SECRET,
        redirect_uri: env.GOOGLE_REDIRECT_URI,
        grant_type: 'authorization_code',
        code_verifier: pending.codeVerifier,
      }),
    });
    if (!tokenResponse.ok) return frontendRedirect(env, 'error');
    const tokens = await tokenResponse.json<GoogleTokens>();

    const profileResponse = await fetch('https://openidconnect.googleapis.com/v1/userinfo', {
      headers: { authorization: `Bearer ${tokens.access_token}` },
    });
    if (!profileResponse.ok) return frontendRedirect(env, 'error');
    const profile = await profileResponse.json<{ email?: string; email_verified?: boolean }>();
    if (!profile.email || profile.email_verified !== true) return frontendRedirect(env, 'error');

    const sessionId = randomToken();
    for (const [existingSessionId, session] of browserSessions) {
      if (session.deviceId === pending.deviceId) browserSessions.delete(existingSessionId);
    }
    connections.set(pending.deviceId, {
      deviceId: pending.deviceId,
      connectionId: sessionId,
      email: profile.email,
      tokens,
      accessTokenExpiresAt: Date.now() + Math.max(0, (tokens.expires_in ?? 3600) - 60) * 1000,
      connectedAt: Date.now(),
    });
    browserSessions.set(sessionId, { deviceId: pending.deviceId, connectionId: sessionId, createdAt: Date.now() });

    const response = frontendRedirect(env, 'connected');
    const secure = new URL(env.WEB_ORIGIN).protocol === 'https:';
    response.headers.append(
      'set-cookie',
      `${COOKIE_NAME}=${sessionId}; Path=/; HttpOnly; SameSite=Lax; Max-Age=${SESSION_TTL_MS / 1000}${secure ? '; Secure' : ''}`,
    );
    return response;
  } catch {
    // Never include provider response bodies or token values in logs or redirects.
    return frontendRedirect(env, 'error');
  }
}

function handleConnectionStatus(request: Request, env: Env, url: URL): Response {
  pruneExpiredEntries();
  const deviceId = url.searchParams.get('device_id');
  if (!validDeviceId(deviceId)) return jsonResponse(request, env, { success: false, error: 'Invalid device_id' }, 400);

  const sessionId = cookieValue(request, COOKIE_NAME);
  const session = sessionId ? browserSessions.get(sessionId) : undefined;
  if (!session || session.deviceId !== deviceId) {
    return jsonResponse(request, env, { connected: false });
  }

  const connection = connectionForSession(request, deviceId);
  return jsonResponse(request, env, {
    connected: Boolean(connection),
    ...(connection ? { email: connection.email } : {}),
  });
}

async function refreshGoogleAccessToken(connection: Connection, env: Env): Promise<string> {
  if (!connection.tokens.refresh_token) {
    connections.delete(connection.deviceId);
    throw new CalendarError('Google permission has expired or was revoked. Please reconnect your Google account.', 401);
  }

  let response: Response;
  try {
    response = await fetch('https://oauth2.googleapis.com/token', {
      method: 'POST',
      headers: { 'content-type': 'application/x-www-form-urlencoded' },
      body: new URLSearchParams({
        client_id: env.GOOGLE_CLIENT_ID,
        client_secret: env.GOOGLE_CLIENT_SECRET,
        refresh_token: connection.tokens.refresh_token,
        grant_type: 'refresh_token',
      }),
    });
  } catch {
    throw new CalendarError('Could not reach Google to refresh authorization. Please try again.', 502);
  }

  if (!response.ok) {
    let providerError = '';
    try {
      providerError = (await response.json<{ error?: string }>()).error ?? '';
    } catch {
      // Do not expose provider response bodies to the caller.
    }
    if (response.status === 400 && providerError === 'invalid_grant') {
      connections.delete(connection.deviceId);
      throw new CalendarError('Google permission has expired or was revoked. Please reconnect your Google account.', 401);
    }
    throw new CalendarError('Google could not refresh authorization. Please try again.', 502);
  }

  const refreshed = await response.json<GoogleTokens>();
  connection.tokens = { ...connection.tokens, ...refreshed, refresh_token: connection.tokens.refresh_token };
  connection.accessTokenExpiresAt = Date.now() + Math.max(0, (refreshed.expires_in ?? 3600) - 60) * 1000;
  return connection.tokens.access_token;
}

interface CalendarWindow {
  timeMin: string;
  timeMax: string;
}

async function authorizedGoogleFetch(connection: Connection, env: Env, apiUrl: URL): Promise<Response> {
  let accessToken = connection.accessTokenExpiresAt > Date.now()
    ? connection.tokens.access_token
    : await refreshGoogleAccessToken(connection, env);
  let response: Response;
  try {
    response = await fetch(apiUrl, { headers: { authorization: `Bearer ${accessToken}` } });
  } catch {
    throw new CalendarError('Could not reach Google Calendar. Check your connection and try again.', 502);
  }
  if (response.status === 401) {
    accessToken = await refreshGoogleAccessToken(connection, env);
    try {
      response = await fetch(apiUrl, { headers: { authorization: `Bearer ${accessToken}` } });
    } catch {
      throw new CalendarError('Could not reach Google Calendar. Check your connection and try again.', 502);
    }
  }
  if (response.status === 401) {
    connections.delete(connection.deviceId);
    throw new CalendarError('Google permission has expired or was revoked. Please reconnect your Google account.', 401);
  }
  return response;
}

async function getCalendarEvents(connection: Connection, env: Env, window: CalendarWindow, pageToken?: string): Promise<Response> {
  const calendarId = connection.selectedCalendarId;
  if (!calendarId) throw new CalendarError('Choose a Google Calendar before syncing events.', 409);
  const apiUrl = new URL(`https://www.googleapis.com/calendar/v3/calendars/${encodeURIComponent(calendarId)}/events`);
  apiUrl.searchParams.set('timeMin', window.timeMin);
  apiUrl.searchParams.set('timeMax', window.timeMax);
  apiUrl.searchParams.set('maxResults', String(CALENDAR_PAGE_SIZE));
  apiUrl.searchParams.set('singleEvents', 'true');
  apiUrl.searchParams.set('orderBy', 'startTime');
  if (connection.selectedCalendarTimeZone) apiUrl.searchParams.set('timeZone', connection.selectedCalendarTimeZone);
  if (pageToken) apiUrl.searchParams.set('pageToken', pageToken);
  return authorizedGoogleFetch(connection, env, apiUrl);
}

async function fetchCalendarPage(connection: Connection, env: Env, window: CalendarWindow, pageToken?: string): Promise<Response> {
  return getCalendarEvents(connection, env, window, pageToken);
}

interface CalendarListItem {
  id: string;
  summary?: string;
  primary?: boolean;
  timeZone?: string;
}

async function listGoogleCalendars(connection: Connection, env: Env): Promise<CalendarListItem[]> {
  const calendars: CalendarListItem[] = [];
  const seenPageTokens = new Set<string>();
  let pageToken: string | undefined;
  do {
    const apiUrl = new URL('https://www.googleapis.com/calendar/v3/users/me/calendarList');
    apiUrl.searchParams.set('maxResults', String(CALENDAR_PAGE_SIZE));
    apiUrl.searchParams.set('showHidden', 'true');
    if (pageToken) apiUrl.searchParams.set('pageToken', pageToken);
    const response = await authorizedGoogleFetch(connection, env, apiUrl);
    if (!response.ok) {
      throw new CalendarError(response.status === 403
        ? 'Google denied access to the calendar list. Confirm calendar read permission is granted.'
        : 'Could not load the Google Calendar list. Please try again.', 502);
    }
    const result = await response.json<{ items?: CalendarListItem[]; nextPageToken?: string }>();
    calendars.push(...(result.items ?? []).filter((calendar) => Boolean(calendar.id)));
    pageToken = result.nextPageToken;
    if (pageToken && seenPageTokens.has(pageToken)) {
      throw new CalendarError('Google Calendar list pagination could not continue. Please try again.', 502);
    }
    if (pageToken) seenPageTokens.add(pageToken);
  } while (pageToken);
  return calendars;
}

function normalizeCalendarEvent(event: {
  summary?: string;
  description?: string;
  start?: { dateTime?: string; date?: string };
  end?: { dateTime?: string; date?: string };
}): { title: string; start: string; end: string; description: string } | undefined {
  const start = event.start?.dateTime ?? event.start?.date;
  const end = event.end?.dateTime ?? event.end?.date;
  if (!start || !end) return undefined;
  return {
    title: event.summary ?? '',
    start,
    end,
    description: event.description ?? '',
  };
}

async function handleCalendar(request: Request, env: Env, url: URL): Promise<Response> {
  pruneExpiredEntries();
  const deviceId = url.searchParams.get('device_id');
  if (!validDeviceId(deviceId)) return jsonResponse(request, env, { success: false, error: 'Invalid device_id' }, 400);

  const connection = connectionForSession(request, deviceId);
  if (!connection) {
    return jsonResponse(request, env, { success: false, error: 'Google account is not connected. Please connect it again.' }, 401);
  }
  if (!connection.selectedCalendarId) {
    return jsonResponse(request, env, { success: false, error: 'Choose a Google Calendar before syncing events.' }, 409);
  }

  try {
    const windowStart = Date.now();
    const window = {
      timeMin: new Date(windowStart).toISOString(),
      timeMax: new Date(windowStart + CALENDAR_WINDOW_MS).toISOString(),
    };
    let response = await fetchCalendarPage(connection, env, window);
    if (!response.ok) {
      console.info('[calendar-debug]', JSON.stringify({
        calendarId: connection.selectedCalendarId,
        calendarTimeZone: connection.selectedCalendarTimeZone ?? null,
        timeMin: window.timeMin,
        timeMax: window.timeMax,
        googleStatus: response.status,
        itemCount: 0,
        normalizedCount: 0,
      }));
      return jsonResponse(request, env, {
        success: false,
        error: response.status === 403
          ? 'Google Calendar denied access. Confirm the Calendar API is enabled and the read permission is still granted.'
          : 'Google Calendar could not return events. Please try again.',
      }, 502);
    }

    const events: Array<{ title: string; start: string; end: string; description: string }> = [];
    let page: { items?: Array<Parameters<typeof normalizeCalendarEvent>[0]>; nextPageToken?: string } | undefined = await response.json();
    const seenPageTokens = new Set<string>();
    while (page) {
      const beforeNormalizeCount = events.length;
      for (const event of page.items ?? []) {
        const normalized = normalizeCalendarEvent(event);
        if (normalized) events.push(normalized);
      }
      console.info('[calendar-debug]', JSON.stringify({
        calendarId: connection.selectedCalendarId,
        calendarTimeZone: connection.selectedCalendarTimeZone ?? null,
        timeMin: window.timeMin,
        timeMax: window.timeMax,
        googleStatus: response.status,
        itemCount: page.items?.length ?? 0,
        normalizedCount: events.length - beforeNormalizeCount,
      }));
      const nextPageToken = page.nextPageToken;
      if (!nextPageToken) break;
      if (seenPageTokens.has(nextPageToken)) throw new CalendarError('Google Calendar pagination could not continue. Please try again.', 502);
      seenPageTokens.add(nextPageToken);
      response = await fetchCalendarPage(connection, env, window, nextPageToken);
      if (!response.ok) throw new CalendarError('Google Calendar could not return all events. Please try again.', 502);
      page = await response.json();
    }

    return jsonResponse(request, env, { device_id: deviceId, events });
  } catch (error) {
    if (error instanceof CalendarError) {
      return jsonResponse(request, env, { success: false, error: error.message }, error.status);
    }
    return jsonResponse(request, env, { success: false, error: 'Could not load Google Calendar. Please try again.' }, 502);
  }
}

function connectionForSession(request: Request, deviceId: string): Connection | undefined {
  const sessionId = cookieValue(request, COOKIE_NAME);
  const session = sessionId ? browserSessions.get(sessionId) : undefined;
  if (!session || session.deviceId !== deviceId) return undefined;
  const connection = connections.get(deviceId);
  return connection?.connectionId === session.connectionId ? connection : undefined;
}

function handleOAuthLogout(request: Request, env: Env): Response {
  const sessionId = cookieValue(request, COOKIE_NAME);
  const session = sessionId ? browserSessions.get(sessionId) : undefined;
  if (sessionId) browserSessions.delete(sessionId);

  if (session) {
    const connection = connections.get(session.deviceId);
    if (connection?.connectionId === session.connectionId) {
      connections.delete(session.deviceId);
      for (const [otherSessionId, otherSession] of browserSessions) {
        if (otherSession.connectionId === session.connectionId) browserSessions.delete(otherSessionId);
      }
    }
  }

  const secure = new URL(env.WEB_ORIGIN).protocol === 'https:';
  const headers = corsHeaders(request, env);
  headers.set('content-type', jsonHeaders['content-type']);
  headers.set('cache-control', 'no-store');
  headers.append(
    'set-cookie',
    `${COOKIE_NAME}=; Path=/; HttpOnly; SameSite=Lax; Max-Age=0; Expires=Thu, 01 Jan 1970 00:00:00 GMT${secure ? '; Secure' : ''}`,
  );
  return Response.json({ success: true }, { status: 200, headers });
}

async function handleCalendarList(request: Request, env: Env, url: URL): Promise<Response> {
  pruneExpiredEntries();
  const deviceId = url.searchParams.get('device_id');
  if (!validDeviceId(deviceId)) return jsonResponse(request, env, { success: false, error: 'Invalid device_id' }, 400);
  const connection = connectionForSession(request, deviceId);
  if (!connection) return jsonResponse(request, env, { success: false, error: 'Google account is not connected in this browser session.' }, 401);

  try {
    const calendars = await listGoogleCalendars(connection, env);
    return jsonResponse(request, env, { calendars, selected_calendar_id: connection.selectedCalendarId ?? null });
  } catch (error) {
    if (error instanceof CalendarError) return jsonResponse(request, env, { success: false, error: error.message }, error.status);
    return jsonResponse(request, env, { success: false, error: 'Could not load Google Calendar list. Please try again.' }, 502);
  }
}

async function handleCalendarSelection(request: Request, env: Env): Promise<Response> {
  let body: { device_id?: unknown; calendar_id?: unknown };
  try {
    body = await request.json();
  } catch {
    return jsonResponse(request, env, { success: false, error: 'Invalid JSON body' }, 400);
  }

  const deviceId = typeof body.device_id === 'string' ? body.device_id : null;
  const calendarId = typeof body.calendar_id === 'string' ? body.calendar_id : '';
  if (!validDeviceId(deviceId)) return jsonResponse(request, env, { success: false, error: 'Invalid device_id' }, 400);
  if (!calendarId || calendarId.length > 512) return jsonResponse(request, env, { success: false, error: 'Invalid calendar_id' }, 400);
  const connection = connectionForSession(request, deviceId);
  if (!connection) return jsonResponse(request, env, { success: false, error: 'Google account is not connected in this browser session.' }, 401);

  try {
    const calendars = await listGoogleCalendars(connection, env);
    const selectedCalendar = calendars.find((calendar) => calendar.id === calendarId);
    if (!selectedCalendar) {
      return jsonResponse(request, env, { success: false, error: 'Selected calendar is not available to this Google account.' }, 403);
    }
    connection.selectedCalendarId = calendarId;
    connection.selectedCalendarTimeZone = selectedCalendar.timeZone;
    return jsonResponse(request, env, { success: true });
  } catch (error) {
    if (error instanceof CalendarError) return jsonResponse(request, env, { success: false, error: error.message }, error.status);
    return jsonResponse(request, env, { success: false, error: 'Could not validate the selected calendar.' }, 502);
  }
}

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers: corsHeaders(request, env) });

    if (request.method === 'GET' && url.pathname === '/api/health') {
      return jsonResponse(request, env, { success: true, service: 'desk-buddy-worker', environment: env.APP_ENV });
    }
    if (request.method === 'GET' && url.pathname === '/api/oauth/google/start') {
      return handleOAuthStart(request, env, url);
    }
    if (request.method === 'GET' && url.pathname === '/api/oauth/google/callback') {
      return handleOAuthCallback(request, env, url);
    }
    if (request.method === 'GET' && url.pathname === '/api/oauth/connection') {
      return handleConnectionStatus(request, env, url);
    }
    if (request.method === 'POST' && url.pathname === '/api/oauth/logout') {
      return handleOAuthLogout(request, env);
    }
    if (request.method === 'GET' && url.pathname === '/api/calendars') {
      return handleCalendarList(request, env, url);
    }
    if (request.method === 'POST' && url.pathname === '/api/calendar/selection') {
      return handleCalendarSelection(request, env);
    }

    if (request.method === 'GET' && url.pathname === '/api/calendar') {
      return handleCalendar(request, env, url);
    }
    if (request.method === 'POST' && url.pathname === '/api/device/command') {
      return jsonResponse(request, env, { success: false, error: 'Device command API is not implemented yet' }, 501);
    }
    return jsonResponse(request, env, { success: false, error: 'Not found' }, 404);
  },
};
