// What the server takes in the texts of the actions (docs/protocol/admin-api.md, 4), checked before
// sending: the server checks it again.

/** The most a reason or a message may hold, in bytes of UTF-8, as the server checks it. */
export const MaxTextBytes = 200;

export function byteLength(text: string): number {
  return new TextEncoder().encode(text).length;
}

/** A message the server takes: 1 to 200 bytes, on one line, without control characters. */
export function isValidMessage(text: string): boolean {
  // eslint-disable-next-line no-control-regex
  return text.trim() !== '' && byteLength(text) <= MaxTextBytes && !/[\u0000-\u001f\u007f]/.test(text);
}
