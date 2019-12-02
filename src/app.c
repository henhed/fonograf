#include "memory.h"
#include "maths.h"

#include <stdio.h>

typedef struct
{
  float    *base;
  u32       width;
  u32       height;
} Tensor;

Tensor *
create_tensor (MemoryPool *mpool, u32 width, u32 height)
{
  Tensor *tensor;

  assert (width > 0);
  assert (height > 0);

  u32 nmemb = ALIGN_POW2 (width * height, 4); // for `_mm_load_ps'

  tensor = push_struct (mpool, Tensor, MEMORY_FLAG_NONE);
  tensor->base = push_array_aligned (mpool, float, nmemb,
                                     16, // for `_mm_load_ps'
                                     MEMORY_FLAG_NONE);
  tensor->width = width;
  tensor->height = height;

  return tensor;
}

static inline void
randomize_tensor (Tensor *tensor)
{
  u32 nmemb = tensor->width * tensor->height;
  for (u32 i = 0; i < nmemb; ++i)
    tensor->base[i] = generate_gaussian_noise (0, 1);
}

void
print_tensor (Tensor *t)
{
  u32 w = t->width;
  u32 h = t->height;
  for (u32 y = 0; y < h; ++y)
    {
      u32 offset = y * w;
      for (u32 x = 0; x < w; ++x)
        {
          /* static char *c[] = {" ", "\u2591", "\u2592", "\u2593", "\u2588"}; */
          float v = t->base[offset + x];
          /* v = MAX (MIN (v, .99f), 0.f); */
          /* printf ("%1$s%1$s", c[(int) (v * ARRAY_COUNT (c))]); */
          printf ("%6.2f, ", v);
        }
      printf ("\n");
    }
}

void
tensor_sum_product (const Tensor *a, const Tensor *b, Tensor *out)
{
  u32 aw = a->width;
  u32 ah = a->height;
  u32 bw = b->width;
  u32 bh = b->height;
  u32 ow = out->width;
  u32 oh = out->height;

  assert (ow == bw);
  assert (oh == ah);
  assert (aw == bh);

  /* if ((aw == 1) && (bh == 1)) */
  /*   { */
  /*     for (u32 y = 0; y < ah; ++y) */
  /*       { */
  /*         __m128 a4 = _mm_set1_ps (a->base[y]); */
  /*         for (u32 x = 0, offset = y * bw; x < bw; x += 4) */
  /*           { */
  /*             __m128 b4 = _mm_load_ps (b->base + x); */
  /*             __m128 v4 = _mm_mul_ps (a4, b4); */
  /*             _mm_storeu_ps (out->base + offset + x, v4); */
  /*           } */
  /*       } */
  /*     return; */
  /*   } */

  for (u32 y = 0; y < oh; ++y)
    {
      for (u32 x = 0, oy = y * ow; x < ow; ++x)
        {
          u32 ox = oy + x;
          out->base[ox] = 0;
          for (u32 i = 0; i < aw; ++i)
            out->base[ox] += a->base[(y * aw) + i] * b->base[(i * bw) + x];
        }
    }
}

void
do_some_work (void *user_data)
{
  int data = *(int *) user_data;
  printf ("%s: %d\n", __FUNCTION__, data);
}

void
app_run ()
{
  MemoryPool mpool = {};

  WorkQueue *queue = create_work_queue (100, 10);
  printf ("queue: %p\n", queue);

  int data[20];
  for (u32 i = 0; i < ARRAY_COUNT (data); ++i)
    {
      data[i] = i;
      enqueue_work (queue, do_some_work, data + i);
    }

  complete_all_work (queue);

  {
    Tensor *t1 = create_tensor (&mpool, 1, 5);
    t1->base = (float []) {1, 2, 3, 4, 5};
    Tensor *t2 = create_tensor (&mpool, 6, 1);
    t2->base = (float []) {1, 2, 3, 4, 5, 6};

    Tensor *t3 = create_tensor (&mpool, 6, 5);
    tensor_sum_product (t1, t2, t3);

    /* randomize_tensor (t1); */
    print_tensor (t1); printf ("\n");
    print_tensor (t2); printf ("\n");
    print_tensor (t3); printf ("\n");
  }

  printf ("========================================\n");

  {
    Tensor *t1 = create_tensor (&mpool, 3, 3);
    t1->base = (float []) {1, 1, 1, 2, 2, 2, 3, 3, 3};
    Tensor *t2 = create_tensor (&mpool, 2, 3);
    t2->base = (float []) {1, 2, 2, 3, 3, 4};

    Tensor *t3 = create_tensor (&mpool, 2, 3);
    tensor_sum_product (t1, t2, t3);

    print_tensor (t1); printf ("\n");
    print_tensor (t2); printf ("\n");
    print_tensor (t3); printf ("\n");
  }

  destroy_work_queue (queue);

  clear_memory_pool (&mpool);
}
