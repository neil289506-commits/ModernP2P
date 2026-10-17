import type { FileMeta } from "./types";

export const CODE_RE = /^[A-Z0-9]{8}$/;
export const PEER_ID_RE = /^mp2p-[A-Z0-9]{8}-[a-z0-9]{12}$/;
export const REQUEST_ID_RE = /^[a-f0-9]{32}$/;

export const MAX_FILES = 2000; // keep in sync with StagingModel::kMaxFiles
export const MAX_NAME_LENGTH = 512; // relative paths such as "Album/sub/a.jpg"
/** One pending request must stay far below the 2 MiB Durable Object value limit. */
export const MAX_REQUEST_BYTES = 128 * 1024;
export const MAX_FILE_SIZE = 2 ** 40; // 1 TiB

export function isValidCode(code: string): boolean {
  return CODE_RE.test(code);
}

/** A peer id must belong to the code that registers it. */
export function isValidPeerIdFor(peerId: unknown, code: string): peerId is string {
  return typeof peerId === "string" && PEER_ID_RE.test(peerId) && peerId.slice(5, 13) === code;
}

export function parseFiles(value: unknown): FileMeta[] | null {
  if (!Array.isArray(value) || value.length === 0 || value.length > MAX_FILES) return null;
  const out: FileMeta[] = [];
  for (const item of value) {
    if (typeof item !== "object" || item === null) return null;
    const { name, size } = item as Record<string, unknown>;
    if (typeof name !== "string" || name.length === 0 || name.length > MAX_NAME_LENGTH) return null;
    if (typeof size !== "number" || !Number.isInteger(size) || size < 0 || size > MAX_FILE_SIZE) return null;
    out.push({ name, size });
  }
  return out;
}

export function requestFits(files: FileMeta[]): boolean {
  return new TextEncoder().encode(JSON.stringify(files)).length <= MAX_REQUEST_BYTES;
}

export async function sha256Hex(input: string): Promise<string> {
  const digest = await crypto.subtle.digest("SHA-256", new TextEncoder().encode(input));
  return [...new Uint8Array(digest)].map((b) => b.toString(16).padStart(2, "0")).join("");
}

export function randomHex(bytes: number): string {
  const buf = new Uint8Array(bytes);
  crypto.getRandomValues(buf);
  return [...buf].map((b) => b.toString(16).padStart(2, "0")).join("");
}

/** Constant-time string comparison (both inputs are fixed-length hex hashes). */
export function timingSafeEqual(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}
