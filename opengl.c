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

#include <stdlib.h>
#include <string.h>
#include <GL/glew.h>
#include <GL/glut.h>

char *initialize_opengl(void)
{
   int argc = 1, err;
   char *argv[] = { "dds" };

   glutInit(&argc, argv);
   glutInitDisplayMode(GLUT_RGBA);
   glutInitWindowSize(1, 1);
   glutCreateWindow("GIMP DDS");

   err = glewInit();
   if(err != GLEW_OK)
      return((char*)glewGetErrorString(err));

   glPixelStorei(GL_PACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

   if(GLEW_SGIS_generate_mipmap)
      glHint(GL_GENERATE_MIPMAP_HINT_SGIS, GL_NICEST);

   return(NULL);
}
