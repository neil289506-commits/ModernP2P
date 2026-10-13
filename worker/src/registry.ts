import { DurableObject } from "cloudflare:workers";
import type { Env, TransferRequest } from "./types";
import { isValidPeerIdFor, parseFiles, randomHex, sha256Hex, timingSafeEqual } from "./validate";

interface Owner {
  tokenHash: string;
  peerId: string | null;
  lastSeen: number;
}

const REQUEST_TTL_MS = 10 * 60 * 1000;
const CLAIM_TTL_MS = 90 * 24 * 3600 * 1000;
const MAX_PENDING = 50;

const json = (body: unknown, status = 200) =>
  new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });
const err = (message: string, status: number) => json({ error: message }, status);

/**
 * One instance per 8-character code. A Durable Object gives us the atomic
 * "claim if free" semantics that KV cannot.
 *
 * These routes are internal: only the Worker in index.ts can reach them.
 */
export class CodeRegistry extends DurableObject<Env> {
  async fetch(request: Request): Promise<Response> {
    const url = new URL(request.url);
    const code = request.headers.get("x-code") ?? "";

    switch (`${request.method} ${url.pathname}`) {
      case "POST /claim":
        return this.claim();
      case "POST /release":
        return this.release(request);
      case "GET /lookup":
        return this.lookup();
      case "POST /session":
        return this.session(request, code);
      case "GET /verify":
        return this.verify(request);
      case "POST /request":
        return this.addRequest(request);
      case "GET /inbox":
        return this.inbox(request);
      case "POST /respond":
        return this.respond(request);
      case "POST /notify":
        return this.notify(request);
      case "GET /ws":
        return this.openSocket(request);
      default:
        return err("not found", 404);
    }
  }

  // ---- helpers -----------------------------------------------------------

  private async authenticate(request: Request): Promise<Owner | null> {
    const header = request.headers.get("authorization") ?? "";
    if (!header.startsWith("Bearer ")) return null;
    const owner = await this.ctx.storage.get<Owner>("owner");
    if (!owner) return null;
    const hash = await sha256Hex(header.slice(7));
    return timingSafeEqual(hash, owner.tokenHash) ? owner : null;
  }

  private broadcast(message: unknown): void {
    const payload = JSON.stringify(message);
    for (const ws of this.ctx.getWebSockets()) {
      try {
        ws.send(payload);
      } catch {
        /* socket already closed */
      }
    }
  }

  private async pending(): Promise<TransferRequest[]> {
    const all = (await this.ctx.storage.get<TransferRequest[]>("requests")) ?? [];
    const fresh = all.filter((r) => Date.now() - r.createdAt < REQUEST_TTL_MS);
    if (fresh.length !== all.length) await this.ctx.storage.put("requests", fresh);
    return fresh;
  }

  // ---- routes ------------------------------------------------------------

  private async claim(): Promise<Response> {
    const existing = await this.ctx.storage.get<Owner>("owner");
    if (existing && Date.now() - existing.lastSeen < CLAIM_TTL_MS) return err("code taken", 409);

    const token = randomHex(32);
    const owner: Owner = { tokenHash: await sha256Hex(token), peerId: null, lastSeen: Date.now() };
    await this.ctx.storage.deleteAll();
    await this.ctx.storage.put("owner", owner);
    return json({ ownerToken: token }, 201);
  }

  private async release(request: Request): Promise<Response> {
    if (!(await this.authenticate(request))) return err("unauthorized", 401);
    for (const ws of this.ctx.getWebSockets()) ws.close(1000, "released");
    await this.ctx.storage.deleteAll();
    return json({ ok: true });
  }

  private async lookup(): Promise<Response> {
    const owner = await this.ctx.storage.get<Owner>("owner");
    if (!owner) return json({ exists: false, online: false });
    return json({ exists: true, online: this.ctx.getWebSockets().length > 0 && owner.peerId !== null });
  }

  private async session(request: Request, code: string): Promise<Response> {
    const owner = await this.authenticate(request);
    if (!owner) return err("unauthorized", 401);
    const body = (await request.json().catch(() => null)) as { peerId?: unknown } | null;
    if (!body || !isValidPeerIdFor(body.peerId, code)) return err("invalid peerId", 400);
    owner.peerId = body.peerId;
    owner.lastSeen = Date.now();
    await this.ctx.storage.put("owner", owner);
    return json({ ok: true });
  }

  private async verify(request: Request): Promise<Response> {
    const owner = await this.authenticate(request);
    if (!owner) return err("unauthorized", 401);
    if (!owner.peerId) return err("session not registered", 409);
    return json({ peerId: owner.peerId });
  }

  private async addRequest(request: Request): Promise<Response> {
    const owner = await this.ctx.storage.get<Owner>("owner");
    if (!owner) return err("code not found", 404);
    const body = (await request.json().catch(() => null)) as Record<string, unknown> | null;
    const files = parseFiles(body?.files);
    if (!body || !files || typeof body.from !== "string" || !isValidPeerIdFor(body.fromPeerId, body.from)) {
      return err("invalid request", 400);
    }
    const list = await this.pending();
    if (list.filter((r) => r.status === "pending").length >= MAX_PENDING) return err("inbox full", 429);

    const entry: TransferRequest = {
      id: randomHex(16),
      from: body.from,
      fromPeerId: body.fromPeerId,
      files,
      createdAt: Date.now(),
      status: "pending",
    };
    list.push(entry);
    await this.ctx.storage.put("requests", list);
    this.broadcast({ type: "request", request: entry });
    return json({ id: entry.id }, 202);
  }

  private async inbox(request: Request): Promise<Response> {
    if (!(await this.authenticate(request))) return err("unauthorized", 401);
    const list = (await this.pending()).filter((r) => r.status === "pending");
    return json({ requests: list });
  }

  private async respond(request: Request): Promise<Response> {
    const owner = await this.authenticate(request);
    if (!owner) return err("unauthorized", 401);
    const body = (await request.json().catch(() => null)) as { id?: unknown; accept?: unknown } | null;
    if (!body || typeof body.id !== "string" || typeof body.accept !== "boolean") return err("invalid body", 400);
    if (body.accept && !owner.peerId) return err("session not registered", 409);

    const list = await this.pending();
    const entry = list.find((r) => r.id === body.id && r.status === "pending");
    if (!entry) return err("request not found", 404);
    entry.status = body.accept ? "accepted" : "rejected";
    await this.ctx.storage.put("requests", list);
    return json({ from: entry.from, peerId: owner.peerId, requestId: entry.id, accepted: body.accept });
  }

  private async notify(request: Request): Promise<Response> {
    this.broadcast(await request.json());
    return json({ ok: true });
  }

  private async openSocket(request: Request): Promise<Response> {
    if (request.headers.get("upgrade") !== "websocket") return err("expected websocket", 426);
    const owner = await this.authenticate(request);
    if (!owner) return err("unauthorized", 401);

    const pair = new WebSocketPair();
    this.ctx.acceptWebSocket(pair[1]);
    owner.lastSeen = Date.now();
    await this.ctx.storage.put("owner", owner);
    return new Response(null, { status: 101, webSocket: pair[0] });
  }

  async webSocketMessage(ws: WebSocket, message: string | ArrayBuffer): Promise<void> {
    if (message === "ping") ws.send("pong");
  }

  async webSocketClose(ws: WebSocket, code: number): Promise<void> {
    ws.close(code, "closing");
  }
}
