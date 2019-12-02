#ifndef TYPES_H
#define TYPES_H 1

#include <stdalign.h>
#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>

typedef int8_t   s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t   u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef u64 flags_t;

typedef struct _MemoryBlock
{
  flags_t               flags;
  u64                   size;
  u8                   *base;
  uintptr_t             used;
  struct _MemoryBlock  *prev;
} MemoryBlock;

typedef struct
{
  MemoryBlock      *current_block;
  u32               temp_count;
} MemoryPool;

#endif /* ! TYPES_H */
