## Client
- [x] prompt
- [x] sending message
- [x] bug: do not allow sending empty message
- [x] wrapping messages
- [x] bug: when sending message after diconnect (serverfd?)
- [x] Handle disconnection thiin a thread, the best way would be
- [x] Add limit_y to printf_wrap
- [x] id2string on clients
- [x] ctrl+z to suspend
- [x] bug: when reconnecting nrecv != -1
- [x] bug: when disconnecting
- [x] use error type success to say that authentication succeeded

## Server
- [x] import clients

## Common
- [x] handle messages that are too large
- [x] refactor i&self into conn
- [x] logging
- [x] Req|Inf connection per client
- [x] connect/disconnect messages
- [x] bug: blocking after `Added pollfd`, after importing a client and then connecting with the
  id/or without?  After reconnection fails chatty blocks (remove sleep)
- [x] connect/disconnections messages
- [x] asserting, logging if fail / halt execution
