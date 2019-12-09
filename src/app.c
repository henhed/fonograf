#include "memory.h"
#include "maths.h"
#include "network.h"

#include <stdio.h>

static Network *app_network;
static WorkQueue *app_work_queue;

static u32
fread_u32 (FILE *f)
{
  u32 result;
  size_t nread;
  nread = fread (&result, sizeof (result), 1, f);
  if (nread != 1)
    return (u32) -1;
  result = (((result & 0x000000FF) << 24) |
            ((result & 0x0000FF00) <<  8) |
            ((result & 0x00FF0000) >>  8) |
            ((result & 0xFF000000) >> 24));
  return result;
}

void
do_training_work (void *user_data)
{
  (void) user_data;

  FILE *images_fh = fopen ("data/train-images-idx3-ubyte", "r");
  FILE *labels_fh = fopen ("data/train-labels-idx1-ubyte", "r");
  assert (images_fh);
  assert (labels_fh);

  u32 magic;
  magic = fread_u32 (images_fh);
  assert (magic == 2051);
  magic = fread_u32 (labels_fh);
  assert (magic == 2049);

  u32 num_images = fread_u32 (images_fh);
  u32 num_rows = fread_u32 (images_fh);
  u32 num_cols = fread_u32 (images_fh);
  u32 num_labels = fread_u32 (labels_fh);
  printf ("num_images: %u\n", num_images);
  printf ("num_rows: %u\n", num_rows);
  printf ("num_cols: %u\n", num_cols);
  printf ("num_labels: %u\n", num_labels);

  assert (num_labels == num_images);

  /* srand (time (NULL)); */

  u32 images_buffer_size = ((num_rows * num_cols) + 10) * num_images; // +10 for labels
  float *images_buffer = push_array (&app_network->mpool, float, images_buffer_size,
                                     MEMORY_FLAG_NONE);
  for (u32 i = 0, j = 0; i < num_images; ++i)
    {
      size_t nread;
      u8 buffer[num_cols * num_rows];
      nread = fread (buffer, sizeof (buffer[0]), num_cols * num_rows, images_fh);
      if (nread != sizeof (buffer[0]) * num_cols * num_rows)
        {
          fprintf (stderr, "nread = %lu (of %u)\n", nread, num_cols * num_rows);
          fclose (images_fh);
          fclose (labels_fh);
          exit (EXIT_FAILURE);
        }
      for (u32 k = 0; k < sizeof (buffer) / sizeof (buffer[0]); ++k, ++j)
        images_buffer[j] = ((float) buffer[k]) / 255.f;

      u8 label = (u8) -1;
      (void) fread (&label, sizeof (label), 1, labels_fh);
      if (label >= 10)
        {
          fprintf (stderr, "label = %u\n", label);
          fclose (images_fh);
          fclose (labels_fh);
          exit (EXIT_FAILURE);
        }

      for (u8 k = 0; k < 10; ++k, ++j)
        images_buffer[j] = (k == label) ? 1.f : 0.f;
    }

  network_sgd (app_network, images_buffer, num_images, 30, 0.025f, 5.f);

  fclose (images_fh);
  fclose (labels_fh);
}

void
app_init ()
{
  u32 sizes[] = {784, 30, 10};
  app_network = create_network (sizes, ARRAY_COUNT (sizes), 10);
  app_work_queue = create_work_queue (2, 1);

  enqueue_work (app_work_queue, do_training_work, NULL);
}

void
app_shutdown ()
{
  destroy_work_queue (app_work_queue);
  destroy_network (app_network);
}

void
app_update_ui (struct nk_context *ctx, float dt)
{
  (void) dt;

  if (nk_begin (ctx, "Demo",
                nk_rect (0, 0, g_platform->window.width, g_platform->window.height),
                0))
    {
      struct nk_command_buffer *canvas = nk_window_get_canvas (ctx);
      struct nk_rect size = nk_layout_space_bounds (ctx);
      float ui_y = size.y + 20;
      float cell_width  = 2.f;
      float cell_height = 2.f;

      for (u32 i = 0; i < app_network->layers.nmemb; ++i)
        {
          NetworkLayer *layer = &app_network->layers.base[i];
          u32 width = layer->width;
          u32 height = layer->height;
          for (u32 y = 0; y < height; ++y)
            {
              {
                float value  = layer->biases[y];
                float red    = (value < 0) ? MIN (-value, 4.f) : 0;
                float green  = (value > 0) ? MIN ( value, 4.f) : 0;
                float blue   = 4.f - (red + green);
                nk_fill_rect (canvas,
                              nk_rect (size.x,
                                       ui_y + (y * cell_height),
                                       cell_width,
                                       cell_height),
                              0.f,
                              nk_rgb ((int) (red   * 64),
                                      (int) (green * 64),
                                      (int) (blue  * 64)));
              }
              for (u32 x = 0, offset = y * width; x < width; ++x)
                {
                  float value  = layer->weights[offset + x];
                  float red    = (value < 0) ? MIN (-value, 4.f) : 0;
                  float green  = (value > 0) ? MIN ( value, 4.f) : 0;
                  float blue   = 4.f - (red + green);
                  nk_fill_rect (canvas,
                                nk_rect (size.x + ((x + 2) * cell_width),
                                         ui_y + (y * cell_height),
                                         cell_width,
                                         cell_height),
                                0.f,
                                nk_rgb ((int) (red   * 64),
                                        (int) (green * 64),
                                        (int) (blue  * 64)));
                }
            }
          ui_y += (height * cell_height) + 20;

          cell_width  *= 2;
          cell_height *= 2;
        }
    }
  nk_end (ctx);
}
