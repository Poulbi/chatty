# chatty: The terminal chat application

## Overview
`chatty` is a terminal chat application.
Included is also a server.

### Client features
- users are saved
- you can send messages
- you can pause and resume with `Ctrl-Z` and the `fg` command
- messages can have basic markdown formatting
- basic shortcuts for editing the message
- reconnecting on 
#### Shortcuts
- `Ctrl+C` | `Ctrl+D`: quits
- `Ctrl+U`: Erase input line
- `Ctrl+W`: Erase word behind cursor
- `Ctrl+Y`: Paste clipboard into input field

### Server features
- multiple users
- recovering on invalid messages
- send "connected"/"disconnected" messages to other clients

## Build
Run the build script.
```sh
./source/build.sh
```

## Try it out
Run the server with
```sh
./build/server
```
> You can stop it with `Ctrl-D`

In another prompt, start a client with
```sh
./build/chatty Poulbi
```

# Resources
- terminal library: [Termbox2](https://github.com/termbox/termbox2)
- source code I looked at:
    - https://github.com/git-bruh/matrix-tui
    - https://github.com/NikitaIvanovV/ictree
- *mmap & gdb*: [Tsoding - "Why linux has this syscall?" ](https://youtu.be/sFYFuBzu9Ow?si=CX32IzFVA8OPDZvS)
- *pthreads*: [C for dummies](https://c-for-dummies.com/blog/?p=5365)
- *unicode and wide characters*: [C for dummies](https://c-for-dummies.com/blog/?p=2578)
- *sockets*: [Nir Lichtman - Making Minimalist Chat Server in C on Linux](https://www.youtube.com/watch?v=gGfTjKwLQxY)
- syscall manpages `man`
- UTF8 Comprssion: [Casey Muratori - Simple RLE Compressor](https://www.youtube.com/watch?v=kikLEdc3C1c&t=6312s)
