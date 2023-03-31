/* Copyright (C) 2001-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* Wrapper for png.h */

#ifndef png__INCLUDED
#  define png__INCLUDED

#if SHARE_LIBPNG
#include <png.h>
#else
#include "png.h"
#endif

#endif /* png__INCLUDED */
