/* Copyright (C) 1996, 1997 Aladdin Enterprises.  All rights reserved.
   Unauthorized use, copying, and/or distribution prohibited.
 */

/* pcdraw.h */
/* Interface to PCL5 drawing utilities */

#ifndef pcdraw_INCLUDED
#  define pcdraw_INCLUDED

/* ------ Coordinate system ------ */

/* Set the CTM from the current left and top offset, orientation, */
/* and (optionally) print direction. */
/* We export this only for HP-GL/2. */
int pcl_set_ctm(P2(pcl_state_t *pcls, bool print_direction));

/* Compute the logical page size from physical size, orientation, */
/* and offsets. */
void pcl_compute_logical_page_size(P1(pcl_state_t *pcls));

/* Set all necessary graphics state parameters for PCL drawing */
/* (currently only CTM and clipping region). */
int pcl_set_graphics_state(P2(pcl_state_t *pcls, bool print_direction));

/* ------ Cursor ------ */

/* Home the cursor to the left edge of the logical page at the top margin. */
void pcl_home_cursor(P1(pcl_state_t *pcls));

/* Set the cursor X or Y position, with clamping to either the margins */
/* or the logical page limit. */
void pcl_set_cursor_x(P3(pcl_state_t *pcls, coord cx, bool to_margins));
void pcl_set_cursor_y(P3(pcl_state_t *pcls, coord cy, bool to_margins));

/* Clamp the cursor position after resetting a margin. */
#define pcl_clamp_cursor_x(pcls, to_margins)\
  pcl_set_cursor_x(pcls, pcls->cap.x, to_margins)
#define pcl_clamp_cursor_y(pcls, to_margins)\
  pcl_set_cursor_y(pcls, pcls->cap.y, to_margins)

/* Set the HMI. */
void pcl_set_hmi(P3(pcl_state_t *pcls, coord hmi, bool explicit));

/* ------ Color / pattern ------ */

/* Define the header of a pattern definition, */
/* including the storage status that we prefix to it. */
typedef struct pcl_pattern_s {
  pcl_entity_common;
	/* The first 8 or 12 bytes are defined by PCL5. */
	/* See p. 13-18 of the PCL5 Technical Reference Manual. */
  byte format;
  byte continuation;
  byte encoding;
  byte reserved;
  byte height[2];
  byte width[2];
	/* The following fields are optional (!). */
  byte x_resolution[2];
  byte y_resolution[2];
	/* We may insert padding here so that the following bit data */
	/* have the proper alignment. */
	/* Note that we round up the stored width to bitmap alignment, */
	/* and the height to a multiple of 8. */
} pcl_pattern_t;
#define pattern_extra offset_of(pcl_pattern_t, format)
#define pattern_data_offset round_up(sizeof(pcl_pattern_t), align_bitmap_mod)

/* Set the color and pattern for drawing. */
int pcl_set_drawing_color(P3(pcl_state_t *pcls, pcl_pattern_type_t type,
			     const pcl_id_t *pid));

#endif				/* pcdraw_INCLUDED */
