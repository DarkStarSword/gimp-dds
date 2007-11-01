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

#ifndef ENDIAN_H
#define ENDIAN_H

#define GETL64(buf) \
   (((unsigned long long)(buf)[0]      ) | \
    ((unsigned long long)(buf)[1] <<  8) | \
    ((unsigned long long)(buf)[2] << 16) | \
    ((unsigned long long)(buf)[3] << 24) | \
    ((unsigned long long)(buf)[4] << 32) | \
    ((unsigned long long)(buf)[5] << 40) | \
    ((unsigned long long)(buf)[6] << 48) | \
    ((unsigned long long)(buf)[7] << 56))

#define GETL32(buf) \
   (((unsigned int)(buf)[0]      ) | \
    ((unsigned int)(buf)[1] <<  8) | \
    ((unsigned int)(buf)[2] << 16) | \
    ((unsigned int)(buf)[3] << 24))

#define GETL16(buf) \
   (((unsigned short)(buf)[0]     ) | \
    ((unsigned short)(buf)[1] << 8))

#define CHAR32(c0, c1, c2, c3) \
         ((unsigned int)(((c0) & 0xff)      ) | \
                        (((c1) & 0xff) <<  8) | \
                        (((c2) & 0xff) << 16) | \
                        (((c3) & 0xff) << 24))

#define PUTL16(buf, s) \
   (buf)[0] = ((s)     ) & 0xff; \
   (buf)[1] = ((s) >> 8) & 0xff;

#define PUTL32(buf, l) \
   (buf)[0] = ((l)      ) & 0xff; \
	(buf)[1] = ((l) >>  8) & 0xff; \
	(buf)[2] = ((l) >> 16) & 0xff; \
	(buf)[3] = ((l) >> 24) & 0xff;

#endif
