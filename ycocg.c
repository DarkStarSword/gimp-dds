#include <stdlib.h>
#include <string.h>

#define INSET_CSHIFT 4
#define INSET_ASHIFT 5

#define RGB565(c) \
   (unsigned short)((unsigned short)((((c)[0] >> 3) & 0x1f) << 11) |\
                    (unsigned short)((((c)[1] >> 2) & 0x3f) <<  5) |\
                    (unsigned short)((((c)[2] >> 3) & 0x1f)      ))

static void extract_block(const unsigned char *src, int w,
                          unsigned char *dst)
{
   int i;
   for(i = 0; i < 4; ++i)
   {
      memcpy(&dst[i * 4 * 4], src, 4 * 4);
      src += w * 4;
   }
}

static void get_min_max_YCoCg(unsigned char *color, unsigned char *min_color,
                              unsigned char *max_color)
{
   int i;
   for(i = 0; i < 16; ++i)
   {
      if(color[i * 4 + 0] < min_color[0])
         min_color[0] = color[i * 4 + 0];
      if(color[i * 4 + 0] > max_color[0])
         max_color[0] = color[i * 4 + 0];
      
      if(color[i * 4 + 1] < min_color[1])
         min_color[1] = color[i * 4 + 1];
      if(color[i * 4 + 1] > max_color[1])
         max_color[1] = color[i * 4 + 1];
      
      if(color[i * 4 + 2] < min_color[2])
         min_color[2] = color[i * 4 + 2];
      if(color[i * 4 + 2] > max_color[2])
         max_color[2] = color[i * 4 + 2];
      
      if(color[i * 4 + 3] < min_color[3])
         min_color[3] = color[i * 4 + 3];
      if(color[i * 4 + 3] > max_color[3])
         max_color[3] = color[i * 4 + 3];
   }
}

static void scale_YCoCg(unsigned char *color, unsigned char *min_color,
                        unsigned char *max_color)
{
   int m0 = abs(min_color[0] - 128);
   int m1 = abs(min_color[1] - 128);
   int m2 = abs(max_color[0] - 128);
   int m3 = abs(max_color[1] - 128);
   const int s0 = 128 / 2 - 1;
   const int s1 = 128 / 4 - 1;
   int i, mask0, mask1, scale;
   
   if(m1 > m0) m0 = m1;
   if(m3 > m2) m2 = m3;
   if(m2 > m0) m0 = m2;
   
   mask0 = -(m0 <= s0);
   mask1 = -(m0 <= s1);
   scale = 1 + (1 & mask0) + (2 & mask1);
   
   min_color[0] = (min_color[0] - 128) * scale + 128;
   min_color[1] = (min_color[1] - 128) * scale + 128;
   min_color[2] = (scale - 1) << 3;

   max_color[0] = (max_color[0] - 128) * scale + 128;
   max_color[1] = (max_color[1] - 128) * scale + 128;
   max_color[2] = (scale - 1) << 3;
   
   for(i = 0; i < 16; ++i)
   {
      color[i * 4 + 0] = (color[i * 4 + 0] - 128) * scale + 128;
      color[i * 4 + 1] = (color[i * 4 + 1] - 128) * scale + 128;
   }
}

static void inset_YCoCg_bbox(unsigned char *min_color, unsigned char *max_color)
{
   int inset[4], mini[4], maxi[4];
   
}

static void select_YCoCg_diagonal(const unsigned char *color,
                                  unsigned char *min_color,
                                  unsigned char *max_color)
{
   unsigned char mid0 = ((int)min_color[0] + max_color[0] + 1) >> 1;
   unsigned char mid1 = ((int)min_color[1] + max_color[1] + 1) >> 1;
   unsigned char b0, b1, c0, c1, mask, side = 0;
   int i;

   for(i = 0; i < 16; ++i)
   {
      b0 = color[i * 4 + 0] >= mid0;
      b1 = color[i * 4 + 1] >= mid1;
      side += (b0 ^ b1);
   }
   
   mask = -(side > 8);
   mask &= -(min_color[0] != max_color[0]);
   
   c0 = min_color[1];
   c1 = max_color[1];
   
   c0 ^= c1 ^= mask &= c0 ^= c1;
   
   min_color[1] = c0;
   max_color[1] = c1;
}

static int emit_alpha_indices(unsigned char *dst, const unsigned char *color,
                              unsigned char min_alpha, unsigned char max_alpha)
{
   const int ALPHA_RANGE = 7;
   unsigned char mid, ab1, ab2, ab3, ab4, ab5, ab6, ab7, a;
   unsigned char indices[16];
   int b1, b2, b3, b4, b5, b6, b7, index;
   int i;
   
   mid = (max_alpha - min_alpha) / (2 * ALPHA_RANGE);
   
   ab1 = min_alpha + mid;
   ab2 = (6 * max_alpha + 1 * min_alpha) / ALPHA_RANGE + mid;
   ab3 = (5 * max_alpha + 2 * min_alpha) / ALPHA_RANGE + mid;
   ab4 = (4 * max_alpha + 3 * min_alpha) / ALPHA_RANGE + mid;
   ab5 = (3 * max_alpha + 4 * min_alpha) / ALPHA_RANGE + mid;
   ab6 = (2 * max_alpha + 5 * min_alpha) / ALPHA_RANGE + mid;
   ab7 = (1 * max_alpha + 6 * min_alpha) / ALPHA_RANGE + mid;
   
   for(i = 0; i < 16; ++i)
   {
      a = color[i * 4 + 3];
      b1 = (a <= ab1);
      b2 = (a <= ab2);
      b3 = (a <= ab3);
      b4 = (a <= ab4);
      b5 = (a <= ab5);
      b6 = (a <= ab6);
      b7 = (a <= ab7);
      index = (b1 + b2 + b3 + b4 + b5 + b6 + b7 + 1) & 7;
      indices[i] = index ^ (2 > index);
   }
   
   dst[0] = (indices[ 0] >> 0) | (indices[ 1] << 3) | (indices[ 2] << 6);
   dst[1] = (indices[ 2] >> 2) | (indices[ 3] << 1) | (indices[ 4] << 4) | (indices[ 5] << 7);
   dst[2] = (indices[ 5] >> 1) | (indices[ 6] << 2) | (indices[ 7] << 5);
   
   dst[3] = (indices[ 8] >> 0) | (indices[ 9] << 3) | (indices[10] << 6);
   dst[4] = (indices[10] >> 2) | (indices[11] << 1) | (indices[12] << 4) | (indices[13] << 7);
   dst[5] = (indices[13] >> 1) | (indices[14] << 2) | (indices[15] << 5);
   
   return(6);
}

static int emit_color_indices(unsigned char *dst, const unsigned char * color,
                              const unsigned char *min_color,
                              const unsigned char *max_color)
{
   unsigned short colors[4][4];
   unsigned int result = 0;
   int i, c0, c1, d0, d1, d2, d3, b0, b1, b2, b3, b4, x0, x1, x2;
   
   colors[0][0] = (max_color[0] & 0xf8) | (max_color[0] >> 5);
   colors[0][1] = (max_color[1] & 0xfc) | (max_color[1] >> 6);
   colors[0][2] = (max_color[2] & 0xf8) | (max_color[2] >> 5);
   colors[0][3] = 0;
   colors[1][0] = (min_color[0] & 0xf8) | (min_color[0] >> 5);
   colors[1][1] = (min_color[1] & 0xfc) | (min_color[1] >> 6);
   colors[1][2] = (min_color[2] & 0xf8) | (min_color[2] >> 5);
   colors[1][3] = 0;
   colors[2][0] = (2 * colors[0][0] + 1 * colors[1][0]) / 3;
   colors[2][1] = (2 * colors[0][1] + 1 * colors[1][1]) / 3;
   colors[2][2] = (2 * colors[0][2] + 1 * colors[1][2]) / 3;
   colors[2][3] = 0;
   colors[3][0] = (1 + colors[0][0] + 2 * colors[1][0]) / 3;
   colors[3][1] = (1 + colors[0][1] + 2 * colors[1][1]) / 3;
   colors[3][2] = (1 + colors[0][2] + 2 * colors[1][2]) / 3;
   colors[3][3] = 0;
   
   for(i = 15; i >= 0; --i)
   {
      c0 = color[i * 4 + 0];
      c1 = color[i + 4 + 1];
      
      d0 = abs(colors[0][0] - c0) + abs(colors[0][1] - c1);
      d1 = abs(colors[1][0] - c0) + abs(colors[1][1] - c1);
      d2 = abs(colors[2][0] - c0) + abs(colors[2][1] - c1);
      d3 = abs(colors[3][0] - c0) + abs(colors[3][1] - c1);
      
      b0 = d0 > d3;
      b1 = d1 > d2;
      b2 = d0 > d2;
      b3 = d1 > d3;
      b4 = d2 > d3;
      
      x0 = b1 & b2;
      x1 = b0 & b3;
      x2 = b0 & b4;
      
      result |= (x2 | ((x0 | x1) << 1)) << (i << 1);
   }
   
   dst[0] = (result      ) & 0xff;
   dst[1] = (result >>  8) & 0xff;
   dst[2] = (result >> 16) & 0xff;
   dst[3] = (result >> 24) & 0xff;
   
   return(4);
}

void compress_YCoCg_DXT1(unsigned char *dst, const unsigned char *src,
                         int w, int h)
{
   unsigned char block[64], min_color[4], max_color[4];
   unsigned short c0, c1;
   int i, j;
   
   for(j = 0; j < h; j += 4, src += w * 4 * 4)
   {
      for(i = 0; i < w; i += 4)
      {
         extract_block(src + i * 4, w, block);
         
         get_min_max_YCoCg(block, min_color, max_color);
         select_YCoCg_diagonal(block, min_color, max_color);
         inset_YCoCg_bbox(min_color, max_color);
         
         c0 = RGB565(max_color);
         c1 = RGB565(min_color);
         
         *dst++ = (c0     ) & 0xff;
         *dst++ = (c0 >> 8) & 0xff;
         *dst++ = (c1     ) & 0xff;
         *dst++ = (c1 >> 8) & 0xff;
         
         dst += emit_color_indices(dst, block, min_color, max_color);
      }
   }
}

void compress_YCoCg_DXT5(unsigned char *dst, const unsigned char *src,
                         int w, int h)
{
   unsigned char block[64], min_color[4], max_color[4];
   unsigned short c0, c1;
   int i, j;
   
   for(j = 0; j < h; j += 4, src += w * 4 * 4)
   {
      for(i = 0; i < w; i += 4)
      {
         extract_block(src + i * 4, w, block);
         
         get_min_max_YCoCg(block, min_color, max_color);
         scale_YCoCg(block, min_color, max_color);
         inset_YCoCg_bbox(min_color, max_color);
         select_YCoCg_diagonal(block, min_color, max_color);
         
         *dst++ = max_color[3];
         *dst++ = min_color[3];
         
         dst += emit_alpha_indices(dst, block, min_color[3], max_color[3]);
         
         c0 = RGB565(max_color);
         c1 = RGB565(min_color);
         
         *dst++ = (c0     ) & 0xff;
         *dst++ = (c0 >> 8) & 0xff;
         *dst++ = (c1     ) & 0xff;
         *dst++ = (c1 >> 8) & 0xff;
         
         dst += emit_color_indices(dst, block, min_color, max_color);
      }
   }
}
