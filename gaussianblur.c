/*****************************************************************************
 * gaussianblur.c : gaussian blur video filter
 *****************************************************************************
 * Copyright (C) 2000-2007 VLC authors and VideoLAN
 * $Id: de3bcf4c69512ff10b66dd2a816ffd38ff4c9cdd $
 *
 * Authors: Antoine Cellerier <dionoea -at- videolan -dot- org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_memory.h>
#include <vlc_filter.h>
#include <vlc_picture.h>
#include "filter_picture.h"

#include <stdio.h>
#include <math.h>                                          /* exp(), sqrt() */

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
static int  Create    ( vlc_object_t * );
static void Destroy   ( vlc_object_t * );

#define SIGMA_MIN (0.01)
#define SIGMA_MAX (4096.0)

#define SIGMA_TEXT N_("Gaussian's std deviation")
#define SIGMA_LONGTEXT N_( \
    "Gaussian's standard deviation. The blurring will take " \
    "into account pixels up to 3*sigma away in any direction.")


#define HEIGHT_MAX (100)
#define HEIGHT_MIN (0)

#define BLACK_MAX (100)
#define BLACK_MIN (0)

#define HEIGHT_TEXT N_("Gaussian's std deviation")
#define HEIGHT_LONGTEXT N_( \
    "Gaussian's standard deviation. The blurring will take " \
    "into account pixels up to 3*sigma away in any direction.")

#define GAUSSIAN_HELP N_("Add a blurring effect")

#define FILTER_PREFIX "gaussianblur-"

vlc_module_begin ()
    set_description( N_("Gaussian blur video filter") )
    set_shortname( N_( "Gaussian Blur" ))
    set_help(GAUSSIAN_HELP)
    set_capability( "video filter", 0 )
    set_category( CAT_VIDEO )
    set_subcategory( SUBCAT_VIDEO_VFILTER )

    add_float_with_range( FILTER_PREFIX "sigma", 2., SIGMA_MIN, SIGMA_MAX,
                          SIGMA_TEXT, SIGMA_LONGTEXT,
                          false )

    add_integer_with_range( FILTER_PREFIX "height", -2, HEIGHT_MIN, HEIGHT_MAX,
                          HEIGHT_TEXT, HEIGHT_LONGTEXT,
                          false )
    
    add_integer_with_range( FILTER_PREFIX "black", 0, BLACK_MIN, BLACK_MAX,
                          HEIGHT_TEXT, HEIGHT_LONGTEXT,
                          false )

    set_callbacks( Create, Destroy )
vlc_module_end ()

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static picture_t *Filter( filter_t *, picture_t * );

static const char *const ppsz_filter_options[] = {
    "sigma", "height", "black", NULL
};

/* Comment this to use floats instead of integers (faster for bigger sigma
 * values)
 * For sigma = 2 ints are faster
 * For sigma = 4 floats are faster
 */

#define DONT_USE_FLOATS

#ifdef DONT_USE_FLOATS
#   define type_t int
#else
#   define type_t float
#endif

struct filter_sys_t
{
    double f_sigma;
    int i_dim;
    int i_height;
    // int i_weight;
    // int i_x;
    // int i_y;
    // int i_basis_distrib;

    bool b_black;
    

    type_t *pt_distribution;
    type_t *pt_buffer;
    type_t *pt_scale;
};

static void gaussianblur_InitDistribution( filter_sys_t *p_sys )
{
    double f_sigma = p_sys->f_sigma;
    int i_dim = (int)(3.*f_sigma);
    type_t *pt_distribution = xmalloc( (2*i_dim+1) * sizeof( type_t ) );

    // AS MACRO ???
    // int a = i_dim;
    // int temp = sqrt( exp(-(a*a)/(f_sigma*f_sigma) ) / (2.*M_PI*f_sigma*f_sigma));
    // p_sys->i_basis_distrib = (type_t) ( temp * ( 1 << 8 ) );

    for( int x = -i_dim; x <= i_dim; x++ )
    {
        const float f_distribution = sqrt( exp(-(x*x)/(f_sigma*f_sigma) ) / (2.*M_PI*f_sigma*f_sigma) );

#ifdef DONT_USE_FLOATS
        const float f_factor = 1 << 8;
#else
        const float f_factor = 1;
#endif

        pt_distribution[i_dim+x] = (type_t)( f_distribution * f_factor );
    }
    p_sys->i_dim = i_dim;
    p_sys->pt_distribution = pt_distribution;
}

static int Create( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    if(   p_filter->fmt_in.video.i_chroma != VLC_CODEC_I420
       && p_filter->fmt_in.video.i_chroma != VLC_CODEC_J420
       && p_filter->fmt_in.video.i_chroma != VLC_CODEC_YV12

       && p_filter->fmt_in.video.i_chroma != VLC_CODEC_I422
       && p_filter->fmt_in.video.i_chroma != VLC_CODEC_J422
      )
    {
        /* We only want planar YUV 4:2:0 or 4:2:2 */
        msg_Err( p_filter, "Unsupported input chroma (%4.4s)",
                 (char*)&(p_filter->fmt_in.video.i_chroma) );
        return VLC_EGENERIC;
    }

    if( p_filter->fmt_in.video.i_chroma != p_filter->fmt_out.video.i_chroma )
    {
        msg_Err( p_filter, "Input and output chromas don't match" );
        return VLC_EGENERIC;
    }

    p_filter->p_sys = malloc( sizeof( filter_sys_t ) );
    if( p_filter->p_sys == NULL )
        return VLC_ENOMEM;

    config_ChainParse( p_filter, FILTER_PREFIX, ppsz_filter_options,
                       p_filter->p_cfg );

    p_filter->pf_video_filter = Filter;
    
    p_filter->p_sys->f_sigma =
        var_CreateGetFloat( p_filter, FILTER_PREFIX "sigma" );
    if( p_filter->p_sys->f_sigma <= 0. )
    {
        msg_Err( p_filter, "sigma must be greater than zero" );
        return VLC_EGENERIC;
    }
    
    p_filter->p_sys->f_sigma =
        var_CreateGetFloat( p_filter, FILTER_PREFIX "sigma" );
    if( p_filter->p_sys->f_sigma <= 0. )
    {
        msg_Err( p_filter, "sigma must be greater than zero" );
        return VLC_EGENERIC;
    }
    
    p_filter->p_sys->b_black =
        var_CreateGetBool( p_filter, FILTER_PREFIX "black" );
    if(p_filter->p_sys->b_black != 1){
        p_filter->p_sys->b_black = 0;
    }

    p_filter->p_sys->i_height =
        var_CreateGetInteger( p_filter, FILTER_PREFIX "height" );
    if( p_filter->p_sys->i_height < 0 
        && p_filter->p_sys->i_height != -2)
    {
        msg_Err( p_filter, "height must be greater than zero" );
        return VLC_EGENERIC;
    }
    
    gaussianblur_InitDistribution( p_filter->p_sys );
    msg_Dbg( p_filter, "gaussian distribution is %d pixels wide",
             p_filter->p_sys->i_dim*2+1 );

    p_filter->p_sys->pt_buffer = NULL;
    p_filter->p_sys->pt_scale = NULL;

    return VLC_SUCCESS;
}

static void Destroy( vlc_object_t *p_this )
{
    filter_t *p_filter = (filter_t *)p_this;

    free( p_filter->p_sys->pt_distribution );
    free( p_filter->p_sys->pt_buffer );
    free( p_filter->p_sys->pt_scale );

    free( p_filter->p_sys );
}

static picture_t *Filter( filter_t *p_filter, picture_t *p_pic )
{
    picture_t *p_outpic;
    filter_sys_t *p_sys = p_filter->p_sys;
    const int i_dim = p_sys->i_dim;
    type_t *pt_buffer;
    type_t *pt_scale;
    const type_t *pt_distribution = p_sys->pt_distribution;

    if( !p_pic ) return NULL;

    p_outpic = filter_NewPicture( p_filter );
    if( !p_outpic )
    {
        picture_Release( p_pic );
        return NULL;
    }
    if( !p_sys->pt_buffer )
    {
        p_sys->pt_buffer = realloc_or_free( p_sys->pt_buffer,
                               p_pic->p[Y_PLANE].i_visible_lines *
                               p_pic->p[Y_PLANE].i_pitch * sizeof( type_t ) );
    }


    pt_buffer = p_sys->pt_buffer;
    if( !p_sys->pt_scale )
    {
        const int i_visible_lines = p_pic->p[Y_PLANE].i_visible_lines;
        const int i_visible_pitch = p_pic->p[Y_PLANE].i_visible_pitch;
        const int i_pitch = p_pic->p[Y_PLANE].i_pitch;

        p_sys->pt_scale = xmalloc( i_visible_lines * i_pitch * sizeof( type_t ) );
        pt_scale = p_sys->pt_scale;

        for( int i_line = 0; i_line < i_visible_lines; i_line++ )
        {
            for( int i_col = 0; i_col < i_visible_pitch; i_col++ )
            {
                type_t t_value = 0;

                for( int y = __MAX( -i_dim, -i_line );
                     y <= __MIN( i_dim, i_visible_lines - i_line - 1 );
                     y++ )
                {
                    for( int x = __MAX( -i_dim, -i_col );
                         x <= __MIN( i_dim, i_visible_pitch - i_col + 1 );
                         x++ )
                    {
                        t_value += pt_distribution[y+i_dim] *
                                   pt_distribution[x+i_dim];
                    }
                }
                pt_scale[i_line*i_pitch+i_col] = t_value;

            }
        }
    }

    if(p_filter->p_sys->i_height == -2)
        p_filter->p_sys->i_height = p_pic->p[Y_PLANE].i_visible_lines;

    pt_scale = p_sys->pt_scale;
    printf("%ls | ",pt_scale);
    for( int i_plane = 0 ; i_plane < p_pic->i_planes ; i_plane++ )
    {

        uint8_t *p_in = p_pic->p[i_plane].p_pixels;
        uint8_t *p_out = p_outpic->p[i_plane].p_pixels;

        const int i_visible_lines = p_pic->p[i_plane].i_visible_lines;
        const int i_visible_pitch = p_pic->p[i_plane].i_visible_pitch;
        const int i_in_pitch = p_pic->p[i_plane].i_pitch;

        const int x_factor = p_pic->p[Y_PLANE].i_visible_pitch/i_visible_pitch-1;
        const int y_factor = p_pic->p[Y_PLANE].i_visible_lines/i_visible_lines-1;
        const int i_height = p_sys->i_height;
        // const int i_base = p_sys->i_basis_distrib;

        for( int i_line = 0; i_line < i_visible_lines; i_line++ )
        {
            for( int i_col = 0; i_col < i_visible_pitch; i_col++ )
            {
                type_t t_value = 0;
                const int c = i_line*i_in_pitch+i_col;

                //STEP
                for( int x = __MAX( -i_dim, -i_col*(x_factor+1) );
                    x <= __MIN( i_dim, (i_visible_pitch - i_col)*(x_factor+1) + 1 );
                    x++ )
                {
                    if(i_col<=i_height){
                        t_value += pt_distribution[x+i_dim] *
                                p_in[c+(x>>x_factor)];
                        //BLACK
                        if(p_sys->b_black==1){
                            if(i_plane==0){
                            t_value = 0;
                            // printf("%d | ", t_value);
                            }
                        }
                    } else {
                        // t_value += i_base * p_in[c+(x>>x_factor)];
                        t_value = 1021 * p_in[c+(x>>x_factor)];
                    }
                }
                pt_buffer[c] = t_value;
            }
        }

        for( int i_line = 0; i_line < i_visible_lines; i_line++ )
        {
            for( int i_col = 0; i_col < i_visible_pitch; i_col++ )
            {
                type_t t_value = 0;
                const int c = i_line*i_in_pitch+i_col;

                for( int y = __MAX( -i_dim, (-i_line)*(y_factor+1) );
                    y <= __MIN( i_dim, (i_visible_lines - i_line)*(y_factor+1) - 1 );
                    y++ )
                {
                    if(i_line<=i_height){
                        t_value += pt_distribution[y+i_dim] *
                                pt_buffer[c+(y>>y_factor)*i_in_pitch];
                    } else {
                        // t_value = p_sys->pt_scale * p_in[c];
                        t_value = 1021 * p_in[c];
                    }
                }

                const type_t t_scale = pt_scale[(i_line<<y_factor)*(i_in_pitch<<x_factor)+(i_col<<x_factor)];
                p_out[i_line * p_outpic->p[i_plane].i_pitch + i_col] = (uint8_t)(t_value / t_scale); // FIXME wouldn't it be better to round instead of trunc ?
            }
        }
    }
    return CopyInfoAndRelease( p_outpic, p_pic );
}
