/*
	DDS GIMP plugin

	Copyright (C) 2004-2008 Shawn Kirst <skirst@insightbb.com>,
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

#include <string.h>
#include <math.h>

#include "dds.h"
#include "mipmap.h"
#include "imath.h"

typedef void (*mipmapfunc_t)(unsigned char *, int, int, unsigned char *, int, int, int);
typedef void (*volmipmapfunc_t)(unsigned char *, int, int, int, unsigned char *, int, int, int, int);

int get_num_mipmaps(int width, int height)
{
   int w = width << 1;
   int h = height << 1;
   int n = 0;
   
   while(w != 1 || h != 1)
   {
      if(w > 1) w >>= 1;
      if(h > 1) h >>= 1;
      ++n;
   }
   
   return(n);
}

unsigned int get_mipmapped_size(int width, int height, int bpp,
                                int level, int num, int format)
{
   int w, h, n = 0;
   unsigned int size = 0;
   
   w = width >> level;
   h = height >> level;
   if(w == 0) w = 1;
   if(h == 0) h = 1;
   w <<= 1;
   h <<= 1;
   
   while(n < num && (w != 1 || h != 1))
   {
      if(w > 1) w >>= 1;
      if(h > 1) h >>= 1;
      if(format == DDS_COMPRESS_NONE)
         size += (w * h);
      else
         size += ((w + 3) >> 2) * ((h + 3) >> 2);
      ++n;
   }
   
   if(format == DDS_COMPRESS_NONE)
      size *= bpp;
   else
   {
      if(format == DDS_COMPRESS_BC1 || format == DDS_COMPRESS_BC4)
         size *= 8;
      else
         size *= 16;
   }
   
   return(size);
}

unsigned int get_volume_mipmapped_size(int width, int height,
                                       int depth, int bpp, int level,
                                       int num, int format)
{
   int w, h, d, n = 0;
   unsigned int size = 0;
   
   w = width >> level;
   h = height >> level;
   d = depth >> level;
   if(w == 0) w = 1;
   if(h == 0) h = 1;
   if(d == 0) d = 1;
   w <<= 1;
   h <<= 1;
   d <<= 1;

   while(n < num && (w != 1 || h != 1))
   {
      if(w > 1) w >>= 1;
      if(h > 1) h >>= 1;
      if(d > 1) d >>= 1;
      if(format == DDS_COMPRESS_NONE)
         size += (w * h * d);
      else
         size += (((w + 3) >> 2) * ((h + 3) >> 2) * d);
      ++n;
   }

   if(format == DDS_COMPRESS_NONE)
      size *= bpp;
   else
   {
      if(format == DDS_COMPRESS_BC1 || format == DDS_COMPRESS_BC4)
         size *= 8;
      else
         size *= 16;
   }
   
   return(size);
}

static void scale_image_nearest(unsigned char *dst, int dw, int dh,
                                unsigned char *src, int sw, int sh,
                                int bpp)
{
   int n, x, y;
   int ix, iy;
   int srowbytes = sw * bpp;
   int drowbytes = dw * bpp;
   
   for(y = 0; y < dh; ++y)
   {
      iy = (y * sh + sh / 2) / dh;
      for(x = 0; x < dw; ++x)
      {
         ix = (x * sw + sw / 2) / dw;
         for(n = 0; n < bpp; ++n)
         {
            dst[y * drowbytes + (x * bpp) + n] =
               src[iy * srowbytes + (ix * bpp) + n];
         }
      }
   }
}

static void scale_image_bilinear(unsigned char *dst, int dw, int dh,
                                 unsigned char *src, int sw, int sh,
                                 int bpp)
{
   int x, y, n, ix, iy, wx, wy, v, v0, v1;
   int dstride = dw * bpp;
   unsigned char *s;
   
   for(y = 0; y < dh; ++y)
   {
      if(dh > 1)
      {
         iy = (((sh - 1) * y) << 8) / (dh - 1);
         if(y == dh - 1) --iy;
         wy = iy & 0xff;
         iy >>= 8;
      }
      else
         iy = wy = 0;
      
      for(x = 0; x < dw; ++x)
      {
         if(dw > 1)
         {
            ix = (((sw - 1) * x) << 8) / (dw - 1);
            if(x == dw - 1) --ix;
            wx = ix & 0xff;
            ix >>= 8;
         }
         else
            ix = wx = 0;
         
         s = src + (iy * sw + ix) * bpp;
         
         for(n = 0; n < bpp; ++n)
         {
            v0 = blerp(s[0], s[bpp], wx);
            v1 = blerp(s[sw * bpp], s[(sw + 1) * bpp], wx);
            v = blerp(v0, v1, wy);
            if(v < 0) v = 0;
            if(v > 255) v = 255;
            dst[(y * dstride) + (x * bpp) + n] = v;
            ++s;
         }
      }
   }
}

static void scale_image_bicubic(unsigned char *dst, int dw, int dh,
                                unsigned char *src, int sw, int sh,
                                int bpp)
{
   int x, y, n, ix, iy, wx, wy, v;
   int a, b, c, d;
   int dstride = dw * bpp;
   unsigned char *s;

   for(y = 0; y < dh; ++y)
   {
      if(dh > 1)
      {
         iy = (((sh - 1) * y) << 7) / (dh - 1);
         if(y == dh - 1) --iy;
         wy = iy & 0x7f;
         iy >>= 7;
      }
      else
         iy = wy = 0;
      
      for(x = 0; x < dw; ++x)
      {
         if(dw > 1)
         {
            ix = (((sw - 1) * x) << 7) / (dw - 1);
            if(x == dw - 1) --ix;
            wx = ix & 0x7f;
            ix >>= 7;
         }
         else
            ix = wx = 0;
         
         s = src + ((iy - 1) * sw + (ix - 1)) * bpp;
         
         for(n = 0; n < bpp; ++n)
         {
            b = icerp(s[(sw + 0) * bpp],
                      s[(sw + 1) * bpp],
                      s[(sw + 2) * bpp],
                      s[(sw + 3) * bpp], wx);
            if(iy > 0)
            {
               a = icerp(s[      0],
                         s[    bpp],
                         s[2 * bpp],
                         s[3 * bpp], wx);
            }
            else
               a = b;
            
            c = icerp(s[(2 * sw + 0) * bpp],
                      s[(2 * sw + 1) * bpp],
                      s[(2 * sw + 2) * bpp],
                      s[(2 * sw + 3) * bpp], wx);
            if(iy < dh - 1)
            {
               d = icerp(s[(3 * sw + 0) * bpp],
                         s[(3 * sw + 1) * bpp],
                         s[(3 * sw + 2) * bpp],
                         s[(3 * sw + 3) * bpp], wx);
            }
            else
               d = c;
            
            v = icerp(a, b, c, d, wy);
            if(v < 0) v = 0;
            if(v > 255) v = 255;
            dst[(y * dstride) + (x * bpp) + n] = v;
            ++s;
         }
      }
   }
}

int generate_mipmaps(unsigned char *dst, unsigned char *src,
                     unsigned int width, unsigned int height, int bpp,
                     int indexed, int mipmaps, int filter)
{
   int i;
   unsigned int sw, sh, dw, dh;
   unsigned char *s, *d;
   mipmapfunc_t mipmap_func = NULL;
   
   if(indexed)
      mipmap_func = scale_image_nearest;
   else
   {
      switch(filter)
      {
         case DDS_MIPMAP_NEAREST:  mipmap_func = scale_image_nearest;  break;
         case DDS_MIPMAP_BICUBIC:  mipmap_func = scale_image_bicubic;  break;
         case DDS_MIPMAP_BILINEAR:
         default:                  mipmap_func = scale_image_bilinear; break;
      }
   }
   
   memcpy(dst, src, width * height * bpp);

   s = dst;
   d = dst + (width * height * bpp);
   
   sw = width;
   sh = height;
   
   for(i = 1; i < mipmaps; ++i)
   {
      dw = sw >> 1;
      dh = sh >> 1;
      if(dw < 1) dw = 1;
      if(dh < 1) dh = 1;
  
      mipmap_func(d, dw, dh, s, sw, sh, bpp);

      s = d;
      sw = dw;
      sh = dh;
      d += (dw * dh * bpp);
   }
   
   return(1);
}

static void scale_volume_image_nearest(unsigned char *dst, int dw, int dh, int dd,
                                       unsigned char *src, int sw, int sh, int sd,
                                       int bpp)
{
   int n, x, y, z;
   int ix, iy, iz;

   for(z = 0; z < dd; ++z)
   {
      iz = (z * sd + sd / 2) / dd;
      for(y = 0; y < dh; ++y)
      {
         iy = (y * sh + sh / 2) / dh;
         for(x = 0; x < dw; ++x)
         {
            ix = (x * sw + sw / 2) / dw;
            for(n = 0; n < bpp; ++n)
            {
               dst[(z * (dw * dh)) + (y * dw) + (x * bpp) + n] =
                  src[(iz * (sw * sh)) + (iy * sw) + (ix * bpp) + n];
            }
         }
      }
   }
}

static void scale_volume_image_bilinear(unsigned char *dst, int dw, int dh, int dd,
                                        unsigned char *src, int sw, int sh, int sd,
                                        int bpp)
{
   int x, y, z, n, ix, iy, iz, wx, wy, wz, v, v0, v1, r0, r1;
   unsigned char *s1, *s2, *d = dst;
   
   for(z = 0; z < dd; ++z)
   {
      if(dd > 1)
      {
         iz = (((sd - 1) * z) << 8) / (dd - 1);
         if(z == dd - 1) --iz;
         wz = iz & 0xff;
         iz >>= 8;
      }
      else
         iz = wz = 0;
      
      for(y = 0; y < dh; ++y)
      {
         if(dh > 1)
         {
            iy = (((sh - 1) * y) << 8) / (dh - 1);
            if(y == dh - 1) --iy;
            wy = iy & 0xff;
            iy >>= 8;
         }
         else
            iy = wy = 0;
         
         for(x = 0; x < dw; ++x)
         {
            if(dw > 1)
            {
               ix = (((sw - 1) * x) << 8) / (dw - 1);
               if(x == dw - 1) --ix;
               wx = ix & 0xff;
               ix >>= 8;
            }
            else
               ix = wx = 0;
            
            s1 = src + ((iz * (sw * sh)) + (iy * sw) + ix) * bpp;
            s2 = src + (((iz + 1) * (sw * sh)) + (iy * sw) + ix) * bpp;
            
            for(n = 0; n < bpp; ++n)
            {
               r0 = blerp(s1[0], s1[bpp], wx);
               r1 = blerp(s1[sw * bpp], s1[(sw + 1) * bpp], wx);
               v0 = blerp(r0, r1, wy);
               
               r0 = blerp(s2[0], s2[bpp], wx);
               r1 = blerp(s2[sw * bpp], s2[(sw + 1) * bpp], wx);
               v1 = blerp(r0, r1, wy);
               
               v = blerp(v0, v1, wz);
               if(v < 0) v = 0;
               if(v > 255) v = 255;
               *d++ = v;
               ++s1;
               ++s2;
            }
         }
      }
   }
}

static void scale_volume_image_cubic(unsigned char *dst, int dw, int dh, int dd,
                                     unsigned char *src, int sw, int sh, int sd,
                                     int bpp)
{
   int n, x, y, z;
   int ix, iy, iz;
   int wx, wy, wz;
   int a, b, c, d;
   int val, v0, v1, v2, v3;
   int dstride = dw * bpp;
   int sslice = sw * sh * bpp;
   int dslice = dw * dh * bpp;
   unsigned char *s0, *s1, *s2, *s3;
   
   for(z = 0; z < dd; ++z)
   {
      if(dd > 1)
      {
         iz = (((sd - 1) * z) << 7) / (dd - 1);
         if(z == dd - 1) --iz;
         wz = iz & 0x7f;
         iz >>= 7;
      }
      else
         iz = wz = 0;
      
      for(y = 0; y < dh; ++y)
      {
         if(dh > 1)
         {
            iy = (((sh - 1) * y) << 7) / (dh - 1);
            if(y == dh - 1) --iy;
            wy = iy & 0x7f;
            iy >>= 7;
         }
         else
            iy = wy = 0;

         for(x = 0; x < dw; ++x)
         {
            if(dw > 1)
            {
               ix = (((sw - 1) * x) << 7) / (dw - 1);
               if(x == dw - 1) --ix;
               wx = ix & 0x7f;
               ix >>= 7;
            }
            else 
               ix = wx = 0;

            s0 = src + (((iz - 1) * (sw * sh)) + ((iy - 1) * sw) + (ix - 1)) * bpp;
            s1 = s0 + sslice;
            s2 = s1 + sslice;
            s3 = s2 + sslice;
         
            for(n = 0; n < bpp; ++n)
            {
               b = icerp(s1[(sw + 0) * bpp],
                         s1[(sw + 1) * bpp],
                         s1[(sw + 2) * bpp],
                         s1[(sw + 3) * bpp], wx);
               if(iy > 0)
               {
                  a = icerp(s1[      0],
                            s1[    bpp],
                            s1[2 * bpp],
                            s1[3 * bpp], wx);
               }
               else
                  a = b;
               
               c = icerp(s1[(2 * sw + 0) * bpp],
                         s1[(2 * sw + 1) * bpp],
                         s1[(2 * sw + 2) * bpp],
                         s1[(2 * sw + 3) * bpp], wx);
               if(iy < dh - 1)
               {
                  d = icerp(s1[(3 * sw + 0) * bpp],
                            s1[(3 * sw + 1) * bpp],
                            s1[(3 * sw + 2) * bpp],
                            s1[(3 * sw + 3) * bpp], wx);
               }
               else
                  d = c;
            
               v1 = icerp(a, b, c, d, wy);
               
               if(iz > 0)
               {
                  b = icerp(s0[(sw + 0) * bpp],
                            s0[(sw + 1) * bpp],
                            s0[(sw + 2) * bpp],
                            s0[(sw + 3) * bpp], wx);
                  if(iy > 0)
                  {
                     a = icerp(s0[      0],
                               s0[    bpp],
                               s0[2 * bpp],
                               s0[3 * bpp], wx);
                  }
                  else
                     a = b;
                  
                  c = icerp(s0[(2 * sw + 0) * bpp],
                            s0[(2 * sw + 1) * bpp],
                            s0[(2 * sw + 2) * bpp],
                            s0[(2 * sw + 3) * bpp], wx);
                  if(iy < dh - 1)
                  {
                     d = icerp(s0[(3 * sw + 0) * bpp],
                               s0[(3 * sw + 1) * bpp],
                               s0[(3 * sw + 2) * bpp],
                               s0[(3 * sw + 3) * bpp], wx);
                  }
                  else
                     d = c;
            
                  v0 = icerp(a, b, c, d, wy);
               }
               else
                  v0 = v1;

               b = icerp(s2[(sw + 0) * bpp],
                         s2[(sw + 1) * bpp],
                         s2[(sw + 2) * bpp],
                         s2[(sw + 3) * bpp], wx);
               if(iy > 0)
               {
                  a = icerp(s2[      0],
                            s2[    bpp],
                            s2[2 * bpp],
                            s2[3 * bpp], wx);
               }
               else
                  a = b;
               
               c = icerp(s2[(2 * sw + 0) * bpp],
                         s2[(2 * sw + 1) * bpp],
                         s2[(2 * sw + 2) * bpp],
                         s2[(2 * sw + 3) * bpp], wx);
               if(iy < dh - 1)
               {
                  d = icerp(s2[(3 * sw + 0) * bpp],
                            s2[(3 * sw + 1) * bpp],
                            s2[(3 * sw + 2) * bpp],
                            s2[(3 * sw + 3) * bpp], wx);
               }
               else
                  d = c;
               
               v2 = icerp(a, b, c, d, wy);
               
               if(iz < dd - 1)
               {
                  b = icerp(s3[(sw + 0) * bpp],
                            s3[(sw + 1) * bpp],
                            s3[(sw + 2) * bpp],
                            s3[(sw + 3) * bpp], wx);
                  if(iy > 0)
                  {
                     a = icerp(s3[      0],
                               s3[    bpp],
                               s3[2 * bpp],
                               s3[3 * bpp], wx);
                  }
                  else
                     a = b;
                  
                  c = icerp(s3[(2 * sw + 0) * bpp],
                            s3[(2 * sw + 1) * bpp],
                            s3[(2 * sw + 2) * bpp],
                            s3[(2 * sw + 3) * bpp], wx);
                  if(iy < dh - 1)
                  {
                     d = icerp(s3[(3 * sw + 0) * bpp],
                               s3[(3 * sw + 1) * bpp],
                               s3[(3 * sw + 2) * bpp],
                               s3[(3 * sw + 3) * bpp], wx);
                  }
                  else
                     d = c;
            
                  v3 = icerp(a, b, c, d, wy);
               }
               else
                  v3 = v2;
               
               val = icerp(v0, v1, v2, v3, wz);
               
               if(val <   0) val = 0;
               if(val > 255) val = 255;
               
               dst[(z * dslice) + (y * dstride) + (x * bpp) + n] = val;
               
               ++s0;
               ++s1;
               ++s2;
               ++s3;
            }
         }
      }
   }
}

int generate_volume_mipmaps(unsigned char *dst, unsigned char *src,
                            unsigned int width, unsigned int height,
                            unsigned int depth, int bpp, int indexed,
                            int mipmaps, int filter)
{
   int i;
   unsigned int sw, sh, sd;
   unsigned int dw, dh, dd;
   unsigned char *s, *d;
   volmipmapfunc_t mipmap_func = NULL;
   
   if(indexed)
      mipmap_func = scale_volume_image_nearest;
   else
   {
      switch(filter)
      {
         case DDS_MIPMAP_NEAREST:  mipmap_func = scale_volume_image_nearest;  break;
         case DDS_MIPMAP_BICUBIC:  mipmap_func = scale_volume_image_cubic;    break;
         case DDS_MIPMAP_BILINEAR:
         default:                  mipmap_func = scale_volume_image_bilinear; break;
      }
   }

   memcpy(dst, src, width * height * depth * bpp);

   s = dst;
   d = dst + (width * height * depth * bpp);
   
   sw = width;
   sh = height;
   sd = depth;
   
   for(i = 1; i < mipmaps; ++i)
   {
      dw = sw >> 1;
      dh = sh >> 1;
      dd = sd >> 1;
      if(dw < 1) dw = 1;
      if(dh < 1) dh = 1;
      if(dd < 1) dd = 1;

      mipmap_func(d, dw, dh, dd, s, sw, sh, sd, bpp);

      s = d;
      sw = dw;
      sh = dh;
      sd = dd;
      d += (dw * dh * dd * bpp);
   }
   
   return(1);
}
