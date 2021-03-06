/****************************************************************************
*
*                            Open Watcom Project
*
*    Portions Copyright (c) 1983-2002 Sybase, Inc. All Rights Reserved.
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
* Description:  Support for HTML syntax.
*
****************************************************************************/


#include "vi.h"
#include "sstyle.h"
#include "sstyle_h.h"


/*----- LOCALS -----*/

static  ss_flags_h  flags;
static  char        *firstNonWS;


void InitHTMLLine( char *text )
{
    while( *text && isspace( *text ) ) {
        text++;
    }
    firstNonWS = text;
}

static void getWhiteSpace( ss_block *ss_new, char *start )
{
    char    *text = start + 1;
    while( isspace( *text ) ) {
        text++;
    }
    ss_new->type = SE_WHITESPACE;
    ss_new->len = text - start;
}

static void getText( ss_block *ss_new, char *start )
{
    char    *text = start + 1;
    char    save_char;

    // gather up symbol
    while( isalnum( *text ) || (*text == '_') ) {
        text++;
    }
    ss_new->type = SE_IDENTIFIER;

    // see if symbol is a keyword
    if( flags.inHTMLKeyword ) {
        save_char = *text;
        *text = '\0';
        if( IsKeyword( start, TRUE ) ) {
            ss_new->type = SE_KEYWORD;
        }
        *text = save_char;
        // only check first word after a "<" (change this to check all tokens)
        flags.inHTMLKeyword = FALSE;
    }

    ss_new->len = text - start;
}

static void getSymbol( ss_block *ss_new )
{
    ss_new->type = SE_SYMBOL;
    ss_new->len = 1;
}

static void getBeyondText( ss_block *ss_new )
{
    ss_new->type = SE_WHITESPACE;
    ss_new->len = BEYOND_TEXT;
}

extern void getHTMLComment( ss_block *ss_new, char *start, int skip )
{
    char    *text = start + skip;
    char    comment1;
    char    comment2;
    char    comment3;

    for( ;; ) {
        // check for "-->"
        comment1 = text[0];
        if( comment1 == '\0' ) {
            break;
        }

        comment2 = '\0';
        comment3 = '\0';
        if( comment1 != '\0' ) {
            comment2 = text[1];
        }
        if( comment2 != '\0' ) {
            comment3 = text[2];
        }

        if( comment1 == '-' && comment2 == '-' && comment3 == '>' ) {
            flags.inHTMLComment = FALSE;
            text += 3;
            break;
        }
        text++;
    }
    ss_new->type = SE_COMMENT;
    ss_new->len = text - start;
}

static void getString( ss_block *ss_new, char *start, int skip )
{
    char    *nstart = start + skip;
    char    *text = nstart;

    ss_new->type = SE_STRING;
    while( *text && *text != '"' ) {
        text++;
    }
    if( *text == '\0' ) {
        // string continued on next line
        flags.inString = TRUE;
    } else {
        text++;
        // definitely finished string
        flags.inString = FALSE;
    }
    ss_new->len = text - start;
}

void InitHTMLFlagsGivenValues( ss_flags_h *newFlags )
{
    flags = *newFlags;
}

void GetHTMLFlags( ss_flags_h *storeFlags )
{
    *storeFlags = flags;
}

void InitHTMLFlags( linenum line_no )
{
    flags.inHTMLComment = FALSE;
    flags.inHTMLKeyword = FALSE;
    flags.inString = FALSE;
}


void GetHTMLBlock( ss_block *ss_new, char *start, int line )
{
    line = line;

    if( start[0] == '\0' ) {
        if( firstNonWS == start ) {
            // line is empty -
            // do not flag following line as having anything to do
            // with an unterminated "
            flags.inString = FALSE;
        }
        getBeyondText( ss_new );
        return;
    }

    if( flags.inHTMLComment ) {
        getHTMLComment( ss_new, start, 0 );
        return;
    }
    if( flags.inString ) {
        getString( ss_new, start, 0 );
        return;
    }

    if( isspace( start[0] ) ) {
        getWhiteSpace( ss_new, start );
        return;
    }

    switch( start[0] ) {
    case '"':
        getString( ss_new, start, 1 );
        return;
    case '<':
        if( start[1] == '!' && start[2] == '-' && start[3] == '-' ) {
            flags.inHTMLComment = TRUE;
            getHTMLComment( ss_new, start, 4 );
            return;
        } else {
            flags.inHTMLKeyword = TRUE;
        }
        break;
    case '>':
        flags.inHTMLKeyword = FALSE;
        break;
    }

    if( isalnum( start[0] ) || (start[0] == '_') ) {
        getText( ss_new, start );
    } else {
        getSymbol( ss_new );
    }

}
