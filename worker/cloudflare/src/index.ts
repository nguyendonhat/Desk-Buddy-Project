interface Env {
  APP_ENV: string;
}

const jsonHeaders = { 'content-type': 'application/json; charset=utf-8' };

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);

    if (request.method === 'GET' && url.pathname === '/api/health') {
      return Response.json({ success: true, service: 'desk-buddy-worker', environment: env.APP_ENV });
    }

    // Reserved for the existing API contract; implementations come in later tasks.
    if (request.method === 'GET' && url.pathname === '/api/calendar') {
      return Response.json(
        { success: false, error: 'Calendar API is not implemented yet' },
        { status: 501, headers: jsonHeaders },
      );
    }

    if (request.method === 'POST' && url.pathname === '/api/device/command') {
      return Response.json(
        { success: false, error: 'Device command API is not implemented yet' },
        { status: 501, headers: jsonHeaders },
      );
    }

    return Response.json({ success: false, error: 'Not found' }, { status: 404, headers: jsonHeaders });
  },
};
