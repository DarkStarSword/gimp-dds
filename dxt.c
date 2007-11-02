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

#include "dxt_tables.h"

#ifndef MAX
#define MAX(a, b)  ((a) > (b) ? (a) : (b))
#endif

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

static inline int mul8bit(int a, int b)
{
   int t = a * b + 128;
   return((t + (t >> 8)) >> 8);
}

static void from_16bit(unsigned char *dst, unsigned short v)
{
   int rv = (v & 0xf800) >> 11;
   int gv = (v & 0x07e0) >>  5;
   int bv = (v & 0x001f) >>  0;
   
   dst[0] = expand5[bv];
   dst[1] = expand6[gv];
   dst[2] = expand5[rv];
}

static void lerp_rgb(unsigned char *dst, unsigned char *a, unsigned char *b, int f)
{
   dst[0] = a[0] + mul8bit(b[0] - a[0], f);
   dst[1] = a[1] + mul8bit(b[1] - a[1], f);
   dst[2] = a[2] + mul8bit(b[2] - a[2], f);
}

static void dither_block(unsigned char *dst, const unsigned char *block)
{
   int err[8], *ep1 = err, *ep2 = err + 4, *tmp;
   int c, y;
   unsigned char *bp, *dp;
   const unsigned char *quant;
   
   for(c = 0; c < 3; ++c)
   {
      bp = (unsigned char *)block;
      dp = dst;
      quant = (c == 1) ? quantG + 8 : quantRB + 8;
      
      bp += c;
      dp += c;
      
      memset(err, 0, sizeof(err));
      
      for(y = 0; y < 4; ++y)
      {
         dp[ 0] = quant[bp[ 0] + ((3 * ep2[1] + 5 * ep2[0]) >> 4)];
         ep1[0] = bp[ 0] - dp[ 0];
         
         dp[ 4] = quant[bp[ 4] + ((7 * ep1[0] + 3 * ep2[2] + 5 * ep2[1] + ep2[0]) >> 4)];
         ep1[1] = bp[ 4] - dp[ 4];
         
         dp[ 8] = quant[bp[ 8] + ((7 * ep1[1] + 3 * ep2[3] + 5 * ep2[2] + ep2[1]) >> 4)];
         ep1[2] = bp[ 8] - dp[ 8];
         
         dp[12] = quant[bp[12] + ((7 * ep1[2] + 5 * ep2[3] + ep2[2]) >> 4)];
         ep1[3] = bp[12] - dp[12];
         
         tmp = ep1;
         ep1 = ep2;
         ep2 = tmp;
         
         bp += 16;
         dp += 16;
      }
   }
}

static unsigned int match_colors_block(const unsigned char *block,
                                       const unsigned char *color,
                                       int dither)
{
   unsigned int mask = 0;
   int dirb = color[0] - color[4];
   int dirg = color[1] - color[5];
   int dirr = color[2] - color[6];
   int dots[16], stops[4];
   int c0pt, halfpt, c3pt, dot;
   int i;
   
   for(i = 0; i < 16; ++i)
      dots[i] = block[4 * i] * dirb + block[4 * i + 1] * dirg + block[4 * i + 2] * dirr;
   
   for(i = 0; i < 4; ++i)
      stops[i] = color[4 * i] * dirb + color[4 * i + 1] * dirg + color[4 * i + 2] * dirr;
   
   c0pt = (stops[1] + stops[3]) >> 1;
   halfpt = (stops[3] + stops[2]) >> 1;
   c3pt = (stops[2] + stops[0]) >> 1;
   
   if(!dither)
   {
      for(i = 15; i >= 0; --i)
      {
         mask <<= 2;
         dot = dots[i];
         
         if(dot < halfpt)
            mask |= (dot < c0pt) ? 1 : 3;
         else
            mask |= (dot < c3pt) ? 2 : 0;
      }
   }
   else
   {
      int err[8], *ep1 = err, *ep2 = err + 4, *tmp;
      int *dp = dots, y, lmask, step;
      
      c0pt <<= 4;
      halfpt <<= 4;
      c3pt <<= 4;
      
      memset(err, 0, sizeof(err));
      
      for(y = 0; y < 4; ++y)
      {
         dot = (dp[0] << 4) + (3 * ep2[1] + 5 * ep2[0]);
         if(dot < halfpt)
            step = (dot < c0pt) ? 1 : 3;
         else
            step = (dot < c3pt) ? 2 : 0;

         ep1[0] = dp[0] - stops[step];
         lmask = step;
         
         dot = (dp[1] << 4) + (7 * ep1[0] + 3 * ep2[2] + 5 * ep2[1] + ep2[0]);
         if(dot < halfpt)
            step = (dot < c0pt) ? 1 : 3;
         else
            step = (dot < c3pt) ? 2 : 0;

         ep1[1] = dp[1] - stops[step];
         lmask |= step << 2;

         dot = (dp[2] << 4) + (7 * ep1[1] + 3 * ep2[3] + 5 * ep2[2] + ep2[1]);
         if(dot < halfpt)
            step = (dot < c0pt) ? 1 : 3;
         else
            step = (dot < c3pt) ? 2 : 0;

         ep1[2] = dp[2] - stops[step];
         lmask |= step << 4;
         
         dot = (dp[3] << 4) + (7 * ep1[2] + 5 * ep2[3] + ep2[2]);
         if(dot < halfpt)
            step = (dot < c0pt) ? 1 : 3;
         else
            step = (dot < c3pt) ? 2 : 0;

         ep1[3] = dp[3] - stops[step];
         lmask |= step << 6;

         tmp = ep1;
         ep1 = ep2;
         ep2 = tmp;
         
         dp += 4;
         mask |= lmask << (y * 8);
      }
   }
   
   return(mask);
}

static void optimize_colors_block(const unsigned char *block,
                                  unsigned short *max16, unsigned short *min16)
{
   static const int niterpow = 4;
   
   int mu[3], mn[3], mx[3];
   int i, c, r, g, b, dot, iter;
   int muv, mnv, mxv, mnd, mxd;
   int cov[6];
   unsigned char *bp, mnc[3], mxc[3];
   float covf[6], vfr, vfg, vfb, magn;
   float fr, fg, fb;
   
   for(c = 0; c < 3; ++c)
   {
      bp = (unsigned char *)block + c;
      
      muv = mnv = mxv = bp[0];
      for(i = 4; i < 64; i += 4)
      {
         muv += bp[i];
         if(mnv > bp[i]) mnv = bp[i];
         if(mxv < bp[i]) mxv = bp[i];
      }
      
      mu[c] = (muv + 8) >> 4;
      mn[c] = mnv;
      mx[c] = mxv;
   }
   
   memset(cov, 0, sizeof(cov));
   
   for(i = 0; i < 16; ++i)
   {
      b = block[4 * i + 0] - mu[0];
      g = block[4 * i + 1] - mu[1];
      r = block[4 * i + 2] - mu[3];
      
      cov[0] += r * r;
      cov[1] += r * g;
      cov[2] += r * b;
      cov[3] += g * g;
      cov[4] += g * b;
      cov[5] += b * b;
   }
   
   for(i = 0; i < 6; ++i)
      covf[i] = cov[i] / 255.0f;
   
   vfb = mx[0] - mn[0];
   vfg = mx[1] - mn[1];
   vfr = mx[2] - mn[2];
   
   for(iter = 0; iter < niterpow; ++iter)
   {
      fr = vfr * covf[0] + vfg * covf[1] + vfb * covf[2];
      fg = vfr * covf[1] + vfg * covf[3] + vfb * covf[4];
      fb = vfr * covf[2] + vfg * covf[4] + vfb * covf[5];
      
      vfr = fr;
      vfg = fg;
      vfb = fb;
   }
   
   vfr = fabsf(vfr);
   vfg = fabsf(vfg);
   vfb = fabsf(vfb);
   
   magn = MAX(MAX(vfr, vfg), vfb);
   
   if(magn < 4.0)
   {
      r = 148;
      g = 300;
      b = 58;
   }
   else
   {
      magn = 512.0f / magn;
      r = (int)(vfr * magn);
      g = (int)(vfg * magn);
      b = (int)(vfb * magn);
   }
   
   mnd =  0x7fffffff;
   mxd = -0x7fffffff;
   
   for(i = 0; i < 16; ++i)
   {
      dot = block[4 * i] * b + block[4 * i + 1] * g + block[4 * i + 2] * r;
      
      if(dot < mnd)
      {
         mnd = dot;
         memcpy(mnc, &block[4 * i], 3);
      }
      if(dot > mxd)
      {
         mxd = dot;
         memcpy(mxc, &block[4 * i], 3);
      }
   }

   *max16 = rgb565(mxc);
   *min16 = rgb565(mnc);
}

static int refine_block(const unsigned char *block,
                        unsigned short *max16, unsigned short *min16,
                        unsigned int mask)
{
   static const int w1tab[4] = {3, 0, 2, 1};
   static const int prods[4] = {0x090000, 0x000900, 0x040102, 0x010402};
   int akku = 0;
   int At1_r, At1_g, At1_b;
   int At2_r, At2_g, At2_b;
   unsigned int cm = mask;
   int i, step, w1, r, g, b;
   int xx, yy, xy;
   float frb, fg;
   unsigned short v, oldmin, oldmax;
   int s;
   
   At1_r = At1_g = At1_b = 0;
   At2_r = At2_g = At2_b = 0;
   
   for(i = 0; i < 16; ++i, cm >>= 2)
   {
      step = cm & 3;
      w1 = w1tab[step];
      r = block[4 * i + 2];
      g = block[4 * i + 1];
      b = block[4 * i + 0];
      
      akku  += prods[step];
      At1_r += w1 * r;
      At1_g += w1 * g;
      At1_b += w1 * b;
      At2_r += r;
      At2_g += g;
      At2_b += b;
   }

   At2_r = 3 * At2_r - At1_r;
   At2_g = 3 * At2_g - At1_g;
   At2_b = 3 * At2_b - At1_b;
   
   xx = akku >> 16;
   yy = (akku >> 8) & 0xff;
   xy = (akku >> 0) & 0xff;
   
   if(!yy || !xx || xx * yy == xy * xy)
      return(0);
   
   frb = 3.0f * 31.0f / 255.0f / (xx * yy - xy * xy);
   fg = frb * 63.0f / 31.0f;
   
   oldmin = *min16;
   oldmax = *max16;

   s = (int)((At1_r * yy - At2_r * xy) * frb + 0.5f);
   if(s < 0) s = 0;
   if(s > 31) s = 31;
   v = s << 11;
   s = (int)((At1_g * yy - At2_g * xy) * fg + 0.5f);
   if(s < 0) s = 0;
   if(s > 63) s = 63;
   v |= s << 5;
   s = (int)((At1_b * yy - At2_b * xy) * frb + 0.5f);
   if(s < 0) s = 0;
   if(s > 31) s = 31;
   v |= s;
   *max16 = v;
   
   s = (int)((At2_r * xx - At1_r * xy) * frb + 0.5f);
   if(s < 0) s = 0;
   if(s > 31) s = 31;
   v = s << 11;
   s = (int)((At2_g * xx - At1_g * xy) * fg + 0.5f);
   if(s < 0) s = 0;
   if(s > 63) s = 63;
   v |= s << 5;
   s = (int)((At2_b * xx - At1_b * xy) * frb + 0.5f);
   if(s < 0) s = 0;
   if(s > 31) s = 31;
   v |= s;
   *min16 = v;
   
   return(oldmin != *min16 || oldmax != *max16);
}

static void eval_colors(unsigned char *color,
                        unsigned short c0, unsigned short c1)
{
   from_16bit(&color[0], c0);
   from_16bit(&color[4], c1);
   lerp_rgb(&color[ 8], &color[0], &color[4], 0x55);
   lerp_rgb(&color[12], &color[0], &color[4], 0xaa);
}

static void encode_color_block(unsigned char *dst,
                               const unsigned char *block,
                               int quality)
{
   unsigned char dblock[64], color[16];
   unsigned short min16, max16;
   unsigned int v, mn, mx, mask;
   int i;

   mn = mx = GETL32(block);
   for(i = 0; i < 16; ++i)
   {
      v = GETL32(&block[4 * i]);
      if(v > mx) mx = v;
      if(v < mn) mn = v;
   }
   
   if(mn != mx)
   {
      if(quality)
         dither_block(dblock, block);

      optimize_colors_block(quality ? dblock : block, &max16, &min16);
      if(max16 != min16)
      {
         eval_colors(color, max16, min16);
         mask = match_colors_block(block, color, quality != 0);
      }
      else
         mask = 0;
      
      if(refine_block(quality ? dblock : block, &max16, &min16, mask))
      {
         if(max16 != min16)
         {
            eval_colors(color, max16, min16);
            mask = match_colors_block(block, color, quality != 0);
         }
         else
            mask = 0;
      }
   }
   else
   {
      mask = 0xaaaaaaaa;
      max16 = (omatch5[block[2]][0] << 11) |
              (omatch6[block[1]][0] <<  5) |
              (omatch5[block[0]][0]      );
      min16 = (omatch5[block[2]][1] << 11) |
              (omatch6[block[1]][1] <<  5) |
              (omatch5[block[0]][1]      );
   }
      
   if(max16 < min16)
   {
      max16 ^= min16 ^= max16 ^= min16;
      mask ^= 0x55555555;
   }
   
   PUTL16(&dst[0], max16);
   PUTL16(&dst[2], min16);
   PUTL32(&dst[4], mask);
}

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

static void encode_alpha_block_DXT5(unsigned char *dst,
                                    const unsigned char *block)
{
   int i, v, mn, mx;
   int dist, bias, dist2, dist4, bits, mask;
   int a, idx, t;
   
   block += 3;
   
   mn = mx = block[0];
   for(i = 0; i < 16; ++i)
   {
      v = block[4 * i];
      if(v > mx) mx = v;
      if(v < mn) mn = v;
   }
   
   *dst++ = mx;
   *dst++ = mn;
   
   dist = mx - mn;
   bias = mn * 7 - (dist >> 1);
   dist4 = dist * 4;
   dist2 = dist * 2;
   bits = 0;
   mask = 0;
   
   for(i = 0; i < 16; ++i)
   {
      a = block[4 * i] * 7 - bias;
      
      t = (dist4 - a) >> 31; idx =  t & 4; a -= dist4 & t;
      t = (dist2 - a) >> 31; idx += t & 2; a -= dist2 & t;
      t = (dist  - a) >> 31; idx += t & 1;
      
      idx = -idx & 7;
      idx ^= (2 > idx);
      
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
                          int w, int h)
{
   unsigned char block[64];
   int x, y;
   
   for(y = 0; y < h; y += 4, src += w * 4 * 4)
   {
      for(x = 0; x < w; x += 4)
      {
         extract_block(src + x * 4, w, block);
         encode_color_block(dst, block, 1);
         dst += 8;
      }
   }
}

static void compress_DXT3(unsigned char *dst, const unsigned char *src,
                          int w, int h)
{
   unsigned char block[64];
   int x, y;
   
   for(y = 0; y < h; y += 4, src += w * 4 * 4)
   {
      for(x = 0; x < w; x += 4)
      {
         extract_block(src + x * 4, w, block);
         encode_alpha_block_DXT3(dst, block);
         encode_color_block(dst + 8, block, 1);
         dst += 16;
      }
   }
}

static void compress_DXT5(unsigned char *dst, const unsigned char *src,
                          int w, int h)
{
   unsigned char block[64];
   int x, y;
   
   for(y = 0; y < h; y += 4, src += w * 4 * 4)
   {
      for(x = 0; x < w; x += 4)
      {
         extract_block(src + x * 4, w, block);
         encode_alpha_block_DXT5(dst, block);
         encode_color_block(dst + 8, block, 1);
         dst += 16;
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
         encode_alpha_block_DXT5(dst, block - 1);
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
         encode_alpha_block_DXT5(dst, block - 2);
         encode_alpha_block_DXT5(dst + 8, block - 1);
         dst += 16;
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
   unsigned char *tmp2, *s;
   
   if(!(IS_POT(width) && IS_POT(height)))
      return(0);
   
   size = get_mipmapped_size(width, height, bpp, 0, mipmaps,
                             DDS_COMPRESS_NONE);
   tmp = g_malloc(size);
   generate_mipmaps(tmp, src, width, height, bpp, 0, mipmaps);

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
         tmp2[j + 0] = tmp[i + 0];
         tmp2[j + 1] = tmp[i + 1];
         tmp2[j + 2] = tmp[i + 2];
         tmp2[j + 3] = 255;
      }
      
      g_free(tmp);
      tmp = tmp2;
      bpp = 4;
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
   
   if((c0 > c1) || (format == DDS_COMPRESS_BC3))
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

static void decode_alpha_block_DXT3(unsigned char *dst, unsigned char *src,
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

static void decode_alpha_block_DXT5(unsigned char *dst, unsigned char *src,
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
         if(format == DDS_COMPRESS_BC1)
         {
            decode_color_block(d, s, sx, sy, width * bpp, format);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC2)
         {
            decode_alpha_block_DXT3(d + 3, s, sx, sy, width * bpp);
            s += 8;
            decode_color_block(d, s, sx, sy, width * bpp, format);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC3)
         {
            decode_alpha_block_DXT5(d + 3, s, sx, sy, bpp, width * bpp);
            s += 8;
            decode_color_block(d, s, sx, sy, width * bpp, format);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC4)
         {
            decode_alpha_block_DXT5(d, s, sx, sy, bpp, width * bpp);
            s += 8;
         }
         else if(format == DDS_COMPRESS_BC5)
         {
            decode_alpha_block_DXT5(d, s + 8, sx, sy, bpp, width * bpp);
            decode_alpha_block_DXT5(d + 1, s, sx, sy, bpp, width * bpp);
            s += 16;
         }
      }
   }
   
   return(1);
}
