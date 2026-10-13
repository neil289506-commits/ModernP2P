/**
 * Soft per-IP rate limit backed by KV. KV is eventually consistent and not
 * atomic, so this only slows down abuse; it is not a hard guarantee.
 */
export async function isLimited(
  kv: KVNamespace,
  key: string,
  max: number,
  windowSeconds: number,
): Promise<boolean> {
  const bucket = Math.floor(Date.now() / 1000 / windowSeconds);
  const kvKey = `rl:${key}:${bucket}`;
  const current = parseInt((await kv.get(kvKey)) ?? "0", 10) || 0;
  if (current >= max) return true;
  await kv.put(kvKey, String(current + 1), { expirationTtl: Math.max(60, windowSeconds * 2) });
  return false;
}
