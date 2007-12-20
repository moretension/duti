/* dh: set default handlers for document types based on a settings file. */

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/param.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "dh.h"
#include "plist.h"

extern char		*dh_version;
int			verbose = 0;
int			nroles;


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
sethandler( CFStringRef bid, CFStringRef type, LSRolesMask mask )
{
    OSStatus		rc;

    if ( verbose ) {
	fputs( "setting ", stderr );
	CFShow( bid );
	fputs( "\tas handler for ", stderr );
	CFShow( type );
    }

    rc = LSSetDefaultRoleHandlerForContentType( type, mask, bid );
    if ( rc != noErr ) {
	fprintf( stderr, "Failed to set \"" );
	CFShow( bid );
	fprintf( stderr, "\" as default handler for \"" );
	CFShow( type );
	fprintf( stderr, "\" (error %d)\n", ( int )rc );
    }


    return(( int )rc );
}

    int
fsethandler( char *spath )
{
    FILE	*f = NULL;
    char	line[ MAXPATHLEN * 2 ];
    char	*handler, *doctype, *role;
    char	**lineav = NULL;
    int		i, len, linenum = 0, rc = 0;

    CFStringRef	bid = NULL, type = NULL;

    if ( spath != NULL ) {
	if (( f = fopen( spath, "r" )) == NULL ) {
	    fprintf( stderr, "fopen %s: %s\n", spath, strerror( errno ));
	    return( 1 );
	}
    } else {
	f = stdin;
    }

    while ( fgets( line, sizeof( line ), f ) != NULL ) {
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
	    rc = 1;
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
	    rc = 1;
	    continue;
	}

	/* must have a CF representation of the role handler and doctype */
	if (( bid = CFStringCreateWithBytes( kCFAllocatorDefault,
		    ( UInt8 * )handler, ( CFIndex )strlen( handler ),
		    kCFStringEncodingUTF8, false )) == NULL ) {
	    fprintf( stderr, "CFStringCreateWithBytes %s failed\n", handler );
	    rc = 1;
	    goto cleanup;
	}
	if (( type = CFStringCreateWithBytes( kCFAllocatorDefault,
		    ( UInt8 * )doctype, ( CFIndex )strlen( doctype ),
		    kCFStringEncodingUTF8, false )) == NULL ) {
	    fprintf( stderr, "CFStringCreateWithBytes %s failed\n", doctype );
	    rc = 1;
	    goto cleanup;
	}
	if ( sethandler( bid, type, rtm[ i ].r_mask ) != 0 ) {
	    rc = 1;
	}

cleanup:
	if ( bid != NULL ) {
	    CFRelease( bid );
	}
	if ( type != NULL ) {
	    CFRelease( type );
	}
    }
    if ( ferror( f )) {
	perror( "fgets" );
	rc = 1;
    }
    if ( fclose( f ) != 0 ) {
	perror( "fclose" );
	rc = 1;
    }

    return( rc );
}

    int
psethandler( char *spath )
{
    CFDictionaryRef	plist;
    CFArrayRef		dharray;
    CFDictionaryRef	dhentry;
    CFStringRef		bid, uti, role;
    CFIndex		count, index;

    int			i, rc = 0;
    char		crole[ 255 ];

    if ( !spath ) {
	fprintf( stderr, "%s: invalied argument supplied\n", __FUNCTION__ );
	return( 1 );
    }

    if ( dh_read_plist( spath, &plist ) != 0 ) {
	fprintf( stderr, "%s: failed to read plist\n", __FUNCTION__ );
	return( 1 );
    }

    if ( !plist ) {
	fprintf( stderr, "%s: Invalid plist\n", __FUNCTION__ );
	return( 1 );
    }

    if (( dharray = CFDictionaryGetValue( plist, DH_KEY_SETTINGS )) == NULL ) {
	fprintf( stderr, "plist is missing the settings array\n" );
	CFRelease( plist );
	return( 1 );
    }

    count = CFArrayGetCount( dharray );
    for ( index = 0; index < count; index++ ) {
	dhentry = CFArrayGetValueAtIndex( dharray, index );

	if (( bid = CFDictionaryGetValue( dhentry,
			DH_KEY_BUNDLEID )) == NULL ) {
	    fprintf( stderr, "Entry %d missing bundle ID\n", ( int )index );
	    rc = 1;
	    continue;
	}
	if (( uti = CFDictionaryGetValue( dhentry, DH_KEY_UTI )) == NULL ) {
	    fprintf( stderr, "Entry %d missing UTI\n", ( int )index );
	    rc = 1;
	    continue;
	}
	if (( role = CFDictionaryGetValue( dhentry, DH_KEY_ROLE )) == NULL ) {
	    fprintf( stderr, "Entry %d missing role\n", ( int )index );
	    rc = 1;
	    continue;
	}

	/* get C string form of role to look up LSRoleMask */
	if ( !CFStringGetCString( role, crole,
		( CFIndex )sizeof( crole ), kCFStringEncodingUTF8 )) {
	    fprintf( stderr, "Failed to create C string from role " );
	    CFShow( role );
	    rc = 1;
	    continue;
	}
	for ( i = 0; i < nroles; i++ ) {
            if ( strcasecmp( crole, rtm[ i ].r_role ) == 0 ) {
                break;
            }
        }
        if ( i >= nroles ) {
            fprintf( stderr, "entry %d: role \"%s\" unrecognized\n",
                        ( int )index, crole );
            rc = 1;
            continue;
        }	

	if ( sethandler( bid, uti, rtm[ i ].r_mask ) != 0 ) {
	    rc = 1;
	}
    }

    if ( plist != NULL ) {
	CFRelease( plist );
    }

    return( rc );
}

    int
dirsethandler( char *dirpath )
{
    DIR			*d = NULL;
    struct dirent	*de;
    char		path[ MAXPATHLEN ];
    char		*p;
    int			rc = 0;
    int			( *sh )( char * );

    if (( d = opendir( dirpath )) == NULL ) {
	fprintf( stderr, "opendir %s: %s\n", dirpath, strerror( errno ));
	exit( 2 );
    }

    while (( de = readdir( d )) != NULL ) {
	if ( de->d_name[ 0 ] == '.' ) {
	    continue;
	}

	if ( snprintf( path, MAXPATHLEN, "%s/%s",
		dirpath, de->d_name ) >= MAXPATHLEN ) {
	    fprintf( stderr, "%s/%s: path too long\n", dirpath, de->d_name );
	    continue;
	}

	sh = fsethandler;
	if (( p = strrchr( path, '.' )) != NULL ) {
	    p++;
	    if ( strcmp( p, "plist" ) == 0 ) {
		sh = psethandler;
	    }
	}

	if ( verbose ) {
	    printf( "Processing settings from %s\n", path );
	}
	if ( sh( path ) != 0 ) {
	    rc = 1;
	}
    }

    if ( closedir( d ) != 0 ) {
	fprintf( stderr, "closedir: %s\n", strerror( errno ));
	rc = 1;
    }

    return( rc );
}

    int
main( int ac, char *av[] )
{
    int			c, err = 0;
    int			( *hf )( char * );
    char		*path = NULL;
    extern int		optind;

    /* by default, read from a FILE stream */
    hf = fsethandler;

    while (( c = getopt( ac, av, "d:hVvp:" )) != -1 ) {
	switch ( c ) {
	case 'd':	/* process settings files from directory */
	    hf = dirsethandler;
	    path = optarg;
	    break;

	case 'h':	/* help */
	default:
	    err++;
	    break;

	case 'p':	/* read from plist */
	    hf = psethandler;
	    path = optarg;
	    break;

	case 'V':	/* version */
	    printf( "%s\n", dh_version );
	    exit( 0 );

	case 'v':	/* verbose */
	    verbose = 1;
	    break;
	}
    }

    if ( ac - optind == 1 ) {
	path = av[ optind ];
    } else if ( path == NULL && err ) {
	err++;
    }

    if ( err ) {
	fprintf( stderr, "usage: %s [ -hvV ] [ -d settings_directory ] "
		"[ -p settings_plist ] [ settings_file ]\n", av[ 0 ] );
	exit( 1 );
    }

    err = 0;
    nroles = sizeof( rtm ) / sizeof( rtm[ 0 ] );

    err = hf( path );

    return( err );
}
