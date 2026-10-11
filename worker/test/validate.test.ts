import { describe, expect, it } from "vitest";
import { isValidCode, isValidPeerIdFor, parseFiles, sha256Hex, timingSafeEqual } from "../src/validate";

describe("codes", () => {
  it("accepts 8 uppercase alphanumerics only", () => {
    expect(isValidCode("AB12CD34")).toBe(true);
    expect(isValidCode("ab12cd34")).toBe(false);
    expect(isValidCode("AB12CD3")).toBe(false);
    expect(isValidCode("AB12CD3!")).toBe(false);
  });

  it("binds peer ids to their code", () => {
    expect(isValidPeerIdFor("mp2p-AB12CD34-abcdefghij12", "AB12CD34")).toBe(true);
    expect(isValidPeerIdFor("mp2p-AB12CD34-abcdefghij12", "ZZZZZZZZ")).toBe(false);
    expect(isValidPeerIdFor("evil", "AB12CD34")).toBe(false);
  });
});

describe("parseFiles", () => {
  it("accepts a well-formed list", () => {
    expect(parseFiles([{ name: "a.txt", size: 10 }])).toEqual([{ name: "a.txt", size: 10 }]);
  });
  it("rejects bad input", () => {
    expect(parseFiles([])).toBeNull();
    expect(parseFiles("x")).toBeNull();
    expect(parseFiles([{ name: "", size: 1 }])).toBeNull();
    expect(parseFiles([{ name: "a", size: -1 }])).toBeNull();
    expect(parseFiles([{ name: "a", size: 1.5 }])).toBeNull();
    expect(parseFiles([{ name: "x".repeat(256), size: 1 }])).toBeNull();
  });
});

describe("crypto helpers", () => {
  it("hashes deterministically", async () => {
    expect(await sha256Hex("abc")).toBe("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  });
  it("compares in constant time", () => {
    expect(timingSafeEqual("abcd", "abcd")).toBe(true);
    expect(timingSafeEqual("abcd", "abce")).toBe(false);
    expect(timingSafeEqual("abc", "abcd")).toBe(false);
  });
});
