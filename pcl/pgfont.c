/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pgfont.c */
/* HP-GL/2 stick and arc font */
#include "math_.h"
#include "gstypes.h"
#include "gsccode.h"
#include "gsmemory.h"		/* for gsstate.h */
#include "gsstate.h"		/* for gscoord.h, gspath.h */
#include "gsmatrix.h"
#include "gscoord.h"
#include "gspaint.h"
#include "gspath.h"
#include "gxfixed.h"		/* for gxchar.h */
#include "gxchar.h"
#include "gxfarith.h"
#include "gxfont.h"
#include "plfont.h"
#include "pgfont.h"

/* Note that we misappropriate pfont->StrokeWidth for the chord angle. */

#define num_arc_symbols (256+20) /* HACK forward reference */

/* The client handles all encoding issues for these fonts. */
private gs_glyph
hpgl_stick_arc_encode_char(gs_show_enum *penum, gs_font *pfont, gs_char *pchr)
{	return (gs_glyph)*pchr;
}

private int
hpgl_stick_char_width(const pl_font_t *plfont, const pl_symbol_map_t *map,
  const gs_matrix *pmat, uint char_code, gs_point *pwidth)
{	bool in_range =
	  char_code >= 0x20 && char_code < 0x20 + num_arc_symbols;

	if ( pwidth ) {
	  gs_distance_transform(0.667, 0.0, pmat, pwidth);
	}
	return in_range;
}
/* The arc font is proportionally spaced, but we don't have convenient */
/* access to the escapements. */
private int
hpgl_arc_char_width(const pl_font_t *plfont, const pl_symbol_map_t *map,
  const gs_matrix *pmat, uint char_code, gs_point *pwidth)
{	return hpgl_stick_char_width(plfont, map, pmat, char_code, pwidth);
}

/*
 * Characters are defined by a series of 1-byte points.  Normally, the top
 * nibble is X and the bottom nibble is Y.  If the top nibble is 0xf, the
 * bottom nibble has special meaning as follows:
 */
typedef enum {
  arc_op_pen_up = 0x0,		/* pen up */
  arc_op_draw_180 = 0x1,	/* draw half circle, next delta is center of */
				/* half circle in grid coordinate system. */
  arc_op_draw_360 = 0x2,	/* same as f1 but draw a circle instead. */
  arc_op_line_45 = 0x3,		/* next delta specifies 45 degree line */
				/* instead of an arc. */
  arc_op_vertical = 0x4,	/* force a vertical for an arc. */
  arc_op_BS = 0x5,		/* hpgl backspace char */
  arc_op_up_5 = 0x6,		/* ascend by 5 points */
  arc_op_down_5 = 0x7,		/* descend by 5 points */
  arc_op_back_8 = 0x8,		/* backspace by 8 points */
  arc_op_forward_8 = 0x9	/* forward by 8 points */
  /* 0xa - 0xf */		/* not used */
} arc_op_t;

/* Forward references to data */
private const byte arc_symbol_strokes[];
private const ushort arc_symbol_offsets[];
private const byte arc_symbol_counts[];

/* Forward procedure declarations */
private int hpgl_arc_add_arc(P6(gs_state *pgs, int cx, int cy,
  const gs_int_point *start, floatp sweep_angle, floatp chord_angle));

/* Add a symbol to the path. */
private int
hpgl_stick_arc_build_char(gs_show_enum *penum, gs_state *pgs, gs_font *pfont,
  gs_char ignore_chr, gs_glyph char_index)
{	double chord_angle = pfont->StrokeWidth;
	const byte *ptr;
	const byte *end;
	gs_int_point prev;
	gs_int_point shift;
	bool draw = false;
	enum {
	  arc_direction_other,
	  arc_direction_vertical,
	  arc_direction_horizontal
	} direction = arc_direction_other;
	gs_matrix save_ctm;
	int code = 0;

	if ( char_index < 0x20 || char_index >= 0x20 + num_arc_symbols )
	  return 0;
	gs_setcharwidth(penum, pgs, 0.667, 0.0);
	ptr = &arc_symbol_strokes[arc_symbol_offsets[char_index - 0x20]];
	end = ptr + arc_symbol_counts[char_index - 0x20];
	prev.x = prev.y = 0;
	shift.x = shift.y = 0;
	gs_currentmatrix(pgs, &save_ctm);
	/* The TRM says the stick font is based on a 32x32 unit cell, */
	/* but the font we're using here is only 15x15. */
	/* Also, per TRM 23-18, the character cell is only 2/3 the */
	/* point size. */
#define scale (1.0 / 15 * 2 / 3)
	gs_scale(pgs, scale, scale);
#undef scale
	gs_moveto(pgs, 0.0, 0.0);

	while ( ptr < end ) {
	  int x = *ptr >> 4;
	  int y = *ptr++ & 0x0f;
	  int dx, dy;

	  if ( x == 0xf ) {
	    /* Special function */
	    switch ( (arc_op_t)y )
	      {
	      case arc_op_pen_up:
		draw = false;
		continue;
	      case arc_op_draw_180:
		draw = true;
		x = (*ptr >> 4) + shift.x;
		y = (*ptr++ & 0x0f) + shift.y;
		code = hpgl_arc_add_arc(pgs, x, y, &prev,
					-180.0, chord_angle);
		prev.x = x * 2 - prev.x, prev.y = y * 2 - prev.y;
		break;
	      case arc_op_draw_360:
		draw = true;
		x = (*ptr >> 4) + shift.x;
		y = (*ptr++ & 0x0f) + shift.y;
		code = hpgl_arc_add_arc(pgs, x, y, &prev,
					-360.0, chord_angle);
		prev.x = x, prev.y = y;
		break;
	      case arc_op_line_45:
		draw = true;
		x = (*ptr >> 4) + shift.x;
		y = (*ptr++ & 0x0f) + shift.y;
		code = gs_rlineto(pgs, (floatp)(x - prev.x),
				  (floatp)(y - prev.y));
		direction =
		  (x == prev.x && y != prev.y ? arc_direction_vertical :
		   y == prev.y && x != prev.x ? arc_direction_horizontal :
		   arc_direction_other);
		prev.x = x, prev.y = y;
		break;
	      case arc_op_vertical:
		direction = arc_direction_vertical;
		continue;
	      case arc_op_BS:
		/****** WHAT TO DO? ******/
		continue;
	      case arc_op_up_5:
		shift.y += 5;
		continue;
	      case arc_op_down_5:
		shift.y -= 5;
		continue;
	      case arc_op_back_8:
		shift.x += 8;
		continue;
	      case arc_op_forward_8:
		shift.x -= 8;
		continue;
	      }
	  } else {
	    /* Normal segment */
	    x += shift.x;
	    y += shift.y;
	    dx = x - prev.x;
	    dy = y - prev.y;
	    if ( !draw ) {
	      code = gs_rmoveto(pgs, (floatp)dx, (floatp)dy);
	      direction =
		(x == prev.x ? arc_direction_vertical :
		 y == prev.y ? arc_direction_horizontal :
		 arc_direction_other);
	      draw = true;
	    } else {
	      if ( any_abs(dx) == any_abs(dy) &&
		   direction != arc_direction_other
		 ) {
		/* 45 degree move following horizontal or vertical */
		if ( direction == arc_direction_vertical ) {
		  /*
		   * If the current point is in quadrant 2 or 4 relative to
		   * the previous, draw the arc counter-clockwise, otherwise
		   * draw clockwise.
		   */
		  bool clockwise = !((dx > 0) ^ (dy > 0));

		  code = hpgl_arc_add_arc(pgs, x, prev.y, &prev,
					  (clockwise ? -90.0 : 90.0),
					  chord_angle);
		  direction = arc_direction_horizontal;
		} else {
		  /* direction == arc_direction_horizontal */
		  /*
		   * If the current point is in quadrant 2 or 4 relative to
		   * the previous, draw the arc clockwise, otherwise
		   * draw counter-clockwise.
		   */
		  bool clockwise = (dx > 0) ^ (dy > 0);

		  code = hpgl_arc_add_arc(pgs, prev.x, y, &prev,
					  (clockwise ? -90.0 : 90.0),
					  chord_angle);
		  direction = arc_direction_vertical;
		}
	      } else {
		/* Not 45 degree case, just draw the line. */
		code = gs_rlineto(pgs, (floatp)dx, (floatp)dy);
		direction =
		  (dx == 0 ? arc_direction_vertical :
		   dy == 0 ? arc_direction_horizontal :
		   arc_direction_other);
	      }
	    }
	    prev.x = x, prev.y = y;
	  }
	  if ( code < 0 )
	    break;
	}
	gs_setmatrix(pgs, &save_ctm);
	if ( code < 0 )
	  return code;
	/* Set predictable join and cap styles. */
	gs_setlinejoin(pgs, gs_join_miter);
	gs_setlinecap(pgs, gs_cap_triangle);
	return gs_stroke(pgs);
}

/* Add an arc given the center, start point, and sweep angle. */
private int
hpgl_arc_add_arc(gs_state *pgs, int cx, int cy, const gs_int_point *start,
  floatp sweep_angle, floatp chord_angle)
{	int dx = start->x - cx, dy = start->y - cy;
	double radius, angle;
	int count = (int)ceil(fabs(sweep_angle) / chord_angle);
	double delta = sweep_angle / count;
	int code = 0;

	if ( dx == 0 ) {
	  radius = any_abs(dy);
	  angle = (dy >= 0 ? 90.0 : 270.0);
	} else if ( dy == 0 ) {
	  radius = any_abs(dx);
	  angle = (dx >= 0 ? 0.0 : 180.0);
	} else {
	  radius = hypot((floatp)dx, (floatp)dy);
	  angle = atan2((floatp)dy, (floatp)dx) * radians_to_degrees;
	}
	while ( count-- ) {
	  gs_sincos_t sincos;

	  angle += delta;
	  gs_sincos_degrees(angle, &sincos);
	  code = gs_lineto(pgs, cx + sincos.cos * radius,
			   cy + sincos.sin * radius);
	  if ( code < 0 )
	    break;
	}
	return code;
}

/* Fill in stick/arc font boilerplate. */
private void
hpgl_fill_in_stick_arc_font(gs_font_base *pfont, long unique_id)
{	/* The way the code is written requires FontMatrix = identity. */
	gs_make_identity(&pfont->FontMatrix);
	pfont->FontType = ft_user_defined;
	pfont->PaintType = 1;		/* stroked fonts */
	pfont->BitmapWidths = false;
	pfont->ExactSize = fbit_use_outlines;
	pfont->InBetweenSize = fbit_use_outlines;
	pfont->TransformedChar = fbit_use_outlines;
	pfont->procs.encode_char = hpgl_stick_arc_encode_char;
	pfont->procs.build_char = hpgl_stick_arc_build_char;
	/* p.y of the FontBBox is a guess, because of descenders. */
	/* Because of descenders, we have no real idea what the */
	/* FontBBox should be. */
	pfont->FontBBox.p.x = 0;
	pfont->FontBBox.p.y = -0.333;
	pfont->FontBBox.q.x = 0.667;
	pfont->FontBBox.q.y = 0.667;
	uid_set_UniqueID(&pfont->UID, unique_id);
	pfont->encoding_index = 1;	/****** WRONG ******/
	pfont->nearest_encoding_index = 1;	/****** WRONG ******/
}
void
hpgl_fill_in_stick_font(gs_font_base *pfont, long unique_id)
{	hpgl_fill_in_stick_arc_font(pfont, unique_id);
	pfont->StrokeWidth = 22.5;
#define plfont ((pl_font_t *)pfont->client_data)
	plfont->char_width = hpgl_stick_char_width;
#undef plfont
}
void
hpgl_fill_in_arc_font(gs_font_base *pfont, long unique_id)
{	hpgl_fill_in_stick_arc_font(pfont, unique_id);
#define plfont ((pl_font_t *)pfont->client_data)
	plfont->char_width = hpgl_arc_char_width;
#undef plfont
}

/* The actual font data */

private const byte arc_symbol_strokes[]={
   0xf5,0x54,0x4e,0xf4,0xf1,0x5e,0x54,0xf0,0x52,0xf2
  ,0x51,0xf0,0x67,0xf2,0x57,0xf0,0xf7,0x43,0x46,0xf1
  ,0x56,0x43,0x40,0x43,0xf1,0x53,0x40,0xf4,0x3b,0x2e
  ,0xf1,0x3e,0x3b,0xf4,0xf0,0x8b,0x7e,0xf1,0x8e,0x8b
  ,0x9a,0x4a,0x39,0x38,0x47,0xf0,0x97,0x37,0x15,0x14
  ,0x32,0x82,0x93,0x60,0x40,0x04,0x06,0x28,0x38,0x5a
  ,0x5e,0x4f,0x3f,0x2e,0x2a,0x90,0x04,0x22,0x82,0xa4
  ,0xa6,0x88,0x28,0x0a,0x0b,0x2d,0x8d,0xab,0xf0,0x50
  ,0x5f,0x3c,0x31,0x37,0xf0,0x39,0x3e,0x5e,0x50,0xf0
  ,0x30,0x70,0xf0,0x5c,0x5b,0xf0,0x39,0x59,0x50,0xf7
  ,0x52,0x30,0x20,0x02,0x82,0xf0,0x08,0x88,0xf0,0x4c
  ,0x44,0xf0,0x15,0x7b,0xf0,0x4c,0xf0,0x1b,0x75,0x3f
  ,0x7f,0xf0,0x70,0x30,0xf0,0x50,0x5f,0xf0,0xaf,0x0f
  ,0x08,0xf0,0xf6,0x2c,0xf2,0x3c,0xf0,0x6c,0xf2,0x7c
  ,0xf0,0x0a,0xf7,0xf0,0x0f,0x04,0x40,0x60,0xa4,0xaf
  ,0x50,0x0f,0x20,0x56,0x80,0xaf,0x00,0xf0,0x0a,0xf1
  ,0x5a,0xa5,0xf1,0x55,0x0a,0xbf,0xaf,0x7c,0x73,0xa0
  ,0xb0,0x0c,0x3f,0x7f,0xac,0xab,0x67,0x57,0x02,0x00
  ,0xa0,0x00,0x0f,0xaf,0xf0,0x78,0x08,0x66,0xa6,0xa4
  ,0xf4,0x60,0x40,0x04,0x0b,0x4f,0x6f,0xab,0xa4,0xf0
  ,0x73,0xa0,0x58,0xf0,0x00,0x0f,0x6f,0x9c,0x9b,0x68
  ,0x08,0xf0,0x68,0xa4,0xa3,0x70,0x00,0x0f,0x3f,0xa8
  ,0xa7,0x30,0x00,0xaf,0xf0,0x0f,0xa0,0x00,0xaf,0x0f
  ,0x1f,0x4c,0x43,0x10,0x00,0x09,0xf6,0xf0,0x2c,0xf2
  ,0x3c,0xf0,0x6c,0xf2,0x7c,0xf0,0x00,0xf7,0xf0,0x00
  ,0x5f,0xa0,0xf0,0x86,0x26,0x70,0x7f,0x06,0xa6,0x0f
  ,0xaf,0x59,0x79,0xa6,0xa4,0x60,0x40,0x04,0x06,0x39
  ,0x69,0xf0,0x39,0x1b,0x1c,0x4f,0x6f,0x9c,0x9b,0x79
  ,0x04,0x40,0x60,0xa4,0xa5,0x78,0x38,0x0b,0x0c,0x3f
  ,0x7f,0xac,0xf0,0xf6,0x1f,0x5c,0x9f,0xf7,0x9f,0x4f
  ,0x0b,0x03,0x30,0x60,0xa4,0xa6,0x6a,0x3a,0x07,0x0f
  ,0xaf,0x00,0x0f,0xf0,0xaf,0x08,0xf0,0x4b,0xa0,0xaf
  ,0x59,0x0f,0x00,0x0f,0xf0,0xaf,0xa0,0xf0,0xa8,0x08
  ,0xf5,0xf0,0x08,0xf6,0xf0,0x0c,0x2e,0x4e,0x5d,0xf3
  ,0x6c,0x8c,0xae,0xf0,0x00,0xf7,0xf0,0x00,0x0f,0xa0
  ,0xaf,0x16,0x38,0x48,0x57,0xf3,0x66,0x76,0x98,0xf0
  ,0x9d,0x7b,0x6b,0x5c,0xf3,0x4d,0x3d,0x1b,0xf0,0x19
  ,0x10,0x16,0x49,0x59,0x86,0x80,0xf0,0x1e,0x19,0x13
  ,0x40,0x50,0x83,0xf0,0x89,0x80,0xf2,0x82,0xf0,0x00
  ,0xaf,0xf0,0x2f,0xf2,0x2d,0x13,0x4d,0xf0,0x8d,0x53
  ,0xf0,0x1a,0x9a,0xf0,0x86,0x06,0x86,0xf0,0x8a,0x0a
  ,0xf0,0x02,0x8e,0x52,0xf2,0x51,0xf0,0x53,0x57,0x68
  ,0x78,0x9a,0x9b,0x5f,0x4f,0x0b,0x87,0x03,0xf0,0x01
  ,0x81,0x83,0x07,0x8b,0xf0,0x01,0x81,0xa3,0xf4,0x70
  ,0x50,0x14,0x18,0x5c,0x6c,0xa8,0xa5,0x83,0x87,0x69
  ,0x59,0x37,0x35,0x53,0x63,0x85,0x04,0x40,0x50,0xa5
  ,0xaf,0xa9,0xf4,0x76,0x46,0x0a,0x0b,0x4f,0x6f,0xab
  ,0xa4,0x60,0x10,0xf7,0x10,0x1e,0xf0,0xf6,0x13,0xf4
  ,0x40,0x50,0x83,0x86,0x59,0x49,0x16,0x13,0xf0,0xf7
  ,0x80,0x8e,0x89,0x80,0xf0,0x83,0xf4,0x50,0x40,0x13
  ,0x16,0x49,0x59,0x86,0x85,0x15,0x95,0xf0,0x53,0xf2
  ,0x52,0xf0,0x57,0xf2,0x58,0x1e,0x10,0xf0,0x50,0x40
  ,0x13,0x16,0x49,0x59,0x86,0x83,0x50,0xf0,0xf7,0x8e
  ,0x83,0x50,0x30,0x12,0xf4,0x30,0x60,0x82,0x83,0x65
  ,0x25,0x16,0x17,0x39,0x79,0x88,0xf0,0x1e,0x5b,0x9e
  ,0xf7,0x49,0xf4,0x7c,0x8c,0xb9,0xb8,0x85,0x75,0x48
  ,0x32,0x30,0x3c,0x5e,0x6e,0x8c,0xf0,0x59,0x19,0xf0
  ,0x3d,0x32,0x50,0x60,0x82,0x99,0x70,0x59,0x30,0x19
  ,0x50,0x99,0x00,0x30,0x3f,0x0f,0xa0,0x70,0x7f,0xaf
  ,0x59,0xf0,0x0f,0x59,0x50,0xf0,0x36,0x76,0xf0,0x74
  ,0x34,0x08,0xf6,0xf0,0x2c,0xf2,0x3c,0xf0,0x6c,0xf2
  ,0x7c,0xf0,0xa0,0xf7,0xf0,0xa4,0x60,0x40,0x04,0x0b
  ,0x4f,0x6f,0xab,0xa4,0xf0,0x9f,0x5d,0x1f,0x2f,0x3e
  ,0x39,0x58,0x37,0x31,0x20,0x10,0x19,0x17,0x39,0x49
  ,0x67,0x60,0xf0,0x67,0x89,0x99,0xb7,0xb0,0xbf,0xaf
  ,0x9e,0x99,0x78,0x97,0x91,0xa0,0xb0,0xf5,0x00,0x0f
  ,0x1f,0x10,0x20,0x2f,0x3f,0x30,0x40,0x4f,0x5f,0x50
  ,0x60,0x6f,0x7f,0x70,0x80,0x8f,0x9f,0x90,0xa0,0xaf
  ,0xbf,0xb0,0xc0,0xcf,0xdf,0xd0,0xe0,0xef,0x0f,0x0e
  ,0xee,0xed,0x0d,0x0c,0xec,0xeb,0x0b,0x0a,0xea,0xe9
  ,0x09,0x08,0xe8,0xe7,0x07,0x06,0xe6,0xe5,0x05,0x04
  ,0xe4,0xe3,0x03,0x02,0xe2,0xe1,0x01,0x00,0xe0,0xf5
  ,0x00,0xe0,0x2c,0xf2,0x3c,0xf0,0x6c,0xf2,0x7c,0x6c
  ,0xf0,0x19,0x50,0xf0,0xf7,0x9e,0x30,0x10,0x01,0xf0
  ,0xf6,0x7d,0x3b,0xf7,0x4a,0xf1,0x4c,0x5e,0xf1,0x5c
  ,0x4a,0xf0,0x12,0x17,0x39,0x69,0x87,0x82,0x60,0x30
  ,0x12,0xf0,0x89,0x80,0x91,0x80,0x70,0x33,0x23,0x12
  ,0x11,0x20,0x30,0x52,0x5d,0x7f,0x8f,0xad,0xf0,0x79
  ,0x39,0xf0,0x77,0x37,0x39,0x59,0x7b,0x7c,0x5e,0x3e
  ,0x1c,0x1b,0x39,0x19,0x59,0x86,0x80,0xf0,0x83,0x50
  ,0x30,0x12,0x14,0x36,0x86,0xf0,0x1c,0xf2,0x2c,0xf0
  ,0x6c,0xf2,0x7c,0x24,0x33,0x83,0x94,0x95,0x86,0x36
  ,0x27,0x29,0x3a,0x8a,0x99,0x97,0x86,0xf0,0x8a,0x3a
  ,0x2b,0x2c,0x3d,0x8d,0x9c,0x18,0x3a,0x9a,0xf0,0x92
  ,0x84,0x7a,0xf0,0x4a,0x12,0x3e,0x6e,0x8c,0x8a,0x68
  ,0xf0,0x28,0x78,0x96,0x94,0x72,0x32,0x14,0x32,0x62
  ,0x84,0xaa,0xf0,0x84,0x83,0x92,0xa2,0xf0,0x3a,0x01
  ,0x0c,0x2e,0x3e,0x5c,0x92,0xf0,0x69,0x02,0x5e,0xa2
  ,0x02,0x32,0x07,0x0a,0x4e,0x5e,0x9a,0x97,0x62,0x92
  ,0x1a,0x38,0x43,0x42,0x64,0x9a,0xf0,0x6b,0x41,0xae
  ,0xaf,0x0f,0x58,0x00,0xa0,0xa1,0xf5,0x2a,0x6d,0xaa
  ,0x6a,0x3a,0x18,0x14,0x32,0x62,0x84,0x88,0x6a,0x13
  ,0x16,0x49,0x59,0x86,0x83,0x50,0x40,0x13,0xf0,0x10
  ,0x89,0x83,0x61,0x31,0x13,0x17,0x4a,0x7a,0x98,0x97
  ,0xaa,0x83,0x82,0x91,0xb1,0xa4,0xf4,0x60,0x40,0x04
  ,0x0b,0x4f,0x6f,0xab,0xa4,0xf0,0xaf,0x00,0xaf,0x5f
  ,0x50,0xa0,0xf0,0x96,0x26,0x07,0x29,0x39,0x48,0x41
  ,0x30,0x20,0x02,0x07,0xf0,0x45,0x85,0x87,0x69,0x59
  ,0x48,0x41,0x50,0x60,0x82,0x6d,0x4d,0xf1,0x4e,0x4f
  ,0x6f,0xf1,0x6e,0xf0,0x00,0x4c,0x6c,0xa0,0x86,0x26
  ,0x23,0xf2,0x56,0xf0,0x1a,0x29,0xf0,0x9a,0x89,0xf0
  ,0x92,0x83,0xf0,0x12,0x23,0x2d,0x4f,0x6f,0x9c,0x9b
  ,0x68,0x58,0xf0,0x68,0xa4,0xa3,0x70,0x50,0x32,0x9d
  ,0x5f,0x1d,0x10,0xf0,0x89,0x14,0xf0,0x80,0x46,0x45
  ,0x57,0x65,0xf0,0x57,0x51,0x10,0x89,0xf0,0x19,0x80
  ,0x10,0x89,0x19,0x13,0x40,0x50,0x83,0x80,0x89,0xf0
  ,0x1c,0xf2,0x2c,0xf0,0x6c,0xf2,0x7c,0x6c,0xf0,0x50
  ,0x40,0x13,0x16,0x49,0x59,0x86,0x83,0x50,0x55,0xf0
  ,0x57,0x58,0xf4,0xbe,0xcf,0xdf,0xee,0xdd,0xcd,0xf0
  ,0xdd,0xec,0xdb,0xcb,0xbc,0xf4,0xbe,0xcf,0xdf,0xee
  ,0xed,0xbb,0xeb,0xf7,0x04,0x14,0x10,0xf0,0x21,0x23
  ,0x34,0x44,0x53,0x51,0x40,0x30,0x21,0xf0,0xf7,0xb2
  ,0xe2,0xe3,0xd4,0xc4,0xb3,0xb1,0xc0,0xd0,0xe1,0xbb
  ,0xef,0xf0,0xbf,0xeb,0x5f,0xf2,0x5e,0xf0,0x5c,0x58
  ,0x47,0x37,0x15,0x14,0x50,0x60,0x93,0xf1,0x64,0x49
  ,0xf1,0x78,0x93,0xf0,0xb6,0x26,0x1e,0xf2,0x2d,0x6b
  ,0xf0,0x19,0x59,0x86,0x80,0xf0,0x83,0x50,0x30,0x12
  ,0x14,0x36,0x86,0xf0,0x6d,0x2b,0xf0,0x15,0x85,0x86
  ,0x59,0x49,0x16,0x13,0x40,0x50,0x83,0xf0,0x2d,0x6b
  ,0xf0,0x50,0x40,0x13,0x16,0x49,0x59,0x86,0x83,0x50
  ,0xf0,0x6d,0x2b,0xf0,0x19,0x13,0x40,0x50,0x83,0x80
  ,0x89,0xf0,0x2d,0x6b,0xf7,0x42,0x55,0xf0,0xf7,0x8a
  ,0xf0,0x88,0x55,0x45,0x18,0x1a,0x4d,0x5d,0x8a,0xf0
  ,0x45,0x34,0x33,0x22,0xa4,0xf4,0x60,0x40,0x04,0x0b
  ,0x4f,0x6f,0xab,0xf0,0x00,0xf7,0xf0,0x45,0x63,0x41
  ,0x13,0x1f,0x6f,0x9c,0x9b,0x68,0x18,0xf0,0x68,0xa4
  ,0xa3,0x70,0x20,0x02,0x68,0x3b,0x2b,0x1a,0xf0,0x8f
  ,0x40,0xf5,0x4e,0xf2,0x5e,0xf0,0x8e,0xf2,0x9e,0xf5
  ,0x4d,0xf2,0x5e,0x4c,0xf2,0x5d,0xf0,0x19,0x59,0x86
  ,0x80,0xf0,0x83,0x50,0x30,0x12,0x14,0x36,0x86,0xf5
  ,0xf6,0x0c,0x1d,0x2d,0x3c,0x4c,0x5d,0x88,0x8e,0xf0
  ,0x8e,0x2e,0x22,0xe2,0xee,0x8e,0xf0,0x88,0x88,0x8e
  ,0xf0,0x8e,0x5e,0x2b,0x25,0x52,0xb2,0xe5,0xeb,0xbe
  ,0x8e,0xf0,0x88,0x88,0x8e,0xf0,0x8e,0x22,0xe2,0x8e
  ,0xf0,0x88,0x88,0x8e,0xf0,0x88,0x82,0xf0,0x88,0x28
  ,0xe8,0xf0,0x88,0xf0,0x2e,0xf3,0x2e,0xf3,0xe2,0x88
  ,0x22,0xee,0x88,0x88,0x8e,0xf0,0x8e,0xf3,0x28,0xf3
  ,0x82,0xf3,0xe8,0xf3,0x8e,0xf0,0x88,0x88,0x8e,0xf0
  ,0x8e,0xf3,0x28,0xf3,0xe8,0xe8,0xf3,0x8e,0x82,0xf0
  ,0x88,0xe2,0x22,0xf3,0xee,0x2e,0xf3,0xe2,0xf0,0x88
  ,0x2e,0xee,0xf3,0x22,0xe2,0xf0,0x68,0xa8,0xf0,0x88
  ,0x88,0x2e,0xf0,0x88,0xee,0xf0,0x88,0x82,0xf0,0x88
  ,0x88,0x8a,0xf0,0x8a,0x6a,0xf3,0x2e,0xf3,0x6a,0x66
  ,0xf3,0x22,0xf3,0x66,0xa6,0xf3,0xe2,0xf3,0xa6,0xaa
  ,0xf3,0xee,0xf3,0xaa,0x8a,0xf0,0x88,0x88,0x26,0xf0
  ,0x26,0xe6,0x8e,0x26,0xf0,0x8b,0x2b,0x82,0xeb,0x8b
  ,0xf0,0x88,0x08,0x88,0xf0,0x44,0x4c,0xf0,0x0d,0x8d
  ,0x76,0xa8,0x7a,0xf0,0xa8,0x28,0xf0,0x56,0x28,0x5a
  ,0x06,0x26,0x50,0xcf,0xef,0x54,0x88,0x39,0xf0,0x8d
  ,0x88,0xd9,0xf0,0xb4,0x88,0xf5,0x0f,0xef,0xf5,0x4c
  ,0x7e,0xf5,0x4e,0x7c,0x12,0x1d,0xf0,0x18,0xb8,0x67
  ,0x8a,0xa7,0xf0,0x8a,0x82,0xf0,0x65,0x82,0xa5,0x3c
  ,0x9c,0xf1,0x97,0x32,0xe2,0x82,0xf1,0x87,0xec,0x31
  ,0x37,0xf1,0x87,0xd1,0xdc,0xd6,0xf1,0x86,0x3c,0x80
  ,0x4c,0xcc,0x80,0x23,0x22,0x31,0x41,0x52,0x7c,0x8d
  ,0x9d,0xac,0xab,0x38,0xd8,0xf0,0xd5,0x35,0xf0,0x32
  ,0xd2,0xf0,0x38,0x59,0x69,0xa7,0xb7,0xd8,0xa4,0xf4
  ,0x60,0x40,0x04,0x0b,0x4f,0x6f,0xab,0xa4,0xf0,0x08
  ,0xf6,0xf0,0x0c,0x2e,0x4e,0x5d,0xf3,0x6c,0x8c,0xae
  ,0xf7,0xf0,0x00,0x5f,0xa0,0xf0,0x86,0x26,0xf6,0xf0
  ,0x1a,0x5d,0x9a,0xf7,0x05,0xf6,0xf0,0x7e,0x3c,0xf0
  ,0x00,0xf7,0xf0,0xf0,0x00,0x5f,0xa0,0xf0,0x86,0x26
  ,0xf6,0xf0,0x3e,0x7c,0xf7,0xf0,0x9d,0x7b,0x6b,0x5c
  ,0xf3,0x4d,0x3d,0x1b,0xf0,0x19,0x59,0x86,0x80,0xf0
  ,0x83,0x50,0x30,0x12,0x14,0x36,0x86,0xf0,0x1b,0x5e
  ,0x9b,0x50,0x40,0x13,0x16,0x49,0x59,0x86,0x83,0x50
  ,0xf0,0x78,0x1f,0xf0,0x6e,0x1b,0x53,0x43,0x16,0x19
  ,0x4c,0x5c,0x89,0x86,0x53,0xf0,0x10,0xa0,0xf0,0x8c
  ,0x83,0xf3,0x86,0xf7,0x53,0x43,0x16,0x19,0x4c,0x5c
  ,0x89,0xaf,0xaf,0x59,0xf0,0x0f,0x59,0x50,0xf0,0x08
  ,0xf6,0xf0,0x1c,0xf2,0x2c,0xf0,0x6c,0xf2,0x7c,0xf0
  ,0x00,0xf7,0xf0,0xa0,0x00,0x0f,0xaf,0xf0,0x78,0x08
  ,0xa0,0x00,0x0f,0xaf,0xf0,0x78,0x08,0xf0,0xf6,0xf0
  ,0x1c,0x5f,0x9c,0xf7,0xf0,0xa4,0xf4,0x60,0x40,0x04
  ,0x0b,0x4f,0x6f,0xab,0xa4,0x05,0xf6,0xf0,0x7e,0x3c
  ,0xf0,0x00,0xf7,0xf0,0xa0,0x00,0x0f,0xaf,0xf0,0x78
  ,0x08,0xf6,0xf0,0x3e,0x7c,0xf7,0x1c,0xf2,0x2c,0xf0
  ,0x6c,0xf2,0x7c,0x6c,0xf0,0x83,0xf4,0x50,0x40,0x13
  ,0x16,0x49,0x59,0x86,0x85,0x15,0xf0,0x1a,0x5d,0x9a
  ,0xf0,0x0f,0x04,0x40,0x60,0xa4,0xaf,0xf0,0xf6,0xf0
  ,0x1c,0x5f,0x9c,0xf7,0xf0,0x3f,0x7f,0xf0,0x70,0x30
  ,0xf0,0x50,0x5f,0xf0,0xf6,0xf0,0x2c,0xf2,0x3c,0xf0
  ,0x6c,0xf2,0x7c,0xf7,0xf6,0x7e,0x3c,0xf7,0xf0,0x3f
  ,0x7f,0xf0,0x70,0x30,0xf0,0x50,0x5f,0xf6,0xf0,0x2e
  ,0x7c,0x1a,0x5d,0x9a,0xf0,0x30,0x70,0xf0,0x39,0x59
  ,0x50,0xf0,0x1c,0xf2,0x2c,0xf0,0x6c,0xf2,0x7c,0x6c
  ,0x6d,0x2b,0xf0,0x30,0x70,0xf0,0x39,0x59,0x50,0xf0
  ,0x2d,0x6b,0x9d,0x7b,0x6b,0x5c,0xf3,0x4d,0x3d,0x1b
  ,0xf0,0x50,0x40,0x13,0x16,0x49,0x59,0x86,0x83,0x50
  ,0xf0,0x1b,0x5e,0x9b,0xf0,0x19,0x13,0x40,0x50,0x83
  ,0xf0,0x89,0x80,0xf6,0x7e,0x3c,0xf7,0xf0,0x0f,0x04
  ,0x40,0x60,0xa4,0xaf,0xf6,0xf0,0x3e,0x7c,0xf7,0xf6
  ,0x7e,0x3c,0xf7,0xf0,0xaf,0xaf,0x59,0xf0,0x0f,0x59
  ,0x50,0xf6,0x7e,0x3c,0xf7,0xf0,0xa4,0xf4,0x60,0x40
  ,0x04,0x0b,0x4f,0x6f,0xab,0xa4,0xf6,0xf0,0x3e,0x7c
  ,0xf7,0xf6,0x2e,0x4f,0x6f,0xf1,0x6d,0x3a,0x4a,0xf1
  ,0x58,0x36,0x27,0xf7,0xf0,0xf6,0x2d,0x5e,0x6e,0xf1
  ,0x6c,0x3a,0x6a,0xf1,0x68,0x56,0x27,0xf7,0xf0,0x08
  ,0x98,0xf0,0xf7,0x71,0x7a,0xf4,0x23,0x83,0xf6,0xf0
  ,0xf6,0x3b,0x6e,0x66,0xf7,0xf0,0x08,0x98,0xf0,0xf7
  ,0x29,0x4a,0xf1,0x47,0x33,0x22,0x21,0x71,0xf6,0x4c
  ,0xf2,0x5d,0x1c,0xf2,0x2c,0xf0,0x6c,0xf2,0x7c,0x6c
  ,0x0e,0x9e,0x01,0xa1,0xf0,0x51,0x5f,0xf7,0x25,0x24
  ,0x33,0x53,0x64,0xf6,0x6d,0x7e,0x8e,0x9e,0xad,0xac
  ,0xf0,0x39,0x89,0x60,0x6f,0xf0,0xa2,0x62,0xf1,0x67
  ,0xac,0x0b,0x87,0x03,0xf0,0x4b,0xc7,0x43,0x83,0x07
  ,0x8b,0xf0,0xc3,0x47,0xcb,0x00,0x0a,0x9a,0x90,0x00
  ,0xf7,0x10,0x1e,0xf6,0x1d,0xf0,0x13,0xf4,0x40,0x50
  ,0x83,0x86,0x59,0x49,0x16,0x13,0xf0,0xf7,0x00,0x30
  ,0xf0,0xf6,0x0d,0x3d,0xf8,0x58,0xa8,0xf9,0xf0,0x00
  ,0x0f,0x3f,0xa8,0xa7,0x30,0x00,0xf0,0x5f,0x50,0xf0
  ,0x70,0x7f,0xf0,0x59,0x59,0xf1,0x5c,0x5f,0x9f,0x7d
  ,0x4b,0x28,0xf3,0x4a,0xf0,0x6a,0xf3,0x88,0xf0,0x8d
  ,0xf3,0x6b,0xf0,0x5c,0x5f,0xf0,0x2d,0xf3,0x4b,0x35
  ,0x10,0x25,0xf2,0x37,0xf0,0x65,0x65,0xf2,0x77};

private const ushort arc_symbol_offsets[]={
      1,   1,  27, 395,  66, 385,  52,  27
  , 165, 229, 106, 106,  22, 106,   8, 389
  , 158,  79, 171, 259, 255, 301, 298, 309
  , 261, 461,   8,  12, 431, 405, 425, 413
  , 437, 249, 204, 189, 216, 180, 181, 187
  , 322, 119, 456, 311, 180, 318, 346, 189
  , 204, 189, 201, 280, 125, 144, 149, 151
  , 222, 589, 226, 586, 585, 582, 907, 720
  ,1502, 492, 515, 495, 491, 495, 561, 518
  , 369,  90,  93,1031,  86, 635, 369, 518
  , 473, 478, 369, 533, 567, 378, 579, 575
  ,1045, 731,1049, 648,  82, 627, 351, 658
  , 505, 103,1129,1095,1082,1103,1118, 431
  , 425,1156, 351, 880, 931, 844,1264, 877
  ,  40,1146, 870, 857, 890, 835, 550, 899
  , 909, 919, 890,1039, 155,1000, 405, 589
  , 235, 601, 130, 793,1060,1052,1014,1077
  , 331,1134, 360, 764,1158,1214, 813,1174
  ,1204,1177, 744, 957, 965, 985,1029, 625
  ,1161,1188,1191,1201,1217,1250, 945,1529
  ,1519,1524,1534,1495,1553,1556,1462,1470
  ,1474,1513,1543,1485,1539,1498,1502,1480
  ,1504,1509,1578,1625,1234,1745,1863,1568
  ,1872,1188,2012,2020,2022,2009,1307,1318
  ,1333,1342,1353,1363,1377,1392,1400,1410
  ,1420,1342,1391,1342,1447,1349,1342,   1
  ,   0,   0,1271,1279,  27, 906, 719, 330
  ,1299,   0, 657,1501,1498,   0,1495,   0
  ,1591,1635,1604,1613,1699,1766,1720,1775
  ,1754,1798,1841,1805,1845,1824,1860,1829
  ,1728,1881,1936,1931,1666,1676, 280, 533
  ,1691, 722, 731,1919,1903,1791,1891,1908
  ,2051,2058,2065,1990,1979,1965,2070,2070
  ,2107,2141,2043,2027,2094,1651,2119, 764
  ,1283,1625,2139,2121};

private const byte arc_symbol_counts[]={
    0,10,13,11,15,10,14, 6
  , 6, 6,13, 5, 5, 2, 3, 2
  , 7, 3,10, 9, 4,10,11, 3
  ,19,12, 7,10, 3, 5, 3,13
  ,19, 6,13, 9, 7, 7, 6,11
  , 8, 8, 5, 8, 3, 5, 5,10
  , 7,13,10,12, 5, 6, 3, 5
  , 5, 6, 4, 4, 2, 4, 3, 2
  , 2,12,12, 9,13,11, 8,17
  ,10, 9,11, 8, 6,13, 7, 9
  ,15,14, 6,13, 8, 8, 3, 5
  , 5, 8, 4, 9, 5, 9, 8,61
  ,10, 8, 5, 8,13,14,11, 6
  , 6, 3,17,10,14,14, 7, 4
  ,13,10, 8,13, 6,10,11, 7
  ,10,12, 9, 6,10,15, 8,12
  ,20,24,20,20,18,16,15, 5
  ,20,13,16,17,15,15,22,13
  ,10,13,20, 8,20,15, 3, 3
  ,15,12,12,10,17,14,13, 5
  , 5, 5, 5, 3, 8,12, 8, 6
  , 6, 6,10,10, 4, 3, 2, 5
  , 5, 6,20,22,16,16, 9,23
  ,19,12, 8, 2, 5, 3,11,15
  , 9,11,10,14,14, 8,10,10
  ,27,21, 9, 7,15, 4, 4, 0
  , 0, 0, 8, 4, 6, 4, 3,16
  , 8, 1,62, 3, 3, 1, 3, 0
  ,13,16,16,12,21,21,14,16
  ,12,15,10,19,15,13, 9,12
  ,17,14,15,15,12,15,18,17
  ,22,17,13,12,11,14,12,11
  , 7, 7, 5,19,16,25,24,16
  ,12, 8, 8,16,13,15, 2,20
  ,16,10, 2,18};

/*
 * Character set mappings.  The first byte of each pair is the replacement
 * value, the second byte is the character code.  Each table ends with 0.
 */

static const ushort asciihp[] = {		 /* character set 0, no
						    translation */
    0, 0
};

static const ushort hp9825[] = {		 /* font 1 (hp 9825 character
						    font): first number is CCP
						    character index, second
						    number is ASCII  */
    175, 92,					 /* grid (square root sign) */
    123, 94,					 /* up arrow */
    214, 95,					 /* grid (backspace;
						    underscore) */
    219, 96,					 /* grid (backspace; grave
						    accent) */
    117, 123,					 /* greek upper case letter pi */
    176, 124,					 /* grid (sideways "t") */
    167, 125,					 /* grid (right arrow) */
    216, 126,					 /* grid (backspace; tilde) */
    0, 0
};
static const ushort fr_gr[] = {			/* font 2 (french / german font) */
    139, 35,					 /* english pound sign */
    220, 39,					 /* grid (backspace; acute
						    accent) */
    156, 92,					 /* lower case "c" with cedilla */
    213, 94,					 /* grid (backspace;
						    circumflex) */
    214, 95,					 /* grid (backspace;
						    underscore) */
    219, 96,					 /* grid (backspace; grave
						    accent) */
    210, 123,					 /* grid (backspace; diaeresis
						    or umlaut) */
    211, 124,					 /* grid (backspace; center top
						    circle) */
    210, 125,					 /* grid (backspace; diaeresis
						    or umlaut) */
    7, 126,					 /* single quote */
    0, 0
};
static const ushort scand[] = {			/* font 3 (scandinavian) */
    139, 35,					 /* english pount sign */
    158, 91,					 /* upper case "o" with slash */
    147, 92,					 /* upper case "ae" combination */
    121, 93,					 /* lower case "o" with slash */
    148, 94,					 /* lower case "ae" combination */
    214, 95,					 /* grid (backspace;
						    underscore) */
    210, 123,					 /* grid (backspace; diaeresis
						    or umlaut) */
    211, 124,					 /* grid (backspace; top center
						    circle) */
    210, 125,					 /* grid (backspace; diaeresis
						    or umlaut) */
    211, 126,					 /* grid (backspace; top center
						    circle) */
0, 0};
static const ushort spanish[] = {		/* font 4 (spanish and latin american) */
    137, 35,					 /* inverted question mark */
    220, 39,					 /* grid (backspace; acute
						    accent) */
    135, 92,					 /* lower case dotted "i" */
    213, 94,					 /* grid (backspace;
						    circumflex) */
    214, 95,					 /* grid (backspace;
						    underscore) */
    215, 123,					 /* grid (backspace; long
						    tilde) */
    216, 124,					 /* grid (backspace; tilde) */
    215, 125,					 /* grid (backspace; long
						    tilde) */
    216, 126,					 /* grid (backspace; tilde) */
    0, 0
};
static const ushort special[] = {		/* font 5 (special symbols) */
    190, 65,					 /* grid (face centered symbol) */
    191, 66,					 /* total 17 symbs */
    192, 67,
    193, 68,
    194, 69,
    195, 70,
    196, 71,
    197, 72,
    198, 73,
    199, 74,
    200, 75,
    201, 76,
    202, 77,
    203, 78,
    204, 79,
    205, 80,
    206, 81,
    /* break */
    159, 97,					 /* grid (intersection symbol) */
    160, 98,
    161, 99,
    162, 100,
    0xbb, 101,					 /* over head bar */
    164, 102,					 /* alway equal */
    165, 103,					 /* grid (approximately equal
						    symbol) */
    106, 104,					 /* approximate symbol */
    94, 105,					 /* similar (tilde) symbol */
    103, 106,					 /* less than or equal symbol */
    104, 107,					 /* greater than or equal
						    symbol */
    126, 108,					 /* not equal symbol */
    111, 109,					 /* greek upper case delta */
    117, 110,					 /* greek upper case pi */
    119, 111,					 /* greek upper case sigma
						    (summation symbol) */
    97, 112,					 /* plus or minus sign */
    166, 113,					 /* grid (minus or plus sign) */
    167, 114,					 /* grid (right arrow) */
    177, 115,					 /* up arrow */
    168, 116,					 /* grid (left arrow) */
    169, 117,					 /* grid (down arrow) */
    170, 118,					 /* grid (integral symbol) */
    96, 119,					 /* division sign */
    171, 120,					 /* grid (five pointed star) */
    172, 121,					 /* grid (gradient - inverted
						    greek delta) */
    105, 122,					 /* degrees symbol */
    0, 0
};

static const ushort swedish_iso[] = {		/* font 50 iso swedish */
    125, 36,                                   /* circle with four corner */
    128, 91,                                   /* upper case "A" with 2 dots */
    129, 92,                                   /* upper case "O" with 2 dots */
    149, 93,                                   /* upper case "A" with one dot */
    131, 123,                                  /* lower case "a" with 2 dots */
    132, 124,                                  /* lower case "o" with 2 dots */
    0x110, 125,                                /* lower case "a" with one dot */
    0xbb, 126,                                 /* upper line */
    0,0

};

static const ushort norway_iso1[] = {		/* font 59 iso Norway */
    147, 91,                                   /* upper case "ae" combination   */
    158, 92,                                   /* upper case "O" with slash */
    149, 93,                                   /* upper case "A" with one dot */
    148, 123,                                  /* lower case "ae" combination */
    121, 124,                                  /* lower case "o" with slash */
    0x110, 125,                                /* lower case "a" with one dot */
    0xbb, 126,                                 /* upper line */
     0,0
};
   
static const ushort norway_iso2[] = {		/* font 59 iso Norway */
    142, 35,                                   /* section divider */
    147, 91,                                   /* upper case "ae" combination   */
    158, 92,                                   /* upper case "O" with slash */
    149, 93,                                   /* upper case "A" with one dot */
    148, 123,                                  /* lower case "ae" combination */
    121, 124,                                  /* lower case "o" with slash */
    0x110, 125,                                /* lower case "a" with one dot */
    92, 126,                                   /* vertical line */
     0,0
};

static const ushort roman_ext[] = {		/* character sets 7,17,27 */
	0xe3, 33,
	0xe0, 34,
	0xe8, 35,
	0xe6, 36,
	0xe4, 37,
	0xe9, 38,
	0xeb, 39,
	0x10e,40,
	0x40, 41,
	0x3e, 42,
	0xba, 43,
	0x111, 44,
	0xff, 45,
	0xfd, 46,
	0x10f,47,	
	0xbb, 48,
	0xfb, 49,
	0xfa, 50,
	0xbd, 51,
	0xb4, 52,
	0x9c, 53,
	0x88, 54,
	0x8a, 55,
	0x87, 56,
	0x89, 57,
	0x7d, 58,
	0x8b, 59,
	0x7f, 60,
	0x8e, 61,
	0x10b,62,
	0x10a,63,
	0xe1, 64,
	0xe7, 65,
	0xf1, 66,
	0xfe, 67,
	0x98, 68,
	0x8f, 69,
	0x9a, 70,
	0x9b, 71,
	0x8c, 72,
	0x91, 73,
	0x99, 74,
	0x90, 75,
	0x83, 76,
	0xe5, 77,
	0x84, 78,
	0x85, 79,
	0x95, 80,
	0xea, 81,
	0x9e, 82,
	0x93, 83,
	0x110,84,
	0xee, 85,
	0x79, 86,
	0x94, 87,
	0x80, 88,
	0xb6, 89,
	0x81, 90,
	0x82, 91,
	0xb5, 92,
	0xec, 93,
	0x86, 94,
	0xf0, 95,
	0xe2, 96,
	0xb2, 97,
	0xb3, 98,
	0x10c,99,
	0x10d,100,
	0xed, 101,
	0xef, 102,
	0xf3, 103,
	0xf2, 104,
	0xb7, 105,
	0xb8, 106,
	0xf6, 107,
	0xf7, 108,
	0xfc, 109,
	0xf8, 110,
	0xf9, 111,
	0x106, 112,
	0x107, 113,
	14, 114,
	0x73, 115,
	0x108, 116,
	0x105, 117,
	0x104, 119,
	0x103, 120,
	0xf5, 121,
	0xf4, 122,
	0x101, 123,
	0x102, 124,
	0x100, 125,
	13, 118,
	0x61, 126,
	0, 0
};

static const ushort jis_ascii[] = {		/* character sets 6,16,26 */
	0x7f, 92,
	0xbb, 126,
	0, 0
}; 

static const ushort irv_iso[] = {		/* character sets 9,19,29 */
	0x7d, 36,
	0xbb, 126, 
	0,0
};

static const ushort swedish_name_iso[] = {	/* character sets 31,41,51 */
	0x7d, 36,
	0xb5, 64,
	0x80, 91,
	0x81, 92,
	0x95, 93,
	0x82, 94,
	0x8f, 96,
	0x83, 123,
	0x84, 124,
	0x110, 125,
	0x85, 126,
	0,0
}; 

static const ushort german_iso[] = {		/* character sets 33,43,53 */
	0x8e, 64,
	0x80, 91,
	0x81, 92,
	0x82, 93,
	0x83, 123,
	0x84, 124,
	0x85, 125,
	0x86, 126,
	0,0
};

static const ushort french_iso[] = {		/* character sets 34,44,54 */
	0x8b, 35,
	0x8c, 64,
	0xbd, 91,
	0x9c, 92,
	0x8e, 93,
	0x8f, 123,
	0x90, 124,
	0x91, 125,
	0xba, 126,
	0, 0
};

static const ushort uk_iso[] = {		/* character sets 35,45,55 */
	0x8b, 35,
	0xbb, 126,
	0, 0

};

static const ushort italian_iso[] = {		/* character sets 36,46,56 */
	0x8b, 35,
	0x8e, 64,
	0xbd, 91,
	0x9c, 92,
	0x8f, 93,
	0x90, 96,
	0x8c, 123,
	0x99, 124,
	0x91, 125,
	0xb6, 126,
	0, 0
};

static const ushort spanish_iso[] = {		/* character sets 37,47,57 */
	0x8b, 35,
	0x8e, 64,
	0x87, 91,
	0x88, 92,
	0x89, 93,
	0xbd, 123,
	0x8a, 124,
	0x9c, 125,
	0, 0
};

static const ushort portuguese_iso[] = {	/* character sets 38,48,58 */
	0x8e, 64,
	0xb2, 91,
	0xb4, 92,
	0xb7, 93,
	0xb3, 123,
	0x9c, 124,
	0xb8, 125,
	0xbd, 126,
	0, 0
};

static const ushort french2_iso[] = {		/* character sets 60,70,80 */
	0x8b, 35,
	0x8c, 64,
	0xbd, 91,
	0x9c, 92,
	0x8e, 93,
	0x73, 96,
	0x8f, 123,
	0x90, 124,
	0x91, 125,
	0xba, 126,
	0, 0
};

static const ushort draft[] = {			/* character set 99 */
	0x10a, 35,
	0x10e, 39,
	0x113, 42,
	0x112, 44,
	0x79, 92,
	0xbc, 94,
	0x73, 123,
	0xbd, 124,
	0x109,125,
	0, 0
};

static const ushort config_plt[] = {    /* special character sets   */
    0x110,123,                 /* lower case "a" with one dot */
    0x8f, 124,                  /* Right accent e*/
    0x9a, 125,                  /* Right accent o */
    0x9b, 126,                  /* Right accent u */
    0x83,  91,                  /* Umla a */
    0x8a,  92,                  /* tilde n */
    0x84,  93,                  /* Umla o */
    0x85,  94,                  /* Umla u */
       0,   0
};

static const ushort *hpcharset[] = {
    asciihp,                                     /* set 0, 10 of hp */
    hp9825,                                      /* ..  1, 11 ....  */
    fr_gr,
    scand,
    spanish,
    special,
    jis_ascii,
    roman_ext,
    asciihp,
    irv_iso,
    swedish_iso,
    swedish_name_iso,
    norway_iso1,
    german_iso,
    french_iso,
    uk_iso,
    italian_iso,
    spanish_iso,
    portuguese_iso,
    norway_iso2,
    french2_iso,
    draft,
    config_plt          /* special for use with configuration plot only*/
};
