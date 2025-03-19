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
- [ ] convert tabs to spaces
- [ ] bug: when using lots of markup characters
- [ ] newline support
    - [ ] resizable box

## server
- [ ] check that fds arena does not overflow
    - free clients which disconnected and use free list to give them space
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
- `Ctrl+Y`: Paste clipboard into input field

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
- UTF8 Comprssion: [Casey Muratori - Simple RLE Compressor](https://www.youtube.com/watch?v=kikLEdc3C1c&t=6312s)

### To Read
#### C Programming
- https://www.youtube.com/watch?v=wvtFGa6XJDU
- https://nullprogram.com/blog/2023/02/11/
- https://nullprogram.com/blog/2023/02/13/
- https://nullprogram.com/blog/2023/10/08/
#### Encryption w/ Compression
- https://en.wikipedia.org/wiki/BREACH
- https://en.wikipedia.org/wiki/CRIME
- https://crypto.stackexchange.com/questions/2283/crypto-compression-algorithms
- openpgp https://www.rfc-editor.org/rfc/rfc4880
- https://security.stackexchange.com/questions/19911/crime-how-to-beat-the-beast-successor
- https://blog.qualys.com/product-tech/2012/09/14/crime-information-leakage-attack-against-ssltls
- Algorithms:
    *Symmetric*
    - AESI
    - Blowfish
    - Twofish
    - Rivest Cipher (RC4)
    *Assymetric*
    - Data Encryption Standard (DES)
    - ECDSA
    - RSA
    - Diffie-Hellman
    - PGP
    _Hash_
    - Deflate
    - Huffman Coding
    - LZ77
    Other
    - ChaCha20-Poly1305
    - AES(-GCM)
