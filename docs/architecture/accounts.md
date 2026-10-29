# Accounts

The accounts of the players of a server ([ADR 0024](../decisions/0024-player-accounts.md)): what makes a valid
username, email and password, how a password is hashed, and what the master keeps of the accounts, their
sessions and the codes it sends by email. This page covers the storage: nothing reaches the network yet, the
lobby and the client use it in the next steps of the accounts
([#259](https://github.com/Akinator31/Rtype/issues/259)).

| | |
|---|---|
| Code | `Accounts/AccountRules.{hpp,cpp}` (the rules, pure), `Accounts/PasswordHasher.{hpp,cpp}` (Argon2id, on its threads), `Accounts/AccountStore.{hpp,cpp}` (the tables) in `apps/server/src`; `rtype/network/Crypto.hpp` (random bytes, SHA-256) |
| Threads | The rules anywhere; the store on the database thread; the hashes on the two threads of the hasher |
| Tables | `accounts`, `account_sessions`, `account_codes` (migrations 3 to 5 of `ServerMigrations`) |
| Dependencies | The [reference implementation of Argon2](https://github.com/P-H-C/phc-winner-argon2) (CC0 or Apache 2.0), through CPM with a hash; TF-PSA-Crypto, through `libs/network` |
| Tests | `tests/server_account_store.cpp`, tags `[accounts]` and `[database]`; `tests/crypto.cpp`, tag `[crypto]` |

## The rules

`Accounts/AccountRules.hpp` checks what a player types, before it reaches the database or the hasher:

| | Rule | Compared |
|---|---|---|
| Username | 3 to 16 of `A–Z a–z 0–9 _ -`: no lookalike letter of another alphabet | Ignoring case: `usernameKey()`, its lower case, is unique |
| Email | Trimmed of the spaces around it; one `@` with something before it, a domain with a dot inside, at most 254 characters, no space nor control character. Only a code sent to it proves it receives mail | Ignoring case: `emailKey()`, its lower case, is unique. Only ASCII letters change case |
| Password | 10 to 128 characters (UTF-8 characters, not bytes), valid UTF-8 without control characters, neither the username nor the email, ignoring case | Never: only its hash is kept |

`checkPassword()` says why a password is refused (`TooShort`, `TooLong`, `InvalidText`, `SameAsUsername`,
`SameAsEmail`), so that the lobby can tell the player.

## Passwords

A password is hashed with **Argon2id**, with the parameters OWASP recommends at least: 19 MiB of memory, 2
passes, 1 lane, a random salt of 16 bytes, a hash of 32 bytes. The hash is kept in its standard form, which
carries its parameters, so that they can grow without breaking the hashes already kept:

```
$argon2id$v=19$m=19456,t=2,p=1$<salt in base64>$<hash in base64>
```

`hashPassword()` and `verifyPassword()` work on the calling thread; `verifyPassword()` compares in constant time,
and refuses anything but an Argon2id hash. Argon2 is built from its reference C files, in a target of ours
(`rtype_argon2`): the portable code, without the SSE of `opt.c` nor the threads of Argon2, since a hash uses one
lane.

A hash takes tens of milliseconds and 19 MiB: never on the event loop of the master. `PasswordHasher` runs them
on two threads of its own, and hands each result back through `post`, the `post()` of the loop:

```cpp
PasswordHasher hasher([&](std::function<void()> function) { loop.post(std::move(function)); });

if (!hasher.verify(*account.passwordHash, password, [](bool matches) { /* on the loop */ })) {
    // The queue is full: the lobby answers BUSY at once
}
```

| | |
|---|---|
| Threads | 2: at most two hashes at once, 38 MiB |
| Queue | At most 32 requests not delivered yet, those running included. A request holds its place until its result is delivered on the loop, so that a slow loop slows the requests too. Beyond, `hash()` and `verify()` return `false` at once, and `done` is never called |
| Failure | Argon2 failing (memory missing) gives an empty hash, logged |
| Destruction | The requests not started are dropped, those running finish; no `done` is called once the hasher is gone. `post` must stay valid until then |

The password is never logged, at any level: neither by the hasher nor by the store, which only ever sees hashes.

## The tables

The first migrations of the accounts in `ServerMigrations`, in the SQL both engines share
([Portable SQL](database.md#portable-sql)): dates in Unix milliseconds, every integer a `BIGINT`, the hashes in
lower-case hexadecimal.

| Table | Columns | Migration |
|---|---|---|
| `accounts` | `id`, `username` (as written), `username_key` (lower case, unique), `email` and `email_key` (lower case, unique, both `NULL` for an account of Discord only), `email_verified_at`, `password_hash` (`NULL` for an account of Discord only), `role` (`player` or `admin`), `banned_at` and `ban_reason` (`NULL` unless banned), `code_failures` and `code_failures_day` (the wrong codes of the day), `created_at`, `last_seen_at` | 3 |
| `account_sessions` | `token_hash` (SHA-256 of the token, primary key), `account_id` (deleted with its account), `created_at`, `last_used_at`, `expires_at`, `client` (`game` or `panel`) | 4 |
| `account_codes` | `account_id` (deleted with its account), `purpose` (`verify` or `reset`), `code_hash`, `expires_at`, `attempts`; one code per account and purpose | 5 |

`Accounts/AccountStore.hpp` gives plain functions on a `Database`, as `Master/AdminStore.hpp` does: the master
runs them on its database thread, the tests anywhere. Each takes the time it runs at, so that the tests choose
it.

| Function | Does |
|---|---|
| `createAccount(database, account, now)` | An account, its email not verified; or `UsernameTaken`, then `EmailTaken`, ignoring case |
| `findAccount`, `findAccountByUsername`, `findAccountByEmail` | The account, ignoring case (and the spaces around an email) |
| `markEmailVerified(database, id, now)` | Once its code is accepted |
| `setPassword(database, id, hash)` | A new hash, and **every session of the account ends**: whoever knew the old password is signed out |
| `banAccount(database, id, reason, now)`, `unbanAccount` | Banned, **every session ends**, and none resumes until unbanned |
| `purgeUnverifiedAccounts(database, now)` | Deletes the accounts with an email not verified within **24 hours**, with their codes and sessions. An account without email (Discord) is never purged |

### Sessions

A session is a **token of 256 random bits** (`rtype::network::randomBytes`), given to the client once at sign-in,
which keeps it for this server and sends it back to resume. The database keeps only its **SHA-256**:

- **a copy of the database opens no session**: a backup that leaks, or a reader of the database, gets hashes
  that cannot be turned back into tokens;
- a plain SHA-256 is enough, unlike a password: the token has 256 random bits, nothing to guess from a list,
  so it needs neither a salt nor a slow hash; and the hash is the key of its row, found by an index.

| Function | Does |
|---|---|
| `createSession(database, account, client, now)` | A new token; the account is seen |
| `resumeSession(database, token, client, now)` | The account, its session pushed back to `now` plus its lifetime; or nothing if the token is unknown, expired (it is then deleted), of the other client, or its account banned |
| `endSession(database, token)` | Signing out |
| `endAllSessions(database, account)` | Every session of the account |

A session of the game lasts **30 days without use**, each use pushing it back; a session of the panel (step 12)
**12 hours**, and one cannot stand for the other.

### Codes

A code verifies the email of a new account, or sets a new password, forgotten: **6 digits**, drawn evenly from
the random generator, valid **15 minutes**. A new code of the same purpose replaces the one before it. The
database keeps the SHA-256 of the account, the purpose and the code, compared in constant time: the same code of
another account or purpose does not match. With a million codes, the hash does not stop a reader of the database
who tries them all; the codes are short-lived and limited instead:

| Limit | Beyond | `checkCode()` gives |
|---|---|---|
| A right code | Used once, then deleted | `Accepted`, then `NoCode` |
| 15 minutes | Deleted | `Expired` |
| **5 wrong attempts per code** | Refused, even right: a new code is needed | `TooManyAttempts` |
| **20 wrong attempts per account per day** (UTC), across all its codes | No code is checked until the next day, even right | `DailyLimitReached` on the 20th, so that the owner can be told by email; `DailyLimit` after |

Without the daily limit, an attacker could ask for a new code after every 5 attempts, and try 5 of the million
each time. With it, an account gives away at most 20 tries a day: one chance in 50,000.

## Tests

`tests/server_account_store.cpp` (tags `[server]`, `[accounts]`, `[database]` for those on the database), every
test of the database on SQLite and on PostgreSQL:

- the rules: usernames, emails and passwords accepted and refused, characters counted rather than bytes;
- the hasher: the Argon2id vector of the reference implementation, a hash with the default parameters checked
  and a wrong password refused, a new salt each time; on its threads, results delivered through `post`, and the
  queue full refusing at once until the results are delivered;
- the accounts: found by id, username or email ignoring case, unique usernames and emails ignoring case;
  created, verified with a code, signed in, then resumed a day later from the token alone, the database keeping
  only its hash;
- sessions pushed back by each use then expired, those of the panel apart; signing out, a new password and a
  ban ending them;
- codes used once, bound to their account and their purpose, replaced, expired, refused after 5 attempts, and
  the daily limit across several codes of both purposes, lifted the next day;
- the purge of accounts not verified within 24 hours, with their codes and sessions, those of Discord kept.

`tests/crypto.cpp` (tag `[network]`, `[crypto]`): SHA-256 against the digests of FIPS 180-2, random bytes that
differ from one draw to the next.
