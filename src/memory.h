#ifndef MEMORY_H
#define MEMORY_H 1

#include "platform.h"

#include <assert.h>
#include <stdalign.h>
#include <string.h>

// (val) - (val) is for integral promotion
#define ALIGN_POW2(val, align) (((val) + ((align) - 1)) & ~(((val) - (val)) + (align) - 1))
#define IS_POW2(align) (((align) & ~((align) - 1)) == (align))

typedef enum
{
  MEMORY_FLAG_NONE  = 0x0,
  MEMORY_FLAG_ZERO  = 0x1,
} MemoryFlag;

static inline size_t
get_alignment_offset (MemoryPool *pool, size_t alignment)
{
  size_t offset = 0;
  uintptr_t ptr = (uintptr_t) pool->current_block->base + pool->current_block->used;
  uintptr_t mask = alignment - 1;

  if (ptr & mask)
    offset = alignment - (ptr & mask);

  return offset;
}

static inline void *
push_bytes_aligned (MemoryPool *pool, size_t nbytes, size_t alignment, flags_t flags)
{
  void *result;
  size_t size = 0;

  assert (alignment < 128);
  assert (IS_POW2 (alignment));

  if (pool->current_block)
    size = nbytes + get_alignment_offset (pool, alignment);

  if (!pool->current_block
      || ((pool->current_block->used + size) > pool->current_block->size))
    {
      MemoryBlock *new_block = NULL;
      size_t block_size = 1024 * 1024; // @Hardcode

      size = nbytes;

      if (size > block_size)
        block_size = size;

      new_block = g_platform->allocate_memory (block_size,
                                               MEMORY_BLOCK_FLAG_OVERFLOW_CHECK);
      new_block->prev = pool->current_block;
      pool->current_block = new_block;
    }

  assert ((pool->current_block->used + size) <= pool->current_block->size);

  result = (pool->current_block->base
            + pool->current_block->used
            + get_alignment_offset (pool, alignment));
  pool->current_block->used += size;

  assert (size >= nbytes);
  assert (pool->current_block->used <= pool->current_block->size);

  if (flags & MEMORY_FLAG_ZERO)
    memset (result, 0, nbytes);

  return result;
}

static inline char *
push_string (MemoryPool *pool, const char *string)
{
  char *result;
  size_t length = strlen (string);
  result = push_bytes_aligned (pool, length + 1, alignof (char), MEMORY_FLAG_NONE);
  memcpy (result, string, length);
  result[length] = '\0';
  return result;
}

static inline void
free_last_block (MemoryPool *pool)
{
  MemoryBlock *last = pool->current_block;
  pool->current_block = last->prev;
  g_platform->deallocate_memory (last);
}

static inline void
clear_memory_pool (MemoryPool *pool)
{
  while (pool->current_block)
    {
      bool is_last_block = (pool->current_block->prev == NULL);
      free_last_block (pool);
      if (is_last_block)
        break;
    }
}

static inline void *
init_push_bytes (size_t nbytes, size_t alignment, size_t pool_offset, flags_t flags)
{
  MemoryPool bootstrap = {};
  void *data = push_bytes_aligned (&bootstrap, nbytes, alignment, flags);
  *(MemoryPool *) ((u8 *) data + pool_offset) = bootstrap;
  return data;
}

#define push_bytes(pool, nbytes, flags) \
  push_bytes_aligned ((pool), (nbytes), alignof (max_align_t), (flags))
#define init_push_struct(T, pool_member, flags) \
  init_push_bytes (sizeof (T), alignof (T), offsetof (T, pool_member), (flags))
#define push_struct(pool, T, flags) \
  push_bytes_aligned ((pool), sizeof (T), alignof (T), (flags))
#define push_array_aligned(pool, T, nmemb, alignment, flags) \
  push_bytes_aligned ((pool), sizeof (T) * (nmemb), (alignment), (flags))
#define push_array(pool, T, nmemb, flags) \
  push_array_aligned ((pool), sizeof (T), (nmemb), alignof (T), (flags))

#endif /* ! MEMORY_H */
