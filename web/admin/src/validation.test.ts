import { describe, expect, it } from 'vitest';

import { fade, fadeByAge } from './format';
import { byteLength, isValidMessage, MaxTextBytes } from './validation';

describe('the texts of the actions', () => {
  it('count their bytes of UTF-8, as the server does', () => {
    expect(byteLength('abc')).toBe(3);
    expect(byteLength('é')).toBe(2);
  });

  it('take a message of 1 to 200 bytes on one line', () => {
    expect(isValidMessage('Server restarts in 5 minutes')).toBe(true);
    expect(isValidMessage('a'.repeat(MaxTextBytes))).toBe(true);
    expect(isValidMessage('a'.repeat(MaxTextBytes + 1))).toBe(false);
    expect(isValidMessage('é'.repeat(101))).toBe(false);
    expect(isValidMessage('')).toBe(false);
    expect(isValidMessage('   ')).toBe(false);
    expect(isValidMessage('two\nlines')).toBe(false);
    expect(isValidMessage('tab\there')).toBe(false);
  });
});

describe('the fading of the feed', () => {
  it('goes from 100 % to 22 % with the rank, as the design', () => {
    expect([0, 1, 2, 3, 4, 9].map(fade)).toEqual([1, 0.8, 0.6, 0.4, 0.22, 0.22]);
  });

  it('goes from 100 % to 22 % with the age of an action', () => {
    const minute = 60_000;
    expect([0, 10 * minute, 2 * 60 * minute, 2 * 1440 * minute, 30 * 1440 * minute].map(fadeByAge)).toEqual([
      1, 0.8, 0.6, 0.4, 0.22,
    ]);
  });
});
