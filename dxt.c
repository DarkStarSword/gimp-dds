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

#ifdef WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <string.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include <glib.h>


#include "dds.h"
#include "endian.h"
#include "mipmap.h"
#include "ycocg.h"

#ifdef USE_SOFTWARE_COMPRESSION
#ifdef WIN32
HANDLE hdxtn = NULL;
#define DXTN_DLL "dxtn.dll"
#define dlopen(handle, flags)  LoadLibrary(handle)
#define dlsym(handle, symbol)  GetProcAddress(handle, symbol)
#define dlclose(handle)        FreeLibrary(handle)
#define RTLD_LAZY   0
#define RTLD_GLOBAL 0
#else
void *hdxtn = NULL;
#define DXTN_DLL "libtxc_dxtn.so"
#endif
static void (*compress_dxtn)(int, int, int, const unsigned char*, int, unsigned char *) = NULL;
#endif // #ifdef USE_SOFTWARE_COMPRESSION

char *initialize_opengl(void)
{
   int argc = 1, err;
   char *argv[] = { "dds" };

   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_RGBA);
   glutInitWindowSize(1, 1);
   glutCreateWindow("GIMP DDS");

   err = glewInit();
   if(err != GLEW_OK)
      return((char*)glewGetErrorString(err));

#ifndef USE_SOFTWARE_COMPRESSION   
   if(!GLEW_ARB_texture_compression)
      return("GL_ARB_texture_compression is not supported by your OpenGL "
             "implementation.\n");
   if(!GLEW_S3_s3tc && !GLEW_EXT_texture_compression_s3tc)
      return("GL_S3_s3tc or GL_EXT_texture_compression_s3tc is not supported "
             "by your OpenGL implementation.\n");
   
   glHint(GL_TEXTURE_COMPRESSION_HINT_ARB, GL_NICEST);
#else
   hdxtn = dlopen(DXTN_DLL, RTLD_LAZY | RTLD_GLOBAL);
   if(hdxtn == NULL)
      return("Unable to load library " DXTN_DLL);
   compress_dxtn = (void*)dlsym(hdxtn, "tx_compress_dxtn");
   if(compress_dxtn == NULL)
   {
      dlclose(hdxtn);
      return("Missing symbol `tx_compress_dxtn' in " DXTN_DLL);
   }
#endif   

   glPixelStorei(GL_PACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   if(GLEW_SGIS_generate_mipmap)
      glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);

   return(0);
}

static void compress_3dc(unsigned char *dst, unsigned char *src,
                         int width, int height, int bpp, int mipmaps,
                         int format)
{
   int i, j, w, h;
   unsigned int offset, size;
   unsigned char *s1, *s2, *d1, *d2, *dtmp, *stmp;
   
#ifdef USE_SOFTWARE_COMPRESSION
   
   if(format == DDS_COMPRESS_BC4)
   {
      size = get_mipmapped_size(width, height, 4, 0, mipmaps, DDS_COMPRESS_NONE);
      stmp = g_malloc(size);
      memset(stmp, 0, size);
      
      for(i = j = 0; j < size; i += bpp, j += 4)
         stmp[j + 3] = src[i];
      
      size = get_mipmapped_size(width, height, 0, 0, mipmaps, DDS_COMPRESS_BC3);
      dtmp = g_malloc(size);
      
      offset = 0;
      w = width;
      h = height;
      s1 = stmp;

      for(i = 0; i < mipmaps; ++i)
      {
         compress_dxtn(4, w, h, s1, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                       dtmp + offset);
         s1 += (w * h * 4);
         offset += get_mipmapped_size(w, h, 0, 0, 1, DDS_COMPRESS_BC3);
         if(w > 1) w >>= 1;
         if(h > 1) h >>= 1;
      }

      for(i = j = 0; i < size; i += 16, j += 8)
         memcpy(dst + j, dtmp + i, 8);
      
      g_free(dtmp);
      g_free(stmp);
   }
   else if(format == DDS_COMPRESS_BC5)
   {
      size = get_mipmapped_size(width, height, 4, 0, mipmaps, DDS_COMPRESS_NONE);
      stmp = g_malloc(size * 2);
      memset(stmp, 0, size * 2);
      
      s1 = stmp;
      s2 = stmp + size;
      
      for(i = j = 0; j < size; i += bpp, j += 4)
      {
         s1[j + 3] = src[i + 1];
         s2[j + 3] = src[i];
      }
      
      size = get_mipmapped_size(width, height, 0, 0, mipmaps, DDS_COMPRESS_BC3);
      dtmp = g_malloc(size * 2);
      
      d1 = dtmp;
      d2 = dtmp + size;
      
      offset = 0;
      w = width;
      h = height;

      for(i = 0; i < mipmaps; ++i)
      {
         compress_dxtn(4, w, h, s1, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, d1 + offset);
         compress_dxtn(4, w, h, s2, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, d2 + offset);
         s1 += (w * h * 4);
         s2 += (w * h * 4);
         offset += get_mipmapped_size(w, h, 0, 0, 1, DDS_COMPRESS_BC3);
         if(w > 1) w >>= 1;
         if(h > 1) h >>= 1;
      }

      for(i = 0; i < size; i += 16)
      {
         memcpy(dst + i + 0, d1 + i, 8);
         memcpy(dst + i + 8, d2 + i, 8);
      }

      g_free(dtmp);
      g_free(stmp);
   }
   
#else // #ifdef USE_SOFTWARE_COMPRESSION

   if(GLEW_SGIS_generate_mipmap)
      glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS, GL_FALSE);
   
   if(format == DDS_COMPRESS_BC4)
   {
      size = get_mipmapped_size(width, height, 4, 0, mipmaps, DDS_COMPRESS_NONE);
      stmp = g_malloc(size);
      memset(stmp, 0, size);
      
      for(i = j = 0; j < size; i += bpp, j += 4)
         stmp[j + 3] = src[i];
                
      size = get_mipmapped_size(width, height, 0, 0, mipmaps, DDS_COMPRESS_BC3);
      dtmp = g_malloc(size);
      
      offset = 0;
      w = width;
      h = height;
      s1 = stmp;
      
      for(i = 0; i < mipmaps; ++i)
      {
         glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                      w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s1);
         glGetCompressedTexImageARB(GL_TEXTURE_2D, 0, dtmp + offset);
         s1 += (w * h * 4);
         offset += get_mipmapped_size(w, h, 0, 0, 1, DDS_COMPRESS_BC3);
         if(w > 1) w >>= 1;
         if(h > 1) h >>= 1;
      }

      for(i = j = 0; i < size; i += 16, j += 8)
         memcpy(dst + j, dtmp + i, 8);
      
      g_free(dtmp);
      g_free(stmp);
   }
   else if(format == DDS_COMPRESS_BC5)
   {
      size = get_mipmapped_size(width, height, 4, 0, mipmaps, DDS_COMPRESS_NONE);
      stmp = g_malloc(size * 2);
      memset(stmp, 0, size * 2);

      s1 = stmp;
      s2 = stmp + size;
      
      for(i = j = 0; j < size; i += bpp, j += 4)
      {
         s1[j + 3] = src[i + 1];
         s2[j + 3] = src[i];
      }
      
      size = get_mipmapped_size(width, height, 0, 0, mipmaps, DDS_COMPRESS_BC3);
      dtmp = g_malloc(size * 2);
      
      d1 = dtmp;
      d2 = dtmp + size;
      
      offset = 0;
      w = width;
      h = height;

      for(i = 0; i < mipmaps; ++i)
      {
         glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                      w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s1);
         glGetCompressedTexImageARB(GL_TEXTURE_2D, 0, d1 + offset);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,
                      w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s2);
         glGetCompressedTexImageARB(GL_TEXTURE_2D, 0, d2 + offset);
         s1 += (w * h * 4);
         s2 += (w * h * 4);
         offset += get_mipmapped_size(w, h, 0, 0, 1, DDS_COMPRESS_BC3);
         if(w > 1) w >>= 1;
         if(h > 1) h >>= 1;
      }

      for(i = 0; i < size; i += 16)
      {
         memcpy(dst + i + 0, d1 + i, 8);
         memcpy(dst + i + 8, d2 + i, 8);
      }

      g_free(dtmp);
      g_free(stmp);
   }
   
#endif // #ifdef USE_SOFTWARE_COMPRESSION
}

int dxt_compress(unsigned char *dst, unsigned char *src, int format,
                 unsigned int width, unsigned int height, int bpp,
                 int mipmaps)
{
   GLenum internal = 0;
   GLenum type = 0;
   int i, size, w, h;
   unsigned int offset;
   unsigned char *tmp;
   int j;
   unsigned char *tmp2, *s;
   
   if(!(IS_POT(width) && IS_POT(height)))
      return(0);
   
   if(format == DDS_COMPRESS_YCOCG)
   {
      offset = 0;
      w = width;
      h = height;
      s = src;
      
      for(i = 0; i < mipmaps; ++i)
      {
         compress_YCoCg_DXT5(dst + offset, s, w, h);
         s += (w * h * bpp);
         offset += get_mipmapped_size(w, h, 0, 0, 1, format);
         if(w > 1) w >>= 1;
         if(h > 1) h >>= 1;
      }
      return(1);
   }
   
   switch(bpp)
   {
      case 1: type = GL_LUMINANCE;       break;
      case 2: type = GL_LUMINANCE_ALPHA; break;
      case 3: type = GL_BGR;             break;
      case 4: type = GL_BGRA;            break;
   }
   
   if(format == DDS_COMPRESS_BC1)
   {
      internal = (bpp == 4 || bpp == 2) ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT :
                                          GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
   }
   else if(format == DDS_COMPRESS_BC2)
      internal = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
   else if(format == DDS_COMPRESS_BC3 ||
           format == DDS_COMPRESS_BC3N)
      internal = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
   
#ifdef USE_SOFTWARE_COMPRESSION   

   size = get_mipmapped_size(width, height, bpp, 0, mipmaps,
                             DDS_COMPRESS_NONE);
   tmp = g_malloc(size);
   generate_mipmaps_software(tmp, src, width, height, bpp, 0, mipmaps);

   if(bpp == 1)
   {
      /* grayscale promoted to RGB */
      
      size = get_mipmapped_size(width, height, 3, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp2 = g_malloc(size);
      
      for(i = j = 0; j < size; ++i, j += 3)
      {
         tmp2[j + 0] = tmp[i];
         tmp2[j + 1] = tmp[i];
         tmp2[j + 2] = tmp[i];
      }
      
      g_free(tmp);
      tmp = tmp2;
      bpp = 3;
   }
   else if(bpp == 2)
   {
      /* gray-alpha promoted to RGBA */
      
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
   else /* bpp >= 3 */
   {
      /* libtxc_dxtn wants BGRA pixels */
      for(i = 0; i < size; i += bpp)
      {
         c = tmp[i];
         tmp[i] = tmp[i + 2];
         tmp[i + 2] = c;
      }
   }
   
   /* add an opaque alpha channel if need be */
   if(bpp == 3 &&
      (format == DDS_COMPRESS_BC2 ||
       format == DDS_COMPRESS_BC3 ||
       format == DDS_COMPRESS_BC3N))
   {
      size = get_mipmapped_size(width, height, 4, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp2 = g_malloc(size);
      
      for(i = j = 0; j < size; i += bpp, j += 4)
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

   if(format <= DDS_COMPRESS_BC3N || format == DDS_COMPRESS_YCOCG)
   {
      for(i = 0; i < mipmaps; ++i)
      {
         compress_dxtn(bpp, w, h, s, internal, dst + offset);
         s += (w * h * bpp);
         offset += get_mipmapped_size(w, h, 0, 0, 1, format);
         if(w > 1) w >>= 1;
         if(h > 1) h >>= 1;
      }
   }
   else /* 3Dc */
      compress_3dc(dst, s, w, h, bpp, mipmaps, format);
   
   g_free(tmp);
   
#else
   
   if(format >= DDS_COMPRESS_BC4)
   {
      size = get_mipmapped_size(width, height, 4, 0, mipmaps, DDS_COMPRESS_NONE);
      tmp = g_malloc(size);
      
      if(GLEW_SGIS_generate_mipmap)
      {
         glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
                         mipmaps > 1 ? GL_TRUE : GL_FALSE);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, 
                         type, GL_UNSIGNED_BYTE, src);
         
         offset = 0;
         for(i = 0; i < mipmaps; ++i)
         {
            glGetTexImage(GL_TEXTURE_2D, i, GL_RGBA, GL_UNSIGNED_BYTE,
                          tmp + offset);
            offset += get_mipmapped_size(width, height, 4, i, 1,
                                         DDS_COMPRESS_NONE);
         }
      }
      else
         generate_mipmaps_software(tmp, src, width, height, bpp, 0, mipmaps);
      
      compress_3dc(dst, tmp, width, height, 4, mipmaps, format);
      
      g_free(tmp);
      
      return(1);
   }
   
   if(GLEW_SGIS_generate_mipmap)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP_SGIS,
                      mipmaps > 1 ? GL_TRUE : GL_FALSE);
      glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, 
                   type, GL_UNSIGNED_BYTE, src);
   }
   else
   {
      size = get_mipmapped_size(width, height, bpp, 0, mipmaps,
                                DDS_COMPRESS_NONE);
      tmp = g_malloc(size);
      generate_mipmaps_software(tmp, src, width, height, bpp, 0, mipmaps);
      
      offset = 0;
      w = width;
      h = height;
      
      for(i = 0; i < mipmaps; ++i)
      {
         glTexImage2D(GL_TEXTURE_2D, i, internal, w, h, 0, type,
                      GL_UNSIGNED_BYTE, tmp + offset);
         offset += (w * h * bpp);
         if(w > 1) w >>= 1;
         if(h > 1) h >>= 1;
      }
      
      g_free(tmp);
   }
   
   glGetCompressedTexImageARB(GL_TEXTURE_2D, 0, dst);
   
   if(mipmaps > 1)
   {
      offset = 0;
      for(i = 1; i < mipmaps; ++i)
      {
         glGetTexLevelParameteriv(GL_TEXTURE_2D, i - 1, 
                                  GL_TEXTURE_COMPRESSED_IMAGE_SIZE_ARB, &size);
         offset += size;
         glGetCompressedTexImageARB(GL_TEXTURE_2D, i, dst + offset);
      }
   }
   
#endif // #ifdef USE_SOFTWARE_COMPRESSION   
   
   return(1);
}

#ifdef USE_SOFTWARE_COMPRESSION

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
      bits = GETL16(&src[2 * y]);
      for(x = 0; x < w; ++x)
      {
         d[0] = (bits & 0x0f) * 17;
         bits >>= 4;
         d += 4;
      }
   }
}

#endif // #ifdef USE_SOFTWARE_COMPRESSION

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
#ifdef USE_SOFTWARE_COMPRESSION

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
   
#else // #ifdef USE_SOFTWARE_COMPRESSION

   GLenum internal = GL_NONE, type = GL_RGB;
   unsigned char *d, *s;
   unsigned int x, y, sx, sy;

   if(!(IS_POT(width) && IS_POT(height)))
      return(0);
   
   if(format >= DDS_COMPRESS_BC4) /* 3Dc */
   {
      sx = (width  < 4) ? width  : 4;
      sy = (height < 4) ? height : 4;
   
      s = src;

      for(y = 0; y < height; y += 4)
      {
         for(x = 0; x < width; x += 4)
         {
            d = dst + (y * width + x) * bpp;
            if(format == DDS_COMPRESS_BC4)
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
	      }
      }
      return(1);
   }
   
   switch(bpp)
   {
      case 1: type = GL_LUMINANCE;       break;
      case 2: type = GL_LUMINANCE_ALPHA; break;
      case 3: type = GL_RGB;             break;
      case 4: type = GL_RGBA;            break;
   }
   
   switch(format)
   {
      case DDS_COMPRESS_BC1:
         internal = (bpp == 4) ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT :
                                 GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
         break;
      case DDS_COMPRESS_BC2:
         internal = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
         break;
      case DDS_COMPRESS_BC3:
      case DDS_COMPRESS_BC3N:
         internal = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
         break;
   }
   
   glCompressedTexImage2DARB(GL_TEXTURE_2D, 0, internal, width, height, 0, size, src);
   glGetTexImage(GL_TEXTURE_2D, 0, type, GL_UNSIGNED_BYTE, dst);
   
#endif // #ifdef USE_SOFTWARE_COMPRESSION
   
   return(1);
}
