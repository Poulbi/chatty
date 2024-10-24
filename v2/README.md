# Chatty
The idea is the following:
- tcp server that you can send messages to
- history upon connecting
- date of messages sent
- authentication
- encrypted communication (tls?)
- client for reading the messages and sending them at the same time

- rooms
- encryption
- authentication

## client
- wrapping messages
- prompt
- sending message

## server
- log messages
- check if when sending and the client is offline (due to connection loss) what happens
- timeout on recv?

## common
- handle messages that are too large
- connect/disconnections messages
- use IP address / domain
- chat history

## Protocol
For now the protocol consists of sending Message type over the network, but in the future something
more flexible might be required.  Because it will make it easier to do things like:
- request chat logs up to a certain point
- connect to a specific room
- connect/disconnect messages

- The null terminator must be sent with the string.
- The text can be arbitrary length
- [ ] use char text[]; instead of char*

- todo: compression?

## Arena's
1. There is an arena for the messages' texts (`msgTextArena`) and an arena for the messages
   (`msgsArena`).
2. The `Message.text` pointer will point to a text buffer entry in `msgTextArena`
3. Good way to do this, if you have message `M`.
```c
M.text = ArenaPush(msgTextArena, M.text_len);
```
Notice, that this depends on knowing the text's length before allocating the memory.

