/* Copyright (C) 1996, 1997, 1998 Aladdin Enterprises.  All rights
   reserved.  Unauthorized use, copying, and/or distribution
   prohibited.  */

/* pcfont.c */
/* PCL5 font selection and text printing commands */
#include "std.h"
#include <stdlib.h>
#include "memory_.h"
/* The following are all for gxfont42.h. */
#include "gx.h"
#include "gsccode.h"
#include "gsmatrix.h"
#include "gxfont.h"
#include "gxfont42.h"
#include "pcommand.h"
#include "pcstate.h"
#include "pcursor.h"
#include "pcfont.h"
#include "pcfsel.h"
#include "pjtop.h"

/*
 * Decache the HMI after resetting the font.  According to TRM 5-22,
 * this always happens, regardless of how the HMI was set; formerly,
 * we only invalidated the HMI if it was set from the font rather than
 * explicitly.
 */
#define pcl_decache_hmi(pcs)\
  do {\
      pcs->hmi_cp = HMI_DEFAULT;\
  } while (0)


/* Clear the font pointer cache after changing a font parameter.  set
 * indicates which font (0/1 for primary/secondary).  -1 means both. */
void
pcl_decache_font(pcl_state_t *pcs, int set)
{	
	if ( set < 0 )
	  {
	    pcl_decache_font(pcs, 0);
	    pcl_decache_font(pcs, 1);
	  }
	else
	  {
	    pcs->font_selection[set].font = NULL;
	    if ( pcs->font_selected == set )
	      {
	        pcs->font = NULL;
		pcs->map = NULL;
		pcl_decache_hmi(pcs);
	      }
	  }
}

/* set current font and symbol table to selected parameter's font and
   symbol table */
private int
pcl_set_font(pcl_state_t *pcs, pcl_font_selection_t *pfs)
{
	pcs->font = pfs->font;
	pcs->map = pfs->map;
	return 0;
}

int
pcl_recompute_substitute_font(pcl_state_t *pcs, const uint chr)
{
	pcl_font_selection_t *pfs = &pcs->font_selection[pcs->font_selected];
	int code = pcl_reselect_substitute_font(pfs, pcs, chr);
	if ( code < 0 )
	  return code;
	pcl_set_font(pcs, pfs);
	return 0;
}

/* Recompute the current font from the descriptive parameters. */
/* This is exported for resetting HMI. */
int
pcl_recompute_font(pcl_state_t *pcs)
{	pcl_font_selection_t *pfs = &pcs->font_selection[pcs->font_selected];
	int code = pcl_reselect_font(pfs, pcs);

	if ( code < 0 )
	  return code;
	pcl_set_font(pcs, pfs);
	return 0;
}

/* The font parameter commands all come in primary and secondary variants. */
private int
pcl_symbol_set(pcl_args_t *pargs, pcl_state_t *pcs, int set)
{	uint num = uint_arg(pargs);
	uint symset;

	if ( num > 1023 )
	  return e_Range;
	/* The following algorithm is from Appendix C of the */
	/* PCL5 Comparison Guide. */
	symset = (num << 5) + pargs->command - 64;
	if ( symset != pcs->font_selection[set].params.symbol_set )
	  {
	    pcs->font_selection[set].params.symbol_set = symset;
	    pcl_decache_font(pcs, set);
	  }
	return 0;
}
private int /* ESC ( <> others */
pcl_primary_symbol_set(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_symbol_set(pargs, pcs, 0);
}
private int /* ESC ) <> others */
pcl_secondary_symbol_set(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_symbol_set(pargs, pcs, 1);
}

private int
pcl_spacing(pcl_args_t *pargs, pcl_state_t *pcs, int set)
{	uint spacing = uint_arg(pargs);

	if ( spacing > 1 )
	  return 0;
	if ( spacing != pcs->font_selection[set].params.proportional_spacing )
	  {
	    pcs->font_selection[set].params.proportional_spacing = spacing;
	    pcl_decache_font(pcs, set);
	  }
	return 0;
}
private int /* ESC ( s <prop_bool> P */
pcl_primary_spacing(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_spacing(pargs, pcs, 0);
}
private int /* ESC ) s <prop_bool> P */
pcl_secondary_spacing(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_spacing(pargs, pcs, 1);
}

private int
pcl_pitch(floatp cpi, pcl_state_t *pcs, int set)
{	uint pitch_cp;
	pcl_font_selection_t *pfs = &pcs->font_selection[set];

	if ( cpi < 0.1 )
	  cpi = 0.1;
	/* Convert characters per inch to 100ths of design units. */
	pitch_cp = 7200.0 / cpi;
	if ( pitch_cp > 65535 )
	  return e_Range;
	if ( pitch_cp < 1 )
	  pitch_cp = 1;
	if ( pitch_cp != pl_fp_pitch_cp(&pfs->params) )
	  {
	    pl_fp_set_pitch_cp(&pfs->params, pitch_cp);
	    if ( !pfs->selected_by_id )
	      pcl_decache_font(pcs, set);
	  }
	return 0;
}
private int /* ESC ( s <pitch> H */
pcl_primary_pitch(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_pitch(float_arg(pargs) + 0.005, pcs, 0);
}
private int /* ESC ) s <pitch> H */
pcl_secondary_pitch(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_pitch(float_arg(pargs) + 0.005, pcs, 1);
}

private int
pcl_height(pcl_args_t *pargs, pcl_state_t *pcs, int set)
{	uint height_4ths = (uint)(float_arg(pargs) * 4 + 0.5);
	pcl_font_selection_t *pfs = &pcs->font_selection[set];

	if ( height_4ths >= 4000 )
	  return e_Range;
	if ( height_4ths != pfs->params.height_4ths )
	  {
	    pfs->params.height_4ths = height_4ths;
	    if ( !pfs->selected_by_id )
	      pcl_decache_font(pcs, set);
	  }
	return 0;
}
private int /* ESC ( s <height> V */
pcl_primary_height(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_height(pargs, pcs, 0);
}
private int /* ESC ) s <height> V */
pcl_secondary_height(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_height(pargs, pcs, 1);
}

private int
pcl_style(pcl_args_t *pargs, pcl_state_t *pcs, int set)
{	uint style = uint_arg(pargs);

	if ( style != pcs->font_selection[set].params.style )
	  {
	    pcs->font_selection[set].params.style = style;
	    pcl_decache_font(pcs, set);
	  }
	return 0;
}
private int /* ESC ( s <style> S */
pcl_primary_style(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_style(pargs, pcs, 0);
}
private int /* ESC ) s <style> S */
pcl_secondary_style(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_style(pargs, pcs, 1);
}

private int
pcl_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcs, int set)
{	int weight = int_arg(pargs);

	if ( weight < -7 )
	  weight = -7;
	else if ( weight > 7 )
	  weight = 7;
	if ( weight != pcs->font_selection[set].params.stroke_weight )
	  {
	    pcs->font_selection[set].params.stroke_weight = weight;
	    pcl_decache_font(pcs, set);
	  }
	return 0;
}
private int /* ESC ( s <weight> B */
pcl_primary_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_stroke_weight(pargs, pcs, 0);
}
private int /* ESC ) s <weight> B */
pcl_secondary_stroke_weight(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_stroke_weight(pargs, pcs, 1);
}

private int
pcl_typeface(pcl_args_t *pargs, pcl_state_t *pcs, int set)
{	uint typeface = uint_arg(pargs);

	if ( typeface != pcs->font_selection[set].params.typeface_family )
	  {
	    pcs->font_selection[set].params.typeface_family = typeface;
	    pcl_decache_font(pcs, set);
	  }
	return 0;
}
private int /* ESC ( s <typeface> T */
pcl_primary_typeface(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_typeface(pargs, pcs, 0);
}
private int /* ESC ) s <typeface> T */
pcl_secondary_typeface(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_typeface(pargs, pcs, 1);
}

private int
pcl_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcs, int set)
{	uint id = uint_arg(pargs);
	pcl_font_selection_t *pfs = &pcs->font_selection[set];
	int code = pcl_select_font_by_id(pfs, id, pcs);

	switch ( code )
	  {
	  default:		/* error */
	    return code;
	  case 0:
	    if ( pcs->font_selected == set )
	      pcs->font = pfs->font;
	    pcl_decache_hmi(pcs);
	  case 1:		/* not found */
	    return 0;
	  }
}
private int /* ESC ( <font_id> X */
pcl_primary_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_font_selection_id(pargs, pcs, 0);
}
private int /* ESC ) <font_id> X */
pcl_secondary_font_selection_id(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_font_selection_id(pargs, pcs, 1);
}

private int
pcl_select_default_font(pcl_args_t *pargs, pcl_state_t *pcs, int set)
{	if ( int_arg(pargs) != 3 )
	  return e_Range;
	pcl_decache_font(pcs, set);
	return e_Unimplemented;
}
private int /* ESC ( 3 @ */
pcl_select_default_font_primary(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_select_default_font(pargs, pcs, 0);
}
private int /* ESC ) 3 @ */
pcl_select_default_font_secondary(pcl_args_t *pargs, pcl_state_t *pcs)
{	return pcl_select_default_font(pargs, pcs, 1);
}

private int /* SO */
pcl_SO(pcl_args_t *pargs, pcl_state_t *pcs)
{	if ( pcs->font_selected != 1 )
	  { pcs->font_selected = secondary;
	    pcs->font = pcs->font_selection[1].font;
	    pcl_decache_hmi(pcs);
	  }
	return 0;
}

private int /* SI */
pcl_SI(pcl_args_t *pargs, pcl_state_t *pcs)
{	if ( pcs->font_selected != 0 )
	  { pcs->font_selected = primary;
	    pcs->font = pcs->font_selection[0].font;
	    pcl_decache_hmi(pcs);
	  }
	return 0;
}

/* This command is listed only on p. A-7 of the PCL5 Comparison Guide. */
private int /* ESC & k <mode> S */
pcl_set_pitch_mode(pcl_args_t *pargs, pcl_state_t *pcs)
{	int mode = int_arg(pargs);
	floatp cpi;

	/*
	 * The specification in the PCL5 Comparison Guide is:
	 *	0 => 10.0, 2 => 16.67, 4 => 12.0.
	 */
	switch ( mode )
	  {
	  case 0: cpi = 10.0; break;
	  case 2: cpi = 16.67; break;
	  case 4: cpi = 12.0; break;
	  default: return e_Range;
	  }
	/*
	 * It's anybody's guess what this is supposed to do.
	 * We're guessing that it sets the pitch of the currently
	 * selected font parameter set (primary or secondary) to
	 * the given pitch in characters per inch.
	 */
	return pcl_pitch(cpi, pcs, pcs->font_selected);
}

/* Initialization */
private int
pcfont_do_registration(
    pcl_parser_state_t *pcl_parser_state,
    gs_memory_t *mem
)
{		/* Register commands */
	{ int chr;
	  /*
	   * The H-P manual only talks about A through Z, but it appears
	   * that characters from A through ^ (94) can be used.
	   */
	  for ( chr = 'A'; chr <= '^'; ++chr )
	    if ( chr != 'X' )
	      { DEFINE_CLASS_COMMAND_ARGS('(', 0, chr, "Primary Symbol Set",
					  pcl_primary_symbol_set,
					  pca_neg_error|pca_big_error);
	        DEFINE_CLASS_COMMAND_ARGS(')', 0, chr, "Secondary Symbol Set",
					  pcl_secondary_symbol_set,
					  pca_neg_error|pca_big_error);
	      }
	}
	DEFINE_CLASS('(')
	  {'s', 'P',
	     PCL_COMMAND("Primary Spacing", pcl_primary_spacing,
			 pca_neg_ignore|pca_big_ignore)},
	  {'s', 'H',
	     PCL_COMMAND("Primary Pitch", pcl_primary_pitch,
			 pca_neg_error|pca_big_error)},
	  {'s', 'V',
	     PCL_COMMAND("Primary Height", pcl_primary_height,
			 pca_neg_error|pca_big_error)},
	  {'s', 'S',
	     PCL_COMMAND("Primary Style", pcl_primary_style,
			 pca_neg_error|pca_big_clamp)},
	  {'s', 'B',
	     PCL_COMMAND("Primary Stroke Weight", pcl_primary_stroke_weight,
			 pca_neg_ok|pca_big_error)},
	  {'s', 'T',
	     PCL_COMMAND("Primary Typeface", pcl_primary_typeface,
			 pca_neg_error|pca_big_ok)},
	  {0, 'X',
	     PCL_COMMAND("Primary Font Selection ID",
			 pcl_primary_font_selection_id,
			 pca_neg_error|pca_big_error)},
	  {0, '@',
	     PCL_COMMAND("Default Font Primary",
			 pcl_select_default_font_primary,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CLASS(')')
	  {'s', 'P',
	     PCL_COMMAND("Secondary Spacing", pcl_secondary_spacing,
			 pca_neg_ignore|pca_big_ignore)},
	  {'s', 'H',
	     PCL_COMMAND("Secondary Pitch", pcl_secondary_pitch,
			 pca_neg_error|pca_big_error)},
	  {'s', 'V',
	     PCL_COMMAND("Secondary Height", pcl_secondary_height,
			 pca_neg_error|pca_big_error)},
	  {'s', 'S',
	     PCL_COMMAND("Secondary Style", pcl_secondary_style,
			 pca_neg_error|pca_big_clamp)},
	  {'s', 'B',
	     PCL_COMMAND("Secondary Stroke Weight",
			 pcl_secondary_stroke_weight,
			 pca_neg_ok|pca_big_error)},
	  {'s', 'T',
	     PCL_COMMAND("Secondary Typeface", pcl_secondary_typeface,
			 pca_neg_error|pca_big_ok)},
	  {0, 'X',
	     PCL_COMMAND("Secondary Font Selection ID",
			 pcl_secondary_font_selection_id,
			 pca_neg_error|pca_big_error)},
	  {0, '@',
	     PCL_COMMAND("Default Font Secondary",
			 pcl_select_default_font_secondary,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	DEFINE_CONTROL(SO, "SO", pcl_SO)
	DEFINE_CONTROL(SI, "SI", pcl_SI)
	DEFINE_CLASS('&')
	  {'k', 'S',
	     PCL_COMMAND("Set Pitch Mode", pcl_set_pitch_mode,
			 pca_neg_error|pca_big_error)},
	END_CLASS
	return 0;
}

/* look up pjl font number with data storage type.  Return 0 on
   success, 1 if we cannot find the font number and -1 if the data
   storage type does not exist.  Return the font parameters for the
   requested font number or the font parameters of the first found
   font for the active resource. */
 private int
pcl_lookup_pjl_font(pcl_state_t *pcs, int pjl_font_number,
		    pcl_data_storage_t pcl_data_storage, pl_font_params_t *params)
{
    pl_dict_enum_t dictp;
    gs_const_string key;
    void *value;
    bool found_resource = false;

    pl_dict_enum_begin(&pcs->soft_fonts, &dictp);
    while ( pl_dict_enum_next(&dictp, &key, &value) ) {
	pl_font_t *fp = (pl_font_t *)value;
	int ds = fp->storage;
	if ( (int)pcl_data_storage == ds ) {
	    found_resource = true;
	    *params = fp->params;
	    if ( fp->params.pjl_font_number == pjl_font_number ) {
		return 0;
	    }
	}
    }
    return (found_resource ? 1 : -1);
}

/* inherit the current pjl font environment */
 int
pcl_set_current_font_environment(pcl_state_t *pcs)
{
    /* Loop through font resources until we find some fonts.  Set up
       pcl's storage identifier to mirror pjl's */
    pcl_data_storage_t pcl_data_storage;
    while( 1 ) {
	/* get current font source */
	pjl_envvar_t *fontsource = pjl_proc_get_envvar(pcs->pjls, "fontsource");
	switch (fontsource[0]) {
	case 'I':
	    // NB what happens if pjl command is not I - hmmph?
	    if (!pcl_load_built_in_fonts(pcs,
			pjl_proc_fontsource_to_path(pcs->pjls, fontsource))) {
		if ( pcs->personality == rtl )
		    /* rtl doesn't use fonts */
		    return 0;
		else {
		    dprintf("No built-in fonts found during initialization\n");
		    return -1;
		}
	    }
	    pcl_data_storage = pcds_internal;
	    break;
	case 'S':
	    /* nothing to load */
	    pcl_data_storage = pcds_permanent;
	    break;
	    /* NB we incorrectly treat C, C1, C2... as one collective resource */
	case 'C':
	    if ( !pcl_load_cartridge_fonts(pcs,
					   pjl_proc_fontsource_to_path(pcs->pjls, fontsource)) ) {
		pjl_proc_set_next_fontsource(pcs->pjls);
		continue; /* try next resource */
	    }
	    pcl_data_storage = pcds_all_cartridges;
	    break;
	    /* NB we incorrectly treat M, M1, M2... as one collective resource */
	case 'M':
	    if ( !pcl_load_simm_fonts(pcs,
				      pjl_proc_fontsource_to_path(pcs->pjls, fontsource)) ) {
		pjl_proc_set_next_fontsource(pcs->pjls);
		continue; /* try next resource */
	    }
	    pcl_data_storage = pcds_all_simms;
	    break;
	default:
	    dprintf("pcfont.c: unknown pjl resource\n");
	    return -1;
	}
	{
	    int code;
	    pl_font_params_t params;
	    code =  pcl_lookup_pjl_font(pcs, 
			pjl_proc_vartoi(pcs->pjls,
					pjl_proc_get_envvar(pcs->pjls, "fontnumber")),
					pcl_data_storage, &params);
	    /* resource found, but if code is 1 we did not match the
               font number.  NB unsure what to do when code == 1. */
	    if ( code >= 0 ) {
		/* copy parameters to the requested font and
		   apply the other pjl settings to construct the
		   initial font request from pjltrm: "The recommended
		   order for setting FONTNUMBER, FONTSOURCE, and
		   SYMSET is SYMSET first, then FONTSOURCE, then
		   FONTNUMBER".  Perhaps this is a clue as to how
		   these interact.  We search for the font number in
		   the fontsource and apply pjl's SYMSET, PTSIZE and
		   PITCH to the font we found.  That in turn becomes
		   the default requested font */
		pcs->font_selection[0].params = params;
		pcs->default_symbol_set_value = pcs->font_selection[0].params.symbol_set;
		/* NB: The fontsource and fontnumber selection
                   parameters get stepped on next, unless pjl symset,
                   pitch and ptsize are properly updated by the PJL
                   interpreter when the font changes.  Our pjl
                   interpreter does not currently do this.
                   Consequently wrong selections are possible. */
		pcs->default_symbol_set_value = pcs->font_selection[0].params.symbol_set =
		    pjl_proc_map_pjl_sym_to_pcl_sym(pcs->pjls, pjl_proc_get_envvar(pcs->pjls, "symset"));
		pl_fp_set_pitch_per_inch(&pcs->font_selection[0].params,
					 pjl_proc_vartof(pcs->pjls, pjl_proc_get_envvar(pcs->pjls, "pitch")));
		pcs->font_selection[0].params.height_4ths = pjl_proc_vartof(pcs->pjls, pjl_proc_get_envvar(pcs->pjls, "ptsize")) * 4.0;
		pcs->font_selection[1] = pcs->font_selection[0];
		pcs->font_selected = primary;
		pcs->font = 0;
	    }
	    else {
		/* no resouce found - Note for everything but 'S' this
		   is a double check, since we should have failed when
		   checking for the resource Note this is fatal for
		   internal resources but should be caught above. */
		pjl_proc_set_next_fontsource(pcs->pjls);
		continue; /* try next resource */
		
	    }
	}
	return 0; 	/* done */
    }
}


private void
pcfont_do_reset(pcl_state_t *pcs, pcl_reset_type_t type)
{	
    if ((type & pcl_reset_initial) != 0) {
	pcs->font_dir = gs_font_dir_alloc(pcs->memory);
	pl_dict_init(&pcs->built_in_fonts, pcs->memory, pl_free_font);
	pl_dict_init(&pcs->soft_fonts, pcs->memory, pl_free_font);
	pl_dict_set_parent(&pcs->soft_fonts, &pcs->built_in_fonts);
    }
    if ( type & (pcl_reset_initial | pcl_reset_printer | pcl_reset_overlay) ) {
	int code = 0;
	if ( pcs->personality != rtl )
	    code = pcl_set_current_font_environment(pcs);
	/* corrupt configuration */
	if ( code != 0 )
	    exit( 1 );
    }
    if ( type & pcl_reset_permanent ) {
	pl_dict_release(&pcs->soft_fonts);
	pl_dict_release(&pcs->built_in_fonts);
    }
}

const pcl_init_t pcfont_init = {
  pcfont_do_registration, pcfont_do_reset, 0
};
