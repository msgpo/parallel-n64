#include <stdint.h>

#include "glide64_gDP.h"
#include "Util.h"

void glide64gDPSetTextureImage(int32_t fmt, int32_t siz,
   int32_t width, int32_t addr)
{
   g_gdp.ti_format  = fmt;
   g_gdp.ti_size    = siz;
   g_gdp.ti_address = addr;
   g_gdp.ti_width   = width;
}

void glide64gDPSetTile(
      uint32_t fmt,
      uint32_t siz,
      uint32_t line,
      uint32_t tmem,
      uint32_t tile,
      uint32_t palette,
      uint32_t cmt,
      uint32_t maskt,
      uint32_t shiftt,
      uint32_t cms,
      uint32_t masks,
      uint32_t shifts )
{
   g_gdp.tile[tile].format   = fmt;
   g_gdp.tile[tile].size     = siz;
   g_gdp.tile[tile].line     = line;
   g_gdp.tile[tile].tmem     = tmem;
   g_gdp.tile[tile].palette  = palette;
   g_gdp.tile[tile].ct       = 1;
   g_gdp.tile[tile].mt       = cmt;
   g_gdp.tile[tile].mask_t   = maskt;
   g_gdp.tile[tile].shift_t  = shiftt;
   g_gdp.tile[tile].cs       = 1;
   g_gdp.tile[tile].ms       = cms;
   g_gdp.tile[tile].mask_s   = masks;
   g_gdp.tile[tile].shift_s  = shifts;
}

void glide64gDPSetTileSize(uint32_t tile, uint32_t uls, uint32_t ult,
      uint32_t lrs, uint32_t lrt)
{
   g_gdp.tile[tile].sh = uls;
   g_gdp.tile[tile].th = ult;
   g_gdp.tile[tile].sl = lrs;
   g_gdp.tile[tile].tl = lrt;
}

void glide64gDPSetScissor( uint32_t mode, float ulx, float uly, float lrx, float lry )
{
   g_gdp.__clip.xh = (uint32_t)ulx;
   g_gdp.__clip.yh = (uint32_t)uly;
   g_gdp.__clip.xl = (uint32_t)lrx;
   g_gdp.__clip.yl = (uint32_t)lry;
}

void glide64gDPLoadBlock( uint32_t tile, uint32_t ul_s, uint32_t ul_t,
      uint32_t lr_s, uint32_t dxt )
{
   uint32_t _dxt, addr, off, cnt;
   uint8_t *dst;

   if (rdp.skip_drawing)
      return;

   if (ucode5_texshiftaddr)
   {
      if (ucode5_texshift % ((lr_s+1)<<3))
      {
         g_gdp.ti_address -= ucode5_texshift;
         ucode5_texshiftaddr = 0;
         ucode5_texshift = 0;
         ucode5_texshiftcount = 0;
      }
      else
         ucode5_texshiftcount++;
   }

   rdp.addr[g_gdp.tile[tile].tmem] = g_gdp.ti_address;

   // ** DXT is used for swapping every other line
   /* double fdxt = (double)0x8000000F/(double)((uint32_t)(2047/(dxt-1))); // F for error
      uint32_t _dxt = (uint32_t)fdxt;*/

   // 0x00000800 -> 0x80000000 (so we can check the sign bit instead of the 11th bit)
   _dxt = dxt << 20;
   addr = RSP_SegmentToPhysical(g_gdp.ti_address);

   g_gdp.tile[tile].sh = ul_s;
   g_gdp.tile[tile].th = ul_t;
   g_gdp.tile[tile].sl = lr_s;

   rdp.timg.set_by = 0; /* load block */

   /* do a quick boundary check before copying 
    * to eliminate the possibility for exception */
   if (ul_s >= 512)
   {
      lr_s = 1; /* 1 so that it doesn't die on memcpy */
      ul_s = 511;
   }
   if (ul_s+lr_s > 512)
      lr_s = 512-ul_s;

   if (addr+(lr_s<<3) > BMASK+1)
      lr_s = (uint16_t)((BMASK-addr)>>3);

   /* angrylion's advice to use ul_s in texture image offset 
    * and cnt calculations.
    *
    * Helps to fix Vigilante 8 JPEG backgrounds and logos */
   off = g_gdp.ti_address + (ul_s << g_gdp.tile[tile].size >> 1);
   dst = ((uint8_t*)g_gdp.tmem) + (g_gdp.tile[tile].tmem<<3);
   cnt = lr_s-ul_s+1;
   if (g_gdp.tile[tile].size == 3)
      cnt <<= 1;

   if (g_gdp.ti_size == G_IM_SIZ_32b)
      LoadBlock32b(tile, ul_s, ul_t, lr_s, dxt);
   else
      loadBlock((uint32_t *)gfx_info.RDRAM, (uint32_t *)dst, off, _dxt, cnt);

   g_gdp.ti_address += cnt << 3;
   g_gdp.tile[tile].tl = ul_t + ((dxt*cnt)>>11);

   g_gdp.flags |= UPDATE_TEXTURE;
}

void glide64gDPLoadTile(uint32_t tile, uint32_t ul_s, uint32_t ul_t,
      uint32_t lr_s, uint32_t lr_t)
{
   uint32_t offs, height, width;
   int line_n;

   if (rdp.skip_drawing)
      return;

   rdp.timg.set_by                 = 1; /* load tile */

   rdp.addr[g_gdp.tile[tile].tmem] = g_gdp.ti_address;

   if (lr_s < ul_s || lr_t < ul_t)
      return;

   if ((settings.hacks&hack_Tonic) && tile == 7)
      glide64gDPSetTileSize(
            0,          /* tile */
            ul_s,       /* ulx  */
            ul_t,       /* uly  */
            lr_s,       /* lrx  */
            lr_t        /* lry  */
            );

   height = lr_t - ul_t + 1; // get height
   width = lr_s - ul_s + 1;

#ifdef TEXTURE_FILTER
   LOAD_TILE_INFO &info = rdp.load_info[g_gdp.tile[tile].tmem];
   info.tile_ul_s       = ul_s;
   info.tile_ul_t       = ul_t;
   info.tile_width      = (g_gdp.tile[tile].ms ? MIN((uint16_t)width,   1 << g_gdp.tile[tile].ms) : (uint16_t)width);
   info.tile_height     = (g_gdp.tile[tile].mt ? MIN((uint16_t)height, 1 << g_gdp.tile[tile].mt) : (uint16_t)height);

   if (settings.hacks&hack_MK64) {
      if (info.tile_width%2)
         info.tile_width--;
      if (info.tile_height%2)
         info.tile_height--;
   }
   info.tex_width       = g_gdp.ti_width;
   info.tex_size        = g_gdp.ti_size;
#endif


   line_n = g_gdp.ti_width << g_gdp.tile[tile].size >> 1;
   offs   = ul_t * line_n;
   offs  += ul_s << g_gdp.tile[tile].size >> 1;
   offs  += g_gdp.ti_address;
   if (offs >= BMASK)
      return;

   if (g_gdp.ti_size == G_IM_SIZ_32b)
   {
      LoadTile32b(tile, ul_s, ul_t, width, height);
   }
   else
   {
      uint8_t *dst, *end;
      uint32_t wid_64;

      // check if points to bad location
      if (offs + line_n*height > BMASK)
         height = (BMASK - offs) / line_n;
      if (height == 0)
         return;

      wid_64 = g_gdp.tile[tile].line;
      dst    = ((uint8_t*)g_gdp.tmem) + (g_gdp.tile[tile].tmem << 3);
      end    = ((uint8_t*)g_gdp.tmem) + 4096 - (wid_64<<3);
      loadTile((uint32_t *)gfx_info.RDRAM, (uint32_t *)dst, wid_64, height, line_n, offs, (uint32_t *)end);
   }
}
