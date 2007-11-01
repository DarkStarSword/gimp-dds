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

#include <string.h>
#include <math.h>

#include "dds.h"
#include "mipmap.h"

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

float cubic_interpolate(float a, float b, float c, float d, float x)
{
   float v0, v1, v2, v3, x2;
   
   x2 = x * x;
   v0 = d - c - a + b;
   v1 = a - b - v0;
   v2 = c - a;
   v3 = b;
   
   return(v0 * x * x2 + v1 * x2 + v2 * x + v3);
}

static void scale_image_cubic(unsigned char *dst, int dw, int dh,
                              unsigned char *src, int sw, int sh,
                              int bpp)
{
   int n, x, y;
   int ix, iy;
   float fx, fy;
   float dx, dy, val;
   float r0, r1, r2, r3;
   int srowbytes = sw * bpp;
   int drowbytes = dw * bpp;
   
#define VAL(x, y, c) \
   (float)src[((y) < 0 ? 0 : (y) >= sh ? sh - 1 : (y)) * srowbytes + \
              (((x) < 0 ? 0 : (x) >= sw ? sw - 1 : (x)) * bpp) + c]
      
   for(y = 0; y < dh; ++y)
   {
      fy = ((float)y / (float)dh) * (float)sh;
      iy = (int)fy;
      dy = fy - (float)iy;
      for(x = 0; x < dw; ++x)
      {
         fx = ((float)x / (float)dw) * (float)sw;
         ix = (int)fx;
         dx = fx - (float)ix;
         
         for(n = 0; n < bpp; ++n)
         {
            r0 = cubic_interpolate(VAL(ix - 1, iy - 1, n),
                                   VAL(ix,     iy - 1, n),
                                   VAL(ix + 1, iy - 1, n),
                                   VAL(ix + 2, iy - 1, n), dx);
            r1 = cubic_interpolate(VAL(ix - 1, iy,     n),
                                   VAL(ix,     iy,     n),
                                   VAL(ix + 1, iy,     n),
                                   VAL(ix + 2, iy,     n), dx);
            r2 = cubic_interpolate(VAL(ix - 1, iy + 1, n),
                                   VAL(ix,     iy + 1, n),
                                   VAL(ix + 1, iy + 1, n),
                                   VAL(ix + 2, iy + 1, n), dx);
            r3 = cubic_interpolate(VAL(ix - 1, iy + 2, n),
                                   VAL(ix,     iy + 2, n),
                                   VAL(ix + 1, iy + 2, n),
                                   VAL(ix + 2, iy + 2, n), dx);
            val = cubic_interpolate(r0, r1, r2, r3, dy);
            if(val <   0) val = 0;
            if(val > 255) val = 255;
            dst[y * drowbytes + (x * bpp) + n] = (unsigned char)val;
         }
      }
   }
#undef VAL   
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

int generate_mipmaps(unsigned char *dst, unsigned char *src,
                     unsigned int width, unsigned int height, int bpp,
                     int indexed, int mipmaps)
{
   int i;
   unsigned int w, h;
   unsigned int offset;

   memcpy(dst, src, width * height * bpp);
   offset = width * height * bpp;
   
   for(i = 1; i < mipmaps; ++i)
   {
      w = width  >> i;
      h = height >> i;
      if(w < 1) w = 1;
      if(h < 1) h = 1;
      
      if(indexed)
         scale_image_nearest(dst + offset, w, h, src, width, height, bpp);
      else
         scale_image_cubic(dst + offset, w, h, src, width, height, bpp);

      offset += (w * h * bpp);
   }
   
   return(1);
}

static void scale_volume_image_cubic(unsigned char *dst, int dw, int dh, int dd,
                                     unsigned char *src, int sw, int sh, int sd,
                                     int bpp)
{
   int n, x, y, z;
   int ix, iy, iz;
   float fx, fy, fz;
   float dx, dy, dz, val;
   float r0, r1, r2, r3;
   float v0, v1, v2, v3;
   int srowbytes = sw * bpp;
   int drowbytes = dw * bpp;
   int sslicebytes = sw * sh * bpp;
   int dslicebytes = dw * dh * bpp;
   
#define VAL(x, y, z, c) \
   (float)src[(((z) < 0 ? 0 : (z) >= sd ? sd - 1 : (z)) * sslicebytes) + \
              (((y) < 0 ? 0 : (y) >= sh ? sh - 1 : (y)) * srowbytes) + \
              (((x) < 0 ? 0 : (x) >= sw ? sw - 1 : (x)) * bpp) + c]

   for(z = 0; z < dd; ++z)
   {
      fz = ((float)z / (float)dd) * (float)sd;
      iz = (int)fz;
      dz = fz - (float)iz;
      for(y = 0; y < dh; ++y)
      {
         fy = ((float)y / (float)dh) * (float)sh;
         iy = (int)fy;
         dy = fy - (float)iy;
         for(x = 0; x < dw; ++x)
         {
            fx = ((float)x / (float)dw) * (float)sw;
            ix = (int)fx;
            dx = fx - (float)ix;
            for(n = 0; n < bpp; ++n)
            {
               r0 = cubic_interpolate(VAL(ix - 1, iy - 1, z - 1, n),
                                      VAL(ix,     iy - 1, z - 1, n),
                                      VAL(ix + 1, iy - 1, z - 1, n),
                                      VAL(ix + 2, iy - 1, z - 1, n), dx);
               r1 = cubic_interpolate(VAL(ix - 1, iy,     z - 1, n),
                                      VAL(ix,     iy,     z - 1, n),
                                      VAL(ix + 1, iy,     z - 1, n),
                                      VAL(ix + 2, iy,     z - 1, n), dx);
               r2 = cubic_interpolate(VAL(ix - 1, iy + 1, z - 1, n),
                                      VAL(ix,     iy + 1, z - 1, n),
                                      VAL(ix + 1, iy + 1, z - 1, n),
                                      VAL(ix + 2, iy + 1, z - 1, n), dx);
               r3 = cubic_interpolate(VAL(ix - 1, iy + 2, z - 1, n),
                                      VAL(ix,     iy + 2, z - 1, n),
                                      VAL(ix + 1, iy + 2, z - 1, n),
                                      VAL(ix + 2, iy + 2, z - 1, n), dx);
               v0 = cubic_interpolate(r0, r1, r2, r3, dy);

               r0 = cubic_interpolate(VAL(ix - 1, iy - 1, z, n),
                                      VAL(ix,     iy - 1, z, n),
                                      VAL(ix + 1, iy - 1, z, n),
                                      VAL(ix + 2, iy - 1, z, n), dx);
               r1 = cubic_interpolate(VAL(ix - 1, iy,     z, n),
                                      VAL(ix,     iy,     z, n),
                                      VAL(ix + 1, iy,     z, n),
                                      VAL(ix + 2, iy,     z, n), dx);
               r2 = cubic_interpolate(VAL(ix - 1, iy + 1, z, n),
                                      VAL(ix,     iy + 1, z, n),
                                      VAL(ix + 1, iy + 1, z, n),
                                      VAL(ix + 2, iy + 1, z, n), dx);
               r3 = cubic_interpolate(VAL(ix - 1, iy + 2, z, n),
                                      VAL(ix,     iy + 2, z, n),
                                      VAL(ix + 1, iy + 2, z, n),
                                      VAL(ix + 2, iy + 2, z, n), dx);
               v1 = cubic_interpolate(r0, r1, r2, r3, dy);

               r0 = cubic_interpolate(VAL(ix - 1, iy - 1, z + 1, n),
                                      VAL(ix,     iy - 1, z + 1, n),
                                      VAL(ix + 1, iy - 1, z + 1, n),
                                      VAL(ix + 2, iy - 1, z + 1, n), dx);
               r1 = cubic_interpolate(VAL(ix - 1, iy,     z + 1, n),
                                      VAL(ix,     iy,     z + 1, n),
                                      VAL(ix + 1, iy,     z + 1, n),
                                      VAL(ix + 2, iy,     z + 1, n), dx);
               r2 = cubic_interpolate(VAL(ix - 1, iy + 1, z + 1, n),
                                      VAL(ix,     iy + 1, z + 1, n),
                                      VAL(ix + 1, iy + 1, z + 1, n),
                                      VAL(ix + 2, iy + 1, z + 1, n), dx);
               r3 = cubic_interpolate(VAL(ix - 1, iy + 2, z + 1, n),
                                      VAL(ix,     iy + 2, z + 1, n),
                                      VAL(ix + 1, iy + 2, z + 1, n),
                                      VAL(ix + 2, iy + 2, z + 1, n), dx);
               v2 = cubic_interpolate(r0, r1, r2, r3, dy);

               r0 = cubic_interpolate(VAL(ix - 1, iy - 1, z + 2, n),
                                      VAL(ix,     iy - 1, z + 2, n),
                                      VAL(ix + 1, iy - 1, z + 2, n),
                                      VAL(ix + 2, iy - 1, z + 2, n), dx);
               r1 = cubic_interpolate(VAL(ix - 1, iy,     z + 2, n),
                                      VAL(ix,     iy,     z + 2, n),
                                      VAL(ix + 1, iy,     z + 2, n),
                                      VAL(ix + 2, iy,     z + 2, n), dx);
               r2 = cubic_interpolate(VAL(ix - 1, iy + 1, z + 2, n),
                                      VAL(ix,     iy + 1, z + 2, n),
                                      VAL(ix + 1, iy + 1, z + 2, n),
                                      VAL(ix + 2, iy + 1, z + 2, n), dx);
               r3 = cubic_interpolate(VAL(ix - 1, iy + 2, z + 2, n),
                                      VAL(ix,     iy + 2, z + 2, n),
                                      VAL(ix + 1, iy + 2, z + 2, n),
                                      VAL(ix + 2, iy + 2, z + 2, n), dx);
               v3 = cubic_interpolate(r0, r1, r2, r3, dy);
               
               val = cubic_interpolate(v0, v1, v2, v3, dz);
               
               if(val <   0) val = 0;
               if(val > 255) val = 255;
               
               dst[(z * dslicebytes) + (y * drowbytes) + (x * bpp) + n] =
                  (unsigned char)val;
            }
         }
      }
   }
#undef VAL   
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

int generate_volume_mipmaps(unsigned char *dst, unsigned char *src,
                            unsigned int width, unsigned int height,
                            unsigned int depth, int bpp, int indexed,
                            int mipmaps)
{
   int i;
   unsigned int w, h, d;
   unsigned int offset;

   memcpy(dst, src, width * height * depth * bpp);
   offset = width * height * depth * bpp;
   
   for(i = 1; i < mipmaps; ++i)
   {
      w = width  >> i;
      h = height >> i;
      d = depth  >> i;
      if(w < 1) w = 1;
      if(h < 1) h = 1;
      if(d < 1) d = 1;
      
      if(indexed)
      {
         scale_volume_image_nearest(dst + offset, w, h, d, src, width, height,
                                    depth, bpp);
      }
      else
      {
         scale_volume_image_cubic(dst + offset, w, h, d, src, width, height,
                                  depth, bpp);
      }

      offset += (w * h * d * bpp);
   }
   
   return(1);
}
