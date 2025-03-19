#include "chatty.h"

u64 ArrayLength(u8 *Array);
void ArrayInsert(u8 *Array, u64 Position, u8 Element);
void ArrayCopy(u8 *To, u8 *From);
void ArrayDelete(u8* Array, u64 Position);
u8* ArrayCreate(u8* Container, u64 Capacity);

// EXAMPLE: CREATE
//
//  u64 Capacity = 15;
//  u8 ArrayContainer[Capacity + sizeof(Capacity)];
//  u8* Array = ArrayCreate(ArrayContainer, Capacity);
//
// EXAMPLE: API
//
//  ArrayCopy(Array, (u8*)"Hello, world!");
//
//  ArrayInsert(Array, 3, 'f');
//  ArrayInsert(Array, 3, 'e');
//  Array[14] = 'd';
//  ArrayDelete(Array, 3);

#ifdef ARRAY_IMPL

#include <strings.h>
#include <string.h>

u64
ArrayLength(u8 *Array)
{
    return *((u64*)(Array - sizeof(u64)));
}

void
ArrayInsert(u8 *Array, u64 Position, u8 Element)
{
    memmove(Array + Position + 1, Array + Position, ArrayLength(Array) - Position - 1);
    Array[Position] = Element;
}

// Copy null terminated string without copying over the null terminator
void
ArrayCopy(u8 *To, u8 *From)
{
    u32 i = 0;
    while (From[i])
    {
        To[i] = From[i];
        i++;
    }
}

void
ArrayDelete(u8* Array, u64 Position)
{
    memmove(Array + Position, Array + Position + 1, ArrayLength(Array) - Position - 1);
    Array[ArrayLength(Array) - 1] = 0;
}

u8*
ArrayCreate(u8* Container, u64 Capacity)
{
    *(u64*)Container = Capacity;
    u8 *Array = Container + sizeof(Capacity);
    bzero(Array, Capacity);
    return Array;
}

#endif
