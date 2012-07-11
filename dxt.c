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
	the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
	Boston, MA 02111-1307, USA.
*/

/*
 * Parts of this code have been generously released in the public domain
 * by Fabian 'ryg' Giesen.  The original code can be found (at the time
 * of writing) here:  http://mollyrocket.com/forums/viewtopic.php?t=392
 *
 * For more information about this code, see the README.dxt file that
 * came with the source.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#include "dds.h"
#include "endian.h"
#include "mipmap.h"
#include "imath.h"
#include "squish.h"

/* extract 4x4 BGRA block */
static void extract_block(const unsigned char *src, int x, int y,
                          int w, int h, unsigned char *block)
{
   int i, j;
   int bw = MIN(w - x, 4);
   int bh = MIN(h - y, 4);
   int bx, by;
   const int rem[] =
   {
      0, 0, 0, 0,
      0, 1, 0, 1,
      0, 1, 2, 0,
      0, 1, 2, 3
   };

   for(i = 0; i < 4; ++i)
   {
      by = rem[(bh - 1) * 4 + i] + y;
      for(j = 0; j < 4; ++j)
      {
         bx = rem[(bw - 1) * 4 + j] + x;
         block[(i * 4 * 4) + (j * 4) + 0] =
            src[(by * (w * 4)) + (bx * 4) + 0];
         block[(i * 4 * 4) + (j * 4) + 1] =
            src[(by * (w * 4)) + (bx * 4) + 1];
         block[(i * 4 * 4) + (j * 4) + 2] =
            src[(by * (w * 4)) + (bx * 4) + 2];
         block[(i * 4 * 4) + (j * 4) + 3] =
            src[(by * (w * 4)) + (bx * 4) + 3];
      }
   }
}

/* pack BGR8 to RGB565 */
static inline unsigned short pack_rgb565(const unsigned char *c)
{
   //return(((c[2] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[0] >> 3));
   return((mul8bit(c[2], 31) << 11) |
          (mul8bit(c[1], 63) <<  5) |
          (mul8bit(c[0], 31)      ));
}

/* unpack RGB565 to BGR */
static void unpack_rgb565(unsigned char *dst, unsigned short v)
{
   int r = (v >> 11) & 0x1f;
   int g = (v >>  5) & 0x3f;
   int b = (v      ) & 0x1f;

   dst[0] = (b << 3) | (b >> 2);
   dst[1] = (g << 2) | (g >> 4);
   dst[2] = (r << 3) | (r >> 2);
}

/* linear interpolation at 1/3 point between a and b */
static void lerp_rgb13(unsigned char *dst, unsigned char *a, unsigned char *b)
{
   dst[0] = blerp(a[0], b[0], 0x55);
   dst[1] = blerp(a[1], b[1], 0x55);
   dst[2] = blerp(a[2], b[2], 0x55);

   /*
    * according to the S3TC/DX10 specs, this is the correct way to do the
    * interpolation (with no rounding bias)
    *
    * dst = (2 * a + b) / 3;
    *
    */
}

static void get_min_max_YCoCg(const unsigned char *block,
                              unsigned char *mincolor, unsigned char *maxcolor)
{
   int i;

   mincolor[2] = mincolor[1] = 255;
   maxcolor[2] = maxcolor[1] = 0;

   for(i = 0; i < 16; ++i)
   {
      if(block[4 * i + 2] < mincolor[2]) mincolor[2] = block[4 * i + 2];
      if(block[4 * i + 1] < mincolor[1]) mincolor[1] = block[4 * i + 1];
      if(block[4 * i + 2] > maxcolor[2]) maxcolor[2] = block[4 * i + 2];
      if(block[4 * i + 1] > maxcolor[1]) maxcolor[1] = block[4 * i + 1];
   }
}

static void scale_YCoCg(unsigned char *block,
                        unsigned char *mincolor, unsigned char *maxcolor)
{
   const int s0 = 128 / 2 - 1;
   const int s1 = 128 / 4 - 1;
   int m0, m1, m2, m3;
   int mask0, mask1, scale;
   int i;

   m0 = abs(mincolor[2] - 128);
   m1 = abs(mincolor[1] - 128);
   m2 = abs(maxcolor[2] - 128);
   m3 = abs(maxcolor[1] - 128);

   if(m1 > m0) m0 = m1;
   if(m3 > m2) m2 = m3;
   if(m2 > m0) m0 = m2;

   mask0 = -(m0 <= s0);
   mask1 = -(m0 <= s1);
   scale = 1 + (1 & mask0) + (2 & mask1);

   mincolor[2] = (mincolor[2] - 128) * scale + 128;
   mincolor[1] = (mincolor[1] - 128) * scale + 128;
   mincolor[0] = (scale - 1) << 3;

   maxcolor[2] = (maxcolor[2] - 128) * scale + 128;
   maxcolor[1] = (maxcolor[1] - 128) * scale + 128;
   maxcolor[0] = (scale - 1) << 3;

   for(i = 0; i < 16; ++i)
   {
      block[i * 4 + 2] = (block[i * 4 + 2] - 128) * scale + 128;
      block[i * 4 + 1] = (block[i * 4 + 1] - 128) * scale + 128;
   }
}

#define INSET_SHIFT  4

static void inset_bbox_YCoCg(unsigned char *mincolor, unsigned char *maxcolor)
{
   int inset[4], mini[4], maxi[4];

   inset[2] = (maxcolor[2] - mincolor[2]) - ((1 << (INSET_SHIFT - 1)) - 1);
   inset[1] = (maxcolor[1] - mincolor[1]) - ((1 << (INSET_SHIFT - 1)) - 1);

   mini[2] = ((mincolor[2] << INSET_SHIFT) + inset[2]) >> INSET_SHIFT;
   mini[1] = ((mincolor[1] << INSET_SHIFT) + inset[1]) >> INSET_SHIFT;

   maxi[2] = ((maxcolor[2] << INSET_SHIFT) - inset[2]) >> INSET_SHIFT;
   maxi[1] = ((maxcolor[1] << INSET_SHIFT) - inset[1]) >> INSET_SHIFT;

   mini[2] = (mini[2] >= 0) ? mini[2] : 0;
   mini[1] = (mini[1] >= 0) ? mini[1] : 0;

   maxi[2] = (maxi[2] <= 255) ? maxi[2] : 255;
   maxi[1] = (maxi[1] <= 255) ? maxi[1] : 255;

   mincolor[2] = (mini[2] & 0xf8) | (mini[2] >> 5);
   mincolor[1] = (mini[1] & 0xfc) | (mini[1] >> 6);

   maxcolor[2] = (maxi[2] & 0xf8) | (maxi[2] >> 5);
   maxcolor[1] = (maxi[1] & 0xfc) | (maxi[1] >> 6);
}

static void select_diagonal_YCoCg(const unsigned char *block,
                                  unsigned char *mincolor,
                                  unsigned char *maxcolor)
{
   unsigned char mid0, mid1, side, mask, b0, b1, c0, c1;
   int i;

   mid0 = ((int)mincolor[2] + maxcolor[2] + 1) >> 1;
   mid1 = ((int)mincolor[1] + maxcolor[1] + 1) >> 1;

   side = 0;
   for(i = 0; i < 16; ++i)
   {
      b0 = block[i * 4 + 2] >= mid0;
      b1 = block[i * 4 + 1] >= mid1;
      side += (b0 ^ b1);
   }

   mask = -(side > 8);
   mask &= -(mincolor[2] != maxcolor[2]);

   c0 = mincolor[1];
   c1 = maxcolor[1];

   c0 ^= c1;
   mask &= c0;
   c1 ^= mask;
   c0 ^= c1;

   mincolor[1] = c0;
   maxcolor[1] = c1;
}

/* write DXT3 alpha block */
static void encode_alpha_block_DXT3(unsigned char *dst,
                                    const unsigned char *block)
{
   int i, a1, a2;

   block += 3;

   for(i = 0; i < 8; ++i)
   {
      a1 = block[8 * i + 0];
      a2 = block[8 * i + 4];
      *dst++ = ((a2 >> 4) << 4) | (a1 >> 4);
   }
}

/* Write DXT5 alpha block */
static void encode_alpha_block_DXT5(unsigned char *dst,
                                    const unsigned char *block,
                                    const int offset)
{
   int i, v, mn, mx;
   int dist, bias, dist2, dist4, bits, mask;
   int a, idx, t;

   block += offset;
   block += 3;

   /* find min/max alpha pair */
   mn = mx = block[0];
   for(i = 0; i < 16; ++i)
   {
      v = block[4 * i];
      if(v > mx) mx = v;
      if(v < mn) mn = v;
   }

   /* encode them */
   *dst++ = mx;
   *dst++ = mn;

   /*
    * determine bias and emit indices
    * given the choice of mx/mn, these indices are optimal:
    * http://fgiesen.wordpress.com/2009/12/15/dxt5-alpha-block-index-determination/
    */
   dist = mx - mn;
   dist4 = dist * 4;
   dist2 = dist * 2;
   bias = (dist < 8) ? (dist - 1) : (dist / 2 + 2);
   bias -= mn * 7;
   bits = 0;
   mask = 0;

   for(i = 0; i < 16; ++i)
   {
      a = block[4 * i] * 7 + bias;

      /* select index. this is a "linear scale" lerp factor between 0 (val=min) and 7 (val=max). */
      t = (a >= dist4) ? -1 : 0; idx =  t & 4; a -= dist4 & t;
      t = (a >= dist2) ? -1 : 0; idx += t & 2; a -= dist2 & t;
      idx += (a >= dist);

      /* turn linear scale into DXT index (0/1 are extremal pts) */
      idx = -idx & 7;
      idx ^= (2 > idx);

      /* write index */
      mask |= idx << bits;
      if((bits += 3) >= 8)
      {
         *dst++ = mask;
         mask >>= 8;
         bits -= 8;
      }
   }
}

static void compress_DXT1(unsigned char *dst, const unsigned char *src,
                          int w, int h, int flags)
{
   unsigned char block[64], *p;
   int x, y;

#pragma omp parallel for schedule(dynamic) private(y, x, block, p)
   for(y = 0; y < h; y += 4)
   {
      for(x = 0; x < w; x += 4)
      {
         p = dst + ((y >> 2) * (8 * (w >> 2)) + (8 * (x >> 2)));
         extract_block(src, x, y, w, h, block);
         squish_compress(p, block, SQUISH_DXT1 | flags);
      }
   }
}

static void compress_DXT3(unsigned char *dst, const unsigned char *src,
                          int w, int h, int flags)
{
   unsigned char block[64], *p;
   int x, y;

#pragma omp parallel for schedule(dynamic) private(y, x, block, p)
   for(y = 0; y < h; y += 4)
   {
      for(x = 0; x < w; x += 4)
      {
         p = dst + ((y >> 2) * (16 * (w >> 2)) + (16 * (x >> 2)));
         extract_block(src, x, y, w, h, block);
         encode_alpha_block_DXT3(p, block);
         squish_compress(p + 8, block, SQUISH_DXT3 | flags);
      }
   }
}

static void compress_DXT5(unsigned char *dst, const unsigned char *src,
                          int w, int h, int flags)
{
   unsigned char block[64], *p;
   int x, y;

#pragma omp parallel for schedule(dynamic) private(y, x, block, p)
   for(y = 0; y < h; y += 4)
   {
      for(x = 0; x < w; x += 4)
      {
         p = dst + ((y >> 2) * (16 * (w >> 2)) + (16 * (x >> 2)));
         extract_block(src, x, y, w, h, block);
         encode_alpha_block_DXT5(p, block, 0);
         squish_compress(p + 8, block, SQUISH_DXT5 | flags);
      }
   }
}

static void compress_BC4(unsigned char *dst, const unsigned char *src,
                         int w, int h)
{
   unsigned char block[64], *p;
   int x, y;

#pragma omp parallel for schedule(dynamic) private(y, x, block, p)
   for(y = 0; y < h; y += 4)
   {
      for(x = 0; x < w; x += 4)
      {
         p = dst + ((y >> 2) * (8 * (w >> 2)) + (8 * (x >> 2)));
         extract_block(src, x, y, w, h, block);
         encode_alpha_block_DXT5(p, block, -1);
      }
   }
}

static void compress_BC5(unsigned char *dst, const unsigned char *src,
                         int w, int h)
{
   unsigned char block[64], *p;
   int x, y;

#pragma omp parallel for schedule(dynamic) private(y, x, block, p)
   for(y = 0; y < h; y += 4)
   {
      for(x = 0; x < w; x += 4)
      {
         p = dst + ((y >> 2) * (16 * (w >> 2)) + (16 * (x >> 2)));
         extract_block(src, x, y, w, h, block);
         encode_alpha_block_DXT5(p, block, -2);
         encode_alpha_block_DXT5(p + 8, block, -1);
      }
   }
}

static void compress_YCoCg(unsigned char *dst, const unsigned char *src,
                           int w, int h)
{
   unsigned char block[64], colors[4][3];
   unsigned char *p, *maxcolor, *mincolor;
   unsigned int mask;
   int c0, c1, d0, d1, d2, d3;
   int b0, b1, b2, b3, b4;
   int x0, x1, x2;
   int x, y, i;

#pragma omp parallel for schedule(dynamic) \
   private(block, colors, p, maxcolor, mincolor, mask, c0, c1, d0, d1, d2, d3, \
           b0, b1, b2, b3, b4, x0, x1, x2, x, y, i)
   for(y = 0; y < h; y += 4)
   {
      for(x = 0; x < w; x += 4)
      {
         p = dst + ((y >> 2) * (16 * (w >> 2)) + (16 * (x >> 2)));

         extract_block(src, x, y, w, h, block);

         encode_alpha_block_DXT5(p, block, 0);

         maxcolor = &colors[0][0];
         mincolor = &colors[1][0];

         get_min_max_YCoCg(block, mincolor, maxcolor);
         scale_YCoCg(block, mincolor, maxcolor);
         inset_bbox_YCoCg(mincolor, maxcolor);
         select_diagonal_YCoCg(block, mincolor, maxcolor);

         lerp_rgb13(&colors[2][0], maxcolor, mincolor);
         lerp_rgb13(&colors[3][0], mincolor, maxcolor);

         mask = 0;

         for(i = 15; i >= 0; --i)
         {
            c0 = block[4 * i + 2];
            c1 = block[4 * i + 1];

            d0 = abs(colors[0][2] - c0) + abs(colors[0][1] - c1);
            d1 = abs(colors[1][2] - c0) + abs(colors[1][1] - c1);
            d2 = abs(colors[2][2] - c0) + abs(colors[2][1] - c1);
            d3 = abs(colors[3][2] - c0) + abs(colors[3][1] - c1);

            b0 = d0 > d3;
            b1 = d1 > d2;
            b2 = d0 > d2;
            b3 = d1 > d3;
            b4 = d2 > d3;

            x0 = b1 & b2;
            x1 = b0 & b3;
            x2 = b0 & b4;

            mask <<= 2;
            mask |= (x2 | ((x0 | x1) << 1));
         }

         PUTL16(p +  8, pack_rgb565(maxcolor));
         PUTL16(p + 10, pack_rgb565(mincolor));
         PUTL32(p + 12, mask);
      }
   }
}

int dxt_compress(unsigned char *dst, unsigned char *src, int format,
                 unsigned int width, unsigned int height, int bpp,
                 int mipmaps, int flags)
{
   int i, size, w, h;
   unsigned int offset;
   unsigned char *tmp = NULL;
   int j;
   unsigned char *s;

   if(bpp == 1)
   {
      /* grayscale promoted to BGRA */

      size = get_mipmapped_size(width, height, 4, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp = g_malloc(size);

      for(i = j = 0; j < size; ++i, j += 4)
      {
         tmp[j + 0] = src[i];
         tmp[j + 1] = src[i];
         tmp[j + 2] = src[i];
         tmp[j + 3] = 255;
      }

      bpp = 4;
   }
   else if(bpp == 2)
   {
      /* gray-alpha promoted to BGRA */

      size = get_mipmapped_size(width, height, 4, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp = g_malloc(size);

      for(i = j = 0; j < size; i += 2, j += 4)
      {
         tmp[j + 0] = src[i];
         tmp[j + 1] = src[i];
         tmp[j + 2] = src[i];
         tmp[j + 3] = src[i + 1];
      }

      bpp = 4;
   }
   else if(bpp == 3)
   {
      size = get_mipmapped_size(width, height, 4, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp = g_malloc(size);

      for(i = j = 0; j < size; i += 3, j += 4)
      {
         tmp[j + 0] = src[i + 0];
         tmp[j + 1] = src[i + 1];
         tmp[j + 2] = src[i + 2];
         tmp[j + 3] = 255;
      }

      bpp = 4;
   }

   offset = 0;
   w = width;
   h = height;
   s = tmp ? tmp : src;

   for(i = 0; i < mipmaps; ++i)
   {
      switch(format)
      {
         case DDS_COMPRESS_BC1:
            compress_DXT1(dst + offset, s, w, h, flags);
            break;
         case DDS_COMPRESS_BC2:
            compress_DXT3(dst + offset, s, w, h, flags);
            break;
         case DDS_COMPRESS_BC3:
         case DDS_COMPRESS_BC3N:
         case DDS_COMPRESS_RXGB:
         case DDS_COMPRESS_AEXP:
         case DDS_COMPRESS_YCOCG:
            compress_DXT5(dst + offset, s, w, h, flags);
            break;
         case DDS_COMPRESS_BC4:
            compress_BC4(dst + offset, s, w, h);
            break;
         case DDS_COMPRESS_BC5:
            compress_BC5(dst + offset, s, w, h);
            break;
         case DDS_COMPRESS_YCOCGS:
            compress_YCoCg(dst + offset, s, w, h);
            break;
         default:
            compress_DXT5(dst + offset, s, w, h, flags);
            break;
      }
      s += (w * h * bpp);
      offset += get_mipmapped_size(w, h, 0, 0, 1, format);
      w = MAX(1, w >> 1);
      h = MAX(1, h >> 1);
   }

   if(tmp) g_free(tmp);

   return(1);
}

static void decode_color_block(unsigned char *block, unsigned char *src,
                               int format)
{
   int i, x, y;
   unsigned char *d = block;
   unsigned int indexes, idx;
   unsigned char colors[4][3];
   unsigned short c0, c1;

   c0 = GETL16(&src[0]);
   c1 = GETL16(&src[2]);

   unpack_rgb565(colors[0], c0);
   unpack_rgb565(colors[1], c1);

   if((c0 > c1) || (format == DDS_COMPRESS_BC3))
   {
      lerp_rgb13(colors[2], colors[0], colors[1]);
      lerp_rgb13(colors[3], colors[1], colors[0]);
   }
   else
   {
      for(i = 0; i < 3; ++i)
      {
         colors[2][i] = (colors[0][i] + colors[1][i] + 1) >> 1;
         colors[3][i] = 255;
      }
   }

   src += 4;
   for(y = 0; y < 4; ++y)
   {
      indexes = src[y];
      for(x = 0; x < 4; ++x)
      {
         idx = indexes & 0x03;
         d[0] = colors[idx][2];
         d[1] = colors[idx][1];
         d[2] = colors[idx][0];
         if(format == DDS_COMPRESS_BC1)
            d[3] = ((c0 <= c1) && idx == 3) ? 0 : 255;
         indexes >>= 2;
         d += 4;
      }
   }
}

static void decode_alpha_block_DXT3(unsigned char *block, unsigned char *src)
{
   int x, y;
   unsigned char *d = block;
   unsigned int bits;

   for(y = 0; y < 4; ++y)
   {
      bits = GETL16(&src[2 * y]);
      for(x = 0; x < 4; ++x)
      {
         d[0] = (bits & 0x0f) * 17;
         bits >>= 4;
         d += 4;
      }
   }
}

static void decode_alpha_block_DXT5(unsigned char *block, unsigned char *src, int w)
{
   int x, y, code;
   unsigned char *d = block;
   unsigned char a0 = src[0];
   unsigned char a1 = src[1];
   unsigned long long bits = GETL64(src) >> 16;

   for(y = 0; y < 4; ++y)
   {
      for(x = 0; x < 4; ++x)
      {
         code = ((unsigned int)bits) & 0x07;
         if(code == 0)
            d[0] = a0;
         else if(code == 1)
            d[0] = a1;
         else if(a0 > a1)
            d[0] = ((8 - code) * a0 + (code - 1) * a1) / 7;
         else if(code >= 6)
            d[0] = (code == 6) ? 0 : 255;
         else
            d[0] = ((6 - code) * a0 + (code - 1) * a1) / 5;
         bits >>= 3;
         d += 4;
      }
      if(w < 4) bits >>= (3 * (4 - w));
   }
}

static void make_normal(unsigned char *dst, unsigned char x, unsigned char y)
{
   float nx = 2.0f * ((float)x / 255.0f) - 1.0f;
   float ny = 2.0f * ((float)y / 255.0f) - 1.0f;
   float nz = 0.0f;
   float d = 1.0f - nx * nx + ny * ny;
   int z;

   if(d > 0) nz = sqrtf(d);

   z = (int)(255.0f * (nz + 1) / 2.0f);
   z = MAX(0, MIN(255, z));

   dst[0] = x;
   dst[1] = y;
   dst[2] = z;
}

static void normalize_block(unsigned char *block, int format)
{
   int x, y, tmp;

   for(y = 0; y < 4; ++y)
   {
      for(x = 0; x < 4; ++x)
      {
         if(format == DDS_COMPRESS_BC3)
         {
            tmp = block[y * 16 + (x * 4)];
            make_normal(&block[y * 16 + (x * 4)],
                        block[y * 16 + (x * 4) + 3],
                        block[y * 16 + (x * 4) + 1]);
            block[y * 16 + (x * 4) + 3] = tmp;
         }
         else if(format == DDS_COMPRESS_BC5)
         {
            make_normal(&block[y * 16 + (x * 4)],
                        block[y * 16 + (x * 4)],
                        block[y * 16 + (x * 4) + 1]);
         }
      }
   }
}

static void put_block(unsigned char *dst, unsigned char *block,
                      unsigned int bx, unsigned int by,
                      unsigned int width, unsigned height,
                      int bpp)
{
   int x, y, i;
   unsigned char *d;

   for(y = 0; y < 4; ++y)
   {
      d = dst + ((y + by) * width + bx) * bpp;
      for(x = 0; x < 4; ++x)
      {
         for(i = 0; i < bpp; ++ i)
            *d++ = block[y * 16 + (x * 4) + i];
      }
   }
}

int dxt_decompress(unsigned char *dst, unsigned char *src, int format,
                   unsigned int size, unsigned int width, unsigned int height,
                   int bpp, int normals)
{
   unsigned char *s;
   unsigned int i, x, y;
   unsigned char block[16 * 4];

   if(!(IS_MUL4(width) && IS_MUL4(height)))
      return(0);

   s = src;

   for(y = 0; y < height; y += 4)
   {
      for(x = 0; x < width; x += 4)
      {
         memset(block, 0, 16 * 4);
         for(i = 0; i < 16 * 4; i += 4)
            block[i + 3] = 255;

         if(format == DDS_COMPRESS_BC1)
         {
            decode_color_block(block, s, format);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC2)
         {
            decode_alpha_block_DXT3(block + 3, s);
            s += 8;
            decode_color_block(block, s, format);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC3)
         {
            decode_alpha_block_DXT5(block + 3, s, width);
            s += 8;
            decode_color_block(block, s, format);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC4)
         {
            decode_alpha_block_DXT5(block, s, width);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC5)
         {
            decode_alpha_block_DXT5(block, s + 8, width);
            decode_alpha_block_DXT5(block + 1, s, width);
            s += 16;
         }

         if(normals)
            normalize_block(block, format);

         put_block(dst, block, x, y, width, height, bpp);
      }
   }

   return(1);
}
