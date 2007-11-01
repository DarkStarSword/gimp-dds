/*
	DDS GIMP plugin

	Copyright (C) 2004 Shawn Kirst <skirst@fuse.net>,
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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#include "dds.h"
#include "endian.h"
#include "mipmap.h"

#define get_min_max_colors get_min_max_colors_distance
//#define get_min_max_colors get_min_max_colors_luminance
//#define get_min_max_colors get_min_max_colors_inset

/* convert BGR8 to RGB565 */
static inline unsigned short rgb565(const unsigned char *c)
{
   return(((c[2] >> 3) << 11) | ((c[1] >> 2) << 5) | (c[0] >> 3));
}

/* extract 4x4 BGRA block */
static void extract_block(const unsigned char *src, int w,
                          unsigned char *block)
{
   int i;
   for(i = 0; i < 4; ++i)
   {
      memcpy(&block[i * 4 * 4], src, 4 * 4);
      src += w * 4;
   }
}

static void swap_colors(unsigned char *a, unsigned char *b)
{
   unsigned char t[3];
   memcpy(t, a, 3);
   memcpy(a, b, 3);
   memcpy(b, t, 3);
}

static inline int distance(unsigned char a, unsigned char b)
{
   return((a - b) * (a - b));
}

static inline int color_distance(const unsigned char *c1,
                                 const unsigned char *c2)
{
   return(distance(c1[0], c2[0]) +
          distance(c1[1], c2[1]) +
          distance(c1[2], c2[2]));
}

static inline int get_color_luminance(const unsigned char *c)
{
   return((c[2] * 54 + c[1] * 182 + c[0] * 19) >> 8);
}

static void get_min_max_colors_distance(const unsigned char *block,
                                        unsigned char *min_color,
                                        unsigned char *max_color)
{
   int i, j, dist, maxcdist = -1, maxadist = -1;
   
   max_color[3] = min_color[3] = 255;
   
   for(i = 0; i < 64 - 4; i += 4)
   {
      for(j = i + 4; j < 64; j += 4)
      {
         dist = color_distance(&block[i], &block[j]);
         if(dist > maxcdist)
         {
            maxcdist = dist;
            memcpy(min_color, block + i, 3);
            memcpy(max_color, block + j, 3);
         }
         dist = distance(block[i + 3], block[j + 3]);
         if(dist > maxadist)
         {
            maxadist = dist;
            min_color[3] = block[i + 3];
            max_color[3] = block[j + 3];
         }
      }
   }
   
   if(rgb565(max_color) < rgb565(min_color))
      swap_colors(min_color, max_color);
   if(max_color[3] < min_color[3])
      max_color[3] ^= min_color[3] ^= max_color[3] ^= min_color[3];
}

static void get_min_max_colors_luminance(const unsigned char *block,
                                         unsigned char *min_color,
                                         unsigned char *max_color)
{
   int i, lum, maxlum = -1, minlum = 1 << 31;
   
   max_color[3] = 0;
   min_color[3] = 255;
   
   for(i = 0; i < 16; ++i)
   {
      lum = get_color_luminance(block + i * 4);
      if(lum > maxlum)
      {
         maxlum = lum;
         memcpy(max_color, block + i * 4, 3);
      }
      if(lum < minlum)
      {
         minlum = lum;
         memcpy(min_color, block + i * 4, 3);
      }
      
      lum = block[i * 4 + 3];
      if(lum > max_color[3]) max_color[3] = lum;
      if(lum < min_color[3]) min_color[3] = lum;
   }
   
   if(rgb565(max_color) < rgb565(min_color))
      swap_colors(min_color, max_color);
}

#define INSET_SHIFT 4

static void get_min_max_colors_inset(const unsigned char *block,
                                     unsigned char *min_color,
                                     unsigned char *max_color)
{
   int i;
   unsigned char inset[3];
   
   min_color[0] = min_color[1] = min_color[2] = max_color[3] = 255;
   max_color[0] = max_color[1] = max_color[2] = min_color[3] = 255;
   
   for(i = 0; i < 16; ++i)
   {
      if(block[i * 4 + 0] < min_color[0]) min_color[0] = block[i * 4 + 0];
      if(block[i * 4 + 1] < min_color[1]) min_color[1] = block[i * 4 + 1];
      if(block[i * 4 + 2] < min_color[2]) min_color[2] = block[i * 4 + 2];
      if(block[i * 4 + 3] < min_color[3]) min_color[3] = block[i * 4 + 3];
      if(block[i * 4 + 0] > max_color[0]) max_color[0] = block[i * 4 + 0];
      if(block[i * 4 + 1] > max_color[1]) max_color[1] = block[i * 4 + 1];
      if(block[i * 4 + 2] > max_color[2]) max_color[2] = block[i * 4 + 2];
      if(block[i * 4 + 3] > max_color[3]) max_color[3] = block[i * 4 + 3];
   }
   
   inset[0] = (max_color[0] - min_color[0]) >> INSET_SHIFT;
   inset[1] = (max_color[1] - min_color[1]) >> INSET_SHIFT;
   inset[2] = (max_color[2] - min_color[2]) >> INSET_SHIFT;
   inset[3] = (max_color[3] - min_color[3]) >> INSET_SHIFT;
   
   min_color[0] = (min_color[0] + inset[0] <= 255) ? min_color[0] + inset[0] : 255;
   min_color[1] = (min_color[1] + inset[1] <= 255) ? min_color[1] + inset[1] : 255;
   min_color[2] = (min_color[2] + inset[2] <= 255) ? min_color[2] + inset[2] : 255;
   min_color[3] = (min_color[3] + inset[3] <= 255) ? min_color[3] + inset[3] : 255;
   
   max_color[0] = (max_color[0] >= inset[0]) ? max_color[0] - inset[0] : 0;
   max_color[1] = (max_color[1] >= inset[1]) ? max_color[1] - inset[1] : 0;
   max_color[2] = (max_color[2] >= inset[2]) ? max_color[2] - inset[2] : 0;
   max_color[3] = (max_color[3] >= inset[3]) ? max_color[3] - inset[3] : 0;
}

static void get_min_max_channel(const unsigned char *block, int chan,
                                unsigned char *cmin, unsigned char *cmax)
{
   int i, j, dist, maxdist = -1;
   int mn = 1 << 31, mx = -1;
   
   for(i = 0; i < 64 - 4; i += 4)
   {
      for(j = i + 4; j < 64; j += 4)
      {
         dist = distance(block[i + chan], block[j + chan]);
         if(dist > maxdist)
         {
            maxdist = dist;
            mn = block[i + chan];
            mx = block[j + chan];
         }
      }
   }
   
   if(mx < mn)
      mx ^= mn ^= mx ^= mn;

   *cmin = mn;
   *cmax = mx;
}

static void emit_color_indices(unsigned char *dst, const unsigned char *block,
                               const unsigned char *min_color,
                               const unsigned char *max_color)
{
   unsigned short c[4][3];
   unsigned int result = 0;
   int i, c0, c1, c2, d0, d1, d2, d3, b0, b1, b2, b3, b4, x0, x1, x2;
   
   c[0][0] = (max_color[2] & 0xf8) | (max_color[2] >> 5);
   c[0][1] = (max_color[1] & 0xfc) | (max_color[1] >> 6);
   c[0][2] = (max_color[0] & 0xf8) | (max_color[0] >> 5);
   c[1][0] = (min_color[2] & 0xf8) | (min_color[2] >> 5);
   c[1][1] = (min_color[1] & 0xfc) | (min_color[1] >> 6);
   c[1][2] = (min_color[0] & 0xf8) | (min_color[0] >> 5);
   c[2][0] = (2 * c[0][0] + 1 * c[1][0]) / 3;
   c[2][1] = (2 * c[0][1] + 1 * c[1][1]) / 3;
   c[2][2] = (2 * c[0][2] + 1 * c[1][2]) / 3;
   c[3][0] = (1 * c[0][0] + 2 * c[1][0]) / 3;
   c[3][1] = (1 * c[0][1] + 2 * c[1][1]) / 3;
   c[3][2] = (1 * c[0][2] + 2 * c[1][2]) / 3;
   
   for(i = 15; i >= 0; --i)
   {
      c0 = block[i * 4 + 2];
      c1 = block[i * 4 + 1];
      c2 = block[i * 4 + 0];
      
      d0 = abs(c[0][0] - c0) + abs(c[0][1] - c1) + abs(c[0][2] - c2);
      d1 = abs(c[1][0] - c0) + abs(c[1][1] - c1) + abs(c[1][2] - c2);
      d2 = abs(c[2][0] - c0) + abs(c[2][1] - c1) + abs(c[2][2] - c2);
      d3 = abs(c[3][0] - c0) + abs(c[3][1] - c1) + abs(c[3][2] - c2);
      
      b0 = d0 > d3;
      b1 = d1 > d2;
      b2 = d0 > d2;
      b3 = d1 > d3;
      b4 = d2 > d3;
      
      x0 = b1 & b2;
      x1 = b0 & b3;
      x2 = b0 & b4;
      
      result |= (x2 | ((x0 | x1) << 1)) << (i << 1);
   }

   dst[0] = (result      ) & 0xff;
   dst[1] = (result >>  8) & 0xff;
   dst[2] = (result >> 16) & 0xff;
   dst[3] = (result >> 24) & 0xff;
}

static void emit_alpha_data_DXT3(unsigned char *dst, const unsigned char *block)
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

static void emit_alpha_indices_DXT5(unsigned char *dst, const unsigned char *block,
                                    unsigned char min_alpha, unsigned char max_alpha)
{
   unsigned char indices[16];
   unsigned char mid = (max_alpha - min_alpha) / (2 * 7);
   unsigned char a, ab1, ab2, ab3, ab4, ab5, ab6, ab7;
   int i, b1, b2, b3, b4, b5, b6, b7, index;
   
   ab1 = min_alpha + mid;
   ab2 = (6 * max_alpha + 1 * min_alpha) / 7 + mid;
   ab3 = (5 * max_alpha + 2 * min_alpha) / 7 + mid;
   ab4 = (4 * max_alpha + 3 * min_alpha) / 7 + mid;
   ab5 = (3 * max_alpha + 4 * min_alpha) / 7 + mid;
   ab6 = (2 * max_alpha + 5 * min_alpha) / 7 + mid;
   ab7 = (1 * max_alpha + 6 * min_alpha) / 7 + mid;
   
   block += 3;
   
   for(i = 0; i < 16; ++i)
   {
      a = block[i * 4];
      b1 = (a <= ab1);
      b2 = (a <= ab2);
      b3 = (a <= ab3);
      b4 = (a <= ab4);
      b5 = (a <= ab5);
      b6 = (a <= ab6);
      b7 = (a <= ab7);
      index = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + 1) & 7;
      indices[i] = index ^ (2 > index);
   }
   
   dst[0] = (indices[ 0] >> 0) | (indices[ 1] << 3) | (indices[ 2] << 6);
   dst[1] = (indices[ 2] >> 2) | (indices[ 3] << 1) | (indices[ 4] << 4) | (indices[ 5] << 7);
   dst[2] = (indices[ 5] >> 1) | (indices[ 6] << 2) | (indices[ 7] << 5);
   dst[3] = (indices[ 8] >> 0) | (indices[ 9] << 3) | (indices[10] << 6);
   dst[4] = (indices[10] >> 2) | (indices[11] << 1) | (indices[12] << 4) | (indices[13] << 7);
   dst[5] = (indices[13] >> 1) | (indices[14] << 2) | (indices[15] << 5);
}

static void compress_DXT1(unsigned char *dst, const unsigned char *src,
                          int w, int h)
{
   unsigned char block[64];
   unsigned char min_color[4], max_color[4];
   int x, y, c0, c1;
   
   for(y = 0; y < h; y += 4, src += w * 4 * 4)
   {
      for(x = 0; x < w; x += 4)
      {
         extract_block(src + x * 4, w, block);
         get_min_max_colors(block, min_color, max_color);
         
         c0 = rgb565(max_color);
         c1 = rgb565(min_color);
         *dst++ = (c0     ) & 0xff;
         *dst++ = (c0 >> 8) & 0xff;
         *dst++ = (c1     ) & 0xff;
         *dst++ = (c1 >> 8) & 0xff;
         emit_color_indices(dst, block, min_color, max_color);
         dst += 4;
      }
   }
}

static void compress_DXT3(unsigned char *dst, const unsigned char *src,
                          int w, int h)
{
   unsigned char block[64];
   unsigned char min_color[4], max_color[4];
   int x, y, c0, c1;
   
   for(y = 0; y < h; y += 4, src += w * 4 * 4)
   {
      for(x = 0; x < w; x += 4)
      {
         extract_block(src + x * 4, w, block);
         get_min_max_colors(block, min_color, max_color);

         emit_alpha_data_DXT3(dst, block);
         dst += 8;
         
         c0 = rgb565(max_color);
         c1 = rgb565(min_color);
         *dst++ = (c0     ) & 0xff;
         *dst++ = (c0 >> 8) & 0xff;
         *dst++ = (c1     ) & 0xff;
         *dst++ = (c1 >> 8) & 0xff;
         emit_color_indices(dst, block, min_color, max_color);
         dst += 4;
      }
   }
}

static void compress_DXT5(unsigned char *dst, const unsigned char *src,
                          int w, int h)
{
   unsigned char block[64];
   unsigned char min_color[4], max_color[4];
   int x, y, c0, c1;
   
   for(y = 0; y < h; y += 4, src += w * 4 * 4)
   {
      for(x = 0; x < w; x += 4)
      {
         extract_block(src + x * 4, w, block);
         get_min_max_colors(block, min_color, max_color);
         
         *dst++ = max_color[3];
         *dst++ = min_color[3];
         emit_alpha_indices_DXT5(dst, block, min_color[3], max_color[3]);
         dst += 6;
         
         c0 = rgb565(max_color);
         c1 = rgb565(min_color);
         *dst++ = (c0     ) & 0xff;
         *dst++ = (c0 >> 8) & 0xff;
         *dst++ = (c1     ) & 0xff;
         *dst++ = (c1 >> 8) & 0xff;
         emit_color_indices(dst, block, min_color, max_color);
         dst += 4;
      }
   }
}

static void compress_BC4(unsigned char *dst, const unsigned char *src,
                         int w, int h)
{
   unsigned char block[64];
   int x, y;
   
   for(y = 0; y < h; y += 4, src += w * 4 * 4)
   {
      for(x = 0; x < w; x += 4)
      {
         extract_block(src + x * 4, w, block);
         
         get_min_max_channel(block, 0, &dst[1], &dst[0]);
         emit_alpha_indices_DXT5(&dst[2], block - 3, dst[1], dst[0]);
         dst += 8;
      }
   }
}

static void compress_BC5(unsigned char *dst, const unsigned char *src,
                         int w, int h)
{
   unsigned char block[64];
   int x, y;
   
   for(y = 0; y < h; y += 4, src += w * 4 * 4)
   {
      for(x = 0; x < w; x += 4)
      {
         extract_block(src + x * 4, w, block);

         get_min_max_channel(block, 1, &dst[1], &dst[0]);
         emit_alpha_indices_DXT5(&dst[2], block - 2, dst[1], dst[0]);
         dst += 8;
         
         get_min_max_channel(block, 0, &dst[1], &dst[0]);
         emit_alpha_indices_DXT5(&dst[2], block - 3, dst[1], dst[0]);
         dst += 8;
      }
   }
}

int dxt_compress(unsigned char *dst, unsigned char *src, int format,
                 unsigned int width, unsigned int height, int bpp,
                 int mipmaps)
{
   int i, size, w, h;
   unsigned int offset;
   unsigned char *tmp;
   int j;
   unsigned char *tmp2, *s, c;
   
   if(!(IS_POT(width) && IS_POT(height)))
      return(0);
   
   size = get_mipmapped_size(width, height, bpp, 0, mipmaps,
                             DDS_COMPRESS_NONE);
   tmp = g_malloc(size);
   generate_mipmaps_software(tmp, src, width, height, bpp, 0, mipmaps);

   if(bpp == 1)
   {
      /* grayscale promoted to BGRA */
      
      size = get_mipmapped_size(width, height, 4, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp2 = g_malloc(size);
      
      for(i = j = 0; j < size; ++i, j += 4)
      {
         tmp2[j + 0] = tmp[i];
         tmp2[j + 1] = tmp[i];
         tmp2[j + 2] = tmp[i];
         tmp2[j + 3] = 255;
      }
      
      g_free(tmp);
      tmp = tmp2;
      bpp = 4;
   }
   else if(bpp == 2)
   {
      /* gray-alpha promoted to BGRA */
      
      size = get_mipmapped_size(width, height, 4, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp2 = g_malloc(size);
      
      for(i = j = 0; j < size; i += 2, j += 4)
      {
         tmp2[j + 0] = tmp[i];
         tmp2[j + 1] = tmp[i];
         tmp2[j + 2] = tmp[i];
         tmp2[j + 3] = tmp[i + 1];
      }
      
      g_free(tmp);
      tmp = tmp2;
      bpp = 4;
   }
   else if(bpp == 3)
   {
      size = get_mipmapped_size(width, height, 4, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp2 = g_malloc(size);
      
      for(i = j = 0; j < size; i += 3, j += 4)
      {
         tmp2[j + 0] = tmp[i + 2];
         tmp2[j + 1] = tmp[i + 1];
         tmp2[j + 2] = tmp[i + 0];
         tmp2[j + 3] = 255;
      }
      
      g_free(tmp);
      tmp = tmp2;
      bpp = 4;
   }
   else /* bpp == 4 */
   {
      /* we want BGRA pixels */
      for(i = 0; i < size; i += bpp)
      {
         c = tmp[i];
         tmp[i] = tmp[i + 2];
         tmp[i + 2] = c;
      }
   }
   
   offset = 0;
   w = width;
   h = height;
   s = tmp;

   for(i = 0; i < mipmaps; ++i)
   {
      switch(format)
      {
         case DDS_COMPRESS_BC1:
            compress_DXT1(dst + offset, s, w, h);
            break;
         case DDS_COMPRESS_BC2:
            compress_DXT3(dst + offset, s, w, h);
            break;
         case DDS_COMPRESS_BC3:
         case DDS_COMPRESS_BC3N:
         case DDS_COMPRESS_YCOCG:
            compress_DXT5(dst + offset, s, w, h);
            break;
         case DDS_COMPRESS_BC4:
            compress_BC4(dst + offset, s, w, h);
            break;
         case DDS_COMPRESS_BC5:
            compress_BC5(dst + offset, s, w, h);
            break;
      }
      s += (w * h * bpp);
      offset += get_mipmapped_size(w, h, 0, 0, 1, format);
      if(w > 1) w >>= 1;
      if(h > 1) h >>= 1;
   }

   g_free(tmp);
   
   return(1);
}

static void decode_color_block(unsigned char *dst, unsigned char *src,
                               int w, int h, int rowbytes, int format)
{
   int i, x, y;
   unsigned int indexes, idx;
   unsigned char *d;
   unsigned char colors[4][3];
   unsigned short c0, c1;
   
   c0 = GETL16(&src[0]);
   c1 = GETL16(&src[2]);

   colors[0][0] = ((c0 >> 11) & 0x1f) << 3;
   colors[0][1] = ((c0 >>  5) & 0x3f) << 2;
   colors[0][2] = ((c0      ) & 0x1f) << 3;

   colors[1][0] = ((c1 >> 11) & 0x1f) << 3;
   colors[1][1] = ((c1 >>  5) & 0x3f) << 2;
   colors[1][2] = ((c1      ) & 0x1f) << 3;
   
   if((c0 > c1) ||
      (format == DDS_COMPRESS_BC3 ||
       format == DDS_COMPRESS_BC3N))
   {
      for(i = 0; i < 3; ++i)
      {
         colors[2][i] = (2 * colors[0][i] + colors[1][i] + 1) / 3;
         colors[3][i] = (2 * colors[1][i] + colors[0][i] + 1) / 3;
      }
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
   for(y = 0; y < h; ++y)
   {
      d = dst + (y * rowbytes);
      indexes = src[y];
      for(x = 0; x < w; ++x)
      {
         idx = indexes & 0x03;
         d[0] = colors[idx][0];
         d[1] = colors[idx][1];
         d[2] = colors[idx][2];
         if(format == DDS_COMPRESS_BC1)
            d[3] = ((c0 <= c1) && idx == 3) ? 0 : 255;
         indexes >>= 2;
         d += 4;
      }
   }
}

static void decode_dxt3_alpha(unsigned char *dst, unsigned char *src,
                              int w, int h, int rowbytes)
{
   int x, y;
   unsigned char *d;
   unsigned int bits;
   
   for(y = 0; y < h; ++y)
   {
      d = dst + (y * rowbytes);
      bits = GETL16(&src[2 * y]);
      for(x = 0; x < w; ++x)
      {
         d[0] = (bits & 0x0f) * 17;
         bits >>= 4;
         d += 4;
      }
   }
}

static void decode_dxt5_alpha(unsigned char *dst, unsigned char *src,
                              int w, int h, int bpp, int rowbytes)
{
   int x, y, code;
   unsigned char *d;
   unsigned char a0 = src[0];
   unsigned char a1 = src[1];
   unsigned long long bits = GETL64(src) >> 16;
   
   for(y = 0; y < h; ++y)
   {
      d = dst + (y * rowbytes);
      for(x = 0; x < w; ++x)
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
         d += bpp;
      }
      if(w < 4) bits >>= (3 * (4 - w));
   }
}

int dxt_decompress(unsigned char *dst, unsigned char *src, int format,
                   unsigned int size, unsigned int width, unsigned int height,
                   int bpp)
{
   unsigned char *d, *s;
   unsigned int x, y, sx, sy;
   
   if(!(IS_POT(width) && IS_POT(height)))
      return(0);
   
   sx = (width  < 4) ? width  : 4;
   sy = (height < 4) ? height : 4;
   
   s = src;

   for(y = 0; y < height; y += 4)
   {
      for(x = 0; x < width; x += 4)
      {
         d = dst + (y * width + x) * bpp;
         if(format == DDS_COMPRESS_BC2)
         {
            decode_dxt3_alpha(d + 3, s, sx, sy, width * bpp);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC3 || format == DDS_COMPRESS_BC3N)
         {
            decode_dxt5_alpha(d + 3, s, sx, sy, bpp, width * bpp);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC4)
         {
            decode_dxt5_alpha(d, s, sx, sy, bpp, width * bpp);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC5)
         {
            decode_dxt5_alpha(d, s + 8, sx, sy, bpp, width * bpp);
            decode_dxt5_alpha(d + 1, s, sx, sy, bpp, width * bpp);
            s += 16;
         }
        
         if(format <= DDS_COMPRESS_BC3N)
         {
            decode_color_block(d, s, sx, sy, width * bpp, format);
            s += 8;
         }
      }
   }
   
   return(1);
}
