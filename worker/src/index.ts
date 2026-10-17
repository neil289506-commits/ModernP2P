import { CodeRegistry } from "./registry";
import { isLimited } from "./ratelimit";
import type { Env } from "./types";
import { REQUEST_ID_RE, isValidCode, parseFiles, requestFits } from "./validate";

export { CodeRegistry };

const json = (body: unknown, status = 200) =>
  new Response(JSON.stringify(body), { status, headers: { "content-type": "application/json" } });
const err = (message: string, status: number) => json({ error: message }, status);

function stub(env: Env, code: string): DurableObjectStub {
  return env.REGISTRY.get(env.REGISTRY.idFromName(code));
}

/** Forward to a registry object, tagging the call with its code. */
function call(env: Env, code: string, path: string, init: RequestInit = {}): Promise<Response> {
  const headers = new Headers(init.headers);
  headers.set("x-code", code);
  return stub(env, code).fetch(`https://registry${path}`, { ...init, headers });
}

const authHeaders = (request: Request): HeadersInit => ({
  authorization: request.headers.get("authorization") ?? "",
});

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);
    const ip = request.headers.get("cf-connecting-ip") ?? "unknown";

    if (url.pathname === "/health") return json({ ok: true });

    const m = url.pathname.match(/^\/v1\/codes\/([A-Z0-9]{8})(?:\/(.*))?$/);
    if (!m) return err("not found", 404);
    const code = m[1];
    const rest = m[2] ?? "";
    const method = request.method;

    // GET /v1/codes/:code  -> does this code exist / is it online?
    if (rest === "" && method === "GET") {
      if (await isLimited(env.RATE, `lookup:${ip}`, 30, 60)) return err("rate limited", 429);
      return call(env, code, "/lookup");
    }

    // POST /v1/codes/:code/claim
    if (rest === "claim" && method === "POST") {
      if (await isLimited(env.RATE, `claim:${ip}`, 10, 60)) return err("rate limited", 429);
      return call(env, code, "/claim", { method: "POST" });
    }

    if (rest === "release" && method === "POST") {
      return call(env, code, "/release", { method: "POST", headers: authHeaders(request) });
    }

    if (rest === "session" && method === "POST") {
      return call(env, code, "/session", { method: "POST", headers: authHeaders(request), body: request.body });
    }

    if (rest === "ws" && method === "GET") {
      return call(env, code, "/ws", { headers: request.headers });
    }

    if (rest === "requests" && method === "GET") {
      return call(env, code, "/inbox", { headers: authHeaders(request) });
    }

    // POST /v1/codes/:to/requests  (sender authenticates as X-From-Code)
    if (rest === "requests" && method === "POST") {
      if (await isLimited(env.RATE, `request:${ip}`, 20, 60)) return err("rate limited", 429);
      const from = request.headers.get("x-from-code") ?? "";
      if (!isValidCode(from) || from === code) return err("invalid sender", 400);

      const body = (await request.json().catch(() => null)) as { files?: unknown } | null;
      const files = parseFiles(body?.files);
      if (!files) return err("invalid files", 400);
      if (!requestFits(files)) return err("request too large", 413);

      const verify = await call(env, from, "/verify", { headers: authHeaders(request) });
      if (!verify.ok) return err("sender not authorized", 401);
      const { peerId } = (await verify.json()) as { peerId: string };

      return call(env, code, "/request", {
        method: "POST",
        body: JSON.stringify({ from, fromPeerId: peerId, files }),
      });
    }

    // POST /v1/codes/:code/requests/:id/respond
    const respondMatch = rest.match(/^requests\/([a-f0-9]{32})\/respond$/);
    if (respondMatch && method === "POST") {
      const id = respondMatch[1];
      if (!REQUEST_ID_RE.test(id)) return err("invalid id", 400);
      const body = (await request.json().catch(() => null)) as { accept?: unknown } | null;
      if (typeof body?.accept !== "boolean") return err("invalid body", 400);

      const res = await call(env, code, "/respond", {
        method: "POST",
        headers: authHeaders(request),
        body: JSON.stringify({ id, accept: body.accept }),
      });
      if (!res.ok) return res;

      const r = (await res.json()) as { from: string; peerId: string; requestId: string; accepted: boolean };
      await call(env, r.from, "/notify", {
        method: "POST",
        body: JSON.stringify({
          type: "response",
          requestId: r.requestId,
          accepted: r.accepted,
          from: code,
          peerId: r.accepted ? r.peerId : null,
        }),
      });
      return json({ ok: true });
    }

    return err("not found", 404);
  },
};
