# Chatty
- use memory arena's to manage memory
- spec for the stream protocol

# Server
The idea is the following:
- tcp server that you can send messages to
- encrypted communication
- history upon connecting
- date of messages sent
- author
- fingeprint as ID for authorship
- client for reading the messages and sending them at the same time

- min height & width
- wrapping input
- max y for new messages and make them scroll
- check resize event
- asynchronously receive/send a message
- fix receiving messages with arbitrary text length

## TODO: send message to all other clients
- implement different rooms
- implement history
 - [ ] fix server copying the bytes correctly
- implement tls


# Client
- bug: when having multiple messages and resizing a lot, the output will be in shambles
- bug: when resizing afters sending messages over network it crashes
- use pointer for add_message
- validation of sent/received messages
- handle disconnection

# Questions
- will two consecutive sends be read in one recv
- can you recv a message in two messages
