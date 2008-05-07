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

#ifndef __DDSPLUGIN_H
#define __DDSPLUGIN_H

typedef struct
{
	int compression;
	int mipmaps;
   int savetype;
   int format;
   int transindex;
   int color_type;
   int dither;
   int mipmap_filter;
   int show_adv_opt;
} DDSWriteVals;

typedef struct
{
   int show_dialog;
   int mipmaps;
} DDSReadVals;

extern DDSWriteVals dds_write_vals;
extern DDSReadVals dds_read_vals;

extern GimpPDBStatusType read_dds(gchar *filename, gint32 *imageID);
extern GimpPDBStatusType write_dds(gchar *, gint32, gint32);

extern gint interactive_dds;
extern gchar *prog_name;
extern gchar *filename;
extern FILE *errorFile;

#define LOAD_PROC "file-dds-load"
#define SAVE_PROC "file-dds-save"

#endif
