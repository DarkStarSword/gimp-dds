# GIMP DDS Plugin #

## Contents ##
  * [Overview](#Overview.md)
  * [Features](#Features.md)
  * [Roadmap](#Roadmap.md)
  * [Downloads](#Downloads.md)
  * [Changes](#Changes.md)
  * [Screenshots](#Screenshots.md)

## Overview ##
This is a plugin for GIMP version 2.8.x.  It allows you to load and save images in the Direct Draw Surface (DDS) format.

## Features ##
  * Load/save DDS files, optionally using DirectX texture compression (DXT)
  * Optional automatic mipmap generation when saving
  * Load mipmaps into separate layers
  * Load cube map faces and volume map slices into separate layers
  * Save cube maps and volume maps with automatic mipmap generation support
  * Save image with a custom pixel format
  * Non-power-of-two image loading and saving support with automatic mipmap generation support
  * Compliant with DirectX 10 block compressed (BC) formats

## Roadmap ##
**3.x**:
3.x will be maintenance releases with only minor feature enhancements.

**4.x**:
4.x will be the next major version of the plugin.  New features will include:
  * GIMP 2.9/2.10 support
  * 16 and 32-bit integer/floating point image support

## Downloads ##
Downloads are provided in either source code form, or as a pre-built Windows binary.  You can find them to the left on the side bar, or under the Downloads tab.

## Changes ##
**3.0.1** (2013-12-03) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-3.0.1)
  * Minor bug fix release

**3.0.0** (2013-11-28) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-3.0.0)
  * New major "turkey-day" release!
  * New DXT compressor
  * OpenMP support
  * New mipmap generator, with better support for alpha mipmaps
  * Support reading/writing DX10 texture arrays

**2.2.1** (2012-07-31) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.2.1)
  * Bugfix release.  Fixes artifacts introduced by the compression code in some images.

**2.2.0** (2012-07-26) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.2.0)
  * Compressed non-power-of-2 texture support
  * Fix compressed mipmap images with width or height less than 4

**2.1.0** (2012-06-23) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.1.0)
  * GIMP 2.8 support
  * DXT compression quality enhancements
  * Various bug fixes
  * Support for saving existing mipmap chain (2D images only)
  * Native Windows 64-bit binaries

**2.0.9** (2010-03-11) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.9)
  * Fixed bug in the box filter mipmap generation that caused invalid results. This is the default mipmap filter, so an upgrade is recommended. Thanks to Michalis Kamburelis and Vincent Fourmond for bringing this issue to my attention.

**2.0.8** (2010-02-24) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.8)
  * Various bug-fixes
  * Fixed bug in YCoCg encoder (Thanks Liguo!)
  * Added support for gamma-correct mipmap filtering in the advanced options

**2.0.7** (2008-12-12) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.7)
  * First release for GIMP 2.6
  * Now properly generates a full mimpap chain for compressed images
  * Added some filters to decode YCoCg images. These are found in the Filters->Colors menu
  * Fixed saving non-compressed YCoCg images

**2.0.6** (2008-06-03) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.6)
  * Fixed mipmap generation for images with differing dimensions using the Lanczos filter
  * Fixed mipmap generation for compressed images (correctly stops at 4x4 rather than 1x1)
  * Allow compression of non-power-of-two images, if the image dimensions are a multiple of 4. Mipmaps currently not supported.

**2.0.5** (2008-05-09) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.5)
  * Added box mipmap filter and made it default.
  * Added Lanczos mipmap filter. Produces very high quality mipmaps at the cost of a little speed.

**2.0.4** (2008-05-07) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.4)
  * Added selectable mipmap filter in advanced options
  * Improved mipmap quality

**2.0.3** (2008-04-11) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.3)
  * Fixed writing BGR8 format
  * Added dialog when opening DDS files to allow choosing whether to load mipmap layers or not
  * Documentation for code found in dxt.c (Thanks to Fabian 'ryg' Giesen)

**2.0.2** (2007-11-29) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.2)
  * Added support for reading and writing alpha-only (A8) images
  * Some minor fixes and optimizations for the DXT compressor

**2.0.1** (2007-11-07) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0.1)
  * Fixed saving DXT1 compressed images with alpha
  * Added YCoCg scaled compressed format. This format adds the extensions described in the NVIDIA/iD paper for greater color precision. Use the normal YCoCg format for images that need high quality compression, at the loss of some color precision, but with the alpha channel stored in the blue component. Otherwise, use the YCoCg scaled format for the highest quality.

**2.0** (2007-11-02) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-2.0)
  * GIMP 2.4 support! GIMP 2.4 is now the only supported GIMP version. For previous versions of the GIMP, please use the 1.2.1 release.
  * Completely re-written DXT compression backend. Now uses 100% software compression without the need for an external library. This also removes the dependence on OpenGL and GLEW. The quality is very decent with the default settings, but a new advanced settings area in the save dialog allows you to tweak some parameters to the DXT compressor to fine-tune your image.
  * Added DirectX 10 compressed format support.
  * Added two new compressed format options using DXT5 compression. One stores the image in YCoCg colorspace as described in [this](http://developer.nvidia.com/object/real-time-ycocg-dxt-compression.html) paper. The other stores a shared exponent in the alpha channel for added color precision. The benefit of the shared exponent version over the YCoCg version is that only one instruction is required in a fragment shader to decode (color.rgb `*=` color.a), great for when your instruction budget is low. Otherwise, the YCoCg format is superiour in quality, but more expensive to decode in a shader.

**1.2.1** (2007-01-31) [SVN](http://code.google.com/p/gimp-dds/source/browse/#svn/tags/release-1.2.1)
  * Fixed non-interactive saving
  * Added option to save dialog that allows you to specify the transparent color index when saving paletted images

**1.2** (2006-09-18)
  * Added support for loading/saving 3Dc compressed images. This feature can be used even on systems with video cards that do not support this format in hardware.
  * Added optional support for software compression. Requires libtxc\_dxtn.so on Linux or dxtn.dll on Windows

**1.0.3** (2006-08-15)
  * Added proper support for 8-bit paletted images. They are now loaded into GIMP indexed mode images. Writing support has also been added with mipmap generation. Indexed cube maps and volume maps are also supported
  * Removed requirement for the GL\_SGIS\_generate\_mipmap extension
  * Removed the "DDSD\_PITCH or DDSD\_LINEARSIZE not set" warning message
  * Fixed non-power-of-two volume map writing and mipmap generation

**1.0.2** (2006-07-26)
  * Added support for reading 8-bit paletted images. This was previously causing a segfault. Thanks to Jason for pointing this out.
  * Added a warning when the pixel format of an image cannot be determined

**1.0.1** (2005-12-30)
  * Fixed a bug when saving uncompressed images. Some pixel formats were being written incorrectly, and the plugin couldn't read them as a result.

**1.0** (2005-09-16)
  * Added support to save cube and volume maps. To save a cube map, have 6 layers all the same size and format, and use special strings inside the layer name to identify what cube face the layer is to the plugin. These strings are (and you can use any of them): "positive x", "negative x", "positive y", "negative y", "positive z", "negative z", "pos x", "neg x", "pos y", "neg y", "pos z", "neg z", "+x", "-x", "+y", "-y", "+z" and "-z". To save a volume map, you need an image with more than one layer, and all the same size and format. Currently compression is not supported for volume maps.
  * Added support to load and save non-power-of-two sized images with automatic mipmap generation support.
  * Added support to save images with a custom pixel format
  * Added support to load images with custom pixel formats
  * Added better detection of image properties when loading. You should now be able to open just about any DDS image
  * Cleaned up non-interactive plugin interface for GIMP scripting
  * Fixed many, many bugs

**0.4** (2005-03-30)
  * Fixed a bug that was causing an assertion failure in gimp\_drawable\_get

**0.3** (2004-10-22)
  * Added feature to swap red and alpha channels on save.
This is useful for saving normal maps in DXT5 compressed format to preserve more quality.
  * Fixed a minor issue in reading support

**0.2** (2004-10-14)
  * Added saving support
  * Various code cleanups

**0.1** (2004-10-12)
  * Initial import of Arne's code

## Screenshots ##
<table cellspacing='10'>
<blockquote><tr align='center'>
<blockquote><td><b>Version 3.x</b></td>
</blockquote></tr>
<tr>
<blockquote><td valign='top'><img src='http://gimp-dds.googlecode.com/svn/wiki/images/screenshot-release.png' /></td>
</blockquote></tr>
</table>