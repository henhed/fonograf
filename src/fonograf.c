#include "types.h"

#include <assert.h>
#include <xmmintrin.h>

#define TAU 6.28318530718f
#define TWELFTH_ROOT_OF_TWO 1.05946309436f
#define sinf(x) __builtin_sinf (x)

typedef struct
{
  float *samples;
  u8     nchannels;
  u32    nframes;
} SoundWindow;

static struct {
  u32 frame_rate;
  u32 window_size;
} g_config = {};

SoundWindow test_win1;
SoundWindow test_win2;

void
init_fonograf (u32 frame_rate, u32 window_size)
{
  g_config.frame_rate = frame_rate;
  g_config.window_size = window_size;
  test_win1 = (SoundWindow) {
    .samples   = calloc (window_size, sizeof (float)),
    .nchannels = 1,
    .nframes   = window_size
  };
  test_win2 = (SoundWindow) {
    .samples   = calloc (window_size, sizeof (float)),
    .nchannels = 1,
    .nframes   = window_size
  };
}

void
cleanup_fonograf ()
{
  free (test_win1.samples);
  free (test_win2.samples);
}

void
amplify (SoundWindow win, float amount)
{
  assert ((win.nframes & ~(win.nframes - 1)) == win.nframes); // Assert POW2
  u32 nsamples = win.nframes * win.nchannels;
  __m128 multiplier = _mm_set1_ps (amount);
  for (u32 i = 0; i < nsamples; i += 4)
    _mm_store_ps (win.samples + i,
                  _mm_mul_ps (_mm_load_ps (win.samples + i),
                              multiplier));
}

void
cross_the_streams (SoundWindow left, SoundWindow right, SoundWindow out)
{
  assert (out.nframes == left.nframes);
  assert (out.nframes == right.nframes);

  for (u32 f = 0; f < out.nframes; ++f)
    {
      u16 oc = 0;
      u32 of = f * out.nchannels;
      u32 lf = f * left.nchannels;
      u32 rf = f * right.nchannels;
      for (u8 c = 0; c < left.nchannels; ++c, ++oc)
        out.samples[of + (oc % out.nchannels)] += left.samples[lf + c];
      for (u8 c = 0; c < right.nchannels; ++c, ++oc)
        out.samples[of + (oc % out.nchannels)] += right.samples[rf + c];
    }
}

void
generate_sound (SoundWindow output)
{
  assert (output.nframes == test_win1.nframes);
  assert (output.nframes == test_win2.nframes);

  static u32 call_count = 0;
  static float hz1 = 440.f;
  static float hz2 = 880.f;

  if (0 == (call_count++ & 0x3F))
    {
      hz1 *= TWELFTH_ROOT_OF_TWO;
      hz2 *= TWELFTH_ROOT_OF_TWO;
    }

  assert (test_win1.nchannels == 1);
  for (u32 f = 0; f < test_win1.nframes; ++f)
    {
      static float t1 = 0.f;
      float sample = sinf (t1 * TAU) * .4f;
      test_win1.samples[f] = sample;
      t1 += 1.f / ((float) g_config.frame_rate / hz1);
    }
  amplify (test_win1, .3f);

  assert (test_win2.nchannels == 1);
  for (u32 f = 0; f < test_win2.nframes; ++f)
    {
      static float t2 = 0.f;
      float sample = sinf (t2 * TAU) * .4f;
      test_win2.samples[f] = sample;
      t2 += 1.f / ((float) g_config.frame_rate / hz2);
    }

  u32 nsamples = output.nframes * output.nchannels;
  for (u32 i = 0; i < nsamples; i += 4)
    _mm_store_ps (output.samples + i, _mm_set1_ps (0.f));

  cross_the_streams (test_win1, test_win2, output);
}
