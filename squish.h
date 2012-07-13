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
   the Free Software Foundation, 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301 USA.
*/

#ifndef SQUISH_H
#define SQUISH_H

#define SQUISH_DXT1              0x00000001
#define SQUISH_DXT3              0x00000002
#define SQUISH_DXT5              0x00000004
#define SQUISH_WEIGHTBYALPHA     0x00000008
#define SQUISH_PERCEPTUALMETRIC  0x00000010

void squish_compress(unsigned char *dst, const unsigned char *block, int flags);

#endif
