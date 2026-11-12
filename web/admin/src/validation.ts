// What the server takes in the texts of the actions (docs/protocol/admin-api.md, 4), checked before
// sending: the server checks it again.

/** The most a reason or a message may hold, in bytes of UTF-8, as the server checks it. */
export const MaxTextBytes = 200;

/** From these many bytes, a field shows how many it holds. */
export const NearLimit = 150;

export function byteLength(text: string): number {
  return new TextEncoder().encode(text).length;
}

/** A message the server takes: 1 to 200 bytes, on one line, without control characters. */
export function isValidMessage(text: string): boolean {
  // eslint-disable-next-line no-control-regex
  return text.trim() !== '' && byteLength(text) <= MaxTextBytes && !/[\u0000-\u001f\u007f]/.test(text);
}

/** The longest ban the server takes, in seconds: 7 days. */
export const MaxBanSeconds = 604800;

export function isValidBanDuration(seconds: number): boolean {
  return Number.isInteger(seconds) && seconds >= 1 && seconds <= MaxBanSeconds;
}

/** The longest mute of the voice of an account, as the server takes it: 30 days. */
export const MaxVoiceMuteSeconds = 2592000;

export function isValidVoiceMuteDuration(seconds: number): boolean {
  return Number.isInteger(seconds) && seconds >= 1 && seconds <= MaxVoiceMuteSeconds;
}

const ipv4 = /^(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)(\.(25[0-5]|2[0-4]\d|1\d\d|[1-9]?\d)){3}$/;

/** An IP address without a port, as the server takes it: IPv4, or IPv6 written without brackets. */
export function isAddress(text: string): boolean {
  if (ipv4.test(text)) return true;
  if (!text.includes(':') || !/^[0-9a-fA-F:.]+$/.test(text)) return false;
  const halves = text.split('::');
  if (halves.length > 2) return false;
  const groups = halves.flatMap((half) => (half === '' ? [] : half.split(':')));
  // An IPv4 address may end an IPv6 one: it counts for two groups
  const last = groups[groups.length - 1] ?? '';
  const tail = last.includes('.') ? (ipv4.test(last) ? 2 : -100) : 0;
  const words = groups.length - (tail > 0 ? 1 : 0);
  if (tail < 0 || !groups.slice(0, words).every((group) => /^[0-9a-fA-F]{1,4}$/.test(group))) return false;
  const count = words + tail;
  return halves.length === 2 ? count < 8 : count === 8;
}

/** The local token in a text: alone, or in the address of admin-url.txt. Nothing if it holds none. */
export function findToken(text: string): string | null {
  const trimmed = text.trim();
  const inAddress = /[?&]token=([0-9a-fA-F]{64})(?![0-9a-fA-F])/.exec(trimmed);
  if (inAddress?.[1] !== undefined) return inAddress[1].toLowerCase();
  const alone = trimmed.replace(/\s+/g, '');
  return /^[0-9a-fA-F]{64}$/.test(alone) ? alone.toLowerCase() : null;
}
