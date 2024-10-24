#ifndef ARENA_IMPL
#define ARENA_IMPL

#include "common.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGESIZE 4096

#ifndef ARENA_MEMORY
#define ARENA_MEMORY PAGESIZE
#endif

struct Arena {
    void *memory;
    u64 size;
    u64 pos;
} typedef Arena;

// Create an arena
Arena *ArenaAlloc(void);
// Destroy an arena
void ArenaRelease(Arena *arena);

// Push bytes on to the arena | allocating
void *ArenaPush(Arena *arena, u64 size);
void *ArenaPushZero(Arena *arena, u64 size);

#define PushArray(arena, type, count) (type *)ArenaPush((arena), sizeof(type)*(count))
#define PushArrayZero(arena, type, count) (type *)ArenaPushZero((arena), sizeof(type) * (count))
#define PushStruct(arena, type) PushArray((arena), (type), 1)
#define PushStructZero(arena, type) PushArrayZero((arena), (type), 1)

// Free some bytes by popping the stack
void ArenaPop(Arena *arena, u64 size);
// Get the number of bytes allocated
u64 ArenaGetPos(Arena *arena);

void ArenaSetPosBack(Arena *arena, u64 pos);
void ArenaClear(Arena *arena);

Arena *ArenaAlloc(void)
{
    // NOTE: If the arena is created here the pointer to the memory get's overwritten with size in
    // ArenaPush, so we are forced to use malloc
    Arena *arena = malloc(sizeof(Arena));

    arena->memory = mmap(NULL, ARENA_MEMORY, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (arena->memory == MAP_FAILED)
        return NULL;

    arena->pos = 0;
    arena->size = ARENA_MEMORY;

    return arena;
}

void ArenaRelease(Arena *arena)
{
    munmap(arena->memory, ARENA_MEMORY);
    free(arena);
}

void *ArenaPush(Arena *arena, u64 size)
{
    u8 *mem;
    mem = (u8 *)arena->memory + arena->pos;
    arena->pos += size;
    return mem;
}

void *ArenaPushZero(Arena *arena, u64 size)
{
    u8 *mem;
    mem = (u8 *)arena->memory + arena->pos;
    bzero(mem, size);
    arena->pos += size;
    return mem;
}

void ArenaPop(Arena *arena, u64 size)
{
    arena->pos -= size;
}

u64 ArenaGetPos(Arena *arena)
{
    return arena->pos;
}

void ArenaSetPosBack(Arena *arena, u64 pos)
{
    arena->pos -= pos;
}

void ArenaClear(Arena *arena)
{
    bzero(arena->memory, arena->size);
    arena->pos = 0;
}

#endif
