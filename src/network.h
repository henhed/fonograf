#ifndef NETWORK_H
#define NETWORK_H 1

#include "memory.h"
#include "maths.h"

/* void */
/* tensor_sum_product (const Tensor *a, const Tensor *b, Tensor *out) */
/* { */
/*   u32 aw = a->width; */
/*   u32 ah = a->height; */
/*   u32 bw = b->width; */
/*   u32 bh = b->height; */
/*   u32 ow = out->width; */
/*   u32 oh = out->height; */

/*   assert (ow == bw); */
/*   assert (oh == ah); */
/*   assert (aw == bh); */

/*   /\* if ((aw == 1) && (bh == 1)) *\/ */
/*   /\*   { *\/ */
/*   /\*     for (u32 y = 0; y < ah; ++y) *\/ */
/*   /\*       { *\/ */
/*   /\*         __m128 a4 = _mm_set1_ps (a->base[y]); *\/ */
/*   /\*         for (u32 x = 0, offset = y * bw; x < bw; x += 4) *\/ */
/*   /\*           { *\/ */
/*   /\*             __m128 b4 = _mm_load_ps (b->base + x); *\/ */
/*   /\*             __m128 v4 = _mm_mul_ps (a4, b4); *\/ */
/*   /\*             _mm_storeu_ps (out->base + offset + x, v4); *\/ */
/*   /\*           } *\/ */
/*   /\*       } *\/ */
/*   /\*     return; *\/ */
/*   /\*   } *\/ */

/*   for (u32 y = 0; y < oh; ++y) */
/*     { */
/*       for (u32 x = 0, oy = y * ow; x < ow; ++x) */
/*         { */
/*           u32 ox = oy + x; */
/*           out->base[ox] = 0; */
/*           for (u32 i = 0; i < aw; ++i) */
/*             out->base[ox] += a->base[(y * aw) + i] * b->base[(i * bw) + x]; */
/*         } */
/*     } */
/* } */

typedef struct _Network Network;

typedef struct
{
  u32 height;
  float *zs;
  float *activation;
} ForwardResultLayer;

typedef struct
{
  struct
  {
    ForwardResultLayer *base;
    u32                 nmemb;
  } layers;
} ForwardResult;

typedef struct
{
  u32 width;
  u32 height;
  float *delta_b;
  float *delta_w;
} BackwardResultLayer;

typedef struct
{
  struct
  {
    BackwardResultLayer    *base;
    u32                     nmemb;
  } layers;
} BackwardResult;

typedef struct
{
  Network          *network;
  float            *input;
  float            *output;
  ForwardResult    *forward;
  BackwardResult   *backward;
} MiniBatchResult;

typedef struct
{
  u32 width;
  u32 height;
  float *biases;
  float *weights;
} NetworkLayer;

struct _Network
{
  MemoryPool        mpool;
  struct
  {
    NetworkLayer   *base;
    u32             nmemb;
  } layers;
  struct
  {
    MiniBatchResult    *base;
    u32                 nmemb;
  } mini_batch_results;
  ForwardResult    *validation_forward_result;
  BackwardResult   *mini_batch_backward_result;
  WorkQueue        *work_queue;
};

ForwardResult *
create_forward_result (Network *network)
{
  ForwardResult *result = push_struct (&network->mpool, ForwardResult,
                                       MEMORY_FLAG_NONE);
  result->layers.nmemb = network->layers.nmemb;
  result->layers.base = push_array (&network->mpool, ForwardResultLayer,
                                    result->layers.nmemb, MEMORY_FLAG_NONE);
  for (u32 i = 0; i < result->layers.nmemb; ++i)
    {
      ForwardResultLayer *layer;
      layer = &result->layers.base[i];
      layer->height = network->layers.base[i].height;
      layer->zs = push_array (&network->mpool, float, layer->height,
                              MEMORY_FLAG_NONE);
      layer->activation = push_array (&network->mpool, float, layer->height,
                                      MEMORY_FLAG_NONE);
    }
  return result;
}

BackwardResult *
create_backward_result (Network *network)
{
  BackwardResult *result = push_struct (&network->mpool, BackwardResult,
                                        MEMORY_FLAG_NONE);
  result->layers.nmemb = network->layers.nmemb;
  result->layers.base = push_array (&network->mpool, BackwardResultLayer,
                                    result->layers.nmemb, MEMORY_FLAG_NONE);
  for (u32 i = 0; i < result->layers.nmemb; ++i)
    {
      BackwardResultLayer *layer;
      layer = &result->layers.base[i];
      layer->width = network->layers.base[i].width;
      layer->height = network->layers.base[i].height;
      layer->delta_w = push_array (&network->mpool, float,
                                   layer->width * layer->height,
                                   MEMORY_FLAG_NONE);
      layer->delta_b = push_array (&network->mpool, float, layer->height,
                                   MEMORY_FLAG_NONE);
    }
  return result;
}

Network *
create_network (u32 *sizes, u32 nlayers, u32 mini_batch_size)
{
  Network *network;

  assert (sizes != NULL);
  assert (nlayers > 1);
  assert (mini_batch_size > 0);

  network = init_push_struct (Network, mpool, MEMORY_FLAG_ZERO);

  network->layers.nmemb = nlayers - 1;
  network->layers.base = push_array (&network->mpool, NetworkLayer, nlayers - 1,
                                     MEMORY_FLAG_NONE);
  for (u32 i = 0; i < network->layers.nmemb; ++i)
    {
      NetworkLayer *layer = &network->layers.base[i];
      layer->width = sizes[i];
      layer->height = sizes[i + 1];
      layer->weights = push_array (&network->mpool, float,
                                   layer->width * layer->height,
                                   MEMORY_FLAG_ZERO);
      layer->biases = push_array (&network->mpool, float, layer->height,
                                  MEMORY_FLAG_ZERO);
      for (u32 y = 0; y < layer->height; ++y)
        {
          layer->biases[y] = generate_gaussian_noise (0, 1);
          for (u32 x = 0, offset = y * layer->width; x < layer->width; ++x)
            layer->weights[offset + x] = (generate_gaussian_noise (0, 1)
                                          / sqrtf ((float) layer->width));
        }
    }

  network->mini_batch_results.nmemb = mini_batch_size;
  network->mini_batch_results.base
    = push_array (&network->mpool, MiniBatchResult,
                  network->mini_batch_results.nmemb, MEMORY_FLAG_NONE);
  for (u32 i = 0; i < network->mini_batch_results.nmemb; ++i)
    {
      MiniBatchResult *result = &network->mini_batch_results.base[i];
      result->network = network;
      result->forward = create_forward_result (network);
      result->backward = create_backward_result (network);
    }

  network->validation_forward_result = create_forward_result (network);

  network->mini_batch_backward_result = create_backward_result (network);

  network->work_queue = create_work_queue (mini_batch_size * 2, 3); // @Hardcode

  return network;
}

void
destroy_network (Network *network)
{
  destroy_work_queue (network->work_queue);
  clear_memory_pool (&network->mpool);
}

static int
rand_cmp (const void *a, const void *b)
{
  (void) a;
  (void) b;
  return (rand () % 3) - 1;
}

static inline float
sigmoid_ (float z)
{
  return 1.f / (1.f + powf (M_E, -z));
}

static inline float
sigmoid_prime_ (float z)
{
  return sigmoid_ (z) * (1.f - sigmoid_ (z));
}

static inline void
sigmoid (const float *input, float *output, u32 nmemb)
{
  for (u32 i = 0; i < nmemb; ++i)
    output[i] = sigmoid_ (input[i]);
}

static inline void
sigmoid_prime (const float *input, const float *z, u32 nmemb, float *out)
{
  for (u32 i = 0; i < nmemb; ++i)
    out[i] = input[i] * sigmoid_prime_ (z[i]);
}

static inline void
vec_sum (const float *a, const float *b, u32 nmemb, float *out)
{
  for (u32 i = 0; i < nmemb; ++i)
    out[i] = a[i] + b[i];
}

static inline void
mat_nm_vec_m_product (const float *a, const float *b, u32 n, u32 m, float *out)
{
  for (u32 y = 0; y < m; ++y)
    {
      out[y] = 0;
      for (u32 x = 0, offset = y * n; x < n; ++x)
        out[y] += a[offset + x] * b[x];
    }
}

static inline void
mat_nm_vec_m_transpose_product (const float *a, const float *b, u32 n, u32 m,
                                float *out)
{
  for (u32 x = 0; x < n; ++x)
    {
      out[x] = 0;
      for (u32 y = 0; y < m; ++y)
        out[x] += a[(y * n) + x] * b[y];
    }
}

static inline void
mat_1n_mat_m1_product (const float *a, const float *b, u32 n, u32 m, float *out)
{
  for (u32 y = 0; y < n; ++y)
    {
      for (u32 x = 0, offset = y * m; x < m; ++x)
        out[offset + x] = b[x] * a[y];
    }
}

static inline float
cost (const float *a, const float *y, u32 nmemb)
{
  float norm = 0.f;
  for (u32 i = 0; i < nmemb; ++i)
    {
      float d = ABS (a[i] - y[i]);
      norm += d * d;
    }
  norm = powf (norm, .5f);
  return .5f * (norm * norm);
}

static inline void
cost_derivative (const float *activations, const float *truth, const float *zs,
                 u32 nmemb, float *output)
{
  for (u32 i = 0; i < nmemb; ++i)
    {
      (void) zs;
      output[i] = (activations[i] - truth[i]);// * sigmoid_prime_ (zs[i]);
    }
}

static inline void
feedforward (Network *network, float *input, ForwardResult *result)
{
  assert (result->layers.nmemb == network->layers.nmemb);
  float *activation = input;
  for (u32 i = 0; i < network->layers.nmemb; ++i)
    {
      NetworkLayer *nl = &network->layers.base[i];
      float *b = nl->biases;
      float *w = nl->weights;
      ForwardResultLayer *rl = &result->layers.base[i];
      assert (rl->height == nl->height);
      mat_nm_vec_m_product (w, activation, nl->width, nl->height, rl->zs);
      vec_sum (rl->zs, b, nl->height, rl->zs);
      sigmoid (rl->zs, rl->activation, rl->height);
      activation = rl->activation;
    }
}

static inline bool
evaluate_network (Network *network, float *x, float *y)
{
  ForwardResult *fr = network->validation_forward_result;
  assert (fr->layers.nmemb == network->layers.nmemb);
  feedforward (network, x, fr);

  ForwardResultLayer *last_layer = &fr->layers.base[fr->layers.nmemb - 1];
  float *activation = last_layer->activation;

  u32 truth_argmax = (u32) -1;
  u32 guess_argmax = (u32) -1;
  float truth_argmax_value = -1.f;
  float guess_argmax_value = -1.f;
  for (u32 i = 0; i < last_layer->height; ++i)
    {
      if (y[i] > truth_argmax_value)
        {
          truth_argmax_value = y[i];
          truth_argmax = i;
        }
      if (activation[i] > guess_argmax_value)
        {
          guess_argmax_value = activation[i];
          guess_argmax = i;
        }
    }
  bool success = (guess_argmax == truth_argmax) ? true : false;
  return success;
}

static inline void
backprop (Network *network, float *x, float *y,
          ForwardResult *fr, BackwardResult *br)
{
  u32 nlayers = network->layers.nmemb;

  assert (fr->layers.nmemb == nlayers);
  assert (br->layers.nmemb == nlayers);

  feedforward (network, x, fr);

  float *activations[nlayers + 1];
  activations[0] = x;
  for (u32 i = 0; i < nlayers; ++i)
    activations[i + 1] = fr->layers.base[i].activation;

  float *delta = br->layers.base[nlayers - 1].delta_b;
  cost_derivative (activations[nlayers],
                   y,
                   fr->layers.base[nlayers - 1].zs,
                   br->layers.base[nlayers - 1].height,
                   delta);
  mat_1n_mat_m1_product (delta,
                         activations[nlayers - 1],
                         br->layers.base[nlayers - 1].height,
                         br->layers.base[nlayers - 1].width,
                         br->layers.base[nlayers - 1].delta_w);

  for (u32 i = nlayers - 2; i != (u32) -1; --i)
    {
      mat_nm_vec_m_transpose_product (network->layers.base[i + 1].weights,
                                      delta,
                                      network->layers.base[i + 1].width,
                                      network->layers.base[i + 1].height,
                                      br->layers.base[i].delta_b);
      sigmoid_prime (br->layers.base[i].delta_b,
                     fr->layers.base[i].zs,
                     network->layers.base[i + 1].width,
                     br->layers.base[i].delta_b);
      delta = br->layers.base[i].delta_b;
      mat_1n_mat_m1_product (delta,
                             activations[i],
                             br->layers.base[i].height,
                             br->layers.base[i].width,
                             br->layers.base[i].delta_w);
    }
}

static inline void
do_backprop_work (void *user_data)
{
  MiniBatchResult *result = (MiniBatchResult *) user_data;
  backprop (result->network,
            result->input, result->output,
            result->forward, result->backward);
}

static void
update_mini_batch (Network *network, float *mini_batch, u32 mini_batch_size,
                   float eta, float lmbda, u32 n)
{
  u32 input_size = network->layers.base[0].width;
  u32 output_size = network->layers.base[network->layers.nmemb - 1].height;
  u32 sample_size = input_size + output_size;

  assert (mini_batch_size <= network->mini_batch_results.nmemb);

  BackwardResult *result = network->mini_batch_backward_result;
  for (u32 i = 0; i < result->layers.nmemb; ++i)
    {
      BackwardResultLayer *layer = &result->layers.base[i];
      memset (layer->delta_b, 0, sizeof (float) * layer->height);
      memset (layer->delta_w, 0, sizeof (float) * layer->width * layer->height);
    }

  for (u32 i = 0; i < mini_batch_size; ++i)
    {
      u32 input_offset = i * sample_size;
      u32 output_offset = input_offset + input_size;

      MiniBatchResult *result = &network->mini_batch_results.base[i];
      result->input = mini_batch + input_offset;
      result->output = mini_batch + output_offset;

      /* do_backprop_work (result); */
      enqueue_work (network->work_queue, do_backprop_work, result);
    }
  complete_all_work (network->work_queue);

  for (u32 i = 0; i < mini_batch_size; ++i)
    {
      BackwardResult *delta = network->mini_batch_results.base[i].backward;
      for (u32 j = 0; j < delta->layers.nmemb; ++j)
        {
          BackwardResultLayer *layer = &result->layers.base[j];
          BackwardResultLayer *dlayer = &delta->layers.base[j];
          u32 width = dlayer->width;
          u32 height = dlayer->height;
          for (u32 y = 0; y < height; ++y)
            {
              layer->delta_b[y] += dlayer->delta_b[y];
              for (u32 x = 0, offset = y * width; x < width; ++x)
                layer->delta_w[offset + x] += dlayer->delta_w[offset + x];
            }
        }
    }

  for (u32 i = 0; i < result->layers.nmemb; ++i)
    {
      NetworkLayer *layer = &network->layers.base[i];
      BackwardResultLayer *dlayer = &result->layers.base[i];
      u32 width = layer->width;
      u32 height = layer->height;
      float *b = layer->biases;
      float *w = layer->weights;
      float *nb = dlayer->delta_b;
      float *nw = dlayer->delta_w;
      for (u32 y = 0; y < height; ++y)
        {
          b[y] -= ((eta / mini_batch_size) * nb[y]);

          u32 offset = width * y;
          for (u32 x = 0; x < width; ++x)
            {
              float tmp = w[offset + x] - ((eta / mini_batch_size) * nw[offset + x]);
              w[offset + x] = (1.f - (eta * (lmbda / n))) * tmp;
            }
        }
    }
}

void
network_sgd (Network *network, float *training_data, u32 training_data_count,
             u32 epochs, float eta, float lmbda)
{
  u32 sample_size = (network->layers.base[0].width
                     + network->layers.base[network->layers.nmemb - 1].height);

  u32 evaluation_data_count = training_data_count / 60; // @Hardcode
  training_data_count -= evaluation_data_count;
  float *evaluation_data = training_data + (sample_size * training_data_count);

  for (u32 j = 0; j < epochs; ++j)
    {
      qsort (training_data, training_data_count,
             sizeof (training_data[0]) * sample_size, rand_cmp);

      u64 start_tick = get_ticks ();
      u32 mini_batch_size = network->mini_batch_results.nmemb;
      for (u32 k = 0; k < training_data_count; k += mini_batch_size)
        {
          float *mini_batch = training_data + (sample_size * k);
          u32 actual_batch_size = mini_batch_size;
          if (k + mini_batch_size > training_data_count)
            actual_batch_size = training_data_count - k;
          update_mini_batch (network, mini_batch, actual_batch_size, eta, lmbda,
                             training_data_count);
        }
      u64 end_tick = get_ticks ();

      printf ("epoch %u done in %.3fs\n", j,
              (float) (end_tick - start_tick) / TICKS_PER_SECOND);
      u32 correct_count = 0;
      for (u32 k = 0; k < evaluation_data_count; ++k)
        {
          float *sample = evaluation_data + (sample_size * k);
          if (evaluate_network (network, sample,
                                sample + network->layers.base[0].width))
            ++correct_count;
        }
      printf ("Accuracy on evaluation data: %u / %u\n",
              correct_count, evaluation_data_count);
    }
}

#endif /* ! NETWORK_H */
