/*
 *   DDS GIMP plugin
 *
 *   Copyright (C) 2004-2012 Shawn Kirst <skirst@gmail.com>,
 *   with parts (C) 2003 Arne Reuter <homepage@arnereuter.de> where specified.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public
 *   License as published by the Free Software Foundation; either
 *   version 2 of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; see the file COPYING.  If not, write to
 *   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *   Boston, MA 02111-1307, USA.
 */

#ifndef VEC_H
#define VEC_H

#include <math.h>

#ifdef __SSE__
#define USE_SSE 1
#endif

#ifdef USE_SSE
#include <xmmintrin.h>
# ifdef __SSE2__
#  include <emmintrin.h>
# endif
#endif

#include "imath.h"

typedef float vec3_t[3];
typedef float vec4_t __attribute__((vector_size(16)));
typedef float sym3x3_t[6];

#define VEC4_CONST4(x, y, z, w)  {x, y, z, w}
#define VEC4_CONST1(x)           {x, x, x, x}

static inline void vec3_set(vec3_t v, float x, float y, float z)
{
   v[0] = x; v[1] = y; v[2] = z;
}

static inline void vec3_copy(vec3_t r, const vec3_t v)
{
   r[0] = v[0]; r[1] = v[1]; r[2] = v[2];
}

static inline void vec3_add(vec3_t v, const vec3_t a, const vec3_t b)
{
   v[0] = a[0] + b[0];
   v[1] = a[1] + b[1];
   v[2] = a[2] + b[2];
}

static inline void vec3_sub(vec3_t v, const vec3_t a, const vec3_t b)
{
   v[0] = a[0] - b[0];
   v[1] = a[1] - b[1];
   v[2] = a[2] - b[2];
}

static inline void vec3_mul(vec3_t v, const vec3_t a, const vec3_t b)
{
   v[0] = a[0] * b[0];
   v[1] = a[1] * b[1];
   v[2] = a[2] * b[2];
}

static inline void vec3_muls(vec3_t v, const vec3_t a, float b)
{
   v[0] = a[0] * b;
   v[1] = a[1] * b;
   v[2] = a[2] * b;
}

static inline void vec3_madds(vec3_t v, const vec3_t a, const vec3_t b, float c)
{
   v[0] = a[0] * b[0] + c;
   v[1] = a[1] * b[1] + c;
   v[2] = a[2] * b[2] + c;
}

static inline float vec3_dot(const vec3_t a, const vec3_t b)
{
   return(a[0] * b[0] + a[1] * b[1] + a[2] * b[2]);
}

static inline void vec3_mins(vec3_t r, const vec3_t v, float s)
{
   r[0] = v[0] < s ? v[0] : s;
   r[1] = v[1] < s ? v[1] : s;
   r[2] = v[2] < s ? v[2] : s;
}

static inline void vec3_maxs(vec3_t r, const vec3_t v, float s)
{
   r[0] = v[0] > s ? v[0] : s;
   r[1] = v[1] > s ? v[1] : s;
   r[2] = v[2] > s ? v[2] : s;
}

static inline void vec3_trunc(vec3_t r, const vec3_t v)
{
   r[0] = v[0] > 0.0f ? floorf(v[0]) : ceilf(v[0]);
   r[1] = v[1] > 0.0f ? floorf(v[1]) : ceilf(v[1]);
   r[2] = v[2] > 0.0f ? floorf(v[2]) : ceilf(v[2]);
}

static inline vec4_t vec4_set(float x, float y, float z, float w)
{
   vec4_t v = {x, y, z, w};
   return(v);
}

static inline vec4_t vec4_set1(float f)
{
   vec4_t v = {f, f, f, f};
   return(v);
}

static inline void vec4_to_vec3(vec3_t r, const vec4_t v)
{
   r[0] = v[0]; r[1] = v[1]; r[2] = v[2];
}

static inline vec4_t vec4_splatx(const vec4_t v)
{
#ifdef USE_SSE
   return(_mm_shuffle_ps(v, v, 0x00));
#else
   vec4_t r = {v[0], v[0], v[0], v[0]};
   return(r);
#endif
}

static inline vec4_t vec4_splaty(const vec4_t v)
{
#ifdef USE_SSE
   return(_mm_shuffle_ps(v, v, 0x55));
#else
   vec4_t r = {v[1], v[1], v[1], v[1]};
   return(r);
#endif
}

static inline vec4_t vec4_splatz(const vec4_t v)
{
#ifdef USE_SSE
   return(_mm_shuffle_ps(v, v, 0xaa));
#else
   vec4_t r = {v[2], v[2], v[2], v[2]};
   return(r);
#endif
}

static inline vec4_t vec4_splatw(const vec4_t v)
{
#ifdef USE_SSE
   return(_mm_shuffle_ps(v, v, 0xff));
#else
   vec4_t r = {v[3], v[3], v[3], v[3]};
   return(r);
#endif
}

static inline vec4_t vec4_rcp(const vec4_t v)
{
#ifdef USE_SSE
   __m128 est  = _mm_rcp_ps(v);
   __m128 diff = _mm_sub_ps(_mm_set1_ps(1.0f), _mm_mul_ps(est, v));
   return(_mm_add_ps(_mm_mul_ps(diff, est), est));
#else
   vec4_t one = {1.0f, 1.0f, 1.0f, 1.0f};
   return(one / v);
#endif
}

static inline vec4_t vec4_min(const vec4_t a, const vec4_t b)
{
#ifdef USE_SSE
   return(_mm_min_ps(a, b));
#else
   return(vec4_set(MIN(a[0], b[0]), MIN(a[1], b[1]), MIN(a[2], b[2]), MIN(a[3], b[3])));
#endif
}

static inline vec4_t vec4_max(const vec4_t a, const vec4_t b)
{
#ifdef USE_SSE
   return(_mm_max_ps(a, b));
#else
   return(vec4_set(MAX(a[0], b[0]), MAX(a[1], b[1]), MAX(a[2], b[2]), MAX(a[3], b[3])));
#endif
}

static inline vec4_t vec4_trunc(const vec4_t v)
{
#ifdef USE_SSE
# ifdef __SSE2__
   return(_mm_cvtepi32_ps(_mm_cvttps_epi32(v)));
# else
   // convert to ints
   __m128 in = v;
   __m64 lo = _mm_cvttps_pi32(in);
   __m64 hi = _mm_cvttps_pi32(_mm_movehl_ps(in, in));
   // convert to floats
   __m128 part = _mm_movelh_ps(in, _mm_cvtpi32_ps(in, hi));
   __m128 trunc = _mm_cvtpi32_ps(part, lo);
   // clear mmx state
   _mm_empty();
   return(trunc);
# endif
#else
   vec4_t r = {
      v[0] > 0.0f ? floorf(v[0]) : ceil(v[0]),
      v[1] > 0.0f ? floorf(v[1]) : ceil(v[1]),
      v[2] > 0.0f ? floorf(v[2]) : ceil(v[2]),
      v[3] > 0.0f ? floorf(v[3]) : ceil(v[3]),
   };
   return(r);
#endif
}

static inline int vec4_cmplt(const vec4_t l, const vec4_t r)
{
#ifdef USE_SSE
   __m128 bits = _mm_cmplt_ps(l, r);
   int value = _mm_movemask_ps(bits);
   return(value != 0);
#else
   return((l[0] < r[0]) || (l[1] < r[1]) || (l[2] < r[2]) || (l[3] < r[3]));
#endif
}

#endif
