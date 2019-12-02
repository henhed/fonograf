#ifndef MATHS_H
#define MATHS_H 1

#include <stdlib.h>
#include <float.h>

#include <immintrin.h>

#define TWO_PI 6.28318530718f

#define sqrtf(x)    _mm_cvtss_f32 (_mm_sqrt_ss (_mm_set_ss (x)))
#define sinf(x)     __builtin_sinf (x)
#define cosf(x)     __builtin_cosf (x)
#define logf(x)     __builtin_logf (x)

float
generate_gaussian_noise (float median, float variance)
{

  static float z1;
  static bool generate;
  generate = !generate;

  if (!generate)
    return (z1 * variance) + median;

  float u1, u2;
  do
    {
      u1 = rand () * (1.f / RAND_MAX);
      u2 = rand () * (1.f / RAND_MAX);
    }
  while (u1 <= FLT_MIN);

  float z0;
  z0 = sqrtf (-2.f * logf (u1)) * cosf (TWO_PI * u2);
  z1 = sqrtf (-2.f * logf (u1)) * sinf (TWO_PI * u2);

  return (z0 * variance) + median;
}

#endif /* ! MATHS_H */
