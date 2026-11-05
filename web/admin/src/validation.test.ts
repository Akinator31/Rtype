import { describe, expect, it } from 'vitest';

import { fade, fadeByAge } from './format';
import {
  byteLength,
  findToken,
  isAddress,
  isValidBanDuration,
  isValidMessage,
  MaxTextBytes,
} from './validation';

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

describe('the addresses of a ban', () => {
  it('takes IPv4 and IPv6 without a port', () => {
    for (const address of [
      '198.51.100.4',
      '0.0.0.0',
      '2001:db8::7',
      '::1',
      '::ffff:203.0.113.9',
      '1:2:3:4:5:6:7:8',
    ]) {
      expect(isAddress(address), address).toBe(true);
    }
    for (const address of [
      '198.51.100.4:4242',
      '256.1.1.1',
      '1.2.3',
      '[::1]',
      '1::2::3',
      'host',
      '',
      '1:2:3:4:5:6:7:8:9',
    ]) {
      expect(isAddress(address), address).toBe(false);
    }
  });

  it('takes a duration of 1 s to 7 days', () => {
    expect([0, 1, 3600, 604800, 604801, 1.5, Number.NaN].map(isValidBanDuration)).toEqual([
      false,
      true,
      true,
      true,
      false,
      false,
      false,
    ]);
  });
});

describe('the token of admin-url.txt', () => {
  const token = 'ab'.repeat(32);

  it('is found alone, or in the address', () => {
    expect(findToken(token)).toBe(token);
    expect(findToken(` ${token.toUpperCase()}\n`)).toBe(token);
    expect(findToken(`http://127.0.0.1:8080/?token=${token}\n`)).toBe(token);
  });

  it('is nothing else', () => {
    expect(findToken('abc')).toBeNull();
    expect(findToken(token + 'a')).toBeNull();
    expect(findToken(`http://127.0.0.1:8080/?token=${token}ff`)).toBeNull();
    expect(findToken('')).toBeNull();
  });
});

describe('the fading of the feed', () => {
  it('goes from 100 % to 26 % with the rank, as the design', () => {
    expect([0, 1, 2, 3, 4, 5, 6, 9].map(fade)).toEqual([1, 0.9, 0.8, 0.65, 0.5, 0.38, 0.26, 0.26]);
  });

  it('dims an older line, down to 40 %', () => {
    const minute = 60_000;
    expect([0, 2 * 60 * minute, 12 * 60 * minute, 2 * 1440 * minute].map(fadeByAge)).toEqual([
      1, 0.8, 0.6, 0.4,
    ]);
  });
});
