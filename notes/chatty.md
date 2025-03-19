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
- [ ] BUG: text is not appearing after typing
- [ ] BUG: when connecting two clients of the same account
- [ ] BUG: wrapping does not work and displays nothing if there is no screen space
- [ ] BUG: reconnect does not work when server does not know id
- [ ] TODO: Convert tabs to spaces
- [ ] BUG: when using lots of markup characters
- [ ] TODO: Newline support
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
