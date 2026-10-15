# Network protocol

!!! warning "Draft — not yet implemented"

    This specification defines the structure and conventions of the protocol. Message definitions are
    added as they are implemented, using the [message template](../contributing/templates.md#protocol-message).

| | |
|---|---|
| **Status** | Draft |
| **Version** | 0 |
| **Transport** | TCP with TLS 1.3 for the lobby, encrypted UDP for the games ([ADR 0012](../decisions/0012-tcp-lobby-udp-game.md), [ADR 0014](../decisions/0014-encryption.md)) |

## 1. Introduction

This document specifies the binary protocol used between `r-type_client` and `r-type_server`. Its goal is
that anyone can write a compatible client or server by reading this document only.

## 2. Conventions

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT** and **MAY** are to be interpreted as
described in [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119).

- All multi-byte integers are transmitted in **network byte order** (big-endian).
- Sizes are given in bytes.
- Integer types: `u8`, `u16`, `u32`, `i16`, `i32` (unsigned / signed, size in bits).

## 3. Transport

- All in-game communication (entities, movements, events) **MUST** use UDP.
- One UDP datagram carries exactly one message.
- A datagram **MUST NOT** exceed the maximum size defined in [section 4](#4-message-format), to avoid IP
  fragmentation.

## 4. Message format

Every message starts with a common header, followed by a payload whose layout depends on the message type.

| Offset | Field | Type | Description |
|---|---|---|---|
| — | *to be defined* | | |

## 5. Messages

### 5.1 Client to server

*No message defined yet.*

### 5.2 Server to client

*No message defined yet.*

## 6. Error handling

A receiver **MUST** silently discard any datagram that is malformed: too short, unknown type, payload size
inconsistent with the type, or out-of-range values. A malformed datagram **MUST NOT** crash the receiver
nor cause unbounded memory allocation.

## 7. Security considerations

*To be completed: spoofing, flooding, and validation of client inputs by the server.*
