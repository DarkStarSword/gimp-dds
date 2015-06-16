## Introduction ##

This article intends to compare and explain the various methods the gimp-dds plugin can use to generate higher quality DXT compressed images.

## Details ##

While in most cases, DXT compression can produce quite acceptable quality on its own, there are some cases where more quality is desired for a particular image.  There are a few tricks you can use to preserve more of the quality of an image, at the expense of adding instructions to a shader to obtain the RGB color for display.  The gimp-dds plugin supports two methods that can aid in the creation of these assets.

One method is to store your image in the YCoCg color-space, with Y (luma) in the alpha channel, and `CoCg` (chroma) in the red and green channels.  The result is then compressed with DXT5.  This method is described in more detail in [this article](http://developer.nvidia.com/object/real-time-ycocg-dxt-compression.html) from NVIDIA.  This method produces the best results, but requires a minimum of 3 added instructions in a shader to decode, or 6 if you use the YCoCg "scaled" method.

The other method is a trick I came up with called "Alpha exponent" can preserve some quality too, and is cheaper to decode in a shader (1 added instruction).  I am not sure if I am really the first person to come up with this trick, as it is very simple.  The method works by finding the maximum R, G or B value in each pixel, and dividing each channel with the result.  This maximum value is then stored in the alpha channel.  The same result can be achieved by using GIMP's "Color to alpha" function in the "Colors" menu, and selecting black as the color.  The result is then compressed with DXT5.  Decoding in the shader is as simple as multiplying the RGB components by the alpha channel.
```
color.rgb *= color.a;
```
I find that this method works great for images such as skybox cube maps.  It removes a lot of banding artifacts DXT1 produces that you usually find in the horizon gradient.  And if the skybox covers a lot of the screen, it won't eat a lot of your fill-rate as only 1 extra instruction is used to decode the image.  It makes a perfect fit when running on older or slower hardware.

Below are some examples of the reduction of artifacts when using these various methods.  A reference image is compressed with each method, then decompressed and finding the difference from this result and the reference image.
<table border='0'>
<blockquote><tr align='center'>
<blockquote><td></td>
<td><b>Difference from reference image (greatly exaggerated to show effect)</b></td>
</blockquote></tr>
<tr>
<blockquote><td><img src='http://gimp-dds.googlecode.com/svn/wiki/images/dxt-compression-quality/reference.jpg' /></td>
<td><img src='http://gimp-dds.googlecode.com/svn/wiki/images/dxt-compression-quality/difference-dxt1.jpg' /></td>
<td><img src='http://gimp-dds.googlecode.com/svn/wiki/images/dxt-compression-quality/difference-aexp.jpg' /></td>
<td><img src='http://gimp-dds.googlecode.com/svn/wiki/images/dxt-compression-quality/difference-ycocgs.jpg' /></td>
</blockquote></tr>
<tr align='center'>
<blockquote><td><b>Reference image</b></td>
<td><b>DXT1</b></td>
<td><b>AExp (DXT5)</b></td>
<td><b>YCoCg scaled (DXT5)</b></td>
</blockquote></tr>
<tr align='center'>
<blockquote><td><b>PSNR</b></td>
<td><b>34.6102</b></td>
<td><b>37.0543</b></td>
<td><b>39.3863</b></td>
</blockquote></tr>
</table></blockquote>

The artifacts produced by DXT1 compression are immediately noticeable.  There is a lot of loss in fine detailed areas, and a lot of noise in the sky.  The AExp image shows a lot of reduction to the sky artifacts, and noticeable reduction of artifacts in fine detailed areas.  There is also notable reduction in high-contrast transition areas (where the mountains meet the sky).  The YCoCg method (and the clear winner in quality) has almost completely removed artifacts in the sky, and has greatly reduced artifacts in fine detailed areas and high-contrast transition areas.

## Conclusion ##

I hope this article has explained and presented these methods well and has shown the benefits of each.  The use of YCoCg is favored when maximum quality is desired, and the expense of the added instructions in a shader for decoding the image is of no concern.  The AExp method is great for preserving more quality than DXT1, and is cheap to decode in a shader making it ideal for slower or older hardware.  There are some other tricks you can use to store higher quality compressed images, but they are out of scope for this article.  You can read about these other methods in [this article](http://code.google.com/p/nvidia-texture-tools/wiki/HighQualityDXTCompression) on the nvidia-texture-tools project page.