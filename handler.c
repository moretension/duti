#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "handler.h"
#include "plist.h"
#include "util.h"

extern int		verbose;
extern struct roles	rtm[];
int			nroles;

    int
sethandler( CFStringRef bid, CFStringRef type, LSRolesMask mask )
{
    OSStatus		rc;
    char		cb[ MAXPATHLEN ];
    char		ct[ MAXPATHLEN ];

    /* get C strings to use in output. CFShow is inadequate. */
    if ( cf2c( bid, cb, sizeof( cb )) != 0 ) {
	/* something reasonable on error */
	strlcpy( cb, "bundle_id", sizeof( cb ));
    }
    if ( cf2c( type, ct, sizeof( ct )) != 0 ) {
	strlcpy( ct, "UTI", sizeof( ct ));
    }

    if ( verbose ) {
	printf( "setting %s as handler for %s\n", cb, ct );
    }

    rc = LSSetDefaultRoleHandlerForContentType( type, mask, bid );
    if ( rc != noErr ) {
	fprintf( stderr, "failed to set %s as handler "
		"for %s (error %d)\n", cb, ct, ( int )rc );
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
	if ( c2cf( handler, &bid ) != 0 ) {
	    rc = 1;
	    goto cleanup;
	}
	if ( c2cf( doctype, &type ) != 0 ) {
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
	fprintf( stderr, "%s: invalid argument supplied\n", __FUNCTION__ );
	return( 1 );
    }

    if ( read_plist( spath, &plist ) != 0 ) {
	fprintf( stderr, "%s: failed to read plist\n", __FUNCTION__ );
	return( 1 );
    }

    if ( !plist ) {
	fprintf( stderr, "%s: Invalid plist\n", __FUNCTION__ );
	return( 1 );
    }

    if (( dharray = CFDictionaryGetValue( plist,
			DUTI_KEY_SETTINGS )) == NULL ) {
	fprintf( stderr, "%s is missing the settings array\n", spath );
	CFRelease( plist );
	return( 1 );
    }

    count = CFArrayGetCount( dharray );
    for ( index = 0; index < count; index++ ) {
	dhentry = CFArrayGetValueAtIndex( dharray, index );

	if (( bid = CFDictionaryGetValue( dhentry,
			DUTI_KEY_BUNDLEID )) == NULL ) {
	    fprintf( stderr, "Entry %d missing bundle ID\n", ( int )index );
	    rc = 1;
	    continue;
	}
	if (( uti = CFDictionaryGetValue( dhentry, DUTI_KEY_UTI )) == NULL ) {
	    fprintf( stderr, "Entry %d missing UTI\n", ( int )index );
	    rc = 1;
	    continue;
	}
	if (( role = CFDictionaryGetValue( dhentry, DUTI_KEY_ROLE )) == NULL ) {
	    fprintf( stderr, "Entry %d missing role\n", ( int )index );
	    rc = 1;
	    continue;
	}

	/* get C string form of role to look up LSRoleMask */
	if ( cf2c( role, crole, sizeof( crole )) != 0 ) {
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
    struct ll		*head = NULL;
    struct ll		*cur, *tmp;
    struct stat		st;
    char		path[ MAXPATHLEN ];
    char		*p;
    int			rc = 0;
    int			( *dhandler_f )( char * );

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

	/* skip anything that isn't a file or does not resolve to a file. */
	if ( stat( path, &st ) != 0 ) {
	    fprintf( stderr, "stat %s: %s\n", path, strerror( errno ));
	    continue;
	}
	if ( !S_ISREG( st.st_mode )) {
	    continue;
	}
	
	/*
	 * build a sorted list of files in the directory
	 * so the admin can dictate in what order they'll
	 * be processed, allowing handler precedence.
	 *
	 * HFS+ appears to store files in lexical order
	 * on disk, but we can't count on an HFS+ volume.
	 */
	lladd( path, &head );
    }
    if ( closedir( d ) != 0 ) {
	fprintf( stderr, "closedir: %s\n", strerror( errno ));
	rc = 1;
    }

    for ( cur = head; cur != NULL; cur = tmp ) {
	dhandler_f = fsethandler;
	if (( p = strrchr( cur->l_path, '.' )) != NULL ) {
	    p++;
	    if ( strcmp( p, "plist" ) == 0 ) {
		dhandler_f = psethandler;
	    }
	}

	if ( verbose ) {
	    printf( "Applying settings from %s\n", cur->l_path );
	}
	if ( dhandler_f( cur->l_path ) != 0 ) {
	    rc = 1;
	}

	tmp = cur->l_next;
	free( cur->l_path );
	free( cur );
    }

    return( rc );
}
