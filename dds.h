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

#ifndef __DDS_H
#define __DDS_H

typedef enum
{
   DDS_COMPRESS_NONE = 0,
   DDS_COMPRESS_DXT1,
   DDS_COMPRESS_DXT3,
   DDS_COMPRESS_DXT5
} DDS_COMPRESSION_TYPE;

#define DDS_HEADERSIZE             128

#define DDSD_CAPS                  0x00000001
#define DDSD_HEIGHT                0x00000002
#define DDSD_WIDTH                 0x00000004
#define DDSD_PITCH                 0x00000008
#define DDSD_PIXELFORMAT           0x00001000
#define DDSD_MIPMAPCOUNT           0x00020000
#define DDSD_LINEARSIZE            0x00080000
#define DDSD_DEPTH                 0x00800000

#define DDPF_ALPHAPIXELS           0x00000001
#define DDPF_FOURCC                0x00000004
#define DDPF_RGB                   0x00000040

#define DDSCAPS_COMPLEX            0x00000008
#define DDSCAPS_TEXTURE            0x00001000
#define DDSCAPS_MIPMAP             0x00400000

#define DDSCAPS2_CUBEMAP           0x00000200
#define DDSCAPS2_CUBEMAP_POSITIVEX 0x00000400
#define DDSCAPS2_CUBEMAP_NEGATIVEX 0x00000800
#define DDSCAPS2_CUBEMAP_POSITIVEY 0x00001000
#define DDSCAPS2_CUBEMAP_NEGATIVEY 0x00002000
#define DDSCAPS2_CUBEMAP_POSITIVEZ 0x00004000
#define DDSCAPS2_CUBEMAP_NEGATIVEZ 0x00008000
#define DDSCAPS2_VOLUME            0x00200000

typedef struct __attribute__((packed))
{
   unsigned int size;
   unsigned int flags;
   char fourcc[4];
   unsigned int bpp;
   unsigned int rmask;
   unsigned int gmask;
   unsigned int bmask;
   unsigned int amask;
} dds_pixel_format_t;

typedef struct __attribute__((packed))
{
   unsigned int caps1;
   unsigned int caps2;
   unsigned int reserved[2];
} dds_caps_t;

typedef struct __attribute__((packed))
{
   char magic[4];
   unsigned int size;
   unsigned int flags;
   unsigned int height;
   unsigned int width;
   unsigned int pitch_or_linsize;
   unsigned int depth;
   unsigned int num_mipmaps;
   unsigned char reserved[4 * 11];
   dds_pixel_format_t pixelfmt;
   dds_caps_t caps;
   unsigned int reserved2;
} dds_header_t;

#endif
