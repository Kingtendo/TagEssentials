# Public Helpers server

This is the wins only backend for TagEssentials. It keeps the Hypixel API
key on your VPS, shares one UUID cache across all mod users, coalesces simultaneous
lookups, and never returns a player's real profile name to a client.

It is intentionally **not** a general Hypixel API proxy. The only public data
route is:

`GET /v1/tnttag/wins?uuid=<uuid>&name=<visible_name>`

The service returns wins only when the cached Hypixel display name matches the
visible name supplied by the client. This preserves the mod's nick protection.
If a successful fresh Hypixel response either has no player profile or contains
a profile name that differs from the visible in game name, the service checks
the UUID against Minecraft's official session profile service. It returns
`nicked: true` only when that second check confirms a different account name or
a synthetic UUID, and never exposes the real profile name. Matching Minecraft
identities, timeouts, rate limits, and upstream failures never return the
nicked state.

Review the current [Hypixel API policy](https://developer.hypixel.net/policies)
and register the public release as a Production application in the
[developer dashboard](https://developer.hypixel.net/). Do not repurpose this
service into an API that third party developers can query for arbitrary fields.

## Requirements

* A small Linux VPS with Docker Engine and the Docker Compose plugin.
* A reserved public IPv4 address assigned to the VPS.
* TCP ports 80 and 443 open to the internet.
* A reviewed Hypixel Production application/key before a public release.

## Deploy

1. Copy this entire `public-helpers-server` directory to the VPS.
2. In the directory, run `cp .env.example .env`.
3. Edit `.env` and set:
   * `PUBLIC_HELPERS_ADDRESS` to your VPS public IP, without `https://`.
   * `HYPIXEL_API_KEY` to the key stored only on the VPS.
   * `PUBLIC_HELPERS_TOKEN` to the output of `openssl rand -hex 32`.
4. Start it with `docker compose up -d --build`.
5. Check it with `curl https://YOUR_PUBLIC_IP/healthz`.
6. View logs with `docker compose logs -f public-helpers`.

Caddy obtains and renews a short lived Let's Encrypt IP certificate
automatically. The Node service
is reachable only through the private Compose network; only Caddy publishes VPS
ports.

## Configure the Windows client

Copy `public_helpers_server.example.txt` to `public_helpers_server.txt` beside
`build-release.ps1`, then replace its two lines with:

```text
https://YOUR_PUBLIC_IP
THE_SAME_PUBLIC_HELPERS_TOKEN
```

The release script includes this file beside the EXE. The token prevents casual
unauthorized traffic, but it is distributed with the client and must not be
treated as a secret. Per IP, global client, and upstream Hypixel limits remain
the actual abuse controls.

## Updating

From the server directory:

```sh
docker compose up -d --build
docker compose logs --tail=100 public-helpers
```

The `public_helpers_data` Docker volume preserves cached results across updates
and restarts. Do not run `docker compose down -v` unless you intentionally want
to erase that cache and Caddy's certificate data.

## Defaults

* Fresh shared wins cache: 30 minutes.
* Minecraft identity confirmation cache: 5 minutes.
* Stale fallback after upstream errors: 24 hours.
* Hypixel upstream budget: 280 requests per 5 minutes.
* Per client IP budget: 240 service requests per 5 minutes.
* Simultaneous requests for the same UUID use one Hypixel request.

Change these through `.env` only after checking the limit assigned to your
Hypixel application. The service also honors Hypixel rate limit reset headers.
