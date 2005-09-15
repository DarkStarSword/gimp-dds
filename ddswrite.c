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

#define IS_POT(x)  (!((x) & ((x) - 1)))

static gint save_dialog(gint32 image_id, gint32 drawable);
static void save_dialog_response(GtkWidget *widget, gint response_id, gpointer data);
static void compression_selected(GtkWidget *widget, gpointer data);
static void toggle_clicked(GtkWidget *widget, gpointer data);
static int write_image(FILE *fp, gint32 image_id, gint32 drawable_id);
static int get_num_mipmaps(int width, int height);
static unsigned int get_mipmapped_size(int width, int height, int bpp,
                                       int level, int num, int format);

static int runme = 0;

const char *cubemap_face_names[3][6] = 
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
   }
};

static gint cubemap_faces[6];
static gint is_cubemap = 0;
static gint is_volume = 0;

GtkWidget *mipmap_check;
GtkWidget *compress_opt;
GtkWidget *compress_menu;

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
            for(k = 0; k < 3; ++k)
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

   switch(gimp_drawable_type(drawable_id))
   {
      case GIMP_RGB_IMAGE:
      case GIMP_RGBA_IMAGE:
      case GIMP_GRAY_IMAGE:
      case GIMP_GRAYA_IMAGE:
         break;
      default:
         g_message("DDS: Cannot operate on unknown image types.\n"
                   "Only RGB and Grayscale images accepted.");
         return(GIMP_PDB_EXECUTION_ERROR);
   }
   
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

static int get_num_mipmaps(int width, int height)
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

static unsigned int get_mipmapped_size(int width, int height, int bpp,
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
      size *= (format == DDS_COMPRESS_DXT1) ? 8 : 16;
   
   return(size);
}

static unsigned int get_volume_mipmapped_size(int width, int height,
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

   while(n < num && (w != 1 || h != 1))
   {
      if(w > 1) w >>= 1;
      if(h > 1) h >>= 1;
      if(format == DDS_COMPRESS_NONE)
         size += (w * h * d);
      else
         size += (((w + 3) >> 2) * ((h + 3) >> 2) * d);
      d >>= 1;
      if(d == 0) d = 1;
      ++n;
   }
   
   if(format == DDS_COMPRESS_NONE)
      size *= bpp;
   else
      size *= (format == DDS_COMPRESS_DXT1) ? 8 : 16;
   
   return(size);
}

static void write_layer(FILE *fp, gint32 drawable_id, int w, int h, int bpp,
                        int mipmaps)
{
   GimpDrawable *drawable;
   GimpPixelRgn rgn;
   unsigned char *src, *dst, c;
   int i, x, y, size, offset;

   drawable = gimp_drawable_get(drawable_id);
   src = g_malloc(w * h * bpp);
   gimp_pixel_rgn_init(&rgn, drawable, 0, 0, w, h, 0, 0);
   gimp_pixel_rgn_get_rect(&rgn, src, 0, 0, w, h);
   
   
   if(ddsvals.swapRA && bpp == 4)
   {
      for(y = 0; y < drawable->height; ++y)
      {
         for(x = 0; x < drawable->width; ++x)
         {
            c = src[y * (drawable->width * 4) + (x * 4)];
            src[y * (drawable->width * 4) + (x * 4)] =
               src[y * (drawable->width * 4) + (x * 4) + 3];
            src[y * (drawable->width * 4) + (x * 4) + 3] = c;
         }
      }
   }
      
   if(ddsvals.compression == DDS_COMPRESS_NONE)
   {
      if(mipmaps > 1)
      {
         size = get_mipmapped_size(w, h, bpp, 0, mipmaps, DDS_COMPRESS_NONE);
         dst = malloc(size);
         generate_mipmaps(dst, src, w, h, bpp, mipmaps);
            
         offset = 0;
            
         for(i = 0; i < mipmaps; ++i)
         {
            size = get_mipmapped_size(w, h, bpp, i, 1, DDS_COMPRESS_NONE);
            fwrite(dst + offset, 1, size, fp);
            offset += size;
            printf("wrote mipmap %d (%d)\n", i, size);
         }
          
         free(dst);
      }
      else
      {
         fwrite(src, 1, h * w * bpp, fp);
      }
   }
   else
   {
      size = get_mipmapped_size(w, h, bpp, 0, mipmaps, ddsvals.compression);
         
      dst = malloc(size);
      dxt_compress(dst, src, ddsvals.compression, w, h, bpp, mipmaps);
         
      offset = 0;
         
      for(i = 0; i < mipmaps; ++i)
      {
         size = get_mipmapped_size(w, h, bpp, i, 1, ddsvals.compression);
         fwrite(dst + offset, 1, size, fp);
         offset += size;
      }
         
      free(dst);
   }
      
   g_free(src);

   gimp_drawable_detach(drawable);
}

static void write_volume_mipmaps(FILE *fp, gint *layers, int w, int h, int d,
                                 int bpp, int mipmaps)
{
   int i, size, offset;
   unsigned char *src, *dst;
   GimpDrawable *drawable;
   GimpPixelRgn rgn;
   
   if(ddsvals.compression != DDS_COMPRESS_NONE) return;
   
   size = get_volume_mipmapped_size(w, h, d, bpp, 0, mipmaps,
                                    ddsvals.compression);
   
   src = g_malloc(w * h * bpp * d);
   dst = g_malloc(size);
   
   offset = 0;
   for(i = 0; i < d; ++i)
   {
      drawable = gimp_drawable_get(layers[i]);
      gimp_pixel_rgn_init(&rgn, drawable, 0, 0, w, h, 0, 0);
      gimp_pixel_rgn_get_rect(&rgn, src + offset, 0, 0, w, h);
      offset += (w * h * bpp);
      gimp_drawable_detach(drawable);
   }
   
   offset = get_volume_mipmapped_size(w, h, d, bpp, 0, 1,
                                      ddsvals.compression);
   
   generate_volume_mipmaps(dst, src, w, h, d, bpp, mipmaps);
   fwrite(dst + offset, 1, size, fp);
   
   g_free(src);
   g_free(dst);
}

static int write_image(FILE *fp, gint32 image_id, gint32 drawable_id)
{
   GimpDrawable *drawable;
   GimpImageType drawable_type;
   GimpPixelRgn rgn;
   int i, w, h, bpp = 0;
   int num_mipmaps;
   unsigned char hdr[DDS_HEADERSIZE];
   unsigned int flags = 0, caps = 0, caps2 = 0, size = 0;
   unsigned int rmask, gmask, bmask, amask;
   char *format;
   gint num_layers, *layers;

   layers = gimp_image_get_layers(image_id, &num_layers);
   
   drawable = gimp_drawable_get(drawable_id);

   w = drawable->width;
   h = drawable->height;
   
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
      case GIMP_RGB_IMAGE:
         bpp = 3;
         break;
      case GIMP_RGBA_IMAGE:
         bpp = 4;
         break;
      case GIMP_GRAY_IMAGE:
         bpp = 1;
         break;
      case GIMP_GRAYA_IMAGE:
         bpp = 2;
         break;
      default:
         break;
   }
   
   memset(hdr, 0, DDS_HEADERSIZE);
   
   memcpy(hdr, "DDS ", 4);
   PUT32(hdr + 4, 124);
   PUT32(hdr + 12, h);
   PUT32(hdr + 16, w);
   PUT32(hdr + 76, 32);
   PUT32(hdr + 88, bpp << 3);

   if(bpp == 1)
   {
      rmask = gmask = bmask = 0x000000ff;
      amask = 0;
   }
   else if(bpp == 2)
   {
      rmask = gmask = bmask = 0x000000ff;
      amask = 0x0000ff00;
   }
   else
   {
      rmask = 0x000000ff;
      gmask = 0x0000ff00;
      bmask = 0x00ff0000;
      amask = 0xff000000;
   }
   
   PUT32(hdr + 92,  rmask);
   PUT32(hdr + 96,  gmask);
   PUT32(hdr + 100, bmask);
   PUT32(hdr + 104, amask);
   
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
      PUT32(hdr + 24, num_layers);
      flags |= DDSD_DEPTH;
      caps |= DDSCAPS_COMPLEX;
      caps2 |= DDSCAPS2_VOLUME;
   }
   
   PUT32(hdr + 28, num_mipmaps);
   PUT32(hdr + 108, caps);
   PUT32(hdr + 112, caps2);
   
   if(ddsvals.compression == DDS_COMPRESS_NONE)
   {
      flags |= DDSD_PITCH;
      PUT32(hdr + 8, flags);
      PUT32(hdr + 20, w * bpp);
      PUT32(hdr + 80, (bpp == 4 || bpp == 2) ? DDPF_RGB | DDPF_ALPHAPIXELS : DDPF_RGB);
   }
   else
   {
      flags |= DDSD_LINEARSIZE;
      PUT32(hdr + 8, flags);
      PUT32(hdr + 80, DDPF_FOURCC);
      switch(ddsvals.compression)
      {
         case DDS_COMPRESS_DXT1: format = "DXT1"; break;
         case DDS_COMPRESS_DXT3: format = "DXT3"; break;
         case DDS_COMPRESS_DXT5:
         default:                format = "DXT5"; break;
      }
      memcpy(hdr + 84, format, 4);

      size = ((w + 3) >> 2) * ((h + 3) >> 2);
      size *= (ddsvals.compression == DDS_COMPRESS_DXT1) ? 8 : 16;

      PUT32(hdr + 20, size);
   }

   fwrite(hdr, DDS_HEADERSIZE, 1, fp);

   if(ddsvals.savetype == DDS_SAVE_CUBEMAP)
   {
      for(i = 0; i < 6; ++i)
      {
         write_layer(fp, cubemap_faces[i], w, h, bpp, num_mipmaps);
         if(interactive_dds)
            gimp_progress_update((float)(i + 1) / 6.0);
      }
   }
   else if(ddsvals.savetype == DDS_SAVE_VOLUMEMAP)
   {
      for(i = 0; i < num_layers; ++i)
      {
         write_layer(fp, layers[i], w, h, bpp, 1);
         if(interactive_dds)
            gimp_progress_update((float)i / (float)num_layers);
      }
      
      if(num_mipmaps > 1)
         write_volume_mipmaps(fp, layers, w, h, num_layers, bpp, num_mipmaps);
   }
   else
   {
      write_layer(fp, drawable_id, w, h, bpp, num_mipmaps);
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
   ddsvals.compression = (gint)data;
}

static void savetype_selected(GtkWidget *widget, gpointer data)
{
   int n = (int)data;

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

static void toggle_clicked(GtkWidget *widget, gpointer data)
{
   int *flag = (int*)data;
   (*flag) = !(*flag);
}

static gint save_dialog(gint32 image_id, gint32 drawable_id)
{
   GtkWidget *dlg;
   GtkWidget *vbox;
   GtkWidget *table;
   GtkWidget *label;
   GtkWidget *opt;
   GtkWidget *menu;
   GtkWidget *menuitem;
   GtkWidget *check;
   GimpImageType type;
   int w, h;
   
   if(is_cubemap)
      ddsvals.savetype = DDS_SAVE_CUBEMAP;
   else if(is_volume)
      ddsvals.savetype = DDS_SAVE_VOLUMEMAP;
   else
      ddsvals.savetype = DDS_SAVE_SELECTED_LAYER;
   
   type = gimp_drawable_type(drawable_id);
   
   w = gimp_image_width(image_id);
   h = gimp_image_height(image_id);
   
   dlg = gimp_dialog_new("Save as DDS", "dds",
                         0, GTK_WIN_POS_MOUSE, gimp_standard_help_func, 0,
                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                         GTK_STOCK_OK, GTK_RESPONSE_OK,
                         0);

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
   
   table = gtk_table_new(2, 2, 0);
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
   
   menuitem = gtk_menu_item_new_with_label("None");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(compression_selected),
                      (gpointer)DDS_COMPRESS_NONE);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("DXT1");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(compression_selected),
                      (gpointer)DDS_COMPRESS_DXT1);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   if(gimp_drawable_has_alpha(drawable_id))
      gtk_widget_set_sensitive(menuitem, 0);
   menuitem = gtk_menu_item_new_with_label("DXT3");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(compression_selected),
                      (gpointer)DDS_COMPRESS_DXT3);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   menuitem = gtk_menu_item_new_with_label("DXT5");
   gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
                      GTK_SIGNAL_FUNC(compression_selected),
                      (gpointer)DDS_COMPRESS_DXT5);
   gtk_widget_show(menuitem);
   gtk_menu_append(GTK_MENU(menu), menuitem);
   
   gtk_menu_set_active(GTK_MENU(menu), ddsvals.compression);
   
   gtk_option_menu_set_menu(GTK_OPTION_MENU(opt), menu);
   
   compress_opt = opt;
   compress_menu = menu;
   
   label = gtk_label_new("Save:");
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

   check = gtk_check_button_new_with_label("Swap red and alpha");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), ddsvals.swapRA);
   gtk_box_pack_start(GTK_BOX(vbox), check, 0, 0, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(toggle_clicked), &ddsvals.swapRA);
   gtk_widget_show(check);
   gtk_widget_set_sensitive(check, type == GIMP_RGBA_IMAGE);
   
   if(is_volume && ddsvals.savetype == DDS_SAVE_VOLUMEMAP)
   {
      ddsvals.compression = DDS_COMPRESS_NONE;
      gtk_menu_set_active(GTK_MENU(compress_menu), DDS_COMPRESS_NONE);
      gtk_widget_set_sensitive(compress_opt, 0);
   }

   gtk_widget_show(dlg);
   
   runme = 0;
   
   gtk_main();
   
   return(runme);
}
