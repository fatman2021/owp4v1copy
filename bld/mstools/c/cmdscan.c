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
* Description:  Command line parsing routines.
*
****************************************************************************/


#include <ctype.h>
#include <stddef.h>
#include "context.h"
#include "memory.h"
#include "pathconv.h"


static char *   got_char( char *buf, size_t *bufsize, size_t offset, char ch );

int Quoted = 0;

/*
 * Skip all whitespace characters, such that the next read will retrieve the
 * first non-whitespace character.
 */
void CmdScanWhitespace( void )
/****************************/
{
    int                 ch;

    do {
        ch = GetCharContext();
    } while( isspace( ch )  &&  ch != '\0' );
    if( ch != '\0' )  UngetCharContext();
}


/*
 * If the next character is ch (in either uppercase or lowercase form), it
 * is consumed and a non-zero value is returned; otherwise, it is not
 * consumed and zero is returned.
 */
int CmdScanRecogChar( int ch )
/****************************/
{
    if( tolower( GetCharContext() )  ==  tolower( ch ) ) {
        return( 1 );
    } else {
        UngetCharContext();
        return( 0 );
    }
}


/*
 * If the next character is ch, it is consumed and a non-zero value is
 * returned; otherwise, it is not consumed and zero is returned.
 */
int CmdScanRecogCharExact( int ch )
/*********************************/
{
    if( GetCharContext() == ch ) {
        return( 1 );
    } else {
        UngetCharContext();
        return( 0 );
    }
}


/*
 * Scan a string.  No leading whitespace is allowed.  Returns a pointer
 * to newly allocated memory containing the string.  If leading whitespace
 * is found, returns NULL.  Quotes embedded within a string must be escaped
 * by a preceding backslash; two consecutive backslashes are reduced to one.
 */
char *CmdScanString( void )
/*************************/
{
    const char          quotechar = '"';
    int                 ch;
    int                 inQuote = Quoted;   /* true if inside a quoted string */
    int                 backslash = 0;      /* true if last char was a '\\' */
    int                 start;              /* context offset of string start */
    char *              buf = DupStrMem( "" );
    size_t              bufsize = 0;
    size_t              offset = 0;

    /*** Return NULL if there's leading whitespace or no more data ***/
    ch = GetCharContext();
    if( !Quoted && isspace( ch ) ) {
        UngetCharContext();
        return( NULL );
    } else if( ch != '\0' ) {
        UngetCharContext();
    } else {
        return( NULL );
    }

    /*** Count the number of characters in the string ***/
    start = GetPosContext();
    for( ;; ) {
        ch = GetCharContext();
        if( ch == 0 )  break;
        if( !inQuote && isspace( ch ) )  break;
        if( ch == quotechar ) {
            if( backslash ) {
                backslash = 0;          /* handle \" within a string */
            } else if( inQuote ) {
                if( Quoted ) {
                    Quoted = 0;
                    return( buf );
                }
                inQuote = 0;            /* end of a quoted portion */
            } else {
                inQuote = 1;            /* start of a quoted portion */
            }
            buf = got_char( buf, &bufsize, offset, ch );
            offset++;
        } else if( ch == '\\' ) {
            if( backslash ) {
                buf = got_char( buf, &bufsize, offset, ch );
                offset++;
                backslash = 0;      /* second '\\' of a pair */
                if( GetCharContext() == quotechar )
                    buf = got_char( buf, &bufsize, offset++, '\\' );
                UngetCharContext();
            } else {
                backslash = 1;      /* first '\\' of a pair */
            }
        } else {
            if( backslash ) {
                buf = got_char( buf, &bufsize, offset, '\\' );
                offset++;
                backslash = 0;
            }
            buf = got_char( buf, &bufsize, offset, ch );
            offset++;
        }
    }
    if( backslash ) {                   /* store any leftover backslash */
        buf = got_char( buf, &bufsize, offset, '\\' );
        offset++;
    }

    if( ch != 0 )  UngetCharContext();
    return( buf );
}


/*
 * Scan a filename.  No leading whitespace is allowed.  Returns a pointer
 * to newly allocated memory containing the filename string.  If filename
 * contained a quote character, returned string contains quotes. If leading
 * whitespace is found, returns NULL.
 */
char *CmdScanFileName( void )
/***************************/
{
    char *              str;
    char *              newstr;

    str = CmdScanString();
    if( str != NULL ) {
        newstr = PathConvert( str, '"' );
        FreeMem( str );
    } else {
        newstr = NULL;
    }
    return( newstr );
}

/*
 * Scan a filename without quotes.  No leading whitespace is allowed.  Returns a pointer
 * to newly allocated memory containing the filename string.  If leading
 * whitespace is found, returns NULL.
 */
char *CmdScanFileNameWithoutQuotes( void )
/***************************/
{
    char *              str;
    char *              newstr;

    str = CmdScanString();
    if( str != NULL ) {
        newstr = PathConvertWithoutQuotes( str );
        FreeMem( str );
    } else {
        newstr = NULL;
    }
    return( newstr );
}


/*
 * Scan a number.  No leading whitespace is allowed.  Returns true if a
 * number was successfully parsed, or zero on error.
 */
int CmdScanNumber( unsigned *num )
/********************************/
{
    int                 digit;
    int                 numberScanned = 0;
    unsigned            value = 0;
    unsigned            base = 10;

    /* figure out if this is a hex number */
    digit = GetCharContext();
    if( digit == '0' ) {
        digit = GetCharContext();
        if( digit == 'x' || digit == 'X' ) {
            digit = GetCharContext();
            if( isxdigit( digit ) ) {
                base = 16;
                UngetCharContext();
            } else {
                UngetCharContext();
                UngetCharContext();
                UngetCharContext();
            }
        } else {
            UngetCharContext();
            UngetCharContext();
        }
    } else {
        UngetCharContext();
    }

    /* convert the string to an integer */
    for( ;; ) {
        digit = GetCharContext();
        if( isdigit( digit ) ) {
            value *= base;
            value += digit - '0';
            numberScanned = 1;
        } else if( base == 16 && isxdigit( digit ) ) {
            value *= base;
            value += tolower( digit ) - 'a' + 10;
            numberScanned = 1;
        } else {
            UngetCharContext();
            break;
        }
    }

    if( numberScanned )  *num = value;
    return( numberScanned );
}


/*
 * Append a character to a dynamically allocated string, increasing the
 * buffer size if necessary.  Returns a pointer to a buffer containing the
 * new data.
 */
static char *got_char( char *buf, size_t *bufsize, size_t offset, char ch )
/*************************************************************************/
{
    const size_t        blocksize = 64;

    /*** Increase the buffer size if necessary ***/
    while( offset+1 >= *bufsize ) {
        *bufsize += blocksize;
        buf = ReallocMem( buf, (*bufsize)+blocksize);
    }

    /*** Append the character ***/
    buf[offset] = ch;
    buf[offset+1] = '\0';
    return( buf );
}
