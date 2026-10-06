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
  email: string;
  tokens: GoogleTokens;
  connectedAt: number;
}

interface BrowserSession {
  deviceId: string;
  createdAt: number;
}

// Development-only storage. Cloudflare may evict/restart isolates at any time.
const pendingAuthorizations = new Map<string, PendingAuthorization>();
const connections = new Map<string, Connection>();
const browserSessions = new Map<string, BrowserSession>();

const STATE_TTL_MS = 10 * 60 * 1000;
const SESSION_TTL_MS = 7 * 24 * 60 * 60 * 1000;
const COOKIE_NAME = 'desk_buddy_session';
const jsonHeaders = { 'content-type': 'application/json; charset=utf-8' };

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
    headers.set('access-control-allow-methods', 'GET, OPTIONS');
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
    connections.set(pending.deviceId, {
      deviceId: pending.deviceId,
      email: profile.email,
      tokens,
      connectedAt: Date.now(),
    });
    browserSessions.set(sessionId, { deviceId: pending.deviceId, createdAt: Date.now() });

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

  const connection = connections.get(deviceId);
  return jsonResponse(request, env, {
    connected: Boolean(connection),
    ...(connection ? { email: connection.email } : {}),
  });
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

    // Calendar data and device commands remain unimplemented in this task.
    if (request.method === 'GET' && url.pathname === '/api/calendar') {
      return jsonResponse(request, env, { success: false, error: 'Calendar API is not implemented yet' }, 501);
    }
    if (request.method === 'POST' && url.pathname === '/api/device/command') {
      return jsonResponse(request, env, { success: false, error: 'Device command API is not implemented yet' }, 501);
    }
    return jsonResponse(request, env, { success: false, error: 'Not found' }, 404);
  },
};
