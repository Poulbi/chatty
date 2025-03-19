#ifndef ARENA_H
#define ARENA_H

#include <sys/mman.h>

#include <stdint.h>
typedef uint8_t A_u8;
typedef uint16_t A_u16;
typedef uint32_t A_u32;
typedef uint64_t A_u64;
typedef int8_t A_s8;
typedef int16_t A_s16;
typedef int32_t A_s32;
typedef int64_t A_s64;
typedef A_u32 A_b32;

// Arena Allocator
typedef struct {
    void* addr;
    A_u64 size;
    A_u64 pos;
} Arena;
#define PushArray(arena, type, count) (type*)ArenaPush((arena), sizeof(type) * (count))
#define PushArrayZero(arena, type, count) (type*)ArenaPushZero((arena), sizeof(type) * (count))
#define PushStruct(arena, type) PushArray((arena), (type), 1)
#define PushStructZero(arena, type) PushArrayZero((arena), (type), 1)

void ArenaAlloc(Arena* arena, A_u64 size);
void ArenaRelease(Arena* arena);
void* ArenaPush(Arena* arena, A_u64 size);

#endif // ARENA_H

#ifdef ARENA_IMPL

// Returns arena in case of success, or 0 if it failed to alllocate the memory
void
ArenaAlloc(Arena* arena, A_u64 size)
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
ArenaPush(Arena* arena, A_u64 size)
{
    A_u8* mem;
    mem = (A_u8*)arena->addr + arena->pos;
    arena->pos += size;
    Assert(arena->pos <= arena->size);
    return mem;
}

#undef ARENA_IMPL
#endif // ARENA_IMPL
