/*
	DDS GIMP plugin

	Copyright (C) 2004-2012 Shawn Kirst <skirst@gmail.com>,
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
	the Free Software Foundation, 51 Franklin Street, Fifth Floor
	Boston, MA 02110-1301, USA.
*/

#include <libgimp/gimp.h>

static inline float saturate(float a)
{
   if(a < 0) a = 0;
   if(a > 1) a = 1;
   return(a);
}

void decode_ycocg_image(gint32 drawableID)
{
   GimpDrawable *drawable;
   GimpPixelRgn srgn, drgn;
   unsigned char *data;
   unsigned int i, w, h, num_pixels;

   const float offset = 0.5f * 256.0f / 255.0f;
   float Y, Co, Cg, R, G, B;

   drawable = gimp_drawable_get(drawableID);

   w = drawable->width;
   h = drawable->height;
   num_pixels = w * h;

   data = g_malloc(num_pixels * 4);

   gimp_pixel_rgn_init(&srgn, drawable, 0, 0, w, h, 0, 0);
   gimp_pixel_rgn_init(&drgn, drawable, 0, 0, w, h, 1, 1);

   gimp_pixel_rgn_get_rect(&srgn, data, 0, 0, w, h);

   gimp_progress_init("Decoding YCoCg pixels...");

   for(i = 0; i < num_pixels; ++i)
   {
      Y  = (float)data[4 * i + 3] / 255.0f;
      Co = (float)data[4 * i + 0] / 255.0f;
      Cg = (float)data[4 * i + 1] / 255.0f;

      /* convert YCoCg to RGB */
      Co -= offset;
      Cg -= offset;

      R = saturate(Y + Co - Cg);
      G = saturate(Y + Cg);
      B = saturate(Y - Co - Cg);

      /* copy new alpha from blue */
      data[4 * i + 3] = data[4 * i + 2];

      data[4 * i + 0] = (unsigned char)(R * 255.0f);
      data[4 * i + 1] = (unsigned char)(G * 255.0f);
      data[4 * i + 2] = (unsigned char)(B * 255.0f);

      if((i & 255) == 0)
         gimp_progress_update((float)i / (float)num_pixels);
   }

   gimp_pixel_rgn_set_rect(&drgn, data, 0, 0, w, h);

   gimp_progress_update(1.0);

   gimp_drawable_flush(drawable);
   gimp_drawable_merge_shadow(drawable->drawable_id, 1);
   gimp_drawable_update(drawable->drawable_id, 0, 0, w, h);
   gimp_drawable_detach(drawable);

   g_free(data);
}

void decode_ycocg_scaled_image(gint32 drawableID)
{
   GimpDrawable *drawable;
   GimpPixelRgn srgn, drgn;
   unsigned char *data;
   unsigned int i, w, h, num_pixels;

   const float offset = 0.5f * 256.0f / 255.0f;
   float Y, Co, Cg, R, G, B, s;

   drawable = gimp_drawable_get(drawableID);

   w = drawable->width;
   h = drawable->height;
   num_pixels = w * h;

   data = g_malloc(num_pixels * 4);

   gimp_pixel_rgn_init(&srgn, drawable, 0, 0, w, h, 0, 0);
   gimp_pixel_rgn_init(&drgn, drawable, 0, 0, w, h, 1, 1);

   gimp_pixel_rgn_get_rect(&srgn, data, 0, 0, w, h);

   gimp_progress_init("Decoding YCoCg (scaled) pixels...");

   for(i = 0; i < num_pixels; ++i)
   {
      Y  = (float)data[4 * i + 3] / 255.0f;
      Co = (float)data[4 * i + 0] / 255.0f;
      Cg = (float)data[4 * i + 1] / 255.0f;
      s  = (float)data[4 * i + 2] / 255.0f;

      /* convert YCoCg to RGB */
      s = 1.0f / ((255.0f / 8.0f) * s + 1.0f);

      Co = (Co - offset) * s;
      Cg = (Cg - offset) * s;

      R = saturate(Y + Co - Cg);
      G = saturate(Y + Cg);
      B = saturate(Y - Co - Cg);

      data[4 * i + 0] = (unsigned char)(R * 255.0f);
      data[4 * i + 1] = (unsigned char)(G * 255.0f);
      data[4 * i + 2] = (unsigned char)(B * 255.0f);

      /* set alpha to 1 */
      data[4 * i + 3] = 255;

      if((i & 255) == 0)
         gimp_progress_update((float)i / (float)num_pixels);
   }

   gimp_pixel_rgn_set_rect(&drgn, data, 0, 0, w, h);

   gimp_progress_update(1.0);

   gimp_drawable_flush(drawable);
   gimp_drawable_merge_shadow(drawable->drawable_id, 1);
   gimp_drawable_update(drawable->drawable_id, 0, 0, w, h);
   gimp_drawable_detach(drawable);

   g_free(data);
}

void decode_alpha_exp_image(gint32 drawableID)
{
   GimpDrawable *drawable;
   GimpPixelRgn srgn, drgn;
   unsigned char *data;
   unsigned int i, w, h, num_pixels;
   int R, G, B, A;

   drawable = gimp_drawable_get(drawableID);

   w = drawable->width;
   h = drawable->height;
   num_pixels = w * h;

   data = g_malloc(num_pixels * 4);

   gimp_pixel_rgn_init(&srgn, drawable, 0, 0, w, h, 0, 0);
   gimp_pixel_rgn_init(&drgn, drawable, 0, 0, w, h, 1, 1);

   gimp_pixel_rgn_get_rect(&srgn, data, 0, 0, w, h);

   gimp_progress_init("Decoding Alpha-exponent pixels...");

   for(i = 0; i < num_pixels; ++i)
   {
      R = data[4 * i + 0];
      G = data[4 * i + 1];
      B = data[4 * i + 2];
      A = data[4 * i + 3];

      R = (R * A + 1) >> 8;
      G = (G * A + 1) >> 8;
      B = (B * A + 1) >> 8;
      A = 255;

      data[4 * i + 0] = R & 0xff;
      data[4 * i + 1] = G & 0xff;
      data[4 * i + 2] = B & 0xff;
      data[4 * i + 3] = A;

      if((i & 255) == 0)
         gimp_progress_update((float)i / (float)num_pixels);
   }

   gimp_pixel_rgn_set_rect(&drgn, data, 0, 0, w, h);

   gimp_progress_update(1.0);

   gimp_drawable_flush(drawable);
   gimp_drawable_merge_shadow(drawable->drawable_id, 1);
   gimp_drawable_update(drawable->drawable_id, 0, 0, w, h);
   gimp_drawable_detach(drawable);

   g_free(data);
}
