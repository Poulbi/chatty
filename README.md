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
- [x] bug: when sending message after diconnect (serverfd?)
- [x] Handle disconnection thiin a thread, the best way would be
- [x] Add limit_y to printf_wrap
- [x] id2string on clients
- [x] ctrl+z to suspend
- [ ] bug(tb_printf_wrap): text after pfx is wrapped one too soon

## server
- [x] import clients
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
- [x] handle messages that are too large
- [x] refactor i&self into conn
- [x] logging
- [x] Req|Inf connection per client
- [x] connect/disconnect messages
- [ ] bug: blocking after `Added pollfd`, after importing a client and then connecting with the
  id/or without?  After reconnection fails chatty blocks (remove sleep)
- [ ] connect/disconnections messages
- [ ] use IP address / domain
- [ ] chat history
- [ ] asserting, logging if fail / halt execution
- [ ] compression

## Protocol
- see `protocol.h` for more info
- [ ] make sections per message
- request chat logs from a certain point up to now (history)
- connect to a specific room

- The null terminator must be sent with the string.
- The text can be arbitrary length

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
- syscall manpages `man`
