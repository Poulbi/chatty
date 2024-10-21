# Chatty
The idea is the following:
- tcp server that you can send messages to
- history upon connecting
- date of messages sent
- authentication
- encrypted communication (tls?)
- client for reading the messages and sending them at the same time

# Common
- use memory arena's to manage memory
- manage memory for what if it will not fit
    - for just do nothing when the limit is reached

# Server
- min height & width
- wrapping input
- [ ] history
- [x] max y for new messages and make them scroll
- [x] check resize event
- [x] asynchronously receive/send a message
- [x] send message to all other clients
- [x] fix receiving messages with arbitrary text length
- [x] bug: server copying the bytes correctly

- rooms
- encryption
- authentication

# Client
- bug: when having multiple messages and resizing a lot, the output will be in shambles
- bug: when resizing afters sending messages over network it crashes
- bug: all messages using the same buffer for text
- use pointer for add_message
- validation of sent/received messages
- handle disconnection

# Questions
- will two consecutive sends be read in one recv
- can you recv a message in two messages

# Message protocol
Version 1
1 version byte
4 length bytes
12 message_author bytes
- 11 chars + \0
9 timestamp bytes
- 8chars + \0
x text bytes
- x bytes + \0

The variable text bytes can be calculated by substracting the author and timestamp from the length
