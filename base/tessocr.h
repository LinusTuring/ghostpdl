/* Copyright (C) 2020 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/


/* Simple veneer for tesseract */

#ifndef tessocr_h_INCLUDED
#  define tessocr_h_INCLUDED

#include "gsmemory.h"

enum
{
    OCR_ENGINE_DEFAULT = 0,
    OCR_ENGINE_LSTM = 1,
    OCR_ENGINE_LEGACY = 2,
    OCR_ENGINE_BOTH = 3
};

int ocr_image_to_utf8(gs_memory_t *mem,
                      int w, int h, int bpp, int raster,
                      int xres, int yres,
                      void *data, int restore_data,
                      const char *language, int engine, char **out);

int ocr_image_to_hocr(gs_memory_t *mem,
                      int w, int h, int bpp, int raster,
                      int xres, int yres, void *data, int restore,
                      int pagecount, const char *language,
                      int engine, char **out);

int ocr_init_api(gs_memory_t *mem, const char *language, int engine, void **state);

void ocr_fin_api(gs_memory_t *mem, void *api_);

int ocr_recognise(void *api_, int w, int h, void *data,
                  int xres, int yres,
                  int (*callback)(void *, const char *, const int *, const int *, const int *, int),
                  void *arg);

int ocr_bitmap_to_unicodes(void* state,
    const void* data,int data_x,
    int w,int h,int raster,
    int xres,int yres,int* unicode, int* char_count);

int ocr_bitmap_to_unicode(void *state,
                          const void *data, int data_x,
                          int w, int h, int raster,
                          int xres, int yres, int *unicode);

#endif

