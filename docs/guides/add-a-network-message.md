# How to add a network message

This guide adds a message to the protocol, from its specification to its tests, taking `CREATE_GAME` of the
lobby as the example. It is for anyone who needs the client and the server to exchange something new.

## Prerequisites

- The project builds: see [Build](../getting-started/build.md).
- You have read the [protocol conventions](../protocol/index.md#2-conventions) (types, byte order) and the
  serialization part of [Networking](../architecture/networking.md#binary-serialization).

## Steps

1. **Specify the message first**, in the page of its protocol: [lobby](../protocol/lobby.md),
   [game](../protocol/game.md) or [master and instances](../protocol/instances.md). Copy the
   [message template](../contributing/templates.md#protocol-message), take the next free type in the range of
   its direction, and describe every field with its type and its allowed values:

    ```markdown
    #### CREATE_GAME (`0x03`)

    | Offset | Field | Type | Description |
    |---|---|---|---|
    | 0 | Name | `str8` | Name of the game, 1 to 32 bytes |
    | 1 + n | Max players | `u8` | 1 to 4 |
    ```

    The specification is the reference: someone must be able to write a compatible program from it alone.
    Changing an existing message increases the [protocol version](../protocol/index.md#4-versioning).

2. **Write the message as a struct**: its type, its fields, with the C++ types matching the specification.

    ```cpp
    struct CreateGame {
        static constexpr std::uint8_t Type = 0x03;
        static constexpr std::size_t MaxNameLength = 32;
        static constexpr std::uint8_t MaxPlayers = 4;

        std::string name;
        std::uint8_t maxPlayers = MaxPlayers;
    };
    ```

3. **Write `encode()`**: the fields in the order of the specification, nothing else.

    ```cpp
    void encode(const CreateGame &message, rtype::network::Writer &writer)
    {
        writer.writeString(message.name);
        writer.writeU8(message.maxPlayers);
    }
    ```

    The type byte and the size of the frame are written by the layer that sends the message, not here.

4. **Write `decode()`**: read every field in the same order, refuse the values the specification forbids with
    `reader.fail()`, and end with `reader.finish()`, which also refuses bytes left after the last field.

    ```cpp
    std::optional<CreateGame> decode(rtype::network::Reader &reader)
    {
        CreateGame message;
        message.name = reader.readString(1, CreateGame::MaxNameLength);
        message.maxPlayers = reader.readU8();
        if (message.maxPlayers < 1 || message.maxPlayers > CreateGame::MaxPlayers) {
            reader.fail();
        }
        if (!reader.finish()) {
            return std::nullopt;
        }
        return message;
    }
    ```

    - Do not test the reader after each field: once a read fails, the following ones do nothing, and
      `finish()` reports the first problem.
    - Never size memory with a count read from the network (`reserve(count)`): for a list, loop while
      `reader.ok()`.
    - Never throw, never log, never touch anything but the message: the caller logs the reason with
      `toString(reader.error())`, and the decoder stays a pure function that can be fuzzed.

5. **Test it** in `tests/`, with the `[network]` tag. Test the round trip, the bytes of the specification, and
    every way the message can be refused:

    ```cpp
    TEST_CASE("CREATE_GAME survives a round trip", "[network]")
    {
        Writer writer;
        encode(CreateGame{.name = "Room", .maxPlayers = 2}, writer);
        Reader reader(writer.bytes());

        const auto message = decode(reader);
        REQUIRE(message);
        CHECK(message->name == "Room");
        CHECK(message->maxPlayers == 2);
    }

    TEST_CASE("A cut CREATE_GAME is refused at every length", "[network]")
    {
        Writer writer;
        encode(CreateGame{.name = "Room", .maxPlayers = 2}, writer);
        for (std::size_t size = 0; size < writer.size(); ++size) {
            Reader reader(writer.bytes().first(size));
            CHECK_FALSE(decode(reader));
        }
    }

    TEST_CASE("CREATE_GAME with 5 players is refused", "[network]")
    {
        Writer writer;
        writer.writeString("Room");
        writer.writeU8(5);
        Reader reader(writer.bytes());

        CHECK_FALSE(decode(reader));
        CHECK(reader.error() == ReadError::InvalidValue);
    }
    ```

    Add a test for each other rejection: empty or too long name, invalid text, byte left at the end.

## Result

`./build/bin/rtype_tests "[network]"` passes, and the specification describes the message. Sending it and
dispatching the received messages by their type are done by the layer of its protocol: the TCP framing and the
lobby, then the datagram sessions, in the next steps of the "Server & networking" work.
