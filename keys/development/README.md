# Development key pair

`server.key` is committed **on purpose**: it is the key of local servers and tests, not a secret
([ADR 0020](../../docs/decisions/0020-server-key-distribution.md)).

- Debug builds trust it: a Debug server uses it when it has no `server.key`, and a Debug client connects to it
  without asking.
- Release builds refuse it: a release client never connects to a server that presents it, and a release
  server does not start with it.

Never use it for a server that players reach. The key of the official server is made on its host and never
leaves it: see [Generate the server keys](../../docs/guides/generate-the-server-keys.md).
