/****************************************************************************
*
*                            Open Watcom Project
*
*  Copyright (c) 2004-2008 The Open Watcom Contributors. All Rights Reserved.
*
*  ========================================================================
*
*    This file contains Original Code and/or Modifications of Original
*    Code as defined in and that are subject to the Sybase Open Watcom
*    Public License version 1.0 (the 'License'). You may not use this file
*    except in compliance with the License. BY USING THIS FILE YOU AGREE TO
*    ALL TERMS AND CONDITIONS OF THE LICENSE. A copy of the License is
*    provided with the Original Code and Modifications, and is also
*    available at www.sybase.com/developer/opensource.
*
*    The Original Code and all software distributed under the License are
*    distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
*    EXPRESS OR IMPLIED, AND SYBASE AND ALL CONTRIBUTORS HEREBY DISCLAIM
*    ALL SUCH WARRANTIES, INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR
*    NON-INFRINGEMENT. Please see the License for the specific language
*    governing rights and limitations under the License.
*
*  ========================================================================
*
* Description:  WGML tags :ADDRESS and :eADDRESS and :ALINE
*                    with helper functions
*
****************************************************************************/
#include    "wgml.h"
#include    "gvars.h"

static  bool            first_aline;    // special for first :ALINE
static  int8_t          a_spacing;      // spacing between adr lines
static  font_number     font_save;      // save for font
static  group_type      sav_group_type; // save prior group type

/***************************************************************************/
/*  :ADDRESS                                                               */
/***************************************************************************/

void gml_address( const gmltag * entry )
{
    if( !((ProcFlags.doc_sect == doc_sect_titlep) ||
          (ProcFlags.doc_sect_nxt == doc_sect_titlep)) ) {
        g_err( err_tag_wrong_sect, entry->tagname, ":TITLEP section" );
        err_count++;
        show_include_stack();
        scan_start = scan_stop + 1;
        return;
    }
    first_aline = true;
    font_save = g_curr_font;
    g_curr_font = layout_work.address.font;
    rs_loc = address_tag;

    init_nest_cb();
    nest_cb->p_stack = copy_to_nest_stack();
    nest_cb->c_tag = t_ADDRESS;
    nest_cb->left_indent = nest_cb->prev->left_indent;
    nest_cb->right_indent = nest_cb->prev->right_indent;

    spacing = layout_work.titlep.spacing;

    /************************************************************/
    /*  pre_skip is treated as pre_top_skip because             */
    /*  it is always used at the top of the page                */
    /*  this is not what the docs say                           */
    /************************************************************/

    set_skip_vars( NULL, &layout_work.address.pre_skip, NULL, spacing, g_curr_font );

    sav_group_type = cur_group_type;
    cur_group_type = gt_address;
    cur_doc_el_group = alloc_doc_el_group( gt_address );
    cur_doc_el_group->next = t_doc_el_group;
    t_doc_el_group = cur_doc_el_group;
    cur_doc_el_group = NULL;

    scan_start = scan_stop + 1;
    return;
}


/***************************************************************************/
/*  :eADDRESS                                                              */
/***************************************************************************/

void gml_eaddress( const gmltag * entry )
{
    doc_element *   cur_el;
    tag_cb      *   wk;

    if( cur_group_type != gt_address ) {   // no preceding :ADDRESS tag
        g_err_tag_prec( "ADDRESS" );
        scan_start = scan_stop + 1;
        return;
    }
    g_curr_font = font_save;
    nest_cb->prev->left_indent = nest_cb->left_indent;
    nest_cb->prev->right_indent = nest_cb->right_indent;
    rs_loc = titlep_tag;
    wk = nest_cb;
    nest_cb = nest_cb->prev;
    add_tag_cb_to_pool( wk );

    /* Place the accumulated ALINES on the proper page */

    cur_group_type = sav_group_type;
    if( t_doc_el_group != NULL ) {
        cur_doc_el_group = t_doc_el_group;      // detach current element group
        t_doc_el_group = t_doc_el_group->next;  // processed doc_elements go to next group, if any
        cur_doc_el_group->next = NULL;

        if( cur_doc_el_group->first != NULL ) {
            cur_doc_el_group->depth += (cur_doc_el_group->first->blank_lines +
                                cur_doc_el_group->first->subs_skip);
        }

        if( (cur_doc_el_group->depth + t_page.cur_depth) > t_page.max_depth ) {

            /*  the block won't fit on this column */

            if( cur_doc_el_group->depth <= t_page.max_depth ) {

                /*  the block will on the next column */

                next_column();
            }
        }

        while( cur_doc_el_group->first != NULL ) {
            cur_el = cur_doc_el_group->first;
            cur_doc_el_group->first = cur_doc_el_group->first->next;
            cur_el->next = NULL;
            insert_col_main( cur_el );
        }

        add_doc_el_group_to_pool( cur_doc_el_group );
        cur_doc_el_group = NULL;
    }
    scan_start = scan_stop + 1;
    return;
}


/***************************************************************************/
/*  prepare address line for output                                        */
/***************************************************************************/

static void prep_aline( text_line *p_line, const char *p )
{
    text_chars  *   curr_t;
    uint32_t        h_left;
    uint32_t        h_right;

    h_left = nest_cb->left_indent + conv_hor_unit( &layout_work.address.left_adjust, g_curr_font );
    h_right = t_page.max_width + nest_cb->right_indent - conv_hor_unit( &layout_work.address.right_adjust, g_curr_font );

    curr_t = alloc_text_chars( p, strlen( p ), g_curr_font );
    curr_t->count = len_to_trail_space( curr_t->text, curr_t->count );
    curr_t->count = intrans( curr_t->text, curr_t->count, g_curr_font );
    curr_t->width = cop_text_width( curr_t->text, curr_t->count, g_curr_font );
    while( curr_t->width > (h_right - h_left) ) {   // too long for line
        if( curr_t->count < 2) {        // sanity check
            break;
        }
        curr_t->count -= 1;             // truncate text
        curr_t->width = cop_text_width( curr_t->text, curr_t->count, g_curr_font );
    }
    p_line->first = curr_t;
    curr_t->x_address = h_left;
    if( layout_work.address.page_position == pos_center ) {
        if( h_left + curr_t->width < h_right ) {
            curr_t->x_address = h_left + (h_right - h_left - curr_t->width) / 2;
        }
    } else if( layout_work.address.page_position == pos_right ) {
        curr_t->x_address = h_right - curr_t->width;
    }

    return;
}


/***************************************************************************/
/*  :ALINE tag                                                             */
/***************************************************************************/

void gml_aline( const gmltag * entry )
{
    char        *   p;
    doc_element *   cur_el;
    text_line   *   ad_line;

    if( !((ProcFlags.doc_sect == doc_sect_titlep) ||
          (ProcFlags.doc_sect_nxt == doc_sect_titlep)) ) {
        g_err( err_tag_wrong_sect, entry->tagname, ":TITLEP section" );
        err_count++;
        show_include_stack();
    }
    if( cur_group_type != gt_address ) {   // no preceding :ADDRESS tag
        g_err_tag_prec( "ADDRESS" );
    }
    p = scan_start;
    if( *p == '.' ) p++;                // over '.'

    start_doc_sect();               // if not already done
    a_spacing = layout_work.titlep.spacing;
    g_curr_font = layout_work.address.font;
    if( !first_aline ) {

        /************************************************************/
        /*  skip is treated as pre_top_skip because                 */
        /*  it is always used at the top of the page                */
        /*  this is not what the docs say                           */
        /************************************************************/

        set_skip_vars( NULL, &layout_work.aline.skip, NULL, a_spacing, g_curr_font );
    }

    ad_line = alloc_text_line();
    ad_line->line_height = wgml_fonts[g_curr_font].line_height;

    if( *p ) {
        prep_aline( ad_line, p );
    }

    cur_el = init_doc_el( el_text, ad_line->line_height );
    cur_el->element.text.first = ad_line;
    ad_line = NULL;
    insert_col_main( cur_el );

    first_aline = false;
    scan_start = scan_stop + 1;
}

