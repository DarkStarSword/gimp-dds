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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>

#include "ddsplugin.h"
#include "dds.h"
#include "dxt.h"
#include "mipmap.h"
#include "endian.h"

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static gint save_dialog(gint32 image_id, gint32 drawable);
static void save_dialog_response(GtkWidget *widget, gint response_id, gpointer data);
static void compression_selected(GtkWidget *widget, gpointer data);
static void toggle_clicked(GtkWidget *widget, gpointer data);
static int write_image(FILE *fp, gint32 image_id, gint32 drawable_id);

static int runme = 0;

const char *cubemap_face_names[4][6] =
{
   {
      "positive x", "negative x",
      "positive y", "negative y",
      "positive z", "negative z"
   },
   {
      "pos x", "neg x",
      "pos y", "neg y",
      "pos z", "neg z",
   },
   {
      "+x", "-x",
      "+y", "-y",
      "+z", "-z"
   },
   {
      "right", "left",
      "top", "bottom",
      "back", "front"
   }
};

static gint cubemap_faces[6];
static gint is_cubemap = 0;
static gint is_volume = 0;

GtkWidget *mipmap_check;
GtkWidget *compress_opt;
GtkWidget *compress_menu;
GtkWidget *format_opt;
GtkWidget *color_type_opt;
GtkWidget *dither_chk;

static struct
{
   int compression;
   char *string;
} compression_strings[] =
{
   {DDS_COMPRESS_NONE,   "None"},
   {DDS_COMPRESS_BC1,    "BC1 / DXT1"},
   {DDS_COMPRESS_BC2,    "BC2 / DXT3"},
   {DDS_COMPRESS_BC3,    "BC3 / DXT5"},
   {DDS_COMPRESS_BC3N,   "BC3n / DXT5n"},
   {DDS_COMPRESS_BC4,    "BC4 / ATI1"},
   {DDS_COMPRESS_BC5,    "BC5 / ATI2"},
   {DDS_COMPRESS_AEXP,   "Alpha Exponent (DXT5)"},
   {DDS_COMPRESS_YCOCG,  "YCoCg (DXT5)"},
   {DDS_COMPRESS_YCOCGS, "YCoCg scaled (DXT5)"},
   {-1, 0}
};

static struct
{
   int format;
   char *string;
} format_strings[] =
{
   {DDS_FORMAT_DEFAULT, "Default"},
   {DDS_FORMAT_RGB8, "RGB8"},
   {DDS_FORMAT_RGBA8, "RGBA8"},
   {DDS_FORMAT_BGR8, "BGR8"},
   {DDS_FORMAT_ABGR8, "ABGR8"},
   {DDS_FORMAT_R5G6B5, "R5G6B5"},
   {DDS_FORMAT_RGBA4, "RGBA4"},
   {DDS_FORMAT_RGB5A1, "RGB5A1"},
   {DDS_FORMAT_RGB10A2, "RGB10A2"},
   {DDS_FORMAT_R3G3B2, "R3G3B2"},
   {DDS_FORMAT_A8, "A8"},
   {DDS_FORMAT_L8, "L8"},
   {DDS_FORMAT_L8A8, "L8A8"},
   {DDS_FORMAT_YCOCG, "YCoCg"},
   {-1, 0}
};

static struct
{
   int type;
   char *string;
} color_type_strings[] =
{
   {DDS_COLOR_DEFAULT,    "Default"},
   {DDS_COLOR_DISTANCE,   "Distance"},
   {DDS_COLOR_LUMINANCE,  "Luminance"},
   {DDS_COLOR_INSET_BBOX, "Inset bounding box"},
   {-1, 0}
};

static int check_cubemap(gint32 image_id)
{
   gint *layers, num_layers;
   int cubemap = 0, i, j, k, w, h;
   char *layer_name;
   GimpDrawable *drawable;
   GimpImageType type;

   layers = gimp_image_get_layers(image_id, &num_layers);
   
   if(num_layers == 6)
   {
      for(i = 0; i < 6; ++i)
         cubemap_faces[i] = -1;
      
      for(i = 0; i < 6; ++i)
      {
         layer_name = (char*)gimp_drawable_get_name(layers[i]);
         for(j = 0; j < 6; ++j)
         {
            for(k = 0; k < 4; ++k)
            {
               if(strstr(layer_name, cubemap_face_names[k][j]))
               {
                  if(cubemap_faces[j] == -1)
                  {
                     cubemap_faces[j] = layers[i];
                     break;
                  }
               }
            }
         }
      }
      
      cubemap = 1;
      
      /* check for 6 valid faces */
      for(i = 0; i < 6; ++i)
      {
         if(cubemap_faces[i] == -1)
         {
            cubemap = 0;
            break;
         }
      }
      
      /* make sure they are all the same size */
      if(cubemap)
      {
         drawable = gimp_drawable_get(cubemap_faces[0]);
         w = drawable->width;
         h = drawable->height;
         gimp_drawable_detach(drawable);
         for(i = 1; i < 6 && cubemap; ++i)
         {
            drawable = gimp_drawable_get(cubemap_faces[i]);
            if(drawable->width  != w ||
               drawable->height != h)
            {
               cubemap = 0;
            }
            gimp_drawable_detach(drawable);
         }
         
         if(cubemap == 0)
         {
            g_message("DDS: It appears that your image is a cube map,\n"
                      "but not all layers are the same size, thus a cube\n"
                      "map cannot be written.");
         }
      }
      
      /* make sure they are all the same type */
      if(cubemap)
      {
         type = gimp_drawable_type(cubemap_faces[0]);
         for(i = 1; i < 6; ++i)
         {
            if(gimp_drawable_type(cubemap_faces[i]) != type)
            {
               cubemap = 0;
               break;
            }
         }
         
         if(cubemap == 0)
         {
            g_message("DDS: It appears that your image is a cube map,\n"
                      "but not all layers are the same type, thus a cube\n"
                      "map cannot be written (Perhaps some layers have\n"
                      "transparency and others do not?).");
         }
      }
   }
   
   return(cubemap);
}

static int check_volume(gint32 image_id)
{
   gint *layers, num_layers;
   int volume = 0, i, w, h;
   GimpDrawable *drawable;
   GimpImageType type;
   
   layers = gimp_image_get_layers(image_id, &num_layers);
   
   if(num_layers > 1)
   {
      volume = 1;
      
      drawable = gimp_drawable_get(layers[0]);
      w = drawable->width;
      h = drawable->height;
      gimp_drawable_detach(drawable);
      for(i = 1; i < num_layers && volume; ++i)
      {
         drawable = gimp_drawable_get(layers[i]);
         if(drawable->width  != w ||
            drawable->height != h)
         {
            volume = 0;
         }
         gimp_drawable_detach(drawable);
      }
      
      if(!volume)
      {
         g_message("DDS: It appears your image may be a volume map,\n"
                   "but not all layers are the same size, thus a volume\n"
                   "map cannot be written.");
      }
   
      if(volume)
      {
         type = gimp_drawable_type(layers[0]);
         for(i = 1; i < num_layers; ++i)
         {
            if(gimp_drawable_type(layers[i]) != type)
            {
               volume = 0;
               break;
            }
         }
         
         if(!volume)
         {
            g_message("DDS: It appears your image may be a volume map,\n"
                      "but not all layers are the same type, thus a volume\n"
                      "map cannot be written (Perhaps some layers have\n"
                      "transparency and others do not?).");
         }
      }
   }

   return(volume);
}

GimpPDBStatusType write_dds(gchar *filename, gint32 image_id, gint32 drawable_id)
{
   FILE *fp;
   gchar *tmp;
   int rc = 0;
   
   is_cubemap = check_cubemap(image_id);
   is_volume = check_volume(image_id);
   
   if(interactive_dds)
   {
      if(!save_dialog(image_id, drawable_id))
         return(GIMP_PDB_CANCEL);
   }
   else
   {
      if(ddsvals.savetype == DDS_SAVE_CUBEMAP && !is_cubemap)
      {
         g_message("DDS: Cannot save image as cube map");
         return(GIMP_PDB_EXECUTION_ERROR);
      }
      
      if(ddsvals.savetype == DDS_SAVE_VOLUMEMAP && !is_volume)
      {
         g_message("DDS: Cannot save image as volume map");
         return(GIMP_PDB_EXECUTION_ERROR);
      }
      
      if(ddsvals.savetype == DDS_SAVE_VOLUMEMAP &&
         ddsvals.compression != DDS_COMPRESS_NONE)
      {
         g_message("DDS: Cannot save volume map with compression");
         return(GIMP_PDB_EXECUTION_ERROR);
      }
   }
   
   fp = fopen(filename, "wb");
   if(fp == 0)
   {
      g_message("Error opening %s", filename);
      return(GIMP_PDB_EXECUTION_ERROR);
   }
   
   if(interactive_dds)
   {
      if(strrchr(filename, '/'))
         tmp = g_strdup_printf("Saving %s:", strrchr(filename, '/') + 1);
      else
         tmp = g_strdup_printf("Saving %s:", filename);
      gimp_progress_init(tmp);
      g_free(tmp);
   }
   
   rc = write_image(fp, image_id, drawable_id);
   
   fclose(fp);
   
   return(rc ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR);
}

#define TO_R5G6B5(r, g, b) \
   (unsigned short)((unsigned short)((((r) >> 3) & 0x1f) << 11) |\
                    (unsigned short)((((g) >> 2) & 0x3f) <<  5) |\
                    (unsigned short)((((b) >> 3) & 0x1f)      ))
#define TO_RGBA4(r, g, b, a) \
   (unsigned short)((unsigned short)((((a) >> 4) & 0x0f) << 12) |\
                    (unsigned short)((((r) >> 4) & 0x0f) <<  8) |\
                    (unsigned short)((((g) >> 4) & 0x0f) <<  4) |\
                    (unsigned short)((((b) >> 4) & 0x0f)      ))
#define TO_RGB5A1(r, g, b, a) \
   (unsigned short)((unsigned short)((((a) >> 7) & 0x01) << 15) |\
                    (unsigned short)((((r) >> 3) & 0x1f) << 10) |\
                    (unsigned short)((((g) >> 3) & 0x1f) <<  5) |\
                    (unsigned short)((((b) >> 3) & 0x1f)      ))
#define TO_RGB10A2(r, g, b, a) \
   (unsigned int)((unsigned int)((((a) >> 6) & 0x003) << 30) | \
                  (unsigned int)((((r) << 2) & 0x3ff) << 20) | \
                  (unsigned int)((((g) << 2) & 0x3ff) << 10) | \
                  (unsigned int)((((b) << 2) & 0x3ff)      ))
#define TO_R3G3B2(r, g, b) \
   (unsigned char)(((((r) >> 5) & 0x07) << 5) |\
                   ((((g) >> 5) & 0x07) << 2) |\
                   ((((b) >> 6) & 0x03)     ))

#define TO_YCOCG_Y(r, g, b)  (((  (r) +      ((g) << 1) +  (b)      ) + 2) >> 2)
#define TO_YCOCG_CO(r, g, b) ((( ((r) << 1)             - ((b) << 1)) + 2) >> 2)
#define TO_YCOCG_CG(r, g, b) ((( -(r) +      ((g) << 1) -  (b)      ) + 2) >> 2)

#define TO_LUMINANCE(r, g, b) (((r) * 54 + (g) * 182 + (b) * 20) >> 8)

static void swap_rb(unsigned char *pixels, unsigned int n, int bpp)
{
   unsigned int i;
   unsigned char t;

   for(i = 0; i < n; ++i)
   {
      t = pixels[bpp * i + 0];
      pixels[bpp * i + 0] = pixels[bpp * i + 2];
      pixels[bpp * i + 2] = t;
   }
}

static void alpha_exp(unsigned char *dst, int r, int g, int b, int a)
{
   float ar, ag, ab, aa;
   
   ar = (float)r / 255.0f;
   ag = (float)g / 255.0f;
   ab = (float)b / 255.0f;

   aa = MAX(ar, MAX(ag, ab));
   
   if(aa < 1e-04f)
   {
      dst[0] = b;
      dst[1] = g;
      dst[2] = r;
      dst[3] = a;
      return;
   }
   
   ar /= aa;
   ag /= aa;
   ab /= aa;

   dst[0] = (int)(ab * 255);
   dst[1] = (int)(ag * 255);
   dst[2] = (int)(ar * 255);
   dst[3] = (int)(aa * a);
}

static void convert_pixels(unsigned char *dst, unsigned char *src,
                           int format, int w, int h, int bpp,
                           unsigned char *palette, int mipmaps)
{
   unsigned int i, num_pixels;
   unsigned char r, g, b, a;
   
   num_pixels = get_mipmapped_size(w, h, 1, 0, mipmaps, DDS_COMPRESS_NONE);
   
   for(i = 0; i < num_pixels; ++i)
   {
      if(bpp == 1)
      {
         if(palette)
         {
            r = palette[3 * src[i] + 0];
            g = palette[3 * src[i] + 1];
            b = palette[3 * src[i] + 2];
         }
         else
            r = g = b = src[i];
         
         a = 255;
      }
      else if(bpp == 2)
      {
         r = g = b = src[2 * i];
         a = src[2 * i + 1];
      }
      else if(bpp == 3)
      {
         b = src[3 * i + 0];
         g = src[3 * i + 1];
         r = src[3 * i + 2];
         a = 255;
      }
      else
      {
         b = src[4 * i + 0];
         g = src[4 * i + 1];
         r = src[4 * i + 2];
         a = src[4 * i + 3];
      }
      
      switch(format)
      {
         case DDS_FORMAT_RGB8:
            dst[3 * i + 0] = b;
            dst[3 * i + 1] = g;
            dst[3 * i + 2] = r;
            break;
         case DDS_FORMAT_RGBA8:
            dst[4 * i + 0] = b;
            dst[4 * i + 1] = g;
            dst[4 * i + 2] = r;
            dst[4 * i + 3] = a;
            break;
         case DDS_FORMAT_BGR8:
            dst[4 * i + 0] = r;
            dst[4 * i + 1] = g;
            dst[4 * i + 2] = b;
            dst[4 * i + 3] = 255;
            break;
         case DDS_FORMAT_ABGR8:
            dst[4 * i + 0] = r;
            dst[4 * i + 1] = g;
            dst[4 * i + 2] = b;
            dst[4 * i + 3] = a;
            break;
         case DDS_FORMAT_R5G6B5:
            PUTL16(&dst[2 * i], TO_R5G6B5(r, g, b));
            break;   
         case DDS_FORMAT_RGBA4:
            PUTL16(&dst[2 * i], TO_RGBA4(r, g, b, a));
            break;
         case DDS_FORMAT_RGB5A1:
            PUTL16(&dst[2 * i], TO_RGB5A1(r, g, b, a));
            break;
         case DDS_FORMAT_RGB10A2:
            PUTL32(&dst[4 * i], TO_RGB10A2(r, g, b, a));
            break;
         case DDS_FORMAT_R3G3B2:
            dst[i] = TO_R3G3B2(r, g, b);
            break;
         case DDS_FORMAT_A8:
            dst[i] = a;
            break;
         case DDS_FORMAT_L8:
            dst[i] = TO_LUMINANCE(r, g, b);
            break;
         case DDS_FORMAT_L8A8:
            dst[2 * i + 0] = TO_LUMINANCE(r, g, b);
            dst[2 * i + 1] = a;
            break;
         case DDS_FORMAT_YCOCG:
         {
            int co = TO_YCOCG_CO(r, g, b) + 128;
            int cg = TO_YCOCG_CG(r, g, b) + 128;
            dst[4 * i + 0] = a;
            dst[4 * i + 1] = (cg < 0 ? 0 : (cg > 255 ? 255 : cg));
            dst[4 * i + 2] = (co < 0 ? 0 : (co > 255 ? 255 : co));
            dst[4 * i + 3] = TO_YCOCG_Y(r, g, b);
            break;
         }
         case DDS_FORMAT_AEXP:
            alpha_exp(&dst[4 * i], r, g, b, a);
            break;
         default:
            break;
      }
   }
}

static void convert_volume_pixels(unsigned char *dst, unsigned char *src,
                                  int format, int w, int h, int d, int bpp,
                                  unsigned char *palette, int mipmaps)
{
   unsigned int i, num_pixels;
   unsigned char r, g, b, a;
   
   num_pixels = get_volume_mipmapped_size(w, h, d, 1, 0, mipmaps,
                                          DDS_COMPRESS_NONE);
   
   for(i = 0; i < num_pixels; ++i)
   {
      if(bpp == 1)
      {
         if(palette)
         {
            r = palette[3 * src[i] + 0];
            g = palette[3 * src[i] + 1];
            b = palette[3 * src[i] + 2];
         }
         else
            r = g = b = src[i];
         
         a = 255;
      }
      else if(bpp == 2)
      {
         r = g = b = src[2 * i];
         a = src[2 * i + 1];
      }
      else if(bpp == 3)
      {
         b = src[3 * i + 0];
         g = src[3 * i + 1];
         r = src[3 * i + 2];
         a = 255;
      }
      else
      {
         b = src[4 * i + 0];
         g = src[4 * i + 1];
         r = src[4 * i + 2];
         a = src[4 * i + 3];
      }
      
      switch(format)
      {
         case DDS_FORMAT_RGB8:
            dst[3 * i + 0] = b;
            dst[3 * i + 1] = g;
            dst[3 * i + 2] = r;
            break;
         case DDS_FORMAT_RGBA8:
            dst[4 * i + 0] = b;
            dst[4 * i + 1] = g;
            dst[4 * i + 2] = r;
            dst[4 * i + 3] = a;
            break;
         case DDS_FORMAT_BGR8:
            dst[4 * i + 0] = r;
            dst[4 * i + 1] = g;
            dst[4 * i + 2] = b;
            dst[4 * i + 3] = 255;
            break;
         case DDS_FORMAT_ABGR8:
            dst[4 * i + 0] = r;
            dst[4 * i + 1] = g;
            dst[4 * i + 2] = b;
            dst[4 * i + 3] = a;
            break;
         case DDS_FORMAT_R5G6B5:
            PUTL16(&dst[2 * i], TO_R5G6B5(r, g, b));
            break;   
         case DDS_FORMAT_RGBA4:
            PUTL16(&dst[2 * i], TO_RGBA4(r, g, b, a));
            break;
         case DDS_FORMAT_RGB5A1:
            PUTL16(&dst[2 * i], TO_RGB5A1(r, g, b, a));
            break;
         case DDS_FORMAT_RGB10A2:
            PUTL32(&dst[4 * i], TO_RGB10A2(r, g, b, a));
            break;
         case DDS_FORMAT_R3G3B2:
            dst[i] = TO_R3G3B2(r, g, b);
            break;
         case DDS_FORMAT_A8:
            dst[i] = a;
            break;
         case DDS_FORMAT_L8:
            dst[i] = TO_LUMINANCE(r, g, b);
            break;
         case DDS_FORMAT_L8A8:
            dst[2 * i + 0] = TO_LUMINANCE(r, g, b);
            dst[2 * i + 1] = a;
            break;
         case DDS_FORMAT_YCOCG:
         {
            int co = TO_YCOCG_CO(r, g, b) + 128;
            int cg = TO_YCOCG_CG(r, g, b) + 128;
            dst[4 * i + 0] = a;
            dst[4 * i + 1] = (cg < 0 ? 0 : (cg > 255 ? 255 : cg));
            dst[4 * i + 2] = (co < 0 ? 0 : (co > 255 ? 255 : co));
            dst[4 * i + 3] = TO_YCOCG_Y(r, g, b);
            break;
         }
         case DDS_FORMAT_AEXP:
            alpha_exp(&dst[4 * i], r, g, b, a);
            break;
         default:
            break;
      }
   }
}

static void write_layer(FILE *fp, gint32 image_id, gint32 drawable_id,
                        int w, int h, int bpp, int fmtbpp, int mipmaps)
{
   GimpDrawable *drawable;
   GimpPixelRgn rgn;
   GimpImageType basetype, type;
   unsigned char *src, *dst, *fmtdst, *tmp, c;
   unsigned char *palette = NULL;
   int i, x, y, size, fmtsize, offset, colors;
   int compression = ddsvals.compression;

   basetype = gimp_image_base_type(image_id);
   type = gimp_drawable_type(drawable_id);

   drawable = gimp_drawable_get(drawable_id);
   src = g_malloc(w * h * bpp);
   gimp_pixel_rgn_init(&rgn, drawable, 0, 0, w, h, 0, 0);
   gimp_pixel_rgn_get_rect(&rgn, src, 0, 0, w, h);

   if(basetype == GIMP_INDEXED)
   {
      palette = gimp_image_get_colormap(image_id, &colors);
      
      if(type == GIMP_INDEXEDA_IMAGE)
      {
         tmp = g_malloc(w * h);
         for(i = 0; i < w * h; ++i)
            tmp[i] = src[2 * i];
         g_free(src);
         src = tmp;
         bpp = 1;
      }
   }

   if(bpp >= 3)
      swap_rb(src, w * h, bpp);

   if(compression == DDS_COMPRESS_BC3N)
   {
      if(bpp != 4)
      {
         fmtsize = w * h * 4;
         fmtdst = g_malloc(fmtsize);
         convert_pixels(fmtdst, src, DDS_FORMAT_RGBA8, w, h, bpp,
                        palette, 1);
         g_free(src);
         src = fmtdst;
         bpp = 4;
      }
      
      for(y = 0; y < drawable->height; ++y)
      {
         for(x = 0; x < drawable->width; ++x)
         {
            c = src[y * (drawable->width * 4) + (x * 4) + 2];
            src[y * (drawable->width * 4) + (x * 4) + 2] =
               src[y * (drawable->width * 4) + (x * 4) + 3];
            src[y * (drawable->width * 4) + (x * 4) + 3] = c;
         }
      }
      
      compression = DDS_COMPRESS_BC3;
   }

   if(compression == DDS_COMPRESS_YCOCG ||
      compression == DDS_COMPRESS_YCOCGS) /* convert to YCoCG */
   {
      fmtsize = w * h * 4;
      fmtdst = g_malloc(fmtsize);
      convert_pixels(fmtdst, src, DDS_FORMAT_YCOCG, w, h, bpp,
                     palette, 1);
      g_free(src);
      src = fmtdst;
      bpp = 4;
      
      if(compression == DDS_COMPRESS_YCOCG)
         compression = DDS_COMPRESS_BC3;
   }
   
   if(compression == DDS_COMPRESS_AEXP)
   {
      fmtsize = w * h * 4;
      fmtdst = g_malloc(fmtsize);
      convert_pixels(fmtdst, src, DDS_FORMAT_AEXP, w, h, bpp,
                     palette, 1);
      g_free(src);
      src = fmtdst;
      bpp = 4;
      
      compression = DDS_COMPRESS_BC3;
   }
   
   if(compression == DDS_COMPRESS_NONE)
   {
      if(mipmaps > 1)
      {
         /* pre-convert indexed images to RGB for better quality mipmaps
            if a pixel format conversion is requested */
         if(ddsvals.format > DDS_FORMAT_DEFAULT && basetype == GIMP_INDEXED)
         {
            fmtsize = get_mipmapped_size(w, h, 3, 0, mipmaps, DDS_COMPRESS_NONE);
            fmtdst = g_malloc(fmtsize);
            convert_pixels(fmtdst, src, DDS_FORMAT_RGB8, w, h, bpp,
                           palette, 1);
            g_free(src);
            src = fmtdst;
            bpp = 3;
            palette = NULL;
         }

         size = get_mipmapped_size(w, h, bpp, 0, mipmaps, DDS_COMPRESS_NONE);
         dst = g_malloc(size);
         generate_mipmaps(dst, src, w, h, bpp, palette != NULL, mipmaps);
            
         offset = 0;
         
         if(ddsvals.format > DDS_FORMAT_DEFAULT)
         {
            fmtsize = get_mipmapped_size(w, h, fmtbpp, 0, mipmaps,
                                         DDS_COMPRESS_NONE);
            fmtdst = g_malloc(fmtsize);
            
            convert_pixels(fmtdst, dst, ddsvals.format, w, h, bpp,
                           palette, mipmaps);
            
            g_free(dst);
            dst = fmtdst;
            bpp = fmtbpp;
         }

         for(i = 0; i < mipmaps; ++i)
         {
            size = get_mipmapped_size(w, h, bpp, i, 1, DDS_COMPRESS_NONE);
            fwrite(dst + offset, 1, size, fp);
            offset += size;
         }
         
         g_free(dst);
      }
      else
      {
         if(ddsvals.format > DDS_FORMAT_DEFAULT)
         {
            fmtdst = g_malloc(h * w * fmtbpp);
            convert_pixels(fmtdst, src, ddsvals.format, w, h, bpp,
                           palette, 1);
            g_free(src);
            src = fmtdst;
            bpp = fmtbpp;
         }
         
         fwrite(src, 1, h * w * bpp, fp);
      }
   }
   else
   {
      size = get_mipmapped_size(w, h, bpp, 0, mipmaps, compression);

      dst = g_malloc(size);
      
      if(basetype == GIMP_INDEXED)
      {
         fmtsize = get_mipmapped_size(w, h, 3, 0, mipmaps,
                                      DDS_COMPRESS_NONE);
         fmtdst = g_malloc(fmtsize);
         convert_pixels(fmtdst, src, DDS_FORMAT_RGB8, w, h, bpp,
                        palette, mipmaps);
         g_free(src);
         src = fmtdst;
         bpp = 3;
      }
      
      dxt_compress(dst, src, compression, w, h, bpp, mipmaps,
                   ddsvals.color_type, ddsvals.dither);
         
      offset = 0;
         
      for(i = 0; i < mipmaps; ++i)
      {
         size = get_mipmapped_size(w, h, bpp, i, 1, compression);
         fwrite(dst + offset, 1, size, fp);
         offset += size;
      }

      g_free(dst);
   }
      
   g_free(src);

   gimp_drawable_detach(drawable);
}

static void write_volume_mipmaps(FILE *fp, gint32 image_id, gint32 *layers,
                                 int w, int h, int d, int bpp, int fmtbpp,
                                 int mipmaps)
{
   int i, size, offset, colors;
   unsigned char *src, *dst, *tmp, *fmtdst;
   unsigned char *palette = 0;
   GimpDrawable *drawable;
   GimpPixelRgn rgn;
   GimpImageType type;
   
   type = gimp_image_base_type(image_id);

   if(ddsvals.compression != DDS_COMPRESS_NONE) return;

   src = g_malloc(w * h * bpp * d);

   if(gimp_image_base_type(image_id) == GIMP_INDEXED)
      palette = gimp_image_get_colormap(image_id, &colors);
   
   offset = 0;
   for(i = 0; i < d; ++i)
   {
      drawable = gimp_drawable_get(layers[i]);
      gimp_pixel_rgn_init(&rgn, drawable, 0, 0, w, h, 0, 0);
      gimp_pixel_rgn_get_rect(&rgn, src + offset, 0, 0, w, h);
      offset += (w * h * bpp);
      gimp_drawable_detach(drawable);
   }
   
   if(gimp_drawable_type(layers[0]) == GIMP_INDEXEDA_IMAGE)
   {
      tmp = g_malloc(w * h * d);
      for(i = 0; i < w * h * d; ++i)
         tmp[i] = src[2 * i];
      g_free(src);
      src = tmp;
      bpp = 1;
   }

   if(bpp >= 3)
      swap_rb(src, w * h * d, bpp);

   /* pre-convert indexed images to RGB for better mipmaps if a
      pixel format conversion is requested */
   if(ddsvals.format > DDS_FORMAT_DEFAULT && type == GIMP_INDEXED)
   {
      size = get_volume_mipmapped_size(w, h, d, 3, 0, mipmaps,
                                       DDS_COMPRESS_NONE);
      dst = g_malloc(size);
      convert_volume_pixels(dst, src, DDS_FORMAT_RGB8, w, h, d, bpp,
                            palette, 1);
      g_free(src);
      src = dst;
      bpp = 3;
      palette = NULL;
   }

   size = get_volume_mipmapped_size(w, h, d, bpp, 0, mipmaps,
                                    ddsvals.compression);
   
   dst = g_malloc(size);

   offset = get_volume_mipmapped_size(w, h, d, bpp, 0, 1,
                                      ddsvals.compression);

   generate_volume_mipmaps(dst, src, w, h, d, bpp,
                           palette != NULL, mipmaps);

   if(ddsvals.format > DDS_FORMAT_DEFAULT)
   {
      size = get_volume_mipmapped_size(w, h, d, fmtbpp, 0, mipmaps,
                                       ddsvals.compression);
      offset = get_volume_mipmapped_size(w, h, d, fmtbpp, 0, 1,
                                         ddsvals.compression);
      fmtdst = g_malloc(size);
      
      convert_volume_pixels(fmtdst, dst, ddsvals.format, w, h, d, bpp,
                            palette, mipmaps);
      g_free(dst);
      dst = fmtdst;
   }
   
   fwrite(dst + offset, 1, size, fp);
   
   g_free(src);
   g_free(dst);
}

static int write_image(FILE *fp, gint32 image_id, gint32 drawable_id)
{
   GimpDrawable *drawable;
   GimpImageType drawable_type, basetype;
   GimpPixelRgn rgn;
   int i, w, h, bpp = 0, fmtbpp = 0, has_alpha = 0;
   int num_mipmaps;
   unsigned char hdr[DDS_HEADERSIZE];
   unsigned int flags = 0, pflags = 0, caps = 0, caps2 = 0, size = 0;
   unsigned int rmask = 0, gmask = 0, bmask = 0, amask = 0;
   char *format = "XXXX";
   gint32 num_layers, *layers;
   guchar *cmap;
   gint colors;
   unsigned char zero[4] = {0, 0, 0, 0};

   layers = gimp_image_get_layers(image_id, &num_layers);
   
   drawable = gimp_drawable_get(drawable_id);

   w = drawable->width;
   h = drawable->height;

   basetype = gimp_image_base_type(image_id);
   drawable_type = gimp_drawable_type(drawable_id);
   gimp_pixel_rgn_init(&rgn, drawable, 0, 0, w, h, 0, 0);

   if((ddsvals.compression != DDS_COMPRESS_NONE) &&
      !(IS_POT(w) && IS_POT(h)))
   {
      ddsvals.compression = DDS_COMPRESS_NONE;
      g_message("DDS: Cannot compress non power-of-2 sized images.\n"
                "Saved image will not be compressed.");
   }

   switch(drawable_type)
   {
      case GIMP_RGB_IMAGE:      bpp = 3; break;
      case GIMP_RGBA_IMAGE:     bpp = 4; break;
      case GIMP_GRAY_IMAGE:     bpp = 1; break;
      case GIMP_GRAYA_IMAGE:    bpp = 2; break;
      case GIMP_INDEXED_IMAGE:  bpp = 1; break;
      case GIMP_INDEXEDA_IMAGE: bpp = 2; break;
      default:
         break;
   }
   
   if(ddsvals.format > DDS_FORMAT_DEFAULT)
   {
      switch(ddsvals.format)
      {
         case DDS_FORMAT_RGB8:
            fmtbpp = 3;
            rmask = 0x00ff0000;
            gmask = 0x0000ff00;
            bmask = 0x000000ff;
            amask = 0xff000000;
            break;
         case DDS_FORMAT_RGBA8:
            fmtbpp = 4;
            has_alpha = 1;
            rmask = 0x00ff0000;
            gmask = 0x0000ff00;
            bmask = 0x000000ff;
            amask = 0xff000000;
            break;
         case DDS_FORMAT_BGR8:
            fmtbpp = 4;
            rmask = 0x000000ff;
            gmask = 0x0000ff00;
            bmask = 0x00ff0000;
            amask = 0x00000000;
            break;
         case DDS_FORMAT_ABGR8:
            fmtbpp = 4;
            has_alpha = 1;
            rmask = 0x000000ff;
            gmask = 0x0000ff00;
            bmask = 0x00ff0000;
            amask = 0xff000000;
            break;
         case DDS_FORMAT_R5G6B5:
            fmtbpp = 2;
            rmask = 0x0000f800;
            gmask = 0x000007e0;
            bmask = 0x0000001f;
            amask = 0x00000000;
            break;
         case DDS_FORMAT_RGBA4:
            fmtbpp = 2;
            has_alpha = 1;
            rmask = 0x00000f00;
            gmask = 0x000000f0;
            bmask = 0x0000000f;
            amask = 0x0000f000;
            break;
         case DDS_FORMAT_RGB5A1:
            fmtbpp = 2;
            has_alpha = 1;
            rmask = 0x00007c00;
            gmask = 0x000003e0;
            bmask = 0x0000001f;
            amask = 0x00008000;
            break;
         case DDS_FORMAT_RGB10A2:
            fmtbpp = 4;
            has_alpha = 1;
            rmask = 0x3ff00000;
            gmask = 0x000ffc00;
            bmask = 0x000003ff;
            amask = 0xc0000000;
            break;
         case DDS_FORMAT_R3G3B2:
            fmtbpp = 1;
            has_alpha = 0;
            rmask = 0x000000e0;
            gmask = 0x0000001c;
            bmask = 0x00000003;
            amask = 0x00000000;
            break;
         case DDS_FORMAT_A8:
            fmtbpp = 1;
            has_alpha = 0;
            rmask = 0x00000000;
            gmask = 0x00000000;
            bmask = 0x00000000;
            amask = 0x000000ff;
            break;
         case DDS_FORMAT_L8:
            fmtbpp = 1;
            has_alpha = 0;
            rmask = 0x000000ff;
            gmask = 0x000000ff;
            bmask = 0x000000ff;
            amask = 0x00000000;
            break;
         case DDS_FORMAT_L8A8:
            fmtbpp = 2;
            has_alpha = 1;
            rmask = 0x000000ff;
            gmask = 0x000000ff;
            bmask = 0x000000ff;
            amask = 0x0000ff00;
            break;
         case DDS_FORMAT_YCOCG:
            fmtbpp = 4;
            has_alpha = 1;
            rmask = 0x00ff0000;
            gmask = 0x0000ff00;
            bmask = 0x000000ff;
            amask = 0xff000000;
            break;
         default:
            break;
      }
   }
   else if(bpp == 1)
   {
      if(basetype == GIMP_INDEXED)
      {
         fmtbpp = 1;
         has_alpha = 0;
         rmask = bmask = gmask = amask = 0;
      }
      else
      {
         fmtbpp = 1;
         has_alpha = 0;
         rmask = 0x000000ff;
         gmask = bmask = amask = 0;
      }
   }
   else if(bpp == 2)
   {
      if(basetype == GIMP_INDEXED)
      {
         fmtbpp = 1;
         has_alpha = 0;
         rmask = gmask = bmask = amask = 0;
      }
      else
      {
         fmtbpp = 2;
         has_alpha = 1;
         rmask = 0x000000ff;
         gmask = 0x000000ff;
         bmask = 0x000000ff;
         amask = 0x0000ff00;
      }
   }
   else if(bpp == 3)
   {
      fmtbpp = 3;
      rmask = 0x00ff0000;
      gmask = 0x0000ff00;
      bmask = 0x000000ff;
      amask = 0x00000000;
   }
   else
   {
      fmtbpp = 4;
      has_alpha = 1;
      rmask = 0x00ff0000;
      gmask = 0x0000ff00;
      bmask = 0x000000ff;
      amask = 0xff000000;
   }
   
   memset(hdr, 0, DDS_HEADERSIZE);
   
   memcpy(hdr, "DDS ", 4);
   PUTL32(hdr + 4, 124);
   PUTL32(hdr + 12, h);
   PUTL32(hdr + 16, w);
   PUTL32(hdr + 76, 32);
   PUTL32(hdr + 88, fmtbpp << 3);
   PUTL32(hdr + 92,  rmask);
   PUTL32(hdr + 96,  gmask);
   PUTL32(hdr + 100, bmask);
   PUTL32(hdr + 104, amask);
   
   flags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT;
     
   caps = DDSCAPS_TEXTURE;
   if(ddsvals.mipmaps)
   {
      flags |= DDSD_MIPMAPCOUNT;
      caps |= (DDSCAPS_COMPLEX | DDSCAPS_MIPMAP);
      num_mipmaps = get_num_mipmaps(w, h);
   }
   else
      num_mipmaps = 1;
   
   if(ddsvals.savetype == DDS_SAVE_CUBEMAP && is_cubemap)
   {
      caps |= DDSCAPS_COMPLEX;
      caps2 |= (DDSCAPS2_CUBEMAP |
                DDSCAPS2_CUBEMAP_POSITIVEX |
                DDSCAPS2_CUBEMAP_NEGATIVEX |
                DDSCAPS2_CUBEMAP_POSITIVEY |
                DDSCAPS2_CUBEMAP_NEGATIVEY |
                DDSCAPS2_CUBEMAP_POSITIVEZ |
                DDSCAPS2_CUBEMAP_NEGATIVEZ);
   }
   else if(ddsvals.savetype == DDS_SAVE_VOLUMEMAP && is_volume)
   {
      PUTL32(hdr + 24, num_layers);
      flags |= DDSD_DEPTH;
      caps |= DDSCAPS_COMPLEX;
      caps2 |= DDSCAPS2_VOLUME;
   }
   
   PUTL32(hdr + 28, num_mipmaps);
   PUTL32(hdr + 108, caps);
   PUTL32(hdr + 112, caps2);
   
   if(ddsvals.compression == DDS_COMPRESS_NONE)
   {
      flags |= DDSD_PITCH;

      if(ddsvals.format > DDS_FORMAT_DEFAULT)
      {
         if(ddsvals.format == DDS_FORMAT_A8)
            pflags |= DDPF_ALPHA;
         else
         {
            if((fmtbpp == 1 || ddsvals.format == DDS_FORMAT_L8A8) &&
               (ddsvals.format != DDS_FORMAT_R3G3B2))
               pflags |= DDPF_LUMINANCE;
            else
               pflags |= DDPF_RGB;
         }
      }
      else
      {
         if(bpp == 1)
         {
            if(basetype == GIMP_INDEXED)
               pflags |= DDPF_PALETTEINDEXED8;
            else
               pflags |= DDPF_LUMINANCE;
         }
         else if(bpp == 2 && basetype == GIMP_INDEXED)
            pflags |= DDPF_PALETTEINDEXED8;
         else
            pflags |= DDPF_RGB;
      }
      
      if(has_alpha) pflags |= DDPF_ALPHAPIXELS;

      PUTL32(hdr + 8, flags);
      PUTL32(hdr + 20, w * fmtbpp);
      PUTL32(hdr + 80, pflags);
   }
   else
   {
      flags |= DDSD_LINEARSIZE;
      PUTL32(hdr + 8, flags);
      PUTL32(hdr + 80, DDPF_FOURCC);
      switch(ddsvals.compression)
      {
         case DDS_COMPRESS_BC1:    format = "DXT1"; break;
         case DDS_COMPRESS_BC2:    format = "DXT3"; break;
         case DDS_COMPRESS_BC3:
         case DDS_COMPRESS_BC3N:
         case DDS_COMPRESS_YCOCG:
         case DDS_COMPRESS_YCOCGS:
         case DDS_COMPRESS_AEXP:   format = "DXT5"; break;
         case DDS_COMPRESS_BC4:    format = "ATI1"; break;
         case DDS_COMPRESS_BC5:    format = "ATI2"; break;
      }
      memcpy(hdr + 84, format, 4);

      size = ((w + 3) >> 2) * ((h + 3) >> 2);
      if(ddsvals.compression == DDS_COMPRESS_BC1 ||
         ddsvals.compression == DDS_COMPRESS_BC4)
         size *= 8;
      else
         size *= 16;

      PUTL32(hdr + 20, size);
   }

   fwrite(hdr, DDS_HEADERSIZE, 1, fp);
   
   if(basetype == GIMP_INDEXED && ddsvals.format == DDS_FORMAT_DEFAULT &&
      ddsvals.compression == DDS_COMPRESS_NONE)
   {
      cmap = gimp_image_get_colormap(image_id, &colors);
      for(i = 0; i < colors; ++i)
      {
         fwrite(&cmap[3 * i], 1, 3, fp);
         if(i == ddsvals.transindex)
            fputc(0, fp);
         else
            fputc(255, fp);
      }
      for(; i < 256; ++i)
         fwrite(zero, 1, 4, fp);
   }

   if(ddsvals.savetype == DDS_SAVE_CUBEMAP)
   {
      for(i = 0; i < 6; ++i)
      {
         write_layer(fp, image_id, cubemap_faces[i], w, h, bpp, fmtbpp,
                     num_mipmaps);
         if(interactive_dds)
            gimp_progress_update((float)(i + 1) / 6.0);
      }
   }
   else if(ddsvals.savetype == DDS_SAVE_VOLUMEMAP)
   {
      for(i = 0; i < num_layers; ++i)
      {
         write_layer(fp, image_id, layers[i], w, h, bpp, fmtbpp, 1);
         if(interactive_dds)
            gimp_progress_update((float)i / (float)num_layers);
      }
      
      if(num_mipmaps > 1)
         write_volume_mipmaps(fp, image_id, layers, w, h, num_layers,
                              bpp, fmtbpp, num_mipmaps);
   }
   else
   {
      write_layer(fp, image_id, drawable_id, w, h, bpp, fmtbpp,
                  num_mipmaps);
   }
      
   if(interactive_dds)
      gimp_progress_update(1.0);

   gimp_drawable_detach(drawable);
   
   return(1);
}

static void save_dialog_response(GtkWidget *widget, gint response_id,
                                 gpointer data)
{
   switch(response_id)
   {
      case GTK_RESPONSE_OK:
         runme = 1;
      default:
         gtk_widget_destroy(widget);
         break;
   }
}

static void compression_selected(GtkWidget *widget, gpointer data)
{
   ddsvals.compression = (gint)(long)data;
   gtk_widget_set_sensitive(format_opt, ddsvals.compression == DDS_COMPRESS_NONE);
   gtk_widget_set_sensitive(color_type_opt,
                            ddsvals.compression != DDS_COMPRESS_NONE &&
                            ddsvals.compression != DDS_COMPRESS_BC4 &&
                            ddsvals.compression != DDS_COMPRESS_BC5 &&
                            ddsvals.compression != DDS_COMPRESS_YCOCGS);
   gtk_widget_set_sensitive(dither_chk,
                            ddsvals.compression != DDS_COMPRESS_NONE &&
                            ddsvals.compression != DDS_COMPRESS_BC4 &&
                            ddsvals.compression != DDS_COMPRESS_BC5 &&
                            ddsvals.compression != DDS_COMPRESS_YCOCGS);
}

static void savetype_selected(GtkWidget *widget, gpointer data)
{
   int n = (int)(long)data;

   ddsvals.savetype = n;
   
   switch(n)
   {
      case 0:
      case 1:
         gtk_widget_set_sensitive(compress_opt, 1);
         break;
      case 2:
         ddsvals.compression = DDS_COMPRESS_NONE;
         gtk_menu_set_active(GTK_MENU(compress_menu), DDS_COMPRESS_NONE);
         gtk_widget_set_sensitive(compress_opt, 0);
         break;
   }
}

static void format_selected(GtkWidget *widget, gpointer data)
{
   ddsvals.format = (gint)(long)data;
}

static void toggle_clicked(GtkWidget *widget, gpointer data)
{
   int *flag = (int*)data;
   (*flag) = !(*flag);
}

static void transindex_clicked(GtkWidget *widget, gpointer data)
{
   GtkWidget *spin = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "spin"));
      
   if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)))
   {
      ddsvals.transindex = 0;
      gtk_widget_set_sensitive(spin, 1);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), 0);
   }
   else
   {
      gtk_widget_set_sensitive(spin, 0);
      ddsvals.transindex = -1;
   }
}

static void transindex_changed(GtkWidget *widget, gpointer data)
{
   ddsvals.transindex = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
}

static void adv_opt_expanded(GtkWidget *widget, gpointer data)
{
   ddsvals.show_adv_opt = !gtk_expander_get_expanded(GTK_EXPANDER(widget));
}

static void color_type_selected(GtkWidget *widget, gpointer data)
{
   ddsvals.color_type = (gint)(long)data;
}

static gint save_dialog(gint32 image_id, gint32 drawable_id)
{
   GtkWidget *dlg;
   GtkWidget *vbox, *hbox;
   GtkWidget *table;
   GtkWidget *label;
   GtkWidget *opt;
   GtkWidget *menu;
   GtkWidget *menuitem;
   GtkWidget *check;
   GtkWidget *spin;
   GtkWidget *expander;
   GimpImageType type, basetype;
   int i, w, h;
   
   if(is_cubemap)
      ddsvals.savetype = DDS_SAVE_CUBEMAP;
   else if(is_volume)
      ddsvals.savetype = DDS_SAVE_VOLUMEMAP;
   else
      ddsvals.savetype = DDS_SAVE_SELECTED_LAYER;
   
   basetype = gimp_image_base_type(image_id);
   type = gimp_drawable_type(drawable_id);
   
   w = gimp_image_width(image_id);
   h = gimp_image_height(image_id);
   
   dlg = gimp_dialog_new("Save as DDS", "dds", NULL, GTK_WIN_POS_MOUSE,
                         gimp_standard_help_func, SAVE_PROC,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                         NULL);

   gtk_signal_connect(GTK_OBJECT(dlg), "response",
                      GTK_SIGNAL_FUNC(save_dialog_response),
                      0);
   gtk_signal_connect(GTK_OBJECT(dlg), "destroy",
                      GTK_SIGNAL_FUNC(gtk_main_quit),
                      0);
   
   vbox = gtk_vbox_new(0, 8);
   gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
   gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), vbox, 1, 1, 0);
   gtk_widget_show(vbox);
   
   table = gtk_table_new(3, 2, 0);
   gtk_widget_show(table);
   gtk_box_pack_start(GTK_BOX(vbox), table, 1, 1, 0);
   gtk_table_set_row_spacings(GTK_TABLE(table), 8);
   gtk_table_set_col_spacings(GTK_TABLE(table), 8);
   
   label = gtk_label_new("Compression:");
   gtk_widget_show(label);
   gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
   
   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gtk_table_attach(GTK_TABLE(table), opt, 1, 2, 0, 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(GTK_EXPAND), 0, 0);
   
   menu = gtk_menu_new();
   
   for(i = 0; compression_strings[i].string; ++i)
   {
      menuitem = gtk_menu_item_new_with_label(compression_strings[i].string);
      gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                         GTK_SIGNAL_FUNC(compression_selected),
                         (gpointer)(long)compression_strings[i].compression);
      gtk_widget_show(menuitem);
      gtk_menu_append(GTK_MENU(menu), menuitem);
   }
   
   gtk_menu_set_active(GTK_MENU(menu), ddsvals.compression);
   
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   
   compress_opt = opt;
   compress_menu = menu;

   label = gtk_label_new("Format:");
   gtk_widget_show(label);
   gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
   
   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gtk_table_attach(GTK_TABLE(table), opt, 1, 2, 1, 2,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(GTK_EXPAND), 0, 0);
   
   menu = gtk_menu_new();

   for(i = 0; format_strings[i].string; ++i)
   {
      menuitem = gtk_menu_item_new_with_label(format_strings[i].string);
      gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                         GTK_SIGNAL_FUNC(format_selected),
                         (gpointer)(long)format_strings[i].format);
      gtk_widget_show(menuitem);
      gtk_menu_append(GTK_MENU(menu), menuitem);
   }
   
   gtk_menu_set_active(GTK_MENU(menu), ddsvals.format);
   
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);

   gtk_widget_set_sensitive(opt, ddsvals.compression == DDS_COMPRESS_NONE);
   
   format_opt = opt;
   
   label = gtk_label_new("Save:");
   gtk_widget_show(label);
   gtk_table_attach(GTK_TABLE(table), label, 0, 1, 2, 3,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
   
   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gtk_table_attach(GTK_TABLE(table), opt, 1, 2, 2, 3,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(GTK_EXPAND), 0, 0);
   
   menu = gtk_menu_new();
   
   menuitem = gtk_menu_item_new_with_label("Selected layer");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(savetype_selected),
                      (gpointer)DDS_SAVE_SELECTED_LAYER);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("As cube map");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(savetype_selected),
                      (gpointer)DDS_SAVE_CUBEMAP);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   gtk_widget_set_sensitive(menuitem, is_cubemap);
   menuitem = gtk_menu_item_new_with_label("As volume map");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(savetype_selected),
                      (gpointer)DDS_SAVE_VOLUMEMAP);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   gtk_widget_set_sensitive(menuitem, is_volume);
   
   gtk_menu_set_active(GTK_MENU(menu), ddsvals.savetype);
   
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   
   gtk_widget_set_sensitive(opt, is_cubemap || is_volume);
   
   check = gtk_check_button_new_with_label("Generate mipmaps");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), ddsvals.mipmaps);
   gtk_box_pack_start(GTK_BOX(vbox), check, 0, 0, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(toggle_clicked), &ddsvals.mipmaps);
   gtk_widget_show(check);
   mipmap_check = check;

   if(is_volume && ddsvals.savetype == DDS_SAVE_VOLUMEMAP)
   {
      ddsvals.compression = DDS_COMPRESS_NONE;
      gtk_menu_set_active(GTK_MENU(compress_menu), DDS_COMPRESS_NONE);
      gtk_widget_set_sensitive(compress_opt, 0);
   }
   
   hbox = gtk_hbox_new(0, 8);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, 1, 1, 0);
   gtk_widget_show(hbox);
   
   check = gtk_check_button_new_with_label("Transparent index:");
   gtk_box_pack_start(GTK_BOX(hbox), check, 0, 0, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(transindex_clicked), 0);
   gtk_widget_show(check);
   
   spin = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 255, 1, 1, 0)), 1, 0);
   gtk_box_pack_start(GTK_BOX(hbox), spin, 1, 1, 0);
   gtk_spin_button_set_update_policy(GTK_SPIN_BUTTON(spin),
                                     GTK_UPDATE_IF_VALID);
   gtk_signal_connect(GTK_OBJECT(spin), "value_changed",
                      GTK_SIGNAL_FUNC(transindex_changed), 0);
   gtk_widget_show(spin);
   
   g_object_set_data(G_OBJECT(check), "spin", spin);
   
   if(basetype != GIMP_INDEXED)
   {
      gtk_widget_set_sensitive(check, 0);
      gtk_widget_set_sensitive(spin, 0);
   }
   else if(ddsvals.transindex < 0)
   {
      gtk_widget_set_sensitive(spin, 0);
   }
   else if(ddsvals.transindex >= 0)
   {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), 1);
      gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), ddsvals.transindex);
   }

   expander = gtk_expander_new("Advanced Options");
   gtk_expander_set_expanded(GTK_EXPANDER(expander), ddsvals.show_adv_opt);
   gtk_expander_set_spacing(GTK_EXPANDER(expander), 8);
   gtk_signal_connect(GTK_OBJECT(expander), "activate",
                      GTK_SIGNAL_FUNC(adv_opt_expanded), 0);
   gtk_box_pack_start(GTK_BOX(vbox), expander, 1, 1, 0);
   gtk_widget_show(expander);
   
   table = gtk_table_new(2, 2, 0);
   gtk_table_set_row_spacings(GTK_TABLE(table), 8);
   gtk_table_set_col_spacings(GTK_TABLE(table), 8);
   gtk_container_add(GTK_CONTAINER(expander), table);
   gtk_widget_show(table);

   label = gtk_label_new("Color selection:");
   gtk_widget_show(label);
   gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
   
   opt = gtk_option_menu_new();
   gtk_widget_show(opt);
   gtk_table_attach(GTK_TABLE(table), opt, 1, 2, 0, 1,
                    (GtkAttachOptions)(GTK_EXPAND | GTK_FILL),
                    (GtkAttachOptions)(GTK_EXPAND), 0, 0);
   
   menu = gtk_menu_new();
   
   for(i = 0; color_type_strings[i].string; ++i)
   {
      menuitem = gtk_menu_item_new_with_label(color_type_strings[i].string);
      gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                         GTK_SIGNAL_FUNC(color_type_selected),
                         (gpointer)(long)color_type_strings[i].type);
      gtk_widget_show(menuitem);
      gtk_menu_append(GTK_MENU(menu), menuitem);
   }
   
   gtk_menu_set_active(GTK_MENU(menu), ddsvals.color_type);
   
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   
   color_type_opt = opt;

   check = gtk_check_button_new_with_label("Use dithering");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), ddsvals.dither);
   gtk_table_attach(GTK_TABLE(table), check, 0, 2, 1, 2,
                    (GtkAttachOptions)(GTK_FILL),
                    (GtkAttachOptions)(0), 0, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(toggle_clicked), &ddsvals.dither);
   gtk_widget_show(check);
   
   dither_chk = check;

   gtk_widget_set_sensitive(color_type_opt,
                            ddsvals.compression != DDS_COMPRESS_NONE &&
                            ddsvals.compression != DDS_COMPRESS_BC4 &&
                            ddsvals.compression != DDS_COMPRESS_BC5 &&
                            ddsvals.compression != DDS_COMPRESS_YCOCGS);
   gtk_widget_set_sensitive(dither_chk,
                            ddsvals.compression != DDS_COMPRESS_NONE &&
                            ddsvals.compression != DDS_COMPRESS_BC4 &&
                            ddsvals.compression != DDS_COMPRESS_BC5 &&
                            ddsvals.compression != DDS_COMPRESS_YCOCGS);
   
   gtk_widget_show(dlg);
   
   runme = 0;
   
   gtk_main();
   
   return(runme);
}
