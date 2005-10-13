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

/*
** !!! COPYRIGHT NOTICE !!!
**
** The following is based on code (C) 2003 Arne Reuter <homepage@arnereuter.de>
** URL: http://www.dr-reuter.de/arne/dds.html
**
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "ddsplugin.h"
#include "dds.h"
#include "dxt.h"

typedef struct
{
   unsigned char rshift, gshift, bshift, ashift;
   unsigned char rbits, gbits, bbits, abits;
   unsigned int rmask, gmask, bmask, amask;
   unsigned int bpp, gimp_bpp;
   int tile_height;
} dds_load_info_t;

static int read_header(dds_header_t *hdr, FILE *fp);
static int validate_header(dds_header_t *hdr);
static int load_layer(FILE *fp, dds_header_t *hdr, dds_load_info_t *d,
                      gint32 image, unsigned int level, char *prefix,
                      unsigned int *l, guchar *pixels, unsigned char *buf);
static int load_mipmaps(FILE *fp, dds_header_t *hdr, dds_load_info_t *d,
                        gint32 image, char *prefix, unsigned int *l,
                        guchar *pixels, unsigned char *buf);
static int load_face(FILE *fp, dds_header_t *hdr, dds_load_info_t *d,
                     gint32 image, char *prefix, unsigned int *l,
                     guchar *pixels, unsigned char *buf);
static unsigned char color_bits(unsigned int mask);
static unsigned char color_shift(unsigned int mask);

gint32 read_dds(gchar *filename)
{
   gint32 image = 0;
   unsigned char *buf;
   unsigned int l = 0;
   guchar *pixels;
   gchar *tmp;
   FILE *fp;
   dds_header_t hdr;
   dds_load_info_t d;
   gint *layers, layer_count;
   GimpImageBaseType type;
   
   fp = fopen(filename, "rb");
   if(fp == 0)
   {
      g_message("Error opening file.\n");
      return(-1);
   }
   
   if(interactive_dds)
   {
      if(strrchr(filename, '/'))
         tmp = g_strdup_printf("Loading %s:", strrchr(filename, '/') + 1);
      else
         tmp = g_strdup_printf("Loading %s:", filename);
      gimp_progress_init(tmp);
      g_free(tmp);
   }
   
   read_header(&hdr, fp);
   if(!validate_header(&hdr))
   {
      fclose(fp);
      return(-1);
   }
   
   /* a lot of DDS images out there don't have this for some reason -_- */
   if(hdr.pitch_or_linsize == 0)
   {
      if(hdr.pixelfmt.flags & DDPF_FOURCC) /* assume linear size */
      {
         hdr.pitch_or_linsize = ((hdr.width + 3) >> 2) * ((hdr.height + 3) >> 2);
         if(hdr.pixelfmt.fourcc[3] == '1')
            hdr.pitch_or_linsize *= 8;
         else
            hdr.pitch_or_linsize *= 16;
      }
      else /* assume pitch */
      {
         hdr.pitch_or_linsize = hdr.height * hdr.width *
            (hdr.pixelfmt.bpp >> 3);
      }
   }
   
   if(hdr.pixelfmt.flags & DDPF_FOURCC)
      hdr.pixelfmt.flags |= DDPF_ALPHAPIXELS;
   
   if(hdr.pixelfmt.flags & DDPF_FOURCC)
   {
      d.bpp = d.gimp_bpp = 4;
      type = GIMP_RGB;
   }
   else
   {
      d.bpp = hdr.pixelfmt.bpp >> 3;
      //d.gimp_bpp = (hdr.pixelfmt.flags & DDPF_ALPHAPIXELS) ? 4 : 3;
      if(d.bpp == 2)
      {
         if(hdr.pixelfmt.amask == 0xf000) // RGBA4
         {
            d.gimp_bpp = 4;
            type = GIMP_RGB;
         }
         else if(hdr.pixelfmt.amask == 0xff00) //L8A8
         {
            d.gimp_bpp = 2;
            type = GIMP_GRAY;
         }
         else if(hdr.pixelfmt.bmask == 0x1f) //R5G6B5 or RGB5A1
         {
            if(hdr.pixelfmt.amask == 0x8000) // RGB5A1
               d.gimp_bpp = 4;
            else
               d.gimp_bpp = 3;
            
            type = GIMP_RGB;
         }
         else //L16
         {
            d.gimp_bpp = 1;
            type = GIMP_GRAY;
         }
      }
      else
      {
         d.gimp_bpp = d.bpp;
         type = (d.bpp == 1) ? GIMP_GRAY : GIMP_RGB;
      }
   }
   
   image = gimp_image_new(hdr.width, hdr.height, type);
   
   if(image == -1)
   {
      g_message("Can't allocate new image.\n");
      fclose(fp);
      return(-1);
   }
   
   gimp_image_set_filename(image, filename);
   
   d.tile_height = gimp_tile_height();
   
   pixels = g_new(guchar, d.tile_height * hdr.width * d.gimp_bpp);
   buf = g_malloc(hdr.pitch_or_linsize);
   
   d.rshift = color_shift(hdr.pixelfmt.rmask);
   d.gshift = color_shift(hdr.pixelfmt.gmask);
   d.bshift = color_shift(hdr.pixelfmt.bmask);
   d.ashift = color_shift(hdr.pixelfmt.amask);
   d.rbits = color_bits(hdr.pixelfmt.rmask);
   d.gbits = color_bits(hdr.pixelfmt.gmask);
   d.bbits = color_bits(hdr.pixelfmt.bmask);
   d.abits = color_bits(hdr.pixelfmt.amask);
   d.rmask = hdr.pixelfmt.rmask >> d.rshift << (8 - d.rbits);
   d.gmask = hdr.pixelfmt.gmask >> d.gshift << (8 - d.gbits);
   d.bmask = hdr.pixelfmt.bmask >> d.bshift << (8 - d.bbits);
   d.amask = hdr.pixelfmt.amask >> d.ashift << (8 - d.abits);
   
   if(!(hdr.caps.caps2 & DDSCAPS2_CUBEMAP) &&
      !(hdr.caps.caps2 & DDSCAPS2_VOLUME))
   {
      if(!load_layer(fp, &hdr, &d, image, 0, "", &l, pixels, buf))
      {
         fclose(fp);
         return(-1);
      }
   }

   if(hdr.caps.caps1 & DDSCAPS_COMPLEX)
   {
      if(hdr.caps.caps2 & DDSCAPS2_CUBEMAP)
      {
         if((hdr.caps.caps2 & DDSCAPS2_CUBEMAP_POSITIVEX) &&
            !load_face(fp, &hdr, &d, image, "(positive x)", &l, pixels, buf))
         {
            fclose(fp);
            return(-1);
         }
         if((hdr.caps.caps2 & DDSCAPS2_CUBEMAP_NEGATIVEX) &&
            !load_face(fp, &hdr, &d, image, "(negative x)", &l, pixels, buf))
         {
            fclose(fp);
            return(-1);
         }
         if((hdr.caps.caps2 & DDSCAPS2_CUBEMAP_POSITIVEY) &&
            !load_face(fp, &hdr, &d, image, "(positive y)", &l, pixels, buf))
         {
            fclose(fp);
            return(-1);
         }
         if((hdr.caps.caps2 & DDSCAPS2_CUBEMAP_NEGATIVEY) &&
            !load_face(fp, &hdr, &d, image, "(negative y)", &l, pixels, buf))
         {
            fclose(fp);
            return(-1);
         }
         if((hdr.caps.caps2 & DDSCAPS2_CUBEMAP_POSITIVEZ) &&
            !load_face(fp, &hdr, &d, image, "(positive z)", &l, pixels, buf))
         {
            fclose(fp);
            return(-1);
         }
         if((hdr.caps.caps2 & DDSCAPS2_CUBEMAP_NEGATIVEZ) &&
            !load_face(fp, &hdr, &d, image, "(negative z)", &l, pixels, buf))
         {
            fclose(fp);
            return(-1);
         }
      }
      else if((hdr.caps.caps2 & DDSCAPS2_VOLUME) &&
              (hdr.flags & DDSD_DEPTH))
      {
         unsigned int i, level;
         char *plane;
         for(i = 0; i < hdr.depth; ++i)
         {
            plane = g_strdup_printf("(z = %d)", i);
            if(!load_layer(fp, &hdr, &d, image, 0, plane, &l, pixels, buf))
            {
               g_free(plane);
               fclose(fp);
               return(-1);
            }
            g_free(plane);
         }
      
         if((hdr.flags & DDSD_MIPMAPCOUNT) &&
            (hdr.caps.caps1 & DDSCAPS_MIPMAP))
         {
            for(level = 1; level < hdr.num_mipmaps; ++level)
            {
               int n = hdr.depth >> level;
               if(n < 1) n = 1;
               for(i = 0; i < n; ++i)
               {
                  plane = g_strdup_printf("(z = %d)", i);
                  if(!load_layer(fp, &hdr, &d, image, level, plane, &l, pixels, buf))
                  {
                     g_free(plane);
                     fclose(fp);
                     return(-1);
                  }
                  g_free(plane);
               }
            }
         }
      }
      else if(!load_mipmaps(fp, &hdr, &d, image, "", &l, pixels, buf))
      {
         fclose(fp);
         return(-1);
      }
   }
   
   g_free(buf);
   g_free(pixels);
   fclose(fp);
   
   layers = gimp_image_get_layers(image, &layer_count);
   gimp_image_set_active_layer(image, layers[0]);
               
   return(image);
}

static int read_header(dds_header_t *hdr, FILE *fp)
{
   unsigned char buf[DDS_HEADERSIZE];
   
   memset(hdr, 0, sizeof(dds_header_t));
   
   if(fread(buf, 1, DDS_HEADERSIZE, fp) != DDS_HEADERSIZE)
      return(0);
   
   hdr->magic[0] = buf[0];
   hdr->magic[1] = buf[1];
   hdr->magic[2] = buf[2];
   hdr->magic[3] = buf[3];
   
   hdr->size = GET32(buf + 4);
   hdr->flags = GET32(buf + 8);
   hdr->height = GET32(buf + 12);
   hdr->width = GET32(buf + 16);
   hdr->pitch_or_linsize = GET32(buf + 20);
   hdr->depth = GET32(buf + 24);
   hdr->num_mipmaps = GET32(buf + 28);
   
   hdr->pixelfmt.size = GET32(buf + 76);
   hdr->pixelfmt.flags = GET32(buf + 80);
   hdr->pixelfmt.fourcc[0] = buf[84];
   hdr->pixelfmt.fourcc[1] = buf[85];
   hdr->pixelfmt.fourcc[2] = buf[86];
   hdr->pixelfmt.fourcc[3] = buf[87];
   hdr->pixelfmt.bpp = GET32(buf + 88);
   hdr->pixelfmt.rmask = GET32(buf + 92);
   hdr->pixelfmt.gmask = GET32(buf + 96);
   hdr->pixelfmt.bmask = GET32(buf + 100);
   hdr->pixelfmt.amask = GET32(buf + 104);
   
   hdr->caps.caps1 = GET32(buf + 108);
   hdr->caps.caps2 = GET32(buf + 112);
   
   return(1);
}

static int validate_header(dds_header_t *hdr)
{
   if(memcmp(hdr->magic, "DDS ", 4))
   {
      g_message("Invalid DDS file.\n");
      return(0);
   }
   
   if((hdr->flags & DDSD_PITCH) == (hdr->flags & DDSD_LINEARSIZE))
   {
      g_message("Warning: DDSD_PITCH or DDSD_LINEARSIZE is not set.\n");
      if(hdr->pixelfmt.flags & DDPF_FOURCC)
         hdr->flags |= DDSD_LINEARSIZE;
      else
         hdr->flags |= DDSD_PITCH;
   }
/*   
   if((hdr->pixelfmt.flags & DDPF_FOURCC) ==
      (hdr->pixelfmt.flags & DDPF_RGB))
   {
      g_message("Invalid pixel format.\n");
      return(0);
   }
*/   
   if((hdr->pixelfmt.flags & DDPF_FOURCC) &&
      memcmp(hdr->pixelfmt.fourcc, "DXT1", 4) &&
      memcmp(hdr->pixelfmt.fourcc, "DXT3", 4) &&
      memcmp(hdr->pixelfmt.fourcc, "DXT5", 4))
   {
      g_message("Invalid compression format.\n"
                "Only DXT1, DXT3 and DXT5 formats are supported.\n");
      return(0);
   }
   
   if(hdr->pixelfmt.flags & DDPF_RGB)
   {
      if((hdr->pixelfmt.bpp != 16) &&
         (hdr->pixelfmt.bpp != 24) &&
         (hdr->pixelfmt.bpp != 32))
      {
         g_message("Invalid BPP.\n");
         return(0);
      }
   }
   else if(hdr->pixelfmt.flags & DDPF_LUMINANCE)
   {
      if((hdr->pixelfmt.bpp !=  8) &&
         (hdr->pixelfmt.bpp != 16))
      {
         g_message("Invalid BPP.\n");
         return(0);
      }
   }
   
   if(!(hdr->pixelfmt.flags & DDPF_RGB) &&
      !(hdr->pixelfmt.flags & DDPF_FOURCC))
   {
      switch(GET32(hdr->pixelfmt.fourcc))
      {
         case CHAR32('D', 'X', 'T', '1'):
         case CHAR32('D', 'X', 'T', '3'):
         case CHAR32('D', 'X', 'T', '5'):
            hdr->pixelfmt.flags |= DDPF_FOURCC;
            break;
         default:
            switch(hdr->pixelfmt.bpp)
            {
               case 8:
               case 16:
               case 24:
               case 32:
                  hdr->pixelfmt.flags |= DDPF_RGB;
                  break;
               default:
                  g_message("Invalid pixel format.");
                  return(0);
            }
            break;
      }
   }
   
   return(1);
}

static int load_layer(FILE *fp, dds_header_t *hdr, dds_load_info_t *d,
                      gint32 image, unsigned int level, char *prefix,
                      unsigned int *l, guchar *pixels, unsigned char *buf)
{
   GimpDrawable *drawable;
   GimpPixelRgn pixel_region;
   GimpImageType type = GIMP_RGBA_IMAGE;
   gchar *layer_name;
   gint x, y, z, n;
   gint32 layer;
   unsigned int width = hdr->width >> level;
   unsigned int height = hdr->height >> level;
   unsigned int size = hdr->pitch_or_linsize >> (2 * level);
   int format = DDS_COMPRESS_NONE;

   if(width < 1) width = 1;
   if(height < 1) height = 1;
   
   switch(d->bpp)
   {
      case 1: type = GIMP_GRAY_IMAGE;  break;
      case 2:
         if(hdr->pixelfmt.amask == 0xf000) //RGBA4
            type = GIMP_RGBA_IMAGE;
         else if(hdr->pixelfmt.amask == 0xff00) //L8A8
            type = GIMP_GRAYA_IMAGE;
         else if(hdr->pixelfmt.bmask == 0x1f) //R5G6B5 or RGB5A1
            type = (hdr->pixelfmt.amask == 0x8000) ? GIMP_RGBA_IMAGE : GIMP_RGB_IMAGE;
         else //L16
            type = GIMP_GRAY_IMAGE;
         break;
      case 3: type = GIMP_RGB_IMAGE;   break;
      case 4: type = GIMP_RGBA_IMAGE;  break;
   }
   
   layer_name = (level) ? g_strdup_printf("mipmap %d %s", level, prefix) :
                          g_strdup_printf("main surface %s", prefix);
   
   layer = gimp_layer_new(image, layer_name, width, height, type, 100,
                          GIMP_NORMAL_MODE);
   g_free(layer_name);
   
   gimp_image_add_layer(image, layer, *l);
   if((*l)++) gimp_drawable_set_visible(layer, FALSE);
   
   drawable = gimp_drawable_get(layer);
   
   gimp_pixel_rgn_init(&pixel_region, drawable, 0, 0, drawable->width,
                       drawable->height, TRUE, FALSE);
   
   if(hdr->pixelfmt.flags & DDPF_FOURCC)
   {
      unsigned int w = width >> 2;
      unsigned int h = height >> 2;
      
      switch(hdr->pixelfmt.fourcc[3])
      {
         case '1': format = DDS_COMPRESS_DXT1; break;
         case '3': format = DDS_COMPRESS_DXT3; break;
         case '5': format = DDS_COMPRESS_DXT5; break;
      }
      
      if(w == 0) w = 1;
      if(h == 0) h = 1;
      size = w * h * (format == DDS_COMPRESS_DXT1 ? 8 : 16);
   }
   
   if((hdr->flags & DDSD_LINEARSIZE) &&
      !fread(buf, size, 1, fp))
   {
      g_message("Unexpected EOF.\n");
      return(0);
   }
   
   if(hdr->pixelfmt.flags & DDPF_RGB)
   {
      z = 0;
      for(y = 0, n = 0; y < height; ++y, ++n)
      {
         if(n >= d->tile_height)
         {
            gimp_pixel_rgn_set_rect(&pixel_region, pixels, 0, y - n,
                                    drawable->width, n);
            n = 0;
            if(interactive_dds)
               gimp_progress_update((double)y / (double)hdr->height);
         }

         if((hdr->flags & DDSD_PITCH) &&
            !fread(buf, width * d->bpp, 1, fp))
         {
            g_message("Unexpected EOF.\n");
            return(0);
         }
         
         if(!(hdr->flags & DDSD_LINEARSIZE)) z = 0;
         
         for(x = 0; x < drawable->width; ++x)
         {
            unsigned int pixel = buf[z];
            unsigned int pos = (n * drawable->width + x) * d->gimp_bpp;
            
            if(d->bpp > 1) pixel += ((unsigned int)buf[z + 1] <<  8);
            if(d->bpp > 2) pixel += ((unsigned int)buf[z + 2] << 16);
            if(d->bpp > 3) pixel += ((unsigned int)buf[z + 3] << 24);

            if(d->bpp >= 3)
            {
               if(hdr->pixelfmt.amask == 0xc0000000) // handle RGB10A2
               {
                  pixels[pos + 0] = (pixel >> d->rshift) >> 2;
                  pixels[pos + 1] = (pixel >> d->gshift) >> 2;
                  pixels[pos + 2] = (pixel >> d->bshift) >> 2;
                  if(hdr->pixelfmt.flags & DDPF_ALPHAPIXELS)
                     pixels[pos + 3] = (pixel >> d->ashift << (8 - d->abits) & d->amask) * 255 / d->amask;
               }
               else
               {
                  pixels[pos] =
                     (pixel >> d->rshift << (8 - d->rbits) & d->rmask) * 255 / d->rmask;
                  pixels[pos + 1] =
                     (pixel >> d->gshift << (8 - d->gbits) & d->gmask) * 255 / d->gmask;
                  pixels[pos + 2] =
                     (pixel >> d->bshift << (8 - d->bbits) & d->bmask) * 255 / d->bmask;
                  if(hdr->pixelfmt.flags & DDPF_ALPHAPIXELS)
                  {
                     pixels[pos + 3] =
                        (pixel >> d->ashift << (8 - d->abits) & d->amask) * 255 / d->amask;
                  }
               }
            }
            else if(d->bpp == 2)
            {
               if(hdr->pixelfmt.amask == 0xf000) //RGBA4
               {
                  pixels[pos] =
                     (pixel >> d->rshift << (8 - d->rbits) & d->rmask) * 255 / d->rmask;
                  pixels[pos + 1] =
                     (pixel >> d->gshift << (8 - d->gbits) & d->gmask) * 255 / d->gmask;
                  pixels[pos + 2] =
                     (pixel >> d->bshift << (8 - d->bbits) & d->bmask) * 255 / d->bmask;
                  pixels[pos + 3] =
                     (pixel >> d->ashift << (8 - d->abits) & d->amask) * 255 / d->amask;
               }
               else if(hdr->pixelfmt.amask == 0xff00) //L8A8
               {
                  pixels[pos] =
                     (pixel >> d->rshift << (8 - d->rbits) & d->rmask) * 255 / d->rmask;
                  pixels[pos + 1] =
                     (pixel >> d->ashift << (8 - d->abits) & d->amask) * 255 / d->amask;
               }
               else if(hdr->pixelfmt.bmask == 0x1f) //R5G6B5 or RGB5A1
               {
                  pixels[pos] =
                     (pixel >> d->rshift << (8 - d->rbits) & d->rmask) * 255 / d->rmask;
                  pixels[pos + 1] =
                     (pixel >> d->gshift << (8 - d->gbits) & d->gmask) * 255 / d->gmask;
                  pixels[pos + 2] =
                     (pixel >> d->bshift << (8 - d->bbits) & d->bmask) * 255 / d->bmask;
                  if(hdr->pixelfmt.amask == 0x8000)
                  {
                     pixels[pos + 3] =
                         (pixel >> d->ashift << (8 - d->abits) & d->amask) * 255 / d->amask;
                  }
               }
               else //L16
                  pixels[pos] = (unsigned char)(255 * ((float)(pixel & 0xffff) / 65535.0f));
            }
            else
            {
               pixels[pos] =
                  (pixel >> d->rshift << (8 - d->rbits) & d->rmask) * 255 / d->rmask;
            }
            
            z += d->bpp;
         }
      }
      
      gimp_pixel_rgn_set_rect(&pixel_region, pixels, 0, y - n,
                              drawable->width, n);
   }
   else if(hdr->pixelfmt.flags & DDPF_FOURCC)
   {
      unsigned char *dst;
      
      dst = g_malloc(width * height * 16);
      if(!(hdr->flags & DDSD_LINEARSIZE))
      {
         g_message("Image marked as compressed, but DDSD_LINEARSIZE is not set.\n");
         g_free(dst);
         return(0);
      }
      
      dxt_decompress(dst, buf, format, size, width, height, d->gimp_bpp);
      
      z = 0;
      for(y = 0, n = 0; y < height; ++y, ++n)
      {
         if(n >= d->tile_height)
         {
            gimp_pixel_rgn_set_rect(&pixel_region, pixels, 0, y - n,
                                    drawable->width, n);
            n = 0;
            if(interactive_dds)
               gimp_progress_update((double)y / (double)hdr->height);
         }
         
         memcpy(pixels + n * drawable->width * d->gimp_bpp,
                dst + y * drawable->width * d->gimp_bpp,
                width * d->gimp_bpp);
      }
      
      gimp_pixel_rgn_set_rect(&pixel_region, pixels, 0, y - n,
                              drawable->width, n);
      
      g_free(dst);
   }
   
   gimp_drawable_flush(drawable);
   gimp_drawable_detach(drawable);
   
   return(1);
}

static int load_mipmaps(FILE *fp, dds_header_t *hdr, dds_load_info_t *d,
                        gint32 image, char *prefix, unsigned int *l,
                        guchar *pixels, unsigned char *buf)
{
   unsigned int level;
   
   if((hdr->flags & DDSD_MIPMAPCOUNT) &&
      (hdr->caps.caps1 & DDSCAPS_MIPMAP))
   {
      for(level = 1; level < hdr->num_mipmaps; ++level)
      {
         if(!load_layer(fp, hdr, d, image, level, prefix, l, pixels, buf))
            return(0);
      }
   }
   return(1);
}

static int load_face(FILE *fp, dds_header_t *hdr, dds_load_info_t *d,
                     gint32 image, char *prefix, unsigned int *l,
                     guchar *pixels, unsigned char *buf)
{
   if(!load_layer(fp, hdr, d, image, 0, prefix, l, pixels, buf))
      return(0);
   return(load_mipmaps(fp, hdr, d, image, prefix, l, pixels, buf));
}

static unsigned char color_bits(unsigned int mask)
{
   unsigned char i = 0;
   
   while(mask)
   {
      if(mask & 1) ++i;
      mask >>= 1;
   }
   return(i);
}

static unsigned char color_shift(unsigned int mask)
{
   unsigned char i = 0;
   
   if(!mask) return(0);
   while(!((mask >> i) & 1)) ++i;
   return(i);
}
