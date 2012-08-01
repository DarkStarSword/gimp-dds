/*
	DDS GIMP plugin

	Copyright (C) 2004-2012 Shawn Kirst <skirst@gmail.com>,
   with parts (C) 2003 Arne Reuter <homepage@arnereuter.de> where specified.

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public
	License as published by the Free Software Foundation; either
	version 2 of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA.
*/

/* -----------------------------------------------------------------------------

	Copyright (c) 2006 Simon Brown                          si@sjbrown.co.uk
	Copyright (c) 2007 Ignacio Castano                   icastano@nvidia.com

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files (the
	"Software"), to	deal in the Software without restriction, including
	without limitation the rights to use, copy, modify, merge, publish,
	distribute, sublicense, and/or sell copies of the Software, and to
	permit persons to whom the Software is furnished to do so, subject to
	the following conditions:

	The above copyright notice and this permission notice shall be included
	in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
	OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
	CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
	TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   -------------------------------------------------------------------------- */

/*
 * What follows is a C port of Simon Brown's C++ libsquish library
 * http://code.google.com/p/libsquish/
 */

#include <stdlib.h>
#include <math.h>
#include <float.h>

#include "squish.h"
#include "imath.h"
#include "vec.h"
#include "dxt_tables.h"

#define CLUSTERFIT_ITERATIONS  8

#define SWAP(a, b)  do { typeof(a) t; t = a; a = b; b = t; } while(0)

/* SIMD constants */
static const vec4_t V4ZERO       = VEC4_CONST1(0.0f);
static const vec4_t V4ONE        = VEC4_CONST1(1.0f);
static const vec4_t V4TWO        = VEC4_CONST1(2.0f);
static const vec4_t V4HALF       = VEC4_CONST1(0.5f);
static const vec4_t V4HALF_HALF2 = VEC4_CONST4(0.5f, 0.5f, 0.5f, 0.25f);
static const vec4_t V4ONETHIRD   = VEC4_CONST3(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f);
static const vec4_t V4TWOTHIRDS  = VEC4_CONST3(2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f);
static const vec4_t V4ONETHIRD_ONETHIRD2   = VEC4_CONST4(1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 9.0f);
static const vec4_t V4TWOTHIRDS_TWOTHIRDS2 = VEC4_CONST4(2.0f / 3.0f, 2.0f / 3.0f, 2.0f / 3.0f, 4.0f / 9.0f);
static const vec4_t V4TWONINTHS  = VEC4_CONST1(2.0f / 9.0f);
static const vec4_t V4GRID       = VEC4_CONST3(31.0f, 63.0f, 31.0f);
static const vec4_t V4GRIDRCP    = VEC4_CONST3(1.0f / 31.0f, 1.0f / 63.0f, 1.0f / 31.0f);

typedef struct
{
   int count;
   vec4_t points[16];
   float weights[16];
   int remap[16];
   int transparent;
} colorset_t;

typedef struct
{
   colorset_t *colors;
   vec4_t start;
   vec4_t end;
   vec4_t metric;
   float besterror;
} rangefit_t;

typedef struct
{
   colorset_t *colors;
   int flags;
   unsigned char order[16 * CLUSTERFIT_ITERATIONS];
   vec4_t principle;
   vec4_t points_weights[16];
   vec4_t xsum_wsum;
   vec4_t metric;
   float besterror;
} clusterfit_t;

static void vec4_endpoints_to_565(int *start, int *end, const vec4_t a, const vec4_t b)
{
   int c[8] __attribute__((aligned(16)));
   vec4_t ta = a * V4GRID + V4HALF;
   vec4_t tb = b * V4GRID + V4HALF;

#ifdef USE_SSE
# ifdef __SSE2__
   const __m128i C565 = _mm_setr_epi16(31, 63, 31, 0, 31, 63, 31, 0);
   __m128i ia = _mm_cvttps_epi32(ta);
   __m128i ib = _mm_cvttps_epi32(tb);
   __m128i zero = _mm_setzero_si128();
   __m128i s = _mm_packs_epi32(ia, ib);
   s = _mm_min_epi16(C565, _mm_max_epi16(zero, s));
   *((__m128i *)&c[0]) = _mm_unpacklo_epi16(s, zero);
   *((__m128i *)&c[4]) = _mm_unpackhi_epi16(s, zero);
   # else
   const __m64 C565 = _mm_setr_pi16(31, 63, 31, 0);
   __m64 lo, hi, c0, c1;
   __m64 zero = _mm_setzero_si64();
   lo = _mm_cvttps_pi32(ta);
   hi = _mm_cvttps_pi32(_mm_movehl_ps(ta, ta));
   c0 = _mm_packs_pi32(lo, hi);
   lo = _mm_cvttps_pi32(tb);
   hi = _mm_cvttps_pi32(_mm_movehl_ps(tb, tb));
   c1 = _mm_packs_pi32(lo, hi);
   c0 = _mm_min_pi16(C565, _mm_max_pi16(zero, c0));
   c1 = _mm_min_pi16(C565, _mm_max_pi16(zero, c1));
   *((__m64 *)&c[0]) = _mm_unpacklo_pi16(c0, zero);
   *((__m64 *)&c[2]) = _mm_unpackhi_pi16(c0, zero);
   *((__m64 *)&c[4]) = _mm_unpacklo_pi16(c1, zero);
   *((__m64 *)&c[6]) = _mm_unpackhi_pi16(c1, zero);
   _mm_empty();
# endif
#else
   c[0] = (int)ta[0]; c[4] = (int)tb[0];
   c[1] = (int)ta[1]; c[5] = (int)tb[1];
   c[2] = (int)ta[2]; c[6] = (int)tb[2];
   c[0] = MIN(31, MAX(0, c[0]));
   c[1] = MIN(63, MAX(0, c[1]));
   c[2] = MIN(31, MAX(0, c[2]));
   c[4] = MIN(31, MAX(0, c[4]));
   c[5] = MIN(63, MAX(0, c[5]));
   c[6] = MIN(31, MAX(0, c[6]));
#endif

   *start = ((c[2] << 11) | (c[1] << 5) | c[0]);
   *end   = ((c[6] << 11) | (c[5] << 5) | c[4]);
}

static void write_color_block(int a, int b, unsigned char *indices, unsigned char *block)
{
   int i;
   unsigned char *ind;

   block[0] = (unsigned char)(a & 0xff);
   block[1] = (unsigned char)(a >> 8);
   block[2] = (unsigned char)(b & 0xff);
   block[3] = (unsigned char)(b >> 8);

   for(i = 0; i < 4; ++i)
   {
      ind = indices + 4 * i;
      block[4 + i] = ind[0] | (ind[1] << 2) | (ind[2] << 4) | (ind[3] << 6);
   }
}

static void write_color_block3(int a, int b, unsigned char *indices, unsigned char *block)
{
   int i;
   unsigned char remapped[16];

   if(a <= b)
   {
      for(i = 0; i < 16; ++i)
         remapped[i] = indices[i];
   }
   else
   {
      SWAP(a, b);
      for(i = 0; i < 16; ++i)
      {
         if(indices[i] == 0)
            remapped[i] = 1;
         else if(indices[i] == 1)
            remapped[i] = 0;
         else
            remapped[i] = indices[i];
      }
   }

   write_color_block(a, b, remapped, block);
}

static void write_color_block4(int a, int b, unsigned char *indices, unsigned char *block)
{
   int i;
   unsigned char remapped[16];

   if(a < b)
   {
      SWAP(a, b);
      for(i = 0; i < 16; ++i)
         remapped[i] = (indices[i] ^ 0x01) & 0x03;
   }
   else if(a == b)
   {
      for(i = 0; i < 16; ++i)
         remapped[i] = 0;
   }
   else
   {
      for(i = 0; i < 16; ++i)
         remapped[i] = indices[i];
   }

   write_color_block(a, b, remapped, block);
}

static void colorset_init(colorset_t *colors, const unsigned char *rgba, int mask, int flags)
{
   int i, j, bit, oldbit, match, index;
   int dxt1 = (flags & SQUISH_DXT1) != 0;
   int wba = (flags & SQUISH_WEIGHTBYALPHA) != 0;
   float x, y, z, w;

   colors->count = 0;
   colors->transparent = 0;

   // create the minimal set
   for(i = 0; i < 16; ++i)
   {
      // check if this pixel is enabled
      bit = 1 << i;
      if((mask & bit) == 0)
      {
         colors->remap[i] = -1;
         continue;
      }

      // check for transparent pixels when using DXT1
      if(dxt1 && rgba[4 * i + 3] < 128)
      {
         colors->remap[i] = -1;
         colors->transparent = 1;
         continue;
      }

      // loop over previous points for a match
      for(j = 0; ; ++j)
      {
         // allocate a new point
         if(j == i)
         {
            // normalize coordinates to [0,1]
            x = (float)rgba[4 * i + 0] / 255.0f;
            y = (float)rgba[4 * i + 1] / 255.0f;
            z = (float)rgba[4 * i + 2] / 255.0f;

            // ensure there is always non-zero weight even for zero alpha
            w = (float)(rgba[4 * i + 3] + 1) / 256.0f;

            // add the point
            colors->points[colors->count] = vec4_set(x, y, z, 0.0f);
            colors->weights[colors->count] = wba ? w : 1.0f;
            colors->remap[i] = colors->count;

            // advance
            ++colors->count;
            break;
         }

         // check for a match
         oldbit = 1 << j;
         match = ((mask & oldbit) != 0) &&
            (rgba[4 * i + 0] == rgba[4 * j + 0]) &&
            (rgba[4 * i + 1] == rgba[4 * j + 1]) &&
            (rgba[4 * i + 2] == rgba[4 * j + 2]) &&
            (rgba[4 * j + 3] >= 128 || !dxt1);
         if(match)
         {
            // get the index of the match
            index = colors->remap[j];

            // ensure there is always non-zero weight even for zero alpha
            w = (float)(rgba[4 * i + 3] + 1) / 256.0f;

            // map to this point and increase the weight
            colors->weights[index] += (wba ? w : 1.0f);
            colors->remap[i] = index;
            break;
         }
      }
   }

   // square root the weights
   for(i = 0; i < colors->count; ++i)
      colors->weights[i] = sqrtf(colors->weights[i]);
}

static void colorset_remap_indices(colorset_t *colors,
                                   const unsigned char *source,
                                   unsigned char *target)
{
   int i, idx;

   for(i = 0; i < 16; ++i)
   {
      idx = colors->remap[i];
      if(idx == -1)
         target[i] = 3;
      else
         target[i] = source[idx];
   }
}

static void compute_weighted_covariance(sym3x3_t cov, colorset_t *colors)
{
   int i;
   vec4_t total, centroid, w, a, b;

   for(i = 0; i < 6; ++i) cov[i] = 0.0f;

   // compute the centroid
   total = vec4_zero();
   centroid = vec4_zero();

   for(i = 0; i < colors->count; ++i)
   {
      w = vec4_set1(colors->weights[i]);
      total += w;
      centroid += colors->points[i] * w;
   }

   // normalize centroid
   centroid *= vec4_rcp(total);

   // accumulate the covariance matrix
   for(i = 0; i < colors->count; ++i)
   {
      a = colors->points[i] - centroid;
      b = a * vec4_set1(colors->weights[i]);

      cov[0] += a[0] * b[0];
      cov[1] += a[0] * b[1];
      cov[2] += a[0] * b[2];
      cov[3] += a[1] * b[1];
      cov[4] += a[1] * b[2];
      cov[5] += a[2] * b[2];
   }
}

static vec4_t compute_principle_component(const sym3x3_t matrix)
{
   const int POWER_ITERATIONS = 8;
   vec4_t row0, row1, row2, v, w, a;
   int i;

   v = vec4_set1(1.0f);
   row0 = vec4_set(matrix[0], matrix[1], matrix[2], 0.0f);
   row1 = vec4_set(matrix[1], matrix[3], matrix[4], 0.0f);
   row2 = vec4_set(matrix[2], matrix[4], matrix[5], 0.0f);

   for(i = 0; i < POWER_ITERATIONS; ++i)
   {
      // matrix multiply
      w = row0 * vec4_splatx(v);
      w = row1 * vec4_splaty(v) + w;
      w = row2 * vec4_splatz(v) + w;

      // get max component from xyz in all channels
      a = vec4_rcp(vec4_max(vec4_splatx(v), vec4_max(vec4_splaty(v), vec4_splatz(v))));

      // divide through and advance
      v = w * a;
   }

   return(v);
}

static void rangefit_init(rangefit_t *rf, colorset_t *colors, int flags)
{
   sym3x3_t covariance;
   vec4_t principle, start, end;
   float min, max, val;
   int i;

   rf->colors = colors;

   rf->besterror = FLT_MAX;

   if(flags & SQUISH_PERCEPTUAL)
      rf->metric = vec4_set(0.2126f, 0.7152f, 0.0722f, 0.0f);
   else
      rf->metric = vec4_set(1.0f, 1.0f, 1.0f, 0.0f);

   // get the covariance matrix
   compute_weighted_covariance(covariance, rf->colors);
   // compute the principle component
   principle = compute_principle_component(covariance);

   // get the min and max range as the codebook endpoints
   start = end = V4ZERO;

   if(rf->colors->count > 0)
   {
      // compute the range
      start = end = rf->colors->points[0];
      min = max = vec4_dot(rf->colors->points[0], principle);
      for(i = 1; i < rf->colors->count; ++i)
      {
         val = vec4_dot(rf->colors->points[i], principle);
         if(val < min)
         {
            start = rf->colors->points[i];
            min = val;
         }
         else if(val > max)
         {
            end = rf->colors->points[i];
            max = val;
         }
      }
   }

   // clamp the output to [0,1]
   start = vec4_min(V4ONE, vec4_max(V4ZERO, start));
   end   = vec4_min(V4ONE, vec4_max(V4ZERO, end));
   // clamp to the grid and save
   rf->start = (V4GRID * start + V4HALF) * V4GRIDRCP;
   rf->end   = (V4GRID * end   + V4HALF) * V4GRIDRCP;
}

static void rangefit_compress3(rangefit_t *rf, unsigned char *block)
{
   vec4_t codes[3], temp;
   float error = 0, dist, d;
   int i, j, idx, start, end;
   unsigned char closest[16], indices[16];

   // create a codebook
   codes[0] = rf->start;
   codes[1] = rf->end;
   codes[2] = (rf->start * V4HALF) + (rf->end * V4HALF);

   // match each point to the closest code
   for(i = 0; i < rf->colors->count; ++i)
   {
      // find the closest code
      dist = FLT_MAX;
      idx = 0;
      for(j = 0; j < 3; ++j)
      {
         temp = (rf->colors->points[i] - codes[j]) * rf->metric;
         d = vec4_dot(temp, temp);
         if(d < dist)
         {
            dist = d;
            idx = j;
         }
      }
      // save the index and accumulate error
      closest[i] = idx;
      error += dist;
   }

   // save this scheme if it wins
   if(error < rf->besterror)
   {
      // remap the indices
      colorset_remap_indices(rf->colors, closest, indices);
      // convert endpoints
      vec4_endpoints_to_565(&start, &end, rf->start, rf->end);
      // save the block
      write_color_block3(start, end, indices, block);
      // save the error
      rf->besterror = error;
   }
}

static void rangefit_compress4(rangefit_t *rf, unsigned char *block)
{
   vec4_t codes[4], temp;
   float error = 0, dist, d;
   int i, j, idx, start, end;
   unsigned char closest[16], indices[16];

   // create a codebook
   codes[0] = rf->start;
   codes[1] = rf->end;
   codes[2] = (rf->start * V4TWOTHIRDS) + (rf->end * V4ONETHIRD );
   codes[3] = (rf->start * V4ONETHIRD ) + (rf->end * V4TWOTHIRDS);

   // match each point to the closest code
   for(i = 0; i < rf->colors->count; ++i)
   {
      // find the closest code
      dist = FLT_MAX;
      idx = 0;
      for(j = 0; j < 4; ++j)
      {
         temp = (rf->colors->points[i] - codes[j]) * rf->metric;
         d = vec4_dot(temp, temp);
         if(d < dist)
         {
            dist = d;
            idx = j;
         }
      }

      // save the index and accumulate error
      closest[i] = idx;
      error += dist;
   }

   // save this scheme if it wins
   if(error < rf->besterror)
   {
      // remap the indices
      colorset_remap_indices(rf->colors, closest, indices);
      // convert endpoints
      vec4_endpoints_to_565(&start, &end, rf->start, rf->end);
      // save the block
      write_color_block4(start, end, indices, block);
      // save the error
      rf->besterror = error;
   }
}

static void clusterfit_init(clusterfit_t *cf, colorset_t *colors, int flags)
{
   sym3x3_t covariance;

   cf->colors = colors;

   cf->besterror = FLT_MAX;

   if(flags & SQUISH_PERCEPTUAL)
      cf->metric = vec4_set(0.2126f, 0.7152f, 0.0722f, 0.0f);
   else
      cf->metric = vec4_set(1.0f, 1.0f, 1.0f, 0.0f);

   compute_weighted_covariance(covariance, cf->colors);
   cf->principle = compute_principle_component(covariance);
}

static int clusterfit_construct_ordering(clusterfit_t *cf, const vec4_t axis, int iteration)
{
   int i, j, it, same;
   float dps[16];
   unsigned char *order = cf->order + 16 * iteration;
   unsigned char *prev;
   vec4_t p, w, x;

   // build the list of dot products
   for(i = 0; i < cf->colors->count; ++i)
   {
      dps[i] = vec4_dot(cf->colors->points[i], axis);
      order[i] = i;
   }

   // stable sort using them
   for(i = 0; i < cf->colors->count; ++i)
   {
      for(j = i; j > 0 && dps[j] < dps[j - 1]; --j)
      {
         SWAP(dps[j], dps[j - 1]);
         SWAP(order[j], order[j - 1]);
      }
   }

   // check if this ordering is unique
   for(it = 0; it < iteration; ++it)
   {
      prev = cf->order + 16 * it;
      same = 1;
      for(i = 0; i < cf->colors->count; ++i)
      {
         if(order[i] != prev[i])
         {
            same = 0;
            break;
         }
      }
      if(same)
         return(0);
   }

   // copy the ordering and weight all the points
   cf->xsum_wsum = vec4_set1(0);
   for(i = 0; i < cf->colors->count; ++i)
   {
      j = order[i];
      p = vec4_set(cf->colors->points[j][0], cf->colors->points[j][1], cf->colors->points[j][2], 1.0f);
      w = vec4_set1(cf->colors->weights[j]);
      x = p * w;
      cf->points_weights[i] = x;
      cf->xsum_wsum += x;
   }

   return(1);
}

static void clusterfit_compress3(clusterfit_t *cf, unsigned char *block)
{
   const int count = cf->colors->count;

   vec4_t beststart = VEC4_CONST1(0);
   vec4_t bestend = VEC4_CONST1(0);
   float error, besterror = cf->besterror;
   vec4_t part0, part1, part2;
   vec4_t alphax_sum, alpha2_sum;
   vec4_t betax_sum, beta2_sum;
   vec4_t alphabeta_sum;
   vec4_t factor, a, b, e1, e2, e3, e4;
   vec4_t axis;
   unsigned char bestindices[16], unordered[16];
   unsigned char *order;
   int bestiteration = 0, besti = 0, bestj = 0;
   int iteration, i, j, jmin, m, start, end;

   // prepare an ordering using the principle axis
   clusterfit_construct_ordering(cf, cf->principle, 0);

   // loop over iterations (we avoid the case that all the points are in the first or last cluster)
   for(iteration = 0; ; )
   {
      // first cluster [0,i) is at the start
      part0 = vec4_zero();
      for(i = 0; i < count; ++i)
      {
         // second cluster [i,j) is half along
         part1 = (i == 0) ? cf->points_weights[0] : vec4_zero();
         jmin = (i == 0) ? 1 : i;
         for(j = jmin; ; )
         {
            // last cluster [j,count) is at the end
            part2 = cf->xsum_wsum - part1 - part0;

            // compute least squares terms directly
            alphax_sum = part1 * V4HALF_HALF2 + part0;
            alpha2_sum = vec4_splatw(alphax_sum);
            betax_sum  = part1 * V4HALF_HALF2 + part2;
            beta2_sum  = vec4_splatw(betax_sum);

            alphabeta_sum = vec4_splatw(part1 * V4HALF_HALF2);

            // comput the least-squares optimal points
            factor = vec4_rcp(alpha2_sum * beta2_sum - alphabeta_sum * alphabeta_sum);
            a = (alphax_sum * beta2_sum  - betax_sum  * alphabeta_sum) * factor;
            b = (betax_sum  * alpha2_sum - alphax_sum * alphabeta_sum) * factor;

            // clamp to the grid
            a = vec4_min(V4ONE, vec4_max(V4ZERO, a));
            b = vec4_min(V4ONE, vec4_max(V4ZERO, b));
            a = vec4_trunc(V4GRID * a + V4HALF) * V4GRIDRCP;
            b = vec4_trunc(V4GRID * b + V4HALF) * V4GRIDRCP;

            // compute the error (we skip the constant xxsum)
            e1 = (a * a) * alpha2_sum + (b * b * beta2_sum);
            e2 = (a * b * alphabeta_sum) - a * alphax_sum;
            e3 = e2 - b * betax_sum;
            e4 = V4TWO * e3 + e1;

            // apply the metric to error term
            e4 *= cf->metric;

            // accumulate error term
            error = vec4_accum(e4);

            // keep the solution if it wins
            if(error < besterror)
            {
               beststart = a;
               bestend = b;
               besti = i;
               bestj = j;
               besterror = error;
               bestiteration = iteration;
            }

            // advance
            if(j == count)
               break;
            part1 += cf->points_weights[j];
            ++j;
         }

         // advance
         part0 += cf->points_weights[i];
      }

      // stop if we didn't improve in this iteration
      if(bestiteration != iteration)
         break;

      // advance if possible
      ++iteration;
      if(iteration == CLUSTERFIT_ITERATIONS)
         break;

      // stop if a new iteration is an ordering that has already been tried
      //vec4_to_vec3(axis, bestend - beststart);
      axis = bestend - beststart;

      if(!clusterfit_construct_ordering(cf, axis, iteration))
         break;
   }

   // save the block if necessary
   if(besterror < cf->besterror)
   {
      // remap the indices
      order = cf->order + 16 * bestiteration;

      for(m = 0;     m < besti; ++m) unordered[order[m]] = 0;
      for(m = besti; m < bestj; ++m) unordered[order[m]] = 2;
      for(m = bestj; m < count; ++m) unordered[order[m]] = 1;

      colorset_remap_indices(cf->colors, unordered, bestindices);

      // convert endpoints
      vec4_endpoints_to_565(&start, &end, beststart, bestend);

      // save the block
      write_color_block3(start, end, bestindices, block);

      // save the error
      cf->besterror = besterror;
   }
}

static void clusterfit_compress4(clusterfit_t *cf, unsigned char *block)
{
   const int count = cf->colors->count;

   vec4_t beststart = VEC4_CONST1(0.0f);
   vec4_t bestend = VEC4_CONST1(0.0f);
   float error, besterror = cf->besterror;
   vec4_t part0, part1, part2, part3;
   vec4_t alphax_sum, alpha2_sum;
   vec4_t betax_sum, beta2_sum;
   vec4_t alphabeta_sum;
   vec4_t factor, a, b, e1, e2, e3, e4;
   vec4_t axis;
   unsigned char bestindices[16], unordered[16];
   unsigned char *order;
   int bestiteration = 0, besti = 0, bestj = 0, bestk = 0;
   int iteration, i, j, k, kmin, m, start, end;

   // prepare an ordering using the principle axis
   clusterfit_construct_ordering(cf, cf->principle, 0);

   // loop over iterations (we avoid the case that all points are in the first or last cluster)
   for(iteration = 0; ; )
   {
      // first cluster [0,i) is at the start
      part0 = vec4_zero();
      for(i = 0; i < count; ++i)
      {
         // second cluster [i,j) is one third along
         part1 = vec4_zero();
         for(j = i; ; )
         {
            // third cluster [j,k) is two thirds along
            part2 = (j == 0) ? cf->points_weights[0] : vec4_zero();
            kmin = (j == 0) ? 1 : j;
            for(k = kmin; ; )
            {
               // last cluster [k,count) is at the end
               part3 = cf->xsum_wsum - part2 - part1 - part0;

               // compute least squares terms directly
               alphax_sum = part2 * V4ONETHIRD_ONETHIRD2 + (part1 * V4TWOTHIRDS_TWOTHIRDS2 + part0);
               alpha2_sum = vec4_splatw(alphax_sum);
               betax_sum  = part1 * V4ONETHIRD_ONETHIRD2 + (part2 * V4TWOTHIRDS_TWOTHIRDS2 + part3);
               beta2_sum  = vec4_splatw(betax_sum);

               alphabeta_sum = V4TWONINTHS * vec4_splatw(part1 + part2);

               // compute the least-squares optimal points
               factor = vec4_rcp(alpha2_sum * beta2_sum - alphabeta_sum * alphabeta_sum);
               a = (alphax_sum * beta2_sum  - betax_sum  * alphabeta_sum) * factor;
               b = (betax_sum  * alpha2_sum - alphax_sum * alphabeta_sum) * factor;

               // clamp to the grid
               a = vec4_min(V4ONE, vec4_max(V4ZERO, a));
               b = vec4_min(V4ONE, vec4_max(V4ZERO, b));
               a = vec4_trunc(V4GRID * a + V4HALF) * V4GRIDRCP;
               b = vec4_trunc(V4GRID * b + V4HALF) * V4GRIDRCP;

               // compute the error (we skip the constant xxsum)
               e1 = (a * a) * alpha2_sum + (b * b * beta2_sum);
               e2 = (a * b * alphabeta_sum) - a * alphax_sum;
               e3 = e2 - b * betax_sum;
               e4 = V4TWO * e3 + e1;

               // apply the metric to error term
               e4 *= cf->metric;

               // accumulate error term
               error = vec4_accum(e4);

               // keep the solution if it wins
               if(error < besterror)
               {
                  beststart = a;
                  bestend = b;
                  besterror = error;
                  besti = i;
                  bestj = j;
                  bestk = k;
                  bestiteration = iteration;
               }

               // advance
               if(k == count)
                  break;
               part2 += cf->points_weights[k];
               ++k;
            }

            // advance
            if(j == count)
               break;
            part1 += cf->points_weights[j];
            ++j;
         }

         // advance
         part0 += cf->points_weights[i];
      }

      // stop if we didn't improve in this iteration
      if(bestiteration != iteration)
         break;

      // advance if possible
      ++iteration;
      if(iteration == CLUSTERFIT_ITERATIONS)
         break;

      // stop if a new iteration is an ordering that has already been tried
      //vec4_to_vec3(axis, bestend - beststart);
      axis = bestend - beststart;

      if(!clusterfit_construct_ordering(cf, axis, iteration))
         break;
   }

   // save the block if necessary
   if(besterror < cf->besterror)
   {
      // remap the indices
      order = cf->order + 16 * bestiteration;

      for(m = 0;     m < besti; ++m) unordered[order[m]] = 0;
      for(m = besti; m < bestj; ++m) unordered[order[m]] = 2;
      for(m = bestj; m < bestk; ++m) unordered[order[m]] = 3;
      for(m = bestk; m < count; ++m) unordered[order[m]] = 1;

      colorset_remap_indices(cf->colors, unordered, bestindices);

      // convert endpoints
      vec4_endpoints_to_565(&start, &end, beststart, bestend);

      // save the block
      write_color_block4(start, end, bestindices, block);

      // save the error
      cf->besterror = besterror;
   }
}

void squish_compress(unsigned char *dst, const unsigned char *block, int flags)
{
   colorset_t colors;
   rangefit_t rf;
   clusterfit_t cf;
   int i, start, end;
   unsigned int indices;

   colorset_init(&colors, block, 0xffff, flags);

   if(colors.count == 1)
   {
      start = (omatch5[block[2]][0] << 11) |
              (omatch6[block[1]][0] <<  5) |
              (omatch5[block[0]][0]      );
      end   = (omatch5[block[2]][1] << 11) |
              (omatch6[block[1]][1] <<  5) |
              (omatch5[block[0]][1]      );

      indices = 0xaaaaaaaa;  // 101010...

      if(colors.transparent)
      {
         for(i = 0; i < 16; ++i)
         {
            if(block[4 * i +  3] < 128)
               indices |= (3 << (2 * i));  // set index to 3
         }
         if(start > end)
            SWAP(start, end);
      }
      else
      {
         if(start < end)
         {
            SWAP(start, end);
            indices ^= 0x55555555; // 010101...
         }
      }

      // write endpoints
      dst[0] = (unsigned char)(start & 0xff);
      dst[1] = (unsigned char)(start >> 8);
      dst[2] = (unsigned char)(end & 0xff);
      dst[3] = (unsigned char)(end >> 8);
      // write indices
      dst[4] = (unsigned char)(indices      );
      dst[5] = (unsigned char)(indices >>  8);
      dst[6] = (unsigned char)(indices >> 16);
      dst[7] = (unsigned char)(indices >> 24);
   }
   else if(colors.count == 0 || (flags & SQUISH_FASTCOMPRESS))
   {
      rangefit_init(&rf, &colors, flags);
      if(flags & SQUISH_DXT1)
      {
         rangefit_compress3(&rf, dst);
         if(!colors.transparent)
            rangefit_compress4(&rf, dst);
      }
      else
         rangefit_compress4(&rf, dst);
   }
   else
   {
      clusterfit_init(&cf, &colors, flags);
      if(flags & SQUISH_DXT1)
      {
         clusterfit_compress3(&cf, dst);
         if(!colors.transparent)
            clusterfit_compress4(&cf, dst);
      }
      else
         clusterfit_compress4(&cf, dst);
   }
}
