/* Copyright (C) 2011 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/* $Id$ */
/* Debugging flag definitions */

/* This is a chameleonic header file; that is to say, it appears differently
 * in different circumstances. The caller should provide an appropriate
 * definition of FLAG and UNUSED before including it. As such there is no guard against
 * repeated inclusion.
 */

UNUSED(0) /* Never use 0, as lots of things 'imply' 0. */
UNUSED(1)
UNUSED(2)
UNUSED(3)
UNUSED(4)
UNUSED(5)
UNUSED(6)
UNUSED(7)
UNUSED(8)
UNUSED(9)
UNUSED(10)
UNUSED(11)
UNUSED(12)
UNUSED(13)
UNUSED(14)
UNUSED(15)
UNUSED(16)
UNUSED(17)
UNUSED(18)
UNUSED(19)
UNUSED(20)
UNUSED(21)
UNUSED(22)
UNUSED(23)
UNUSED(24)
UNUSED(25)
UNUSED(26)
UNUSED(27)
UNUSED(28)
UNUSED(29)
UNUSED(30)
UNUSED(31)
UNUSED(32)
FLAG(ps_op_names,       '!', 0,   "Postscript operator names"),
FLAG(contexts_detail,   '"', 0,   "Contexts (detail)"),
FLAG(trace_errors,      '#', 0,   "Turn on tracing of error returns from operators"),
FLAG(memfill_obj,       '$', 0,   "Fill unused parts of object with identifiable garbage values"),
FLAG(ext_commands,      '%', 0,   "Externally processed commands"),
UNUSED('&')
FLAG(contexts,          '\'',0,   "Contexts (create/destroy)"),
UNUSED('(')
UNUSED(')')
FLAG(image,             '*', 0,   "Image and rasterop parameters"),
FLAG(min_stack,         '+', 0,   "Use minimum_size stack blocks"),
FLAG(no_path_banding,   ',', 0,   "Don't use path-based banding"),
UNUSED('-')
FLAG(small_mem_tables,  '.', 0,   "Use small-memory table sizes even on large-memory machines"),
FLAG(file_line,         '/', 0,   "Include file/line info on all trace output"),
FLAG(gc,                '0', 0,   "Garbage collection, minimal detail"),
FLAG(type1,             '1', 0,   "Type 1 and type 43 font interpreter"),
FLAG(curve,             '2', 0,   "Curve subdivider/rasterizer"),
FLAG(curve_detail,      '3', 0,   "Curve subdivider/rasterizer (detail)"),
FLAG(gc_strings,        '4', 0,   "Garbage collector (strings)"),
FLAG(gc_strings_detail, '5', 0,   "Garbage collector (strings, detail)"),
FLAG(gc_chunks,         '6', 0,   "Garbage collector (chunks, roots)"),
FLAG(gc_objects,        '7', 0,   "Garbage collector (objects)"),
FLAG(gc_refs,           '8', 0,   "Garbage collector (refs)"),
FLAG(gc_pointers,       '9', 0,   "Garbage collector (pointers)"),
FLAG(time,              ':', 0,   "Command list and allocator time summary"),
UNUSED(';')
UNUSED('<')
UNUSED('=')
UNUSED('>')
FLAG(validate_pointers, '?', 0,   "Validate pointers before/during/after garbage collection/save and restore"),
FLAG(memfill_empty,     '@', 0,   "Fill empty storage with a distinctive bit pattern for debugging"),
FLAG(alloc_detail,      'A', 'a', "Allocator (all calls)"),
FLAG(bitmap_detail,     'B', 'b', "Bitmap images (detail)"),
UNUSED('C')
FLAG(dict_detail,       'D', 'd', "Dictionary (every lookup)"),
UNUSED('E')
FLAG(fill_detail,       'F', 'f', "Fill algorithm (detail)"),
UNUSED('G')
FLAG(halftones_detail,  'H', 'h', "Halftones (detail)"),
FLAG(interp_detail,     'I', 'i', "Interpreter (detail)"),
UNUSED('J')
FLAG(char_cache_detail, 'K', 'k', "Character cache (detail)"),
FLAG(clist_detail,      'L', 'l', "Command list (detail)"),
UNUSED('M')
UNUSED('N')
FLAG(stroke_detail,     'O', 'o', "Stroke (detail)"),
FLAG(paths_detail,      'P', 'p', "Paths (detail)"),
UNUSED('Q')
UNUSED('R')
FLAG(scanner,           'S', 's', "Scanner"),
UNUSED('T')
FLAG(undo_detail,       'U', 'u', "Undo saver (for save/restore) (detail)"),
FLAG(compositors_detail,'V', 'v', "Compositors (alpha/transparency/overprint/rop) (detail)"),
UNUSED('W')
UNUSED('X')
FLAG(type1_hints_detail,'Y', 'y', "Type 1 hints (detail)"),
UNUSED('Z')
UNUSED('[')
UNUSED('\\')
UNUSED(']')
FLAG(ref_counts,        '^', 0,  "Reference counting"),
FLAG(high_level,        '_', 0,  "High level output"),
FLAG(no_hl_img_banding, '`', 0,  "Don't use high_level banded images"),
FLAG(alloc,             'a', 0,  "Allocator (large blocks)"),
FLAG(bitmap,            'b', 0,  "Bitmap images"),
FLAG(color_halftones,   'c', 0,  "Color Halftones"),
FLAG(dict,              'd', 0,  "Dictionary (put/undef)"),
FLAG(external_calls,    'e', 0,  "External (OS related) calls"),
FLAG(fill,              'f', 0,  "Fill algorithm"),
FLAG(gsave,             'g', 0,  "gsave/grestore"),
FLAG(halftones,         'h', 0,  "Halftones"),
FLAG(interp,            'i', 0,  "Interpreter (names only)"),
FLAG(comp_fonts,        'j', 0,  "Composite fonts"),
FLAG(char_cache,        'k', 0,  "Character cache"),
FLAG(clist,             'l', 0,  "Command list (bands)"),
FLAG(makefont,          'm', 0,  "makefont and font cache"),
FLAG(names,             'n', 0,  "Name lookup (new names only)"),
FLAG(stroke,            'o', 0,  "Stroke"),
FLAG(paths,             'p', 0,  "Paths (band list)"),
FLAG(clipping,          'q', 0,  "Clipping"),
FLAG(arcs,              'r', 0,  "Arcs"),
FLAG(streams,           's', 0,  "Streams"),
FLAG(tiling,            't', 0,  "Tiling algorithm"),
FLAG(undo,              'u', 0,  "Undo saver (for save/restore)"),
FLAG(compositors,       'v', 0,  "Compositors (alpha/transparency/overprint/rop)"),
FLAG(compress,          'w', 0,  "Compression encoder/decoder"),
FLAG(transforms,        'x', 0,  "Transformations"),
FLAG(type1_hints,       'y', 0,  "Type 1 hints"),
FLAG(trapezoid_fill,    'z', 0,  "Trapezoid fill"),
UNUSED('{')
UNUSED('|') /* "Reserved for experimental code" */
UNUSED('}')
FLAG(math,              '~', 0,  "Math functions and Functions")
