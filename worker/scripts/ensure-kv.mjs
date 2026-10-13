// Creates (or finds) the KV namespace used for rate limiting and writes its id
// into wrangler.toml. Needs CLOUDFLARE_API_TOKEN and CLOUDFLARE_ACCOUNT_ID.
import { readFileSync, writeFileSync } from "node:fs";

const { CLOUDFLARE_API_TOKEN: token, CLOUDFLARE_ACCOUNT_ID: account } = process.env;
if (!token || !account) {
  console.error("CLOUDFLARE_API_TOKEN and CLOUDFLARE_ACCOUNT_ID must be set");
  process.exit(1);
}

const TITLE = "modernp2p-rate";
const base = `https://api.cloudflare.com/client/v4/accounts/${account}/storage/kv/namespaces`;
const headers = { authorization: `Bearer ${token}`, "content-type": "application/json" };

async function api(url, init) {
  const res = await fetch(url, { ...init, headers });
  const body = await res.json();
  if (!body.success) throw new Error(JSON.stringify(body.errors));
  return body.result;
}

const existing = (await api(`${base}?per_page=100`)).find((n) => n.title === TITLE);
const id = existing?.id ?? (await api(base, { method: "POST", body: JSON.stringify({ title: TITLE }) })).id;

const path = new URL("../wrangler.toml", import.meta.url);
writeFileSync(path, readFileSync(path, "utf8").replace(/id = "[^"]*"/, `id = "${id}"`));
console.log(`KV namespace ${TITLE}: ${id}`);
