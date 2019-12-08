#ifndef PLATFORM_H
#define PLATFORM_H 1

#define ARRAY_COUNT(array) (sizeof (array) / sizeof ((array)[0]))

#define MAX(a, b)                               \
  ({ __typeof__ (a) _a = (a);                   \
    __typeof__ (b) _b = (b);                    \
    _a > _b ? _a : _b; })

#define MIN(a, b)                               \
  ({ __typeof__ (a) _a = (a);                   \
    __typeof__ (b) _b = (b);                    \
    _a < _b ? _a : _b; })

#define ABS(x)                                  \
  ({ __typeof__ (x) _x = (x);                   \
    _x < 0 ? -_x : _x; })

typedef enum
{
  MEMORY_BLOCK_FLAG_NONE            = 0x0,
  MEMORY_BLOCK_FLAG_NOT_RESTORED    = 0x1,
  MEMORY_BLOCK_FLAG_OVERFLOW_CHECK  = 0x2,
  MEMORY_BLOCK_FLAG_UNDERFLOW_CHECK = 0x4
} MemoryBlockFlag;

typedef struct _WorkQueue WorkQueue;
typedef void (*WorkQueueCallback) (void *);

typedef struct
{
  MemoryBlock  *(*allocate_memory)      (size_t, MemoryBlockFlag);
  void          (*deallocate_memory)    (MemoryBlock *);
  WorkQueue    *(*create_work_queue)    (u32, u32);
  void          (*destroy_work_queue)   (WorkQueue *);
  void          (*enqueue_work)         (WorkQueue *, WorkQueueCallback, void *);
  void          (*complete_all_work)    (WorkQueue *);
  struct
  {
    u32 width;
    u32 height;
  } window;
} PlatformApi;

extern PlatformApi *g_platform;

#define create_work_queue(entry_count, thread_count) \
  g_platform->create_work_queue ((entry_count), (thread_count))
#define destroy_work_queue(queue) \
  g_platform->destroy_work_queue (queue)
#define enqueue_work(queue, callback, data) \
  g_platform->enqueue_work ((queue), (callback), (data))
#define complete_all_work(queue) \
  g_platform->complete_all_work (queue)

#endif /* ! PLATFORM_H */
