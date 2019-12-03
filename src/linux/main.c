#define TICKS_PER_SECOND 1000000000

#include "../types.h"
#include "../platform.h"

#define NK_PRIVATE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
/* #define NK_INCLUDE_STANDARD_VARARGS */
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_XLIB_IMPLEMENTATION
#include "../nuklear.h"
#include "nuklear_xlib.h"

#include "../app.c"

#include <asoundlib.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#define WINDOW_WIDTH    800
#define WINDOW_HEIGHT   600

typedef enum
{
  LINUX_MEMORY_FLAG_NONE = 0x0,
} LinuxMemoryBlockFlag;

typedef struct _LinuxMemoryBlock
{
  MemoryBlock block;
  struct _LinuxMemoryBlock *prev;
  struct _LinuxMemoryBlock *next;
  flags_t flags;
} LinuxMemoryBlock;

static MemoryBlock *linux_allocate_memory       (size_t, MemoryBlockFlag);
static void         linux_deallocate_memory     (MemoryBlock *);
static WorkQueue   *linux_create_work_queue     (u32, u32);
static void         linux_destroy_work_queue    (WorkQueue *);
static void         linux_enqueue_work          (WorkQueue *, WorkQueueCallback,
                                                 void *);
static void         linux_complete_all_work     (WorkQueue *);
static u64          linux_get_ticks             (void);
static float        query_xrandr_fps            (Display *, Window);

static PlatformApi linux_platform = {
  .allocate_memory      = linux_allocate_memory,
  .deallocate_memory    = linux_deallocate_memory,
  .create_work_queue    = linux_create_work_queue,
  .destroy_work_queue   = linux_destroy_work_queue,
  .enqueue_work         = linux_enqueue_work,
  .complete_all_work    = linux_complete_all_work
};

typedef struct
{
  Display              *dpy;
  Window                root;
  Visual               *vis;
  Colormap              cmap;
  XWindowAttributes     attr;
  XSetWindowAttributes  swa;
  Window                win;
  int                   screen;
  XFont                *font;
  unsigned int          width;
  unsigned int          height;
  Atom                  wm_delete_window;
} XWindow;

PlatformApi *g_platform = &linux_platform;

static LinuxMemoryBlock linux_memory_sentinel = {};

int
main (int argc, char **argv)
{
  (void) argc;
  (void) argv;

  linux_memory_sentinel.prev = &linux_memory_sentinel;
  linux_memory_sentinel.next = &linux_memory_sentinel;

  XWindow xw = {};

  xw.dpy = XOpenDisplay (":0.0");
  if (!xw.dpy)
    {
      fprintf (stderr, "Cannot open display\n");
      return EXIT_FAILURE;
    }
  xw.screen     = DefaultScreen (xw.dpy);
  xw.root       = RootWindow (xw.dpy, xw.screen);
  xw.vis        = DefaultVisual (xw.dpy, xw.screen);
  xw.cmap       = XCreateColormap (xw.dpy, xw.root, xw.vis, AllocNone);

  xw.swa.colormap = xw.cmap;
  xw.swa.event_mask = (ExposureMask
                       | KeyPressMask
                       | KeyReleaseMask
                       | ButtonPress
                       | ButtonReleaseMask
                       | ButtonMotionMask
                       | Button1MotionMask
                       | Button3MotionMask
                       | Button4MotionMask
                       | Button5MotionMask
                       | PointerMotionMask
                       | KeymapStateMask);
  xw.win = XCreateWindow (xw.dpy, xw.root, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                          XDefaultDepth (xw.dpy, xw.screen), InputOutput,
                          xw.vis, CWEventMask | CWColormap, &xw.swa);

  XStoreName (xw.dpy, xw.win, "FonoGraf");
  XMapWindow (xw.dpy, xw.win);
  xw.wm_delete_window = XInternAtom (xw.dpy, "WM_DELETE_WINDOW", False);
  XSetWMProtocols (xw.dpy, xw.win, &xw.wm_delete_window, 1);
  XGetWindowAttributes (xw.dpy, xw.win, &xw.attr);
  xw.width = (unsigned int) xw.attr.width;
  xw.height = (unsigned int) xw.attr.height;
  xw.font = nk_xfont_create (xw.dpy, "fixed");

  struct nk_context *ctx;
  ctx = nk_xlib_init (xw.font, xw.dpy, xw.screen, xw.win, xw.width, xw.height);

  float fps = 10.f;
  int   rr_event_base;
  int   rr_error_base;
  bool  have_rr = XRRQueryExtension (xw.dpy, &rr_event_base, &rr_error_base);
  if (have_rr)
    {
      XRRSelectInput (xw.dpy, xw.win, RRScreenChangeNotifyMask);
      fps = query_xrandr_fps (xw.dpy, xw.win);
    }
  float render_dt = 1.f / fps;
  u64 ticks_per_frame = (u64) ((float) TICKS_PER_SECOND * render_dt);

  bool quit = false;
  while (!quit)
    {
      u64 start_tick = linux_get_ticks ();
      u64 ticks_elapsed = 0;

      nk_input_begin (ctx);
      while (XPending (xw.dpy))
        {
          XEvent e;
          XNextEvent (xw.dpy, &e);
          switch (e.type)
            {
            case DestroyNotify:
              quit = true;
              break;
            case ClientMessage:
              if ((Atom) e.xclient.data.l[0] == xw.wm_delete_window)
                quit = true;
              break;
            default:
              if (have_rr)
                {
                  switch (e.type - rr_event_base)
                    {
                    case RRScreenChangeNotify:
                      fps = query_xrandr_fps (xw.dpy, xw.win);
                      render_dt = 1.f / fps;
                      ticks_per_frame = (u64) ((float) TICKS_PER_SECOND * render_dt);
                      break;
                    }
                }
              break;
            }
          if (!XFilterEvent (&e, xw.win))
            nk_xlib_handle_event (xw.dpy, xw.screen, xw.win, &e);
        }
      nk_input_end (ctx);

      update_ui (ctx, render_dt);

      XClearWindow (xw.dpy, xw.win);
      nk_xlib_render (xw.win, nk_rgb (30, 30, 30));
      XFlush (xw.dpy);

      ticks_elapsed = linux_get_ticks () - start_tick;
      if (ticks_elapsed < ticks_per_frame)
        {
          // Sleep until fps target minus 1ms
          u64 wait_ticks = ticks_per_frame - ticks_elapsed;
          if (wait_ticks > 1000000)
            {
              struct timespec delay = {};
              wait_ticks -= 1000000;
              delay.tv_nsec = wait_ticks;
              nanosleep (&delay, NULL);
            }
          // Spin for remaining 1ms +/- nanosleep overhead
          while (ticks_elapsed < ticks_per_frame)
            ticks_elapsed = linux_get_ticks () - start_tick;
        }
      else
        fprintf (stderr, "Missed frame rate\n");
    }

  app_run ();

  nk_xfont_del (xw.dpy, xw.font);
  nk_xlib_shutdown ();
  XUnmapWindow (xw.dpy, xw.win);
  XFreeColormap (xw.dpy, xw.cmap);
  XDestroyWindow (xw.dpy, xw.win);
  XCloseDisplay (xw.dpy);

  NK_UNUSED (nk_sin); // #define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
  NK_UNUSED (nk_cos);
  NK_UNUSED (nk_sqrt); // #define NK_INCLUDE_FONT_BAKING
  NK_UNUSED (nk_file_load);

#if 0
  static char *device = "default";            /* playback device */

  int err;
  int card = -1;
  while ((snd_card_next (&card) == 0) && card != -1)
    {
      char *name = NULL;
      if (0 != (err = snd_card_get_name (card, &name)))
        {
          fprintf (stderr, "Error: %s", snd_strerror (err));
          continue;
        }
      printf ("%s (#%d)\n", name, card);
      free (name);
    }

  void **hints;
  snd_device_name_hint (-1, "pcm", &hints);
  for (void **hint = hints; *hint; ++hint)
    {
      char *name = snd_device_name_get_hint (*hint, "NAME");
      char *desc = snd_device_name_get_hint (*hint, "DESC");
      char *ioid = snd_device_name_get_hint (*hint, "IOID");
      printf ("Name: %s\n", name);
      free (name);
      free (desc);
      free (ioid);

    }
  snd_device_name_free_hint (hints);

  snd_pcm_t *handle;
  snd_pcm_sframes_t frames;

  if ((err = snd_pcm_open (&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
      fprintf (stderr, "Playback open error: %s\n", snd_strerror (err));
      return EXIT_FAILURE;
    }

  if ((err = snd_pcm_set_params (handle, SND_PCM_FORMAT_FLOAT,
                                 SND_PCM_ACCESS_RW_INTERLEAVED, 2, 48000, 1,
                                 500000 /* 0.5sec */)) < 0)
    {
      fprintf (stderr, "Playback open error: %s\n", snd_strerror (err));
      return EXIT_FAILURE;
    }

  init_fonograf (48000, 128);

  SoundWindow window = {
    .samples   = calloc (128 * 2, sizeof (float)),
    .nchannels = 2,
    .nframes   = 128
  };

  for (unsigned int i = 0; i < 512; ++i)
    {
      generate_sound (window);

      frames = snd_pcm_writei (handle, window.samples, window.nframes);
      if (frames < 0)
        frames = snd_pcm_recover (handle, frames, 0);
      if (frames < 0)
        {
          fprintf (stderr, "snd_pcm_writei failed: %s\n", snd_strerror (frames));
          break;
        }
      if ((frames > 0) && (frames < window.nframes))
        fprintf (stderr, "Short write (expected %u, wrote %li)\n",
                 window.nframes, frames);
    }

  cleanup_fonograf ();

  free (window.samples);

  snd_pcm_close (handle);
#endif

  return EXIT_SUCCESS;
}

static u64
linux_get_ticks ()
{
  u64 ticks;
  struct timespec now;
  clock_gettime (CLOCK_MONOTONIC, &now);
  ticks = now.tv_sec;
  ticks *= TICKS_PER_SECOND;
  ticks += now.tv_nsec;
  return ticks;
}

static float
query_xrandr_fps (Display *dpy, Window win)
{
  float fps = 60.f;
  XRRScreenConfiguration *screen_config = XRRGetScreenInfo (dpy, win);
  if (screen_config)
    {
      fps = (float) XRRConfigCurrentRate (screen_config);
      XRRFreeScreenConfigInfo (screen_config);
    }
  return fps;
}

////////////////////////////////////////////////////////////////////////////////

static MemoryBlock *
linux_allocate_memory (size_t size, MemoryBlockFlag flags)
{
  LinuxMemoryBlock *block;

  assert (sizeof (LinuxMemoryBlock) == 64);

  static uintptr_t page_size = 0;
  if (page_size == 0)
    page_size = sysconf (_SC_PAGESIZE);

  uintptr_t total_size = size + sizeof (LinuxMemoryBlock);
  uintptr_t base_offset = sizeof (LinuxMemoryBlock);
  uintptr_t protect_offset = 0;

  if (flags & MEMORY_BLOCK_FLAG_UNDERFLOW_CHECK)
    {
      total_size = size + (2 * page_size);
      base_offset = 2 * page_size;
      protect_offset = page_size;
    }
  else if (flags & MEMORY_BLOCK_FLAG_OVERFLOW_CHECK)
    {
      uintptr_t size_rounded_up = ALIGN_POW2 (size, page_size);
      total_size = size_rounded_up + (2 * page_size);
      base_offset = page_size + size_rounded_up - size;
      protect_offset = page_size + size_rounded_up;
    }

  block = mmap ((void *) 0, total_size, PROT_READ | PROT_WRITE,
                MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert (block);
  block->block.base = (u8 *) block + base_offset;
  assert (block->block.used == 0);
  assert (block->block.prev == NULL);

  if (flags & (MEMORY_BLOCK_FLAG_UNDERFLOW_CHECK | MEMORY_BLOCK_FLAG_OVERFLOW_CHECK))
    {
      int err = mprotect ((u8 *) block + protect_offset, page_size, PROT_NONE);
      assert (err == 0);
    }

  block->next = &linux_memory_sentinel;
  block->block.size = size;
  block->block.flags = (flags_t) flags;
  block->flags = LINUX_MEMORY_FLAG_NONE;

  block->prev = linux_memory_sentinel.prev;
  block->prev->next = block;
  block->next->prev = block;

  return &block->block;
}

static void
linux_deallocate_memory (MemoryBlock *block)
{
  LinuxMemoryBlock *linux_block;
  u64               size;
  flags_t           flags;
  uintptr_t         page_size;
  uintptr_t         total_size;

  if (!block)
    return;

  size          = block->size;
  flags         = block->flags;
  page_size     = sysconf (_SC_PAGESIZE);
  total_size    = size + sizeof (LinuxMemoryBlock);
  linux_block   = (LinuxMemoryBlock *) block;

  if (flags & MEMORY_BLOCK_FLAG_UNDERFLOW_CHECK)
    {
      total_size = size + (2 * page_size);
    }
  else if (flags & MEMORY_BLOCK_FLAG_OVERFLOW_CHECK)
    {
      uintptr_t size_rounded_up = ALIGN_POW2 (size, page_size);
      total_size = size_rounded_up + (2 * page_size);
    }

  linux_block->prev->next = linux_block->next;
  linux_block->next->prev = linux_block->prev;

  munmap (linux_block, total_size);
}

////////////////////////////////////////////////////////////////////////////////

#include <pthread.h>
#include <semaphore.h>

typedef struct
{
  WorkQueueCallback callback;
  void *data;
} WorkQueueEntry;

struct _WorkQueue
{
  MemoryPool mpool;
  volatile u32 completion_goal;
  volatile u32 completion_count;
  volatile u32 next_entry_to_read;
  volatile u32 next_entry_to_write;
  volatile bool terminated;
  sem_t semaphore_handle;
  struct
  {
    WorkQueueEntry *base;
    u32 nmemb;
  } entries;
  struct
  {
    pthread_t *base;
    u32 nmemb;
  } threads;
};

static bool
linux_do_next_work_queue_entry (WorkQueue *queue)
{
    bool should_sleep = false;

    u32 orig_next_entry_to_read = queue->next_entry_to_read;
    u32 new_next_entry_to_read = (orig_next_entry_to_read + 1) % queue->entries.nmemb;

    if (orig_next_entry_to_read != queue->next_entry_to_write)
      {
        u32 index = __sync_val_compare_and_swap (&queue->next_entry_to_read,
                                                 orig_next_entry_to_read,
                                                 new_next_entry_to_read);
        if (index == orig_next_entry_to_read)
          {
            WorkQueueEntry entry = queue->entries.base[index];
            entry.callback (entry.data);
            __sync_fetch_and_add (&queue->completion_count, 1);
          }
      }
    else
      {
        should_sleep = true;
      }

    return should_sleep;
}

static void
linux_enqueue_work (WorkQueue *queue, WorkQueueCallback callback, void *data)
{
    u32 new_next_entry_to_write = (queue->next_entry_to_write + 1) % queue->entries.nmemb;
    assert (new_next_entry_to_write != queue->next_entry_to_read);
    WorkQueueEntry *entry = queue->entries.base + queue->next_entry_to_write;
    entry->callback = callback;
    entry->data = data;
    ++queue->completion_goal;

    asm volatile("" ::: "memory");

    queue->next_entry_to_write = new_next_entry_to_write;
    sem_post (&queue->semaphore_handle);
}

static void
linux_complete_all_work (WorkQueue *queue)
{
  while (queue->completion_goal != queue->completion_count)
    linux_do_next_work_queue_entry (queue);

  queue->completion_goal = 0;
  queue->completion_count = 0;
}

static void *
linux_thread_proc (void *user_data)
{
  WorkQueue *queue = (WorkQueue *) user_data;

  while (!queue->terminated)
    {
      if (linux_do_next_work_queue_entry (queue))
        {
          sem_wait (&queue->semaphore_handle);
        }
    }

  return NULL;
}

static WorkQueue *
linux_create_work_queue (u32 entry_count, u32 thread_count)
{
  WorkQueue *queue;

  queue = init_push_struct (WorkQueue, mpool, MEMORY_FLAG_NONE);

  queue->terminated = false;

  queue->completion_goal = 0;
  queue->completion_count = 0;

  queue->next_entry_to_write = 0;
  queue->next_entry_to_read = 0;

  sem_init (&queue->semaphore_handle, 0, 0);

  queue->entries.base = push_array (&queue->mpool, WorkQueueEntry, entry_count,
                                    MEMORY_FLAG_NONE);
  queue->entries.nmemb = entry_count;

  queue->threads.base = push_array (&queue->mpool, pthread_t, thread_count,
                                    MEMORY_FLAG_NONE);
  queue->threads.nmemb = 0;

  for (u32 i = 0; i < thread_count; ++i)
    {
      pthread_t thread_id;
      int err = pthread_create (&thread_id, NULL, linux_thread_proc, queue);
      if (err == 0)
        queue->threads.base[queue->threads.nmemb++] = thread_id;
      else
        fprintf (stderr, "Could not create thread: %s\n", strerror (err));
    }

  return queue;
}

static void
linux_destroy_work_queue (WorkQueue *queue)
{
  queue->terminated = true;
  for (u32 i = 0; i < queue->threads.nmemb; ++i)
    sem_post (&queue->semaphore_handle);
  for (u32 i = 0; i < queue->threads.nmemb; ++i)
    pthread_join (queue->threads.base[i], NULL);
  clear_memory_pool (&queue->mpool);
}
