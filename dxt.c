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
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <GL/glext.h>

#include "dds.h"

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
   int i, size;
   
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
                (bpp == 4) ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, src);

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
   GLenum internal;
   
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
   glGetTexImage(GL_TEXTURE_2D, 0, bpp == 4 ? GL_RGBA : GL_RGB,
                 GL_UNSIGNED_BYTE, dst);
   
   return(1);
}

int generate_mipmaps(unsigned char *dst, unsigned char *src,
                     unsigned int width, unsigned int height, int bpp,
                     int mipmaps)
{
   int i;
   unsigned int w, h;
   GLenum format;
   unsigned int offset;
   
   format = (bpp == 4) ? GL_RGBA : GL_RGB;
   
   glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_TRUE);
   glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0,
                format, GL_UNSIGNED_BYTE, src);
   
   memcpy(dst, src, width * height * bpp);
   
   offset = width * height * bpp;
   
   w = width >> 1;
   h = height >> 1;
   if(w == 0) w = 1;
   if(h == 0) h = 1;
   
   for(i = 1; i < mipmaps; ++i)
   {
      glGetTexImage(GL_TEXTURE_2D, i, format, GL_UNSIGNED_BYTE, dst + offset);
      offset += (w * h * bpp);
      w >>= 1;
      h >>= 1;
   }
   
   return(1);
}
