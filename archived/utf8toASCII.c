#include <stdlib.h>
#include <stdio.h>
#include <stdio.h>
#include <locale.h>
#include <assert.h>
#include <wchar.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

static size_t
UTF8Compress(size_t InSize, wchar_t* In, size_t OutSize, u8* OutBase)
{
    wchar_t* InEnd = (wchar_t*)((u8*)In + InSize);
    u8* Out = OutBase;

#define MAX_LITERAL_COUNT 255
    u8 ASCIILiterals[MAX_LITERAL_COUNT] = {0};
    u8 ASCIILiteralsCount = 0;
    wchar_t UTF8Literals[MAX_LITERAL_COUNT] = {0};
    u8 UTF8LiteralsCount = 0;

    while (In < InEnd)
    {
        wchar_t CurrentChar = In[0];

        // Check consecutive ascii characters
        while(CurrentChar == (u8)CurrentChar &&
              ASCIILiteralsCount < MAX_LITERAL_COUNT)
        {
            ASCIILiterals[ASCIILiteralsCount++] = (u8)CurrentChar;
            CurrentChar = *++In;
        }

        while(CurrentChar != (u8)CurrentChar &&
              UTF8LiteralsCount < MAX_LITERAL_COUNT)
        {
            UTF8Literals[UTF8LiteralsCount++] = CurrentChar;
            CurrentChar = *++In;
        }

        // Encode ASCII/UTF8 pair
        *Out++ = ASCIILiteralsCount;
        for (u8 ch = 0;
             ch < ASCIILiteralsCount;
             ch++)
        {
            *Out = ASCIILiterals[ch];
            Out += sizeof(ASCIILiterals[ch]);
        }
        ASCIILiteralsCount = 0;

        *Out++ = UTF8LiteralsCount;
        for (u8 ch = 0;
             ch < UTF8LiteralsCount;
             ch++)
        {
            *(wchar_t*)Out = UTF8Literals[ch];
            Out += sizeof(UTF8Literals[ch]);
        }
        UTF8LiteralsCount = 0;

    }
#undef MAX_LITERAL_COUNT
    assert(In == InEnd);

    return Out - OutBase;
}

static void
PrintCompressedUTF8(u8* In, size_t InSize)
{
    u8* InEnd = In + InSize;

    while (In < InEnd)
    {
        u8 ASCIICount = *In++;
        wprintf(L"%dA(\"", ASCIICount);
        while(ASCIICount--)
        {
            wprintf(L"%c", *In);
            In += sizeof(u8);
        }
        wprintf(L"\") ");

        u8 UTF8Count = *In++;
        wprintf(L"%dU(\"", UTF8Count);
        while(UTF8Count--)
        {
            wprintf(L"%lc", *(wchar_t*)In);
            In += sizeof(wchar_t);
        }
        wprintf(L"\") ");
    }
    wprintf(L"\n");

    assert(In == InEnd);
}

static void
UTF8Decompress(size_t InSize, u8* In, size_t OutSize, wchar_t* Out)
{
    u8* InEnd = In + InSize;

    while (In < InEnd)
    {
        u8 ASCIICount = *In++;
        while(ASCIICount--)
        {
            *Out++ = *In++;
        }

        u8 UTF8Count = *In++;
        while(UTF8Count--)
        {
            *Out++ = *(wchar_t*)In;
            In += sizeof(wchar_t);
        }
    }
    assert(In == InEnd);
}

// Size is the size of the UTF8 string in bytes. "aaa" would be 12.
size_t
UTF8GetMaximumCompressedSize(size_t Size)
{
    // The largest would be if there was only one unicode point in which case we store 0 for ascii 1
    // for unicode and the raw codepoint. 1 + 1 + 4 * CodepointNum
    return Size + 2;
}

int
main(int Argc, char* Argv[]) {
    assert(setlocale(LC_ALL, "") != 0);

    wchar_t* InBuf = L"text│tt│";
    size_t InSize = wcslen(InBuf) * 4;

    size_t OutSize = UTF8GetMaximumCompressedSize(InSize);
    u8 OutBuf[OutSize];

    size_t CompressedSize = UTF8Compress(InSize, InBuf, OutSize, OutBuf);

    fwprintf(stderr, L"Raw string: \"%ls\"\n", InBuf);
    fwprintf(stderr, L"Compressed %lu bytes -> %lu bytes.\n", InSize, CompressedSize);

    size_t DecompressedSize = InSize;
    wchar_t *DecompressedBuffer = malloc(DecompressedSize);

    UTF8Decompress(CompressedSize, OutBuf, DecompressedSize, DecompressedBuffer);
    fwprintf(stderr, L"Decompressed: \"%ls\"\n", DecompressedBuffer);

    PrintCompressedUTF8(OutBuf, CompressedSize);

    return 0;
}
