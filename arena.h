#ifndef ARENA_H
#define ARENA_H

#include <sys/mman.h>
#include <stdint.h>
#include "types.h"

// Arena Allocator
typedef struct {
    void* addr;
    u64 size;
    u64 pos;
} Arena;
#define PushArray(arena, type, count) (type*)ArenaPush((arena), sizeof(type) * (count))
#define PushArrayZero(arena, type, count) (type*)ArenaPushZero((arena), sizeof(type) * (count))
#define PushStruct(arena, type) PushArray((arena), (type), 1)
#define PushStructZero(arena, type) PushArrayZero((arena), (type), 1)

void ArenaAlloc(Arena* arena, u64 size);
void ArenaRelease(Arena* arena);
void* ArenaPush(Arena* arena, u64 size);

#endif // ARENA_H

#ifdef ARENA_IMPL

// Returns arena in case of success, or 0 if it failed to alllocate the memory
void
ArenaAlloc(Arena* arena, u64 size)
{
    arena->addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    Assert(arena->addr != MAP_FAILED);
    arena->pos = 0;
    arena->size = size;
}

void
ArenaRelease(Arena* arena)
{
    munmap(arena->addr, arena->size);
}

void*
ArenaPush(Arena* arena, u64 size)
{
    u8* mem;
    mem = (u8*)arena->addr + arena->pos;
    arena->pos += size;
    Assert(arena->pos <= arena->size);
    return mem;
}

#endif // ARENA_IMPL
