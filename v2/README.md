# Chatty
The idea is the following:
- tcp server that you can send messages to
- history upon connecting
- date of messages sent
- client for reading the messages and sending them at the same time
- rooms
- encryption
- authentication

## client
- [x] prompt
- [x] sending message
- [x] bug: do not allow sending empty message
- [x] wrapping messages
- [ ] bug: when sending message after diconnect (serverfd?)

## server
- [ ] log messages to file (save history)
- [ ] check if when sending and the client is offline (due to connection loss) what happens
- [ ] timeout on recv?

## common
- [ ] handle messages that are too large
- [ ] connect/disconnections messages
- [ ] use IP address / domain
- [ ] chat history

## Protocol
For now the protocol consists of sending Message type over the network, but in the future something
more flexible might be required.  Because it will make it easier to do things like:
- request chat logs up to a certain point
- connect to a specific room
- connect/disconnect messages

- The null terminator must be sent with the string.
- The text can be arbitrary length

- [ ] compression

## Arena's
1. There is an arena for the messages' texts (`msgTextArena`) and an arena for the messages
   (`msgsArena`).
2. The `Message.text` pointer will point to a text buffer entry in `msgTextArena`
3. Good way to do this, if you have message `M`.
```c
M.text = ArenaPush(msgTextArena, M.text_len);
```
Notice, that this depends on knowing the text's length before allocating the memory.

## Strings
- the length of a string (eg. `Message.text_len`) always **excludes** the null terminator unless stated explicitly
- the `#define *_LEN` are the max length **including** the null terminator 

## Keybinds
- `Ctrl+C` | `Ctrl+D`: quits
- `Ctrl+U`: Erase input line
- `Ctrl+W`: Erase word behind cursor


## Resources I used for building this
- source code I looked at:
    - https://github.com/git-bruh/matrix-tui
    - https://github.com/NikitaIvanovV/ictree
    - https://github.com/termbox/termbox2
- *mmap & gdb*: [Tsoding - "Why linux has this syscall?" ](https://youtu.be/sFYFuBzu9Ow?si=CX32IzFVA8OPDZvS)
- *pthreads*: [C for dummies](https://c-for-dummies.com/blog/?p=5365)
- *unicode and wide characters*: [C for dummies](https://c-for-dummies.com/blog/?p=2578)
- *sockets*: [Nir Lichtman - Making Minimalist Chat Server in C on Linux](https://www.youtube.com/watch?v=gGfTjKwLQxY)
- syscall manpages
