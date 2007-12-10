/* dh: set default handlers for document types based on a settings file. */

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "dh.h"

extern char		*dh_version;

/*
 * parseline is a simplified argcargv. nothing clever here.
 */
    int
parseline( char *line, char ***lineav )
{
    static char		**pav = NULL;
    int			i; 

    if ( pav == NULL ) {
	if (( pav = ( char ** )malloc( 3 * sizeof( char * ))) == NULL ) {
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
    if ( i != 3 ) {
	return( -1 );
    }

    *lineav = pav;

    return( 0 );
}

    int
sethandler( char *handler, char *doctype, LSRolesMask mask )
{
    CFStringRef		bid = NULL;
    CFStringRef		type = NULL;
    OSStatus		rc;

    /* must have a CF representation of the role handler and doctype */
    if (( bid = CFStringCreateWithBytes( kCFAllocatorDefault,
		( UInt8 * )handler, ( CFIndex )strlen( handler ),
		kCFStringEncodingUTF8, false )) == NULL ) {
	fprintf( stderr, "CFStringCreateWithBytes %s failed\n", handler );
	rc = -1;
	goto cleanup;
    }
    if (( type = CFStringCreateWithBytes( kCFAllocatorDefault,
		( UInt8 * )doctype, ( CFIndex )strlen( doctype ),
		kCFStringEncodingUTF8, false )) == NULL ) {
	fprintf( stderr, "CFStringCreateWithBytes %s failed\n", doctype );
	rc = -1;
	goto cleanup;
    }

    rc = LSSetDefaultRoleHandlerForContentType( type, mask, bid );
    if ( rc != noErr ) {
	fprintf( stderr, "Failed to set \"%s\" as default handler "
			 "for \"%s\" (error %d)\n",
			 handler, doctype, ( int )rc );
    }

cleanup:
    if ( bid != NULL ) {
	CFRelease( bid );
    }
    if ( type != NULL ) {
	CFRelease( type );
    }

    return(( int )rc );
}

    int
main( int ac, char *av[] )
{
    FILE	*f = NULL;
    char	line[ MAXPATHLEN * 2 ];
    char	*handler, *doctype, *role;
    char	**lineav = NULL;
    int		len, i, c;
    int		linenum = 0;
    int		err = 0;
    int		verbose = 0;
    int		nroles = sizeof( rtm ) / sizeof( rtm[ 0 ] );

    extern int	optind;

    while (( c = getopt( ac, av, "hVv" )) != -1 ) {
	switch ( c ) {
	case 'h':	/* help */
	default:
	    err++;
	    break;

	case 'V':	/* version */
	    printf( "%s\n", dh_version );
	    exit( 0 );

	case 'v':	/* verbose */
	    verbose = 1;
	    break;
	}
    }

    if ( ac - optind == 0 ) {
	f = stdin;
    } else if ( ac - optind == 1 ) {
	if (( f = fopen( av[ optind ], "r" )) == NULL ) {
	    perror( av[ optind ] );
	    exit( 2 );
	}
    } else {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "usage: %s [ -v ] [ settings_file ]\n", av[ 0 ] );
	exit( 1 );
    }

    err = 0;

    while ( fgets( line, MAXPATHLEN * 2, f ) != NULL ) {
	linenum++;

	len = strlen( line );
	if ( line[ len - 1 ] != '\n' ) {
	    fprintf( stderr, "line %d: line too long\n", linenum );
	    exit( 2 );
	}
	line[ len - 1 ] = '\0';

	/* skip blanks and comments */
	if ( *line == '\0' || *line == '#' ) {
	    continue;
	}
	
	if ( parseline( line, &lineav ) != 0 ) {
	    fprintf( stderr, "line %d: parsing failed\n", linenum );
	    err = 1;
	    continue;
	}

	handler = lineav[ 0 ];
	doctype = lineav[ 1 ];
	role = lineav[ 2 ];

	for ( i = 0; i < nroles; i++ ) {
	    if ( strcasecmp( role, rtm[ i ].r_role ) == 0 ) {
		break;
	    }
	}
	if ( i >= nroles ) {
	    fprintf( stderr, "line %d: role \"%s\" unrecognized\n",
			linenum, role );
	    err = 1;
	    continue;
	}

	if ( verbose ) {
	    printf( "Setting %s as default handler for %s with role %s\n",
			handler, doctype, role );
	}
	if ( sethandler( handler, doctype, rtm[ i ].r_mask ) != 0 ) {
	    err = 1;
	}
    }
    if ( ferror( f )) {
	perror( "fgets" );
	exit( 2 );
    }

    if ( fclose( f ) != 0 ) {
	perror( "fclose" );
	exit( 2 );
    }

    return( err );
}
