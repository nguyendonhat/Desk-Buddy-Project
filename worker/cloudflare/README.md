# Cloudflare Worker setup

## Persistent OAuth storage

OAuth state, browser sessions, and Google tokens use a Cloudflare Workers KV namespace bound as `DESK_BUDDY_STORE`. Without this binding, OAuth routes cannot persist state between requests.

The production namespace `DESK_BUDDY_STORE` has been created and bound to the `desk-buddy-api` Worker. Preview builds use a separate namespace so tests cannot overwrite production OAuth connections. Both IDs are recorded in `wrangler.toml`. For another environment, create a separate namespace:

```powershell
npx wrangler kv namespace create DESK_BUDDY_STORE
```

Copy that environment's returned namespace ID into its Wrangler configuration:

```toml
[[kv_namespaces]]
binding = "DESK_BUDDY_STORE"
id = "<namespace-id>"
```

For local development, use Wrangler's local KV emulation or a separate test namespace. Do not use the production namespace for local development. Put local-only OAuth values in an ignored `.dev.vars` file; never commit it.

Production Worker: `desk-buddy-api`. Production web origin: `https://desk-buddy-web-c1c.pages.dev`. Keep these aligned with the Cloudflare dashboard so future Wrangler deployments target the existing Worker and preserve its web origin.

Cloudflare Workers Builds must use `/worker/cloudflare` as the root directory for both Production and Preview builds so Wrangler uses this package and its lockfile.

OAuth state keys expire after 10 minutes; browser session keys expire after 7 days. Connection records persist until the user logs out, revokes access, or the refresh token becomes invalid. Logout deletes the device's active connection and its Google tokens from KV.

Never expose KV contents or OAuth token values in logs or the Web Dashboard.
