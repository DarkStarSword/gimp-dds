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
#define GL_GLEXT_PROTOTYPES

#include <string.h>
#include <math.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/glext.h>

#include "dds.h"

#define IS_POT(x)      (!((x) & ((x) - 1)))
#define LERP(a, b, t)  ((a) + ((b) - (a)) * (t))

static int has_npot = 0;

static int extension_supported(const char *ext)
{
   const char *start, *where, *end;
   int len = strlen(ext);
   
   end = start = (char *)glGetString(GL_EXTENSIONS);
   
   while((where = strstr(end, ext)))
   {
      end = where + len;
      
      if((where == start || *(where - 1) == ' ') &&
         (*end == ' ' || *end == 0))
         return(1);
   }
   return(0);
}

char *initialize_opengl(void)
{
   int argc = 1;
   char *argv[] = { "dds" };

   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_RGBA);
   glutInitWindowSize(1, 1);
   glutCreateWindow("GIMP DDS");

   if(!extension_supported("GL_ARB_texture_compression"))
      return("GL_ARB_texture_compression is not supported by your OpenGL "
             "implementation.\n");
   if(!extension_supported("GL_S3_s3tc") &&
      !extension_supported("GL_EXT_texture_compression_s3tc"))
      return("GL_S3_s3tc or GL_EXT_texture_compression_s3tc is not supported "
             "by your OpenGL implementation.\n");
   if(!extension_supported("GL_SGIS_generate_mipmap"))
      return("GL_SGIS_generate_mipmap is not supported by your OpenGL "
             "implementation.\n");
   
   has_npot = extension_supported("GL_ARB_texture_non_power_of_two");

   glPixelStorei(GL_PACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   glHint(GL_TEXTURE_COMPRESSION_HINT_ARB, GL_NICEST);
   glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);
   
   return(0);
}

int dxt_compress(unsigned char *dst, unsigned char *src, int format,
                 unsigned int width, unsigned int height, int bpp,
                 int mipmaps)
{
   GLenum internal = 0;
   GLenum type = GL_RGB;
   int i, size;
   
   if(!(IS_POT(width) && IS_POT(height)))
      return(0);
   
   switch(bpp)
   {
      case 1: type = GL_LUMINANCE;       break;
      case 2: type = GL_LUMINANCE_ALPHA; break;
      case 3: type = GL_RGB;             break;
      case 4: type = GL_RGBA;            break;
   }
   
   if(format == DDS_COMPRESS_DXT1)
   {
      internal = (bpp == 4) ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT :
                              GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
   }
   else if(format == DDS_COMPRESS_DXT3)
      internal = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
   else
      internal = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
   
   
   glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
                   mipmaps > 1 ? GL_TRUE : GL_FALSE);
   
   glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, 
                type, GL_UNSIGNED_BYTE, src);

   glGetCompressedTexImage(GL_TEXTURE_2D, 0, dst);
   
   if(mipmaps > 1)
   {
      unsigned int offset = 0;
      for(i = 1; i < mipmaps; ++i)
      {
         glGetTexLevelParameteriv(GL_TEXTURE_2D, i - 1, 
                                  GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &size);
         offset += size;
         glGetCompressedTexImage(GL_TEXTURE_2D, i, dst + offset);
      }
   }
   
   return(1);
}                

int dxt_decompress(unsigned char *dst, unsigned char *src, int format,
                   unsigned int size, unsigned int width, unsigned int height,
                   int bpp)
{
   GLenum internal, type = GL_RGB;
   
   if(!(IS_POT(width) && IS_POT(height)))
      return(0);

   switch(bpp)
   {
      case 1: type = GL_LUMINANCE;       break;
      case 2: type = GL_LUMINANCE_ALPHA; break;
      case 3: type = GL_RGB;             break;
      case 4: type = GL_RGBA;            break;
   }
   
   switch(format)
   {
      case DDS_COMPRESS_DXT1:
         internal = (bpp == 4) ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT :
                                 GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
         break;
      case DDS_COMPRESS_DXT3:
         internal = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
         break;
      case DDS_COMPRESS_DXT5:
         internal = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
         break;
   }
   
   glCompressedTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, size, src);
   glGetTexImage(GL_TEXTURE_2D, 0, type, GL_UNSIGNED_BYTE, dst);
   
   return(1);
}

static void scale_image(unsigned char *dst, int dw, int dh,
                        unsigned char *src, int sw, int sh,
                        int bpp)
{
   int n, x, y;
   int hm1 = sh - 1;
   int wm1 = sw - 1;
   int xfl, yfl;
   float xsrc, ysrc;
   float dx, dy, val;
   unsigned char *s, *d;
   int rowbytes = sw * bpp;
      
   d = dst;
      
   for(y = 0; y < dh; ++y)
   {
      ysrc = ((float)y / (float)dh) * (float)hm1;
      yfl = (int)ysrc;
      dy = ysrc - (float)yfl;
      for(x = 0; x < dw; ++x)
      {
         xsrc = ((float)x / (float)dw) * (float)wm1;
         xfl = (int)xsrc;
         dx = xsrc - (float)xfl;
         
         s = src + ((yfl * rowbytes) + (xfl * bpp));
         for(n = 0; n < bpp; ++n)
         {
            val = LERP(LERP(s[0], s[bpp], dx),
                       LERP(s[rowbytes], s[rowbytes + bpp], dx),
                       dy);
            *d++ = (unsigned char)val;
            s++;
         }
      }
   }
}

static int generate_mipmaps_npot(unsigned char *dst, unsigned char *src,
                                 unsigned int width, unsigned int height,
                                 int bpp, int mipmaps)
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
      
      scale_image(dst + offset, w, h, src, width, height, bpp);
      offset += (w * h * bpp);
   }
   
   return(1);
}

int generate_mipmaps(unsigned char *dst, unsigned char *src,
                     unsigned int width, unsigned int height, int bpp,
                     int mipmaps)
{
   int i;
   unsigned int w, h;
   GLenum format = 0;
   unsigned int offset;
   
   if(!(IS_POT(width) && IS_POT(height)) && !has_npot)
      return(generate_mipmaps_npot(dst, src, width, height, bpp, mipmaps));
   
   switch(bpp)
   {
      case 1: format = GL_LUMINANCE;       break;
      case 2: format = GL_LUMINANCE_ALPHA; break;
      case 3: format = GL_RGB;             break;
      case 4: format = GL_RGBA;            break;
   }
   
   glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
   glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                format, GL_UNSIGNED_BYTE, src);
   
   memcpy(dst, src, width * height * bpp);
   
   offset = width * height * bpp;
   
   for(i = 1; i < mipmaps; ++i)
   {
      w = width  >> i;
      h = height >> i;
      if(w < 1) w = 1;
      if(h < 1) h = 1;
      
      glGetTexImage(GL_TEXTURE_2D, i, format, GL_UNSIGNED_BYTE, dst + offset);

      offset += (w * h * bpp);
   }
   
   return(1);
}

int generate_volume_mipmaps(unsigned char *dst, unsigned char *src,
                            unsigned int width, unsigned int height,
                            unsigned int depth, int bpp, int mipmaps)
{
   int i;
   unsigned int w, h, d;
   GLenum format = 0;
   unsigned int offset;
   
   switch(bpp)
   {
      case 1: format = GL_LUMINANCE;       break;
      case 2: format = GL_LUMINANCE_ALPHA; break;
      case 3: format = GL_RGB;             break;
      case 4: format = GL_RGBA;            break;
   }
   
   glTexParameteri(GL_TEXTURE_3D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
   glTexImage3D(GL_TEXTURE_3D, 0, format, width, height, depth, 0,
                format, GL_UNSIGNED_BYTE, src);
   
   memcpy(dst, src, width * height * bpp * depth);
   
   offset = width * height * bpp * depth;
   
   for(i = 1; i < mipmaps; ++i)
   {
      w = width  >> i;
      h = height >> i;
      d = depth >> i;
      if(w < 1) w = 1;
      if(h < 1) h = 1;
      if(d < 1) d = 1;
      
      glGetTexImage(GL_TEXTURE_3D, i, format, GL_UNSIGNED_BYTE, dst + offset);

      offset += (w * h * bpp * d);
   }
   
   return(1);
}
