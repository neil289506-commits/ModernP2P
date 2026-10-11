# ModernP2P

Windows peer-to-peer file transfer app built with Qt 6.11.0, a Cloudflare Worker for
code registration/lookup/notifications, and PeerJS (WebRTC) for the actual transfer.

## How it works

1. On first launch the app generates a random 8-character code (A-Z, 0-9) and asks the Worker to
   claim it. The Worker uses one Durable Object per code, so a claim is atomic; if the code is
   taken the app silently rolls a new one.
2. Drop files into the staging area, type the peer's code at the bottom left and press **Send**.
   The Worker checks the peer is online and delivers a request to them (**Requests** button).
3. When the peer accepts, the Worker tells the sender the peer's PeerJS id and the files are
   streamed over a WebRTC data channel (SHA-256 verified per file).

The Worker maps a code to a PeerJS id, not to an IP address: WebRTC does not need the IP and
exposing it to anyone who knows a code would be a privacy leak.

## Layout

| Path | Purpose |
|---|---|
| `src/core` | Code generation, file-name sanitising, persisted identity |
| `src/models` | Staging, requests and transfer list models |
| `src/network` | Cloudflare Worker client + notification WebSocket |
| `src/transfer` | PeerJS bridge (hidden WebEngine page) and the transfer protocol engine |
| `src/app` | `AppController` wiring everything to the QML UI |
| `qml/`, `web/` | UI and the PeerJS host page |
| `worker/` | Cloudflare Worker (TypeScript, Durable Object, KV rate limiting) |
| `third_party/peerjs` | Vendored PeerJS 1.5.4 (MIT) |

## Setup

Repository **secrets** (Settings → Secrets and variables → Actions):

- `CF_API_TOKEN`: token with *Workers Scripts: Edit* and *Workers KV Storage: Edit*
- `CF_ACCOUNT_ID`

Repository **variable**:

- `API_BASE_URL`: the deployed Worker URL, e.g. `https://modernp2p-api.<subdomain>.workers.dev`
  (shown in the *Deploy* step log of the Worker workflow). It is baked into the app at build time.

Pushing to `main` deploys the Worker; pushing a `v*` tag builds the app and creates a release.

## Build locally

```sh
cmake -S . -B build -DP2P_API_BASE=https://your-worker.workers.dev
cmake --build build --config Release
ctest --test-dir build -C Release
```

You can also override the server at runtime with the `MODERNP2P_API` environment variable.

## Limitations

- Only individual files (no folders yet), no resume after a dropped connection.
- Uses PeerJS's public signalling server and a public STUN server; strict NATs may need a TURN server.
