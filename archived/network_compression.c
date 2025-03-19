#include <strings.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <fcntl.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

size_t
GetMaximumCompressedOutputSize(size_t FileSize)
{
    // TODO: figure out equation
    return FileSize*2 + 256;
}

u32
RLECompress(size_t InSize, u8* In, size_t MaxOutSize, u8* OutBase)
{
    u8* Out = OutBase;

#define MAX_LITERAL_COUNT 255
#define MAX_RUN_COUNT 255
    u32 LiteralCount = 0;
    u8 Literals[MAX_LITERAL_COUNT] = {0};

    u8 *InEnd = In + InSize;
    while(In < InEnd)
    {
        u8 StartingValue = In[0];
        size_t Run = 1; // first one is the character itself
        while((Run < (InEnd - In)) &&
              (Run < MAX_RUN_COUNT) &&
              (In[Run] == StartingValue))
        {
            ++Run;
        }

        if ((Run > 1) ||
            (LiteralCount == MAX_LITERAL_COUNT)) // stop doing runs when there is no
                                                // space left in the buffer.
        {
            // Encode a literal/run pair
            u8 LiteralCount8 = (u8)LiteralCount;
            assert(LiteralCount8 == LiteralCount);
            *Out++ = LiteralCount8;

            for(u32 LiteralIndex = 0;
                LiteralIndex < LiteralCount;
                ++LiteralIndex)
            {
                *Out++ = Literals[LiteralIndex];
            }
            LiteralCount = 0;

            u8 Run8 = (u8)Run;
            assert(Run8 == Run);
            *Out++ = Run8;

            *Out++ = StartingValue;

            In += Run;
        }
        else
        {
            // Buffer literals, you have to because we are encoding them in pairs
            Literals[LiteralCount++] = StartingValue;
            ++In;
        }

    }
#undef MAX_LITERAL_COUNT
#undef MAX_RUN_COUNT

    assert(In == InEnd);

    size_t OutSize = Out - OutBase;
    assert(OutSize <= MaxOutSize);

    return OutSize;
}

void
RLEDecompress(size_t InSize, u8* In, size_t OutSize, u8* Out)
{
    u8 *InEnd = In + InSize;
    while(In < InEnd)
    {
        // TODO: I think Casey made a mistake and this should be an u8
        u8 LiteralCount = *In++;
        while(LiteralCount--)
        {
            *Out++ = *In++;
        }

        // Alternate to Run
        u8 RepCount = *In++;
        u8 RepValue = *In++;
        while(RepCount--)
        {
            *Out++ = RepValue;
        }
    }

    assert(In == InEnd);
}

int main(int Argc, char* Argv[]) {

    if (Argc < 2)
    {
        fprintf(stderr, "Usage: %s [compress|decompress]\n", Argv[0]);
        return 1;
    }

    char* Command = Argv[1];

    if (!strcmp(Command, "compress"))
    {
        u32* Message = (u32*)L"abcabcbbbbbbb";
        size_t MessageSize = wcslen((const wchar_t*)Message) * 4;
        
        size_t OutBufferSize = GetMaximumCompressedOutputSize(MessageSize);
        u8 OutBuffer[OutBufferSize + 8];
        bzero(OutBuffer, OutBufferSize + 8);
        *(u32*)OutBuffer = MessageSize;

        size_t CompressedSize = RLECompress(MessageSize, (u8*)Message, OutBufferSize, OutBuffer + 8);

        s32 OutFile = open("test.compressed", O_WRONLY | O_CREAT, 0600);

        write(OutFile, OutBuffer, CompressedSize);

        fprintf(stdout, "%lu -> %lu bytes\n", MessageSize, CompressedSize);
    }
    else if (!strcmp(Command, "decompress"))
    {
        fprintf(stderr, "Not implemented yet.\n");
    }
    else
        fprintf(stderr, "Unknown command: '%s'\n", Command);

    return 0;
}
