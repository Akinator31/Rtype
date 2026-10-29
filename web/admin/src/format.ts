// How the panel writes durations, ages and times.

/** A duration in milliseconds, its two largest units: "2 h 05 min", "45 s". */
export function duration(milliseconds: number): string {
  const seconds = Math.max(0, Math.floor(milliseconds / 1000));
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  const rest = seconds % 60;
  const two = (value: number) => String(value).padStart(2, '0');
  if (days > 0) return `${days} d ${two(hours)} h`;
  if (hours > 0) return `${hours} h ${two(minutes)} min`;
  if (minutes > 0) return `${minutes} min ${two(rest)} s`;
  return `${rest} s`;
}

/** The time of day of a Unix time in milliseconds, for the lines of a record. */
export function clock(at: number): string {
  return new Date(at).toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit', second: '2-digit' });
}

/** A date and a time, for what may be days old. */
export function dateTime(at: number): string {
  return new Date(at).toLocaleString('en-GB', {
    day: '2-digit',
    month: 'short',
    hour: '2-digit',
    minute: '2-digit',
  });
}

/** A score with its leading zeros, as the game shows it. */
export function score(value: number): string {
  return String(value).padStart(7, '0');
}

const FadeSteps = [1, 0.8, 0.6, 0.4];

/** The opacity of the line of this rank, the newest being 0: 100, 80, 60, 40, then 22 %. */
export function fade(rank: number): number {
  return FadeSteps[rank] ?? 0.22;
}

const Minute = 60_000;
const Hour = 60 * Minute;
const Day = 24 * Hour;

/** The opacity of a line by its age: the steps of the design's feed, 100, 80, 60, 40 then 22 %. */
export function fadeByAge(age: number): number {
  if (age < 5 * Minute) return 1;
  if (age < Hour) return 0.8;
  if (age < Day) return 0.6;
  if (age < 7 * Day) return 0.4;
  return 0.22;
}
