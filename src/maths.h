#ifndef MATHS_H
#define MATHS_H 1

#include <stdlib.h>
#include <float.h>

#include <immintrin.h>

#define M_PI 3.14159265358979323846
#define M_TAU 6.28318530718
#define M_E 2.7182818284590452354

#define sqrtf(x)    _mm_cvtss_f32 (_mm_sqrt_ss (_mm_set_ss (x)))
#define sinf(x)     __builtin_sinf (x)
#define cosf(x)     __builtin_cosf (x)
#define logf(x)     __builtin_logf (x)
#define powf(x, y)  __builtin_powf (x, y)

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
  z0 = sqrtf (-2.f * logf (u1)) * cosf (M_TAU * u2);
  z1 = sqrtf (-2.f * logf (u1)) * sinf (M_TAU * u2);

  return (z0 * variance) + median;
}

#endif /* ! MATHS_H */
