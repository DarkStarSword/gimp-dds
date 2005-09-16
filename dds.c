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

FILE *errFile;
gchar *prog_name = "dds";
gchar *filename;
gint interactive_dds;

static void query(void);
static void run(const gchar *name, gint nparams, const GimpParam *param,
					 gint *nreturn_vals, GimpParam **return_vals);

GimpPlugInInfo PLUG_IN_INFO =
{
	0, 0, query, run
};


DDSSaveVals ddsvals =
{
	DDS_COMPRESS_NONE, 0, 0, DDS_SAVE_SELECTED_LAYER, DDS_FORMAT_DEFAULT
};

MAIN()
	
static void query(void)
{
	static GimpParamDef load_args[]=
	{
		{GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
		{GIMP_PDB_STRING, "filename", "The name of the file to load"},
		{GIMP_PDB_STRING, "raw_filename", "The name entered"}
	};
	static GimpParamDef load_return_vals[]=
	{
		{GIMP_PDB_IMAGE, "image", "Output image"}
	};
	
	static gint nload_args = sizeof(load_args) / sizeof(load_args[0]);
	static gint nload_return_vals = sizeof(load_return_vals) /
		sizeof(load_return_vals[0]);
	
	static GimpParamDef save_args[]=
	{
		{GIMP_PDB_INT32, "run_mode", "Interactive, non-interactive"},
		{GIMP_PDB_IMAGE, "image", "Input image"},
		{GIMP_PDB_DRAWABLE, "drawable", "Drawable to save"},
		{GIMP_PDB_STRING, "filename", "The name of the file to save the image as"},
		{GIMP_PDB_STRING, "raw_filename", "The name entered"},
		{GIMP_PDB_INT32, "compression_format", "Compression format"},
		{GIMP_PDB_INT32, "generate_mipmaps", "Generate mipmaps"},
      {GIMP_PDB_INT32, "swap_ra", "Swap red and alpha channels (RGBA images only)"},
      {GIMP_PDB_INT32, "savetype", "How to save the image (0 = selected layer, 1 = cube map, 2 = volume map"},
      {GIMP_PDB_INT32, "format", "Custom pixel format (0 = default, 1 = R5G6B5, 2 = RGBA4, 3 = RGB5A1, 4 = RGB10A2)"}
	};
	static gint nsave_args = sizeof(save_args) / sizeof(save_args[0]);
	
	gimp_install_procedure("file_dds_load",
								  "Loads files in DDS image format",
								  "Loads files in DDS image format",
								  "Shawn Kirst",
								  "Shawn Kirst",
								  "2004",
								  "<Load>/DDS image",
								  0,
								  GIMP_PLUGIN,
								  nload_args, nload_return_vals,
								  load_args, load_return_vals);
   
	gimp_install_procedure("file_dds_save",
								  "Saves files in DDS image format",
								  "Saves files in DDS image format",
								  "Shawn Kirst",
								  "Shawn Kirst",
								  "2004",
								  "<Save>/DDS image",
								  "GRAY, RGB",
								  GIMP_PLUGIN,
								  nsave_args, 0,
								  save_args, 0);

	gimp_register_magic_load_handler("file_dds_load",
												"dds",
												"",
												"0,string,DDS");
   
	gimp_register_save_handler("file_dds_save",
										"dds",
										"");
}

static void run(const gchar *name, gint nparams, const GimpParam *param,
					 gint *nreturn_vals, GimpParam **return_vals)
{
	static GimpParam values[2];
	GimpRunMode run_mode;
	GimpPDBStatusType status = GIMP_PDB_SUCCESS;
	gint32 imageID;
	gint32 drawableID;
	GimpExportReturn export = GIMP_EXPORT_CANCEL;
   char *error;
	
	run_mode = param[0].data.d_int32;
	
	*nreturn_vals = 1;
	*return_vals = values;
	
	values[0].type = GIMP_PDB_STATUS;
	values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;
   
   error = initialize_opengl();
   if(error)
   {
      g_message(error);
      return;
   }
	
	if(!strcmp(name, "file_dds_load"))
	{
		switch(run_mode)
		{
			case GIMP_RUN_INTERACTIVE:
			   interactive_dds = 1;
			   break;
			case GIMP_RUN_NONINTERACTIVE:
			   interactive_dds = 0;
			   if(nparams != 3)
				   status = GIMP_PDB_CALLING_ERROR;
			   break;
			default:
			   break;
		}
		
		if(status == GIMP_PDB_SUCCESS)
		{
			imageID = read_dds(param[1].data.d_string);
			if(imageID != -1)
			{
				*nreturn_vals = 2;
				values[1].type = GIMP_PDB_IMAGE;
				values[1].data.d_image = imageID;
			}
			else
				status = GIMP_PDB_EXECUTION_ERROR;
		}
	}
	else if(!strcmp(name,"file_dds_save"))
	{
		imageID = param[1].data.d_int32;
		drawableID = param[2].data.d_int32;
		
		switch(run_mode)
		{
			case GIMP_RUN_INTERACTIVE:
			case GIMP_RUN_WITH_LAST_VALS:
			   gimp_ui_init("dds", 0);
			   export = gimp_export_image(&imageID, &drawableID, "DDS",
                                       (GIMP_EXPORT_CAN_HANDLE_RGB |
                                        GIMP_EXPORT_CAN_HANDLE_GRAY |
                                        GIMP_EXPORT_CAN_HANDLE_ALPHA |
                                        GIMP_EXPORT_CAN_HANDLE_LAYERS));
			   if(export == GIMP_EXPORT_CANCEL)
			   {
					values[0].data.d_status = GIMP_PDB_CANCEL;
					return;
				}
			default:
			   break;
		}
		
		switch(run_mode)
		{
			case GIMP_RUN_INTERACTIVE:
			   gimp_get_data("file_dds_save", &ddsvals);
			   interactive_dds = 1;
			   break;
			case GIMP_RUN_NONINTERACTIVE:
			   interactive_dds = 0;
			   if(nparams != 7)
				   status = GIMP_PDB_CALLING_ERROR;
			   else
			   {
					ddsvals.compression = param[5].data.d_int32;
					ddsvals.mipmaps = param[6].data.d_int32;
               ddsvals.swapRA = param[7].data.d_int32;
               ddsvals.savetype = param[8].data.d_int32;
               ddsvals.format = param[9].data.d_int32;
					if(ddsvals.compression < DDS_COMPRESS_NONE ||
						ddsvals.compression >= DDS_COMPRESS_MAX)
						status = GIMP_PDB_CALLING_ERROR;
               if(ddsvals.savetype < DDS_SAVE_SELECTED_LAYER ||
                  ddsvals.savetype >= DDS_SAVE_MAX)
                  status = GIMP_PDB_CALLING_ERROR;
               if(ddsvals.format < DDS_FORMAT_DEFAULT ||
                  ddsvals.format >= DDS_FORMAT_MAX)
                  status = GIMP_PDB_CALLING_ERROR;
				}
			   break;
			case GIMP_RUN_WITH_LAST_VALS:
			   gimp_get_data("file_dds_save", &ddsvals);
			   interactive_dds = 0;
			   break;
			default:
			   break;
		}
		
		if(status == GIMP_PDB_SUCCESS)
		{
			status = write_dds(param[3].data.d_string, imageID, drawableID);
			if(status == GIMP_PDB_SUCCESS)
				gimp_set_data("file_dds_save", &ddsvals, sizeof(ddsvals));
		}
		
		if(export == GIMP_EXPORT_EXPORT)
			gimp_image_delete(imageID);
	}
	else
		status = GIMP_PDB_CALLING_ERROR;

	values[0].data.d_status = status;
}

