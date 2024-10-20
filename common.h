#include <stdint.h>
#include <stdio.h>

#define PORT 9983
// max buffer size sent over network
// TODO: choose a better size
#define BUF_MAX 256
// max size for a message sent
#define MESSAGE_MAX 256
// max length of author field
#define MESSAGE_AUTHOR_LEN 12
// max length of timestamp field
#define MESSAGE_TIMESTAMP_LEN 9
// current user's name
#define USERNAME "Jef Koek"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

// To serialize the text that could be arbitrary length the lenght is encoded after the author
// string and before the text.
struct message {
    char author[MESSAGE_AUTHOR_LEN];
    char timestamp[MESSAGE_TIMESTAMP_LEN]; // HH:MM:SS
    u16 len;
    char *text;
};

// printf without buffering using write syscall, works when using sockets
void writef(char *format, ...);

u16 str_len(char *str);
void str_cpy(char *to, char *from);

// save the message msg to file in binary format, returns zero on success, returns 1 if the msg.text
// was empty which should not be allowed.
u8 save_message(struct message *msg, FILE *f);
// load the message msg from file f, returns zero on success, returns 1 if the msg.text
// was empty which should not be allowed.
u8 load_message(struct message *msg, FILE *f);
