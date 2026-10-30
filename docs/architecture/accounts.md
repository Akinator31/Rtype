# Accounts

The accounts of the players of a server ([ADR 0024](../decisions/0024-player-accounts.md)): what makes a valid
username, email and password, how a password is hashed, what the master keeps of the accounts, their sessions
and the codes, how it sends those codes by email, and how its lobby answers the requests of the accounts of
[lobby protocol 3](../protocol/lobby.md#6-accounts). The client offers them from its Online menu
([Playing online, accounts](client-online.md#accounts)). To give a server its accounts, see
[Set up accounts](../guides/set-up-accounts.md).

| | |
|---|---|
| Code | `Accounts/AccountRules.{hpp,cpp}` (the rules, pure), `Accounts/PasswordHasher.{hpp,cpp}` (Argon2id, on its threads), `Accounts/AccountStore.{hpp,cpp}` (the tables), `Accounts/MailMessages.{hpp,cpp}` (what the mails say, pure), `Accounts/Mailer.{hpp,cpp}` (the interface, and a mail written for SMTP), `Accounts/CurlMailer.{hpp,cpp}` (the mail server, on its thread), `Accounts/AccountService.{hpp,cpp}` (the requests of the lobby, on the loop) in `apps/server/src`; `rtype/network/Crypto.hpp` (random bytes, SHA-256) |
| Threads | The rules and the messages anywhere; the store on the database thread; the hashes on the two threads of the hasher; the mails on the thread of the mailer; the service and the lobby on the event loop |
| Tables | `accounts`, `account_sessions`, `account_codes` (migrations 3 to 5 of `ServerMigrations`) |
| Dependencies | The [reference implementation of Argon2](https://github.com/P-H-C/phc-winner-argon2) (CC0 or Apache 2.0), through CPM with a hash; TF-PSA-Crypto, through `libs/network`; libcurl for SMTP ([Web server](web.md#libcurl)) |
| Tests | `tests/server_account_store.cpp`, tags `[accounts]` and `[database]`; `tests/server_accounts.cpp` (the lobby), tags `[accounts]` and `[database]`; `tests/server_mail.cpp`, tag `[mail]`; `tests/crypto.cpp`, tag `[crypto]` |

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
| `deleteAccount(database, id)` | The account, its sessions and its codes, at the request of its owner; its administration actions stay, anonymized ([Deleting an account](#deleting-an-account)) |
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

## The mails

The master sends four mails, each in **English and French** by the locale the client gives (`mailLanguage()`:
`fr`, `fr-FR`, `fr_CA` are French, anything else English), as text and as HTML that say the same:

| Mail | `Accounts/MailMessages.hpp` | Sent when |
|---|---|---|
| The verification code | `verificationMail(language, username, code, validFor)` | An account is created, signs in unverified, or asks `RESEND_CODE` |
| The reset code | `resetMail(language, username, code, validFor)` | A forgotten password is asked for |
| "You already have an account" | `existingAccountMail(language, username)` | Someone registers with an email that has an account: its holder learns it, the one who registered learns nothing from the answer of the lobby |
| Too many wrong codes | `tooManyCodesMail(language, username)` | The daily limit of the codes is reached (`DailyLimitReached`) |

A code is shown large, on a line of its own; the mail says how long it is valid, and holds **no link**: the
player types the code in the game, so that a mail that copies ours cannot lead anywhere with a link of its own.
The HTML is in the colours of the game, black and silver, with its styles inline, the only ones mail clients
keep; what it is given (the username) is escaped.

`formatMail()` (`Accounts/Mailer.hpp`) writes a mail as SMTP carries it: `Date`, `From`, `To`, `Subject`,
`Message-ID`, `Auto-Submitted: auto-generated`, then a `multipart/alternative` of the text and the HTML, each in
UTF-8 and base64, every line under 78 characters and ended by CRLF. A subject or a name of the sender that is not
ASCII goes in encoded words of UTF-8 (RFC 2047), each a whole number of characters.

### The mailer

The lobby gives its mails to a `Mailer`, and returns at once: `send()` only queues. `CurlMailer` sends them
through the mail server of the settings ([`[mail]`](../guides/configure-a-server.md#the-keys)) with libcurl, on
**a thread of its own**, never on the event loop:

| | |
|---|---|
| Security | STARTTLS required with `"starttls"` (a server that does not offer it is refused, never used in the clear), TLS from the start with `"tls"`; the certificate of the server checked against the certificates of the system. `"none"` for a relay on the same machine or a test server, without a password |
| Queue | At most 64 mails not sent yet, the one being sent and those waiting for another try included. Beyond, `send()` gives `MailQueued::Busy` at once: `MAIL_UNAVAILABLE` for the lobby |
| Retries | A mail the server refuses, does not take or cannot be reached for is tried again after 30 s, 1 min, then 2 min: 4 tries in all, then given up and logged. A mail waiting for its next try holds back none of the others |
| Per address | At most **5 mails an hour to one address**, ignoring case, counted when they are queued; beyond, `MailQueued::AddressLimit`. Whoever types the email of another cannot flood their mailbox |
| Addresses | Only one address, as `normalizeEmail()` keeps it: a line break, a name or a second address gives `MailQueued::InvalidAddress`, never a header of the mail |
| Timeouts | 10 s to connect, 30 s for a whole mail |
| Destruction | The mails not sent yet are dropped, and the one being sent is cut, without waiting for its timeout |

The logs name a mail by its subject and its address masked, `p…@example.org`: never its content, where a code
is, nor the password of the mail server.

### At start

A master with accounts checks its mail server before it starts, `--check` included: `checkMailServer()`
connects, starts TLS, signs in, sends `NOOP`, then `QUIT`, so that **nothing is sent**. A mail server it cannot
use stops it with the reason, as a port taken does: the codes could not leave, and a player would wait for a
mail that never comes.

```
ERROR master   Cannot use the mail server mail.example.org:587: Login denied
```

In a container that restarts on failure, a mail server down at start makes the server restart again and again,
until it answers ([Set up accounts](../guides/set-up-accounts.md#result)).

## In the lobby

`Accounts/AccountService.hpp` answers the requests of the accounts that the lobby receives, `REGISTER` to
`DELETE_ACCOUNT`, resumes the session of a `HELLO` with a token, and checks the names of guests. It runs on the
event loop, and does its work on the threads of the others: the database thread for the store, the hasher for
the passwords, the mailer for the mails. A master has one when its settings enable `[accounts]`
(`Lobby::setAccounts()`); without it, the lobby answers `ACCOUNTS_DISABLED` and a token `SESSION_EXPIRED`, as
before the accounts.

```cpp
AccountService accounts(loop, databaseThread, hasher, mailer, {.guests = settings.accounts->guests});
lobby.setAccounts(&accounts);
```

### One request at a time

The lobby answers the requests of a connection in the order it receives them
([lobby protocol, section 4](../protocol/lobby.md#4-requests-replies-and-notifications)), but a request of the
accounts takes a round trip to the database, a hash of tens of milliseconds, sometimes both. So each connection
handles **one request at a time**: the requests that follow wait in the queue `CREATE_GAME` already uses
(`LobbyLimits::maxWaitingRequests`, 32, then the client is disconnected), and `Lobby::drain()` answers them once
the reply comes. A `HELLO` waits the same way on a server with accounts, since a token or the name of a guest is
checked in the database. The reply of the service always comes later, on the loop, never inside the call, and
never after the service is gone.

### The requests

| Request | Work | Reply |
|---|---|---|
| `HELLO` with a token | `resumeSession()` | `WELCOME` with the account, the username as the name of the player; `SESSION_EXPIRED` (then closed) if the token is unknown, expired, of the panel, or its account banned (a ban ends its sessions) |
| `HELLO` of a guest | `findAccountByUsername()`, only for a name that could be a username | `WELCOME`, the name with the mark of the guests; `NAME_RESERVED` if it is the username of an account, ignoring case |
| `REGISTER` | The rules, the hash, `createAccount()` and `createCode()`, the mail | `CODE_SENT`, whether the email has an account or not: a new email gets a code, one that has an account the "you already have an account" mail, and no account is made. `USERNAME_TAKEN` is the only thing it tells |
| `VERIFY_EMAIL` | `checkCode()`, `markEmailVerified()`, `createSession()` | `SIGNED_IN`; `INVALID_CODE` for a wrong code or a login without account |
| `RESEND_CODE` | `createCode()`, the mail, for an account not verified | `CODE_SENT` whatever the login; `TOO_MANY_ATTEMPTS` within a minute of the last one for the same login |
| `SIGN_IN` | The account by username or email, the hash checked, `createSession()` | `SIGNED_IN`; `INVALID_CREDENTIALS` for a wrong login or password alike; only after the right password, `BANNED` (then closed), or `EMAIL_NOT_VERIFIED` with a new code sent |
| `SIGN_OUT` | `endSession()` | `DONE`, then closed |
| `FORGOT_PASSWORD` | `createCode()` and the mail, if the email has an account | `CODE_SENT` whatever the email |
| `RESET_PASSWORD` | The hash, `checkCode()`, `setPassword()` (every session ends), the email verified, `createSession()` | `SIGNED_IN` |
| `DELETE_ACCOUNT` | The hash checked, `deleteAccount()` | `DONE`, then closed |
| `DISCORD_SIGN_IN`, `DISCORD_CONFIRM`, `CHOOSE_USERNAME` | None: Discord comes in step 11 | `ACCOUNTS_DISABLED` |

In a game, every request of the accounts gets `ALREADY_IN_GAME`, before any work: a player signs in, out, or
deletes its account from the lobby only, so that nobody changes identity in a game. A request that signs in gives
the connection its account: its name is now the username, for the next games,
and the administration sees the account (`AdminPlayer::accountId`). A database that does not answer, a hasher or
a database thread with a full queue, give `BUSY`; a mailer with a full queue `MAIL_UNAVAILABLE`.

Nothing tells the accounts of others:

- `REGISTER` hashes the password whether the email has an account or not, so that both answers take as long;
- `SIGN_IN` with a login that has no account checks the password all the same, against a hash made at start
  with the parameters of the hasher;
- `RESEND_CODE` counts its minute by the login, before the database, and `FORGOT_PASSWORD` answers `CODE_SENT` for
  any email;
- an address that got its 5 mails of the hour gets its reply as if the mail had gone: only the logs say it did
  not.

### The limits

`AccountLimits`, kept in memory by the service and lost at a restart:

| Limit | Default | Beyond |
|---|---|---|
| Requests of the accounts from one IP address | 10 a minute, every connection of the address together | `TOO_MANY_ATTEMPTS` |
| Wrong passwords of one account in a row (`SIGN_IN`, `DELETE_ACCOUNT`) | 10 | Paused 15 minutes: `TOO_MANY_ATTEMPTS`, the password not checked, even right |
| `RESEND_CODE` for one login | Once a minute | `TOO_MANY_ATTEMPTS` |
| Wrong codes of an account | 5 per code, 20 a day ([Codes](#codes)) | `TOO_MANY_ATTEMPTS`; on the 20th, the holder of the account gets the "too many wrong codes" mail |
| The hasher, the database thread | Their queues | `BUSY` |

With 10 wrong passwords then a pause of 15 minutes, a guesser gets about a thousand tries a day on one account,
and needs as many addresses to go faster, each limited too.

### Guests

`accounts.guests` decides whether players can play without an account. A master without guests welcomes them all
the same, so that they can sign in, and answers `GUESTS_DISABLED` to their `CREATE_GAME` and `JOIN_GAME`.

On a server with accounts, a guest carries **`~` before its name** (`markGuest()`, `GuestMark`), in the lobby, in
the panel and on the score board: no username has it, so a guest named like an account, or with a lookalike
letter of another alphabet, never looks like it. The name is cut to keep both in 16 bytes, on a whole character.
A server without accounts shows the names as they are.

### Deleting an account

`DELETE_ACCOUNT`, with the password of the account, deletes it with its sessions and its codes. Its
administration actions stay, so that the record keeps what was done, without who it was
(`anonymizeActions()`):

- the actions it did, as an administrator (step 12), have the actor `deleted`;
- a `kick` of the player while signed in (its `details` hold `account`) loses its `name`, which becomes
  `deleted`, its `address` and its `account`;
- an `end_game` with the player in it (its `details` hold `accounts`) has its username replaced by `deleted` in
  `players`, and its id taken out of `accounts`.

An account without password, made with Discord, confirms with Discord (step 11): until then, it gets
`DISCORD_NOT_CONFIRMED`.

### The purge

The accounts with an email not verified within 24 hours are deleted at start, then every hour
(`purgeUnverifiedAccounts()` on the database thread), with their codes: an email typed by mistake, or by someone
else, does not stay taken.

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

`tests/server_accounts.cpp` (tags `[server]`, `[accounts]`, `[database]`), a real lobby with its TLS clients, the
real hasher with cheap parameters, and a mailer of the tests that keeps the codes
(`tests/helpers/FakeMailer.hpp`), on SQLite and on PostgreSQL:

- a player who registers, reads its code, verifies, disconnects, and comes back signed in from its token alone;
- a register with an email that has an account answered `CODE_SENT` like any other, its holder told, no account
  made; a username taken ignoring case; the fields checked before anything is kept or sent;
- signing in by username or email, a wrong password and an unknown login answered alike, `ALREADY_SIGNED_IN`;
  an account not verified sent a new code only after its right password;
- codes wrong, unknown, expired and tried too often; `RESEND_CODE` once a minute whatever the login; a forgotten
  password set again, ending every session; signing out; a token unknown, expired or of the panel;
- a banned account refused after its right password, its sessions ended; a guest refused the name of an
  account, and marked; a master without guests;
- requests sent back to back, `HELLO` included, answered in order; the requests of the accounts refused in a game
  of a real instance;
- the limits: per address, per account, the codes of a day mailed to the holder, a full hasher, a full mailer;
- the purge at start; an account deleted with its password, its actions anonymized; Discord not offered yet;
- the logs, which hold neither the password, nor the code, nor the token, nor the email.

`tests/server_mail.cpp` (tags `[server]`, `[mail]`), against a small SMTP server of the tests
(`tests/helpers/SmtpServer.hpp`) in the clear, with STARTTLS, or TLS from the start, with or without an account:

- the messages: the language of a locale, the code, its expiry and no link in both languages, the HTML escaped;
- the format: the headers, CRLF and the length of every line, the parts decoded back, a long subject that is not
  ASCII folded into encoded words of whole characters, the name of the sender quoted or encoded;
- a code mail received in English and in French; the account of the mail server accepted, or refused; the check
  that sends nothing; a server gone, its mails given up after their tries; a mail refused for now then received,
  and one waiting for another try holding none back; the full queue refusing at once; the limit per address,
  ignoring case, lifted once its window has passed; an address with a line break or a name never sent; the
  mailer destroyed while it sends; STARTTLS required, a certificate not trusted refused, TLS from the start;
- the logs, which hold neither the password, nor the code, nor the address;
- a master with accounts checking its mail server at start, and stopping when it is gone.

The TLS of the fake server needs OpenSSL: those tests are skipped on Windows, whose libcurl uses Schannel. A
hidden test sends a code mail in each language through a real mail server, by hand:

```sh
RTYPE_TEST_MAIL_SETTINGS=server.toml RTYPE_TEST_MAIL_TO=you@example.org ./build/bin/rtype_tests "[.mail-server]"
```

`tests/crypto.cpp` (tag `[network]`, `[crypto]`): SHA-256 against the digests of FIPS 180-2, random bytes that
differ from one draw to the next.
