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

static gint save_dialog(gint32 drawable);
static void save_dialog_response(GtkWidget *widget, gint response_id, gpointer data);
static void compression_selected(GtkWidget *widget, gpointer data);
static void mipmaps_clicked(GtkWidget *widget, gpointer data);
static int write_image(FILE *fp, gint32 image_id, gint32 drawable_id);
static int get_num_mipmaps(int width, int height);
static unsigned int get_mipmapped_size(int width, int height, int bpp,
                                       int level, int num, int format);

static int runme = 0;

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
   
   if(interactive_dds)
      if(!save_dialog(drawable_id))
         return(GIMP_PDB_CANCEL);
   
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

static int write_image(FILE *fp, gint32 image_id, gint32 drawable_id)
{
   GimpDrawable *drawable;
   GimpImageType drawable_type;
   GimpPixelRgn rgn;
   int i, j, x, y, bpp = 0;
   int num_mipmaps;
   unsigned int rowbytes;
   guchar *pixels;
   unsigned char *src, *dst;
   unsigned char hdr[DDS_HEADERSIZE];
   unsigned int flags = 0, caps = 0, size = 0, offset = 0;
   char *format;
   /*
   gint *layers, num_layers;
   const char *cubmap_face_names[6] = 
   {
      "positive x", "negative x",
      "positive y", "negative y",
      "positive z", "negative z"
   };
   gint cubmap_faces[6];
   int cubemap = 0;
   char *layer_name;
   */
   
   drawable = gimp_drawable_get(drawable_id);
   
   drawable_type = gimp_drawable_type(drawable->drawable_id);
   gimp_pixel_rgn_init(&rgn, drawable, 0, 0, drawable->width,
                       drawable->height, 0, 0);

   /* cubemap detection */
   /*
   layers = gimp_image_get_layers(image_id, &num_layers);
   if(num_layers == 6)
   {
      for(i = 0; i < 6; ++i)
         cubmap_faces[i] = -1;
      
      for(i = 0; i < 6; ++i)
      {
         layer_name = (char*)gimp_drawable_get_name(layers[i]);
         for(j = 0; j < 6; ++j)
         {
            if(strstr(layer_name, cubemaps_face_names[j]))
               if(cubemap_faces[j] == -1)
                  cubemap_faces[j] = layers[i];
         }
      }
      
      cubmap = 1;
      for(i = 0; i < 6; ++i)
      {
         if(cubemap_faces[i] == -1)
         {
            cubemap = 0;
            break;
         }
      }
   }
   */
   
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
   
   pixels = g_malloc(drawable->width * drawable->height * bpp);
   gimp_pixel_rgn_get_rect(&rgn, pixels, 0, 0, drawable->width, drawable->height);
   
   if(bpp >= 3)
      src = (unsigned char*)pixels;
   else
   {
      if(drawable_type == GIMP_GRAY_IMAGE)
      {
         src = malloc(drawable->width * drawable->height * 3);
         for(y = 0; y < drawable->height; ++y)
         {
            for(x = 0; x < drawable->width; ++x)
            {
               src[y * (drawable->width * 3) + (x * 3) + 0] = 
                  pixels[y * drawable->width + x];
               src[y * (drawable->width * 3) + (x * 3) + 1] = 
                  pixels[y * drawable->width + x];
               src[y * (drawable->width * 3) + (x * 3) + 2] = 
                  pixels[y * drawable->width + x];
            }
         }
         bpp = 3;
      }
      else
      {
         src = malloc(drawable->width * drawable->height * 4);
         for(y = 0; y < drawable->height; ++y)
         {
            for(x = 0; x < drawable->width; ++x)
            {
               src[y * (drawable->width * 3) + (x * 3) + 0] = 
                  pixels[y * (drawable->width * 2) + x + 0];
               src[y * (drawable->width * 3) + (x * 3) + 1] = 
                  pixels[y * (drawable->width * 2) + x + 0];
               src[y * (drawable->width * 3) + (x * 3) + 2] = 
                  pixels[y * (drawable->width * 2) + x + 0];
               src[y * (drawable->width * 3) + (x * 3) + 3] = 
                  pixels[y * (drawable->width * 2) + x + 1];
            }
         }
         bpp = 4;
      }
   }
   
   rowbytes = drawable->width * bpp;
   
   memset(hdr, 0, DDS_HEADERSIZE);
   
   memcpy(hdr, "DDS ", 4);
   PUT32(hdr + 4, 124);
   PUT32(hdr + 12, drawable->height);
   PUT32(hdr + 16, drawable->width);
   PUT32(hdr + 76, 32);
   PUT32(hdr + 88, bpp << 3);
   PUT32(hdr + 92,  0x000000ff);
   PUT32(hdr + 96,  0x0000ff00);
   PUT32(hdr + 100, 0x00ff0000);
   PUT32(hdr + 104, 0xff000000);
   
   flags = DDSD_CAPS | DDSD_PIXELFORMAT | DDSD_WIDTH | DDSD_HEIGHT;
   
   caps = DDSCAPS_TEXTURE;
   if(ddsvals.mipmaps)
   {
      flags |= DDSD_MIPMAPCOUNT;
      caps |= (DDSCAPS_COMPLEX | DDSCAPS_MIPMAP);
      num_mipmaps = get_num_mipmaps(drawable->width, drawable->height);
   }
   else
      num_mipmaps = 1;
   
   PUT32(hdr + 28, num_mipmaps);
   PUT32(hdr + 108, caps);
   
   if(ddsvals.compression == DDS_COMPRESS_NONE)
   {
      flags |= DDSD_PITCH;
      PUT32(hdr + 8, flags);
      PUT32(hdr + 20, rowbytes);
      PUT32(hdr + 80, (bpp == 4) ? DDPF_RGB | DDPF_ALPHAPIXELS : DDPF_RGB);

      fwrite(hdr, DDS_HEADERSIZE, 1, fp);
      
      if(num_mipmaps > 1)
      {
         size = get_mipmapped_size(drawable->width, drawable->height, bpp,
                                   0, num_mipmaps, DDS_COMPRESS_NONE);
         dst = malloc(size);
         generate_mipmaps(dst, src, drawable->width, drawable->height, bpp,
                          num_mipmaps);

         for(i = 0; i < num_mipmaps; ++i)
         {
            size = get_mipmapped_size(drawable->width, drawable->height, bpp,
                                      i, 1, DDS_COMPRESS_NONE);
            fwrite(dst + offset, 1, size, fp);
            offset += size;
            if(interactive_dds)
               gimp_progress_update((double)i / (double)num_mipmaps);
         }
         
         free(dst);
      }
      else
      {
         for(y = 0; y < drawable->height; ++y)
         {
            fwrite(src + (y * rowbytes), 1, rowbytes, fp);
            if(interactive_dds && ((y % 10) == 0))
               gimp_progress_update((double)y / (double)drawable->height);
         }
      }
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

      size = ((drawable->width + 3) >> 2) * ((drawable->height + 3) >> 2);
      size *= (ddsvals.compression == DDS_COMPRESS_DXT1) ? 8 : 16;

      PUT32(hdr + 20, size);
      
      fwrite(hdr, DDS_HEADERSIZE, 1, fp);
      
      size = get_mipmapped_size(drawable->width, drawable->height, bpp,
                                0, num_mipmaps, ddsvals.compression);
      
      dst = malloc(size);
      dxt_compress(dst, src, ddsvals.compression, drawable->width,
                   drawable->height, bpp, num_mipmaps);
      
      for(i = 0; i < num_mipmaps; ++i)
      {
         size = get_mipmapped_size(drawable->width, drawable->height, bpp,
                                   i, 1, ddsvals.compression);
         fwrite(dst + offset, 1, size, fp);
         offset += size;
         if(interactive_dds)
            gimp_progress_update((double)i / (double)num_mipmaps);
      }
      
      free(dst);
   }

   if(interactive_dds)
      gimp_progress_update(1.0);

   if(src != pixels)
      free(src);
    
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

static void mipmaps_clicked(GtkWidget *widget, gpointer data)
{
   ddsvals.mipmaps = !ddsvals.mipmaps;
}

static gint save_dialog(gint32 drawable)
{
   GtkWidget *dlg;
   GtkWidget *vbox;
   GtkWidget *table;
   GtkWidget *label;
   GtkWidget *opt;
   GtkWidget *menu;
   GtkWidget *menuitem;
   GtkWidget *check;
   
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
   if(gimp_drawable_has_alpha(drawable))
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
   
   check = gtk_check_button_new_with_label("Generate mipmaps");
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), ddsvals.mipmaps);
   gtk_box_pack_start(GTK_BOX(vbox), check, 0, 0, 0);
   gtk_signal_connect(GTK_OBJECT(check), "clicked",
                      GTK_SIGNAL_FUNC(mipmaps_clicked), 0);
   gtk_widget_show(check);
   
   gtk_widget_show(dlg);
   
   runme = 0;
   
   gtk_main();
   
   return(runme);
}

