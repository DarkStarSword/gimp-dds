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

#ifndef __DDSPLUGIN_H
#define __DDSPLUGIN_H

typedef struct
{
	int compression;
	int mipmaps;
   int swapRA;
} DDSSaveVals;

extern DDSSaveVals ddsvals;

extern gint32 read_dds(gchar *);
extern GimpPDBStatusType write_dds(gchar *, gint32, gint32);

extern gint interactive_dds;
extern gchar *prog_name;
extern gchar *filename;
extern FILE *errorFile;


#define GET32(buf) ((unsigned int)((buf)[0] | (buf)[1] << 8 | (buf)[2] << 16 | (buf)[3] << 24))
#define CHAR32(c0, c1, c2, c3) \
         ((unsigned int)(((c0) & 0xff)      ) | \
                        (((c1) & 0xff) <<  8) | \
                        (((c2) & 0xff) << 16) | \
                        (((c3) & 0xff) << 24))
#define PUT32(buf, l) \
   (buf)[0] = ((l) & 0x000000FF);\
	(buf)[1] = ((l) & 0x0000FF00) >> 8;\
	(buf)[2] = ((l) & 0x00FF0000) >> 16;\
	(buf)[3] = ((l) & 0xFF000000) >> 24;

#endif
