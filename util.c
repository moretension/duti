/* -*- mode:c; tab-width: 8 -*- */

#include <CoreFoundation/CoreFoundation.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

/*
 * parseline: a simple argument parser.
 *
 * parameters:
 *	line: buffer containing string to parse. Modified.
 *	lineav: argv resulting from parsing.
 *
 * return value:
 *	 0: success, line broken into vector of 3 arguments
 *	-1: failed
 *
 */
    int
parseline( char *line, char ***lineav )
{
    static char		**pav = NULL;
    int			i; 

    if ( pav == NULL ) {
	if (( pav = ( char ** )malloc( 5 * sizeof( char * ))) == NULL ) {
	    perror( "malloc" );
	    exit( 2 );
	}
    }

    pav[ 0 ] = line;

    for ( i = 1; i < 3; i++ ) {
	while ( isspace( *line )) line++;
	while ( *line != '\0' && !isspace( *line )) line++;
	*line++ = '\0';
	while ( isspace( *line )) line++;

	pav[ i ] = line;
	if ( *pav[ i ] == '\0' ) {
	    break;
	}
    }

    *lineav = pav;

    return( i );
}

/*
 * c2cf: convert a C string to CFStringRef
 *
 * parameters:
 *	cstr: the C string to convert
 *	cfstr: will point to result of conversion. must be CFRelease'd.
 *
 * return value:
 *	 0: success
 *	-1: failure
 */
    int
c2cf( char *cstr, CFStringRef *cfstr )
{
    CFStringRef		cftmp;

    if ( cstr == NULL ) {
	fprintf( stderr, "%s: null C String\n", __FUNCTION__ );
	return( -1 );
    }

    if (( cftmp = CFStringCreateWithBytes( kCFAllocatorDefault,
			( UInt8 * )cstr, ( CFIndex )strlen( cstr ),
			kCFStringEncodingUTF8, false )) == NULL ) {
	fprintf( stderr, "Failed to convert C string to CFStringRef\n" );
	return( -1 );
    }

    *cfstr = cftmp;

    return( 0 );
}

/*
 * cf2c: convert a CFStringRef to a C string
 *
 * parameters:
 *	cfstr: CFStringRef to convert to C string
 *	cstr: char buffer that will contain result of conversion
 *	len: size of cstr buffer
 *
 * return value:
 *	-1: conversion failed
 *	 0: success
 */
    int
cf2c( CFStringRef cfstr, char *cstr, int len )
{
    if ( cfstr == NULL ) {
	fprintf( stderr, "%s: null CFStringRef\n", __FUNCTION__ );
	return( -1 );
    }

    if ( CFStringGetCString( cfstr, cstr, ( CFIndex )len,
		kCFStringEncodingUTF8 ) == false ) {
	fprintf( stderr, "Failed to convert CFStringRef to C String\n" );
	return( -1 );
    }

    return( 0 );
}

/*
 * cfurl2path: convert a CFURLRef to a POSIX C string path
 *
 * parameters:
 *	cfurl: CFURLRef to convert to C string path
 *	cstr: char buffer that will contain result of conversion
 *	len: size of cstr buffer
 *
 * return value:
 *	-1: conversion failed
 *	 0: success
 */
    int
cfurl2path( CFURLRef cfurl, char *cstr, int len )
{
    if ( cfurl == NULL ) {
	fprintf( stderr, "Cannot convert a null CFURLRef\n" );
	return( -1 );
    }

    if ( !CFURLGetFileSystemRepresentation( cfurl, false, (UInt8 *)cstr, len)) {
	fprintf( stderr, "Failed to convert CFURLRef to C path\n" );
	return( -1 );
    }

    return( 0 );
}

/*
 * lladd: insert a path into a sorted linked list.
 *
 * parameters:
 *	path: char buffer containing path to insert
 *	head: pointer to pointer at head of linked list. May be modified.
 */
    void
lladd( char *path, struct ll **head )
{
    struct ll		*new;
    struct ll		**cur;

    if (( new = ( struct ll * )malloc( sizeof( struct ll ))) == NULL ) {
	perror( "malloc" );
	exit( 2 );
    }
    if (( new->l_path = strdup( path )) == NULL ) {
	perror( "strdup" );
	exit( 2 );
    }

    for ( cur = head; *cur != NULL; cur = &( *cur )->l_next ) {
	if ( strcmp(( *cur )->l_path, new->l_path ) > 0 ) {
	    break;
	}
    }

    new->l_next = *cur;
    *cur = new;
}
