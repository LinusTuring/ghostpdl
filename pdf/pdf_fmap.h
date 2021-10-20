/* Copyright (C) 2020-2021 Artifex Software, Inc.
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

/* We expect to lookup a name, but what we get back might be a name,
   or might be a string.
 */
int
pdf_fontmap_lookup_font(pdf_context *ctx, pdf_name *fname, pdf_obj **mapname);

/* The name parameter is to allow for internally derived font names to be looked up,
   like, for example, /Adobe-Japan1
 */
int
pdf_fontmap_lookup_cidfont(pdf_context *ctx, pdf_dict *font_dict, pdf_name *name, pdf_obj **mapname, int *findex);
