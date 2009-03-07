/* Copyright (C) 2001-2009 Artifex Software, Inc.
   All Rights Reserved.
  
   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/
   or contact Artifex Software, Inc.,  7 Mt. Lassen Drive - Suite A-134,
   San Rafael, CA  94903, U.S.A., +1(415)492-9861, for further information.
*/
/*  GS ICC link manager.  Initial stubbing of functions.  */


#include "math_.h"
#include "memory_.h"
#include "gx.h"
#include "gserrors.h"
#include "gsiccmanage.h"


/*  Initialize the CMS 
    Prepare the ICC Manager
*/
void 
gsicc_create()
{


}


/*  Shut down the  CMS 
    and the ICC Manager
*/
void 
gsicc_destroy()
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultGray
    member variable in the ICC manager */

void 
gsicc_set_gray_profile(ICCdata graybuffer)
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultRGB
    member variable in the ICC manager */

void 
gsicc_set_rgb_profile(ICCdata rgbbuffer)
{


}

/*  This computes the hash code for the
    ICC data and assigns the code
    and the profile to DefaultCMYK
    member variable in the ICC manager */

void 
gsicc_set_cmyk_profile(ICCdata cmykbuffer)
{


}

/*  This computes the hash code for the
    device profile and assigns the code
    and the profile to the DeviceProfile
    member variable in the ICC Manager
    This should really occur only one time.
    This is different than gs_set_device_profile
    which sets the profile on the output
    device */

void 
gsicc_set_device_profile(ICCdata deviceprofile)
{



}

/* Set the named color profile in the Device manager */

void 
gsicc_set_device_named_color_profile(ICCdata nameprofile)
{


}

/* Set the ouput device linked profile */

void 
gsicc_set_device_linked_profile(ICCdata outputlinkedprofile )
{

}

/* Set the proofing profile and compute
   its hash code */

void 
gsicc_set_proof_profile(ICCdata proofprofile )
{

}

/*  This will occur only if the device did not
    return a profie when asked for one.  This
    will set DeviceProfile member variable to the
    appropriate default device.  If output device
    has more than 4 channels you need to use
    gsicc_set_device_profile with the appropriate
    ICC profile.  */

void 
gsicc_load_default_device_profile(int numchannels)
{



}


/* This is a special case of the functions gsicc_set_gray_profile
    gsicc_set_rgb_profile and gsicc_set_cmyk_profile. In
    this function the number of channels is specified and the 
    appropriate profile contained in Resources/Color/InputProfiles is used */

void 
gsicc_load_default_input_profile(int numchannels)
{


}


/* This is the main function called to obtain a linked transform from the ICC manager
   If the manager has the link ready, it will return it.  If not, it wil request 
   one from the CMS and then return it.*/

void 
gsicc_get_link(gsicc_link_t *icclink, gsiccmanage_colorspace_t  input_colorspace, 
                    gsiccmanage_colorspace_t output_colorspace, 
                    gsicc_rendering_param_t rendering_params)
{





}


/* Used by gs to notify the ICC manager that we are done with this link for now */

void 
gsicc_release_link(gsicc_link_t *icclink)
{


}


void 
setbuffer_desc(gsicc_bufferdesc_t *buffer_desc,unsigned char num_chan,
    unsigned char bytes_per_chan,
    bool has_alpha,
    bool alpha_first,
    bool little_endian,
    bool is_planar,
    gs_icc_colorbuffer_t buffercolor)
{
    buffer_desc->num_chan = num_chan;
    buffer_desc->bytes_per_chan = bytes_per_chan;
    buffer_desc->has_alpha = has_alpha;
    buffer_desc->alpha_first = alpha_first;
    buffer_desc->little_endian = little_endian;
    buffer_desc->is_planar = is_planar;
    buffer_desc->buffercolor = buffercolor;

}











