// How the panel writes durations, ages, times and numbers.

const two = (value: number) => String(value).padStart(2, '0');

/** A duration in milliseconds, its two largest units: "2 h 05 min", "45 s". */
export function duration(milliseconds: number): string {
  const seconds = Math.max(0, Math.floor(milliseconds / 1000));
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const rest = seconds % 60;
  if (days > 0) return `${days} d ${two(hours)} h`;
  if (hours > 0) return `${hours} h ${two(minutes)} min`;
  if (minutes > 0) return `${minutes} min ${two(rest)} s`;
  return `${rest} s`;
}

/** A duration in seconds, as short as it goes: "10 min", "1 h", "7 d", "90 s". */
export function shortDuration(seconds: number): string {
  if (seconds % 86400 === 0) return `${seconds / 86400} d`;
  if (seconds % 3600 === 0) return `${seconds / 3600} h`;
  if (seconds % 60 === 0) return `${seconds / 60} min`;
  return `${seconds} s`;
}

/** Hours and minutes: "18:24". */
export function time(at: number): string {
  return new Date(at).toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit' });
}

/** Hours, minutes and seconds, for the lines of a record: "18:31:04". */
export function clock(at: number): string {
  return new Date(at).toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

/** The day and the time, for what may be days old: "29/10 18:24". */
export function dayTime(at: number): string {
  const date = new Date(at);
  return `${two(date.getDate())}/${two(date.getMonth() + 1)} ${time(at)}`;
}

/** The full date and the time: "26/10/2026 · 14:02". */
export function dateTime(at: number): string {
  const date = new Date(at);
  return `${two(date.getDate())}/${two(date.getMonth() + 1)}/${date.getFullYear()} · ${time(at)}`;
}

/** The date of the top bar: "Thu 29 Oct · 18:42". */
export function today(at: number): string {
  const date = new Date(at);
  const day = date.toLocaleDateString('en-GB', { weekday: 'short', day: 'numeric', month: 'short' });
  return `${day} · ${time(at)}`;
}

/** Seconds with a tenth: "0.4". */
export function tenths(milliseconds: number): string {
  return (Math.max(0, milliseconds) / 1000).toFixed(1);
}

/** A number by groups of three: "54 812". */
export function grouped(value: number): string {
  return String(value).replace(/\B(?=(\d{3})+(?!\d))/g, ' ');
}

/** A score with its leading zeros, as the game shows it. */
export function score(value: number): string {
  return String(value).padStart(7, '0');
}

/** A wave on two digits, as the game shows it: "07". */
export function wave(value: number): string {
  return two(value);
}

const FadeSteps = [1, 0.9, 0.8, 0.65, 0.5, 0.38];

/** The opacity of the line of this rank, the newest being 0: the steps of the design's feed, then 26 %. */
export function fade(rank: number): number {
  return FadeSteps[rank] ?? 0.26;
}

const Minute = 60_000;
const Hour = 60 * Minute;
const Day = 24 * Hour;

/** The opacity of a line by its age: dimmer the older, down to 40 %. */
export function fadeByAge(age: number): number {
  if (age < Hour) return 1;
  if (age < 6 * Hour) return 0.8;
  if (age < Day) return 0.6;
  return 0.4;
}
