/*****************************************************************************
 * main.c: top-level main loop code
 *****************************************************************************
 * Copyright (C) 2014 Peter Lawrence
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at licensing@x264.com.
 *****************************************************************************/

/*
Theory of operation:

YUV data has previously been programmed into flash beginning at location 'YUV_BASE'.

The function fill_picture() reads these frames in turn into the frame store inside x264_picture_t.

Leveraging Rowley's __cross_studio_io.h library, the x264 output is sent to the host PC via the debug pod.
*/

#include <__cross_studio_io.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stm32f429i_discovery_sdram.h"
#include "stm32f4xx_fmc.h"
#include "x264.h"

#define SDRAM_SIZE 0x800000

#define GREYSCALE
//#define STACK_CHECK
#define FILE_WRITE

#ifdef STACK_CHECK
extern unsigned char __stack_start__, __stack_end__;
extern unsigned char __stack_process_start__, __stack_process_end__;
#endif

extern void SystemInit(void);

static void fill_picture(x264_picture_t *pic);
#ifdef STACK_CHECK
static unsigned long get_stack_used(unsigned char *stack_start, unsigned char *stack_end);
#endif
#ifdef GREYSCALE
static void grey_picture(x264_picture_t *pic);
#endif

int main(void)
{
  uint32_t count;
  x264_param_t param;
  x264_t *h = NULL;
  x264_picture_t pic, pic_out;
  x264_nal_t *nal;
  int i_nal, i_frame_size;
  unsigned pts = 0;
#ifdef FILE_WRITE
  DEBUG_FILE *output;
#endif
#ifdef STACK_CHECK
  unsigned long size, process_size;
#endif
  uint32_t start, stop;

  SystemInit();

  SDRAM_Init();

  FMC_SDRAMWriteProtectionConfig(FMC_Bank2_SDRAM, DISABLE);

  memset(&param, 0, sizeof(param));

  x264_param_default_preset( &param, "ultrafast", "zerolatency");

  x264_param_apply_profile(&param, "baseline");

  param.i_threads = 1;
  param.i_width = 176;
  param.i_height = 144;
  param.i_fps_num = 30;
  param.i_fps_den = 1;
  param.i_csp = X264_CSP_I420;

  param.i_keyint_max = 250;
  param.rc.i_rc_method = X264_RC_CRF;

  param.rc.i_bitrate = 80;
  param.rc.i_vbv_max_bitrate = 80;
  param.rc.i_vbv_buffer_size = 100;

//  param.analyse.b_chroma_me = 0;

  // test
//  param.analyse.b_psy = 0;
//  param.analyse.b_psnr = 1;
  param.i_log_level = 255;
  param.rc.i_rc_method = X264_RC_CQP;
  param.rc.i_qp_constant = 37;
//  param.rc.i_rc_method = X264_RC_ABR;

  h = x264_encoder_open( &param );

  x264_picture_alloc(&pic, param.i_csp, param.i_width, param.i_height);

#ifdef GREYSCALE
  grey_picture(&pic);
#endif

  count = 0;

#ifdef FILE_WRITE
  output = debug_fopen("output.264", "wb");
#endif

  for (;;)
  {
#ifdef STACK_CHECK
    size = get_stack_used(&__stack_start__, &__stack_end__);
    process_size = get_stack_used(&__stack_process_start__, &__stack_process_end__);

    debug_printf("stack = %lu %lu\n", size, process_size);
#endif

    fill_picture(&pic);

    start = *((volatile uint32_t *)0xE0001004);
    i_frame_size = x264_encoder_encode(h, &nal, &i_nal, &pic, &pic_out);
    stop = *((volatile uint32_t *)0xE0001004);

    debug_printf("%u\n", stop - start);

//    debug_printf("%i DTS %lu\n", i_frame_size, pic_out.i_dts);
#ifdef FILE_WRITE
    if (i_frame_size)
      debug_fwrite(nal->p_payload, i_frame_size, 1, output);
#endif

    pic.i_pts += 3000;

    if (++count > 31)
      break;
  }

#ifdef FILE_WRITE
  debug_fclose(output);
#endif

  debug_break();

  return 0;
}

void assert_param(int x) {}

void __assert(int expression) {}

/*
I'm not entirely certain if this algorithm produces the same numerical results as the one x264 expects,
but I found a functionally equivalent algorithm online that purported to be just like glibc
*/

float round(float x)
{
  float y;
  int ix = x;
  
  return (float)ix; 

  if (x > 0.0)
  {
    y = floor(x);
    if ((x - y) > 0.5)
      y += 1.0;
  }
  else
  {
    y = ceil(x);
    if ((y - x) > 0.5)
      y -= 1.0;
  }

  return y;
}

#define YUV_BASE (uint8_t *)(0x08100000)

/*
in reality, we should be able to point directly to the flash rather than memcpy it to RAM
however, in a real-world application, we wouldn't be reading from flash anyway
*/

static void fill_picture(x264_picture_t *pic)
{
  static uint8_t *pnt = YUV_BASE;
  static unsigned count = 0;
  int i;
  const unsigned plane_sizes[4] = { 25344, 6336, 6336, 0 };

  for ( i = 0; i < pic->img.i_plane; i++)
  {
#ifdef GREYSCALE
    if (0 == i)
#endif
      memcpy(pic->img.plane[i], pnt, plane_sizes[i]);
    pnt += plane_sizes[i];
  }

  count++;

  if (27 == count)
  {
    count = 0;
    pnt = YUV_BASE;
  }
}

#ifdef STACK_CHECK
static unsigned long get_stack_used(unsigned char *stack_start, unsigned char *stack_end)
{
  unsigned long size;
  for (size = stack_end - stack_start; size && *stack_start++ == 0xCC; size--);
  return size;
}
#endif

#ifdef GREYSCALE
static void grey_picture(x264_picture_t *pic)
{
  int i;
  const unsigned plane_sizes[4] = { 25344, 6336, 6336, 0 };

  for ( i = 0; i < pic->img.i_plane; i++)
  {
    if (i)
      memset(pic->img.plane[i], 127, plane_sizes[i]);
  }
}
#endif
