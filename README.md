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
- [ ] bug: when connecting two clients of the same account
- [ ] bug: wrapping does not work and displays nothing if there is no screen space
- [ ] bug: reconnect does not work when server does not know id
- [ ] markup for messages
- [ ] convert tabs to spaces

## server
- [ ] check that fds arena does not overflow
- [ ] check if when sending and the client is offline (due to connection loss) what happens
- [ ] timeout on recv?
- [ ] use threads to handle clients/ timeout when receiving because a client could theoretically
  stall the entire server.
- [ ] do not crash on errors from clients
    - implement error message?
    - timeout on recv with setsockopt
- [ ] theoretically two clients can connect at the same time.  The uni/bi connections should be
      negotiated.

## common
- [ ] use IP address / domain
- [ ] chat history
- [ ] rooms
- [ ] compression

## Protocol
- see `protocol.h` for more info

- The null terminator must be sent with the string.
- The text can be arbitrary length

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
- syscall manpages `man`
