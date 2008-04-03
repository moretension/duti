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
set_uti_handler( CFStringRef bid, CFStringRef type, LSRolesMask mask )
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
set_url_handler( CFStringRef bid, CFStringRef url_scheme )
{
    OSStatus		rc;
    char		cb[ MAXPATHLEN ];
    char		cu[ MAXPATHLEN ];

    if ( cf2c( bid, cb, sizeof( cb )) != 0 ) {
	strlcpy( cb, "bundle_id", sizeof( cb ));
    }
    if ( cf2c( url_scheme, cu, sizeof( cu )) != 0 ) {
	strlcpy( cu, "url_scheme", sizeof( cu ));
    }

    if ( verbose ) {
	printf( "setting %s as handler for %s:// URLs\n", cb, cu );
    }

    rc = LSSetDefaultHandlerForURLScheme( url_scheme, bid );
    if ( rc != noErr ) {
	fprintf( stderr, "failed to set %s as handler "
		 "for %s:// (error %d)\n", cb, cu, ( int )rc );
    }

    return(( int )rc );
}

    int
fsethandler( char *spath )
{
    FILE	*f = NULL;
    char	line[ MAXPATHLEN * 2 ];
    char	*handler, *type, *role;
    char	**lineav = NULL;
    int		i = 0, linenum = 0, rc = 0;
    int		len, htype;

    CFStringRef	bid = NULL, cftype = NULL;

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
	
	htype = parseline( line, &lineav );
	switch ( htype ) {
	case DUTI_TYPE_UTI_HANDLER:
	    handler = lineav[ 0 ];
	    type = lineav[ 1 ];
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
	    break;

	case DUTI_TYPE_URL_HANDLER:
	    handler = lineav[ 0 ];
	    type = lineav[ 1 ];
	    break;

	default:
	    fprintf( stderr, "line %d: parsing failed\n", linenum );
	    rc = 1;
	    continue;
	}

	/* must have a CF representation of the role handler and type */
	if ( c2cf( handler, &bid ) != 0 ) {
	    rc = 1;
	    goto cleanup;
	}
	if ( c2cf( type, &cftype ) != 0 ) {
	    rc = 1;
	    goto cleanup;
	}

	if ( htype == DUTI_TYPE_UTI_HANDLER ) {
	    if ( set_uti_handler( bid, cftype, rtm[ i ].r_mask ) != 0 ) {
		rc = 1;
	    }
	} else if ( htype == DUTI_TYPE_URL_HANDLER ) {
	    if ( set_url_handler( bid, cftype ) != 0 ) {
		rc = 1;
	    }
	}

cleanup:
	if ( bid != NULL ) {
	    CFRelease( bid );
	}
	if ( cftype != NULL ) {
	    CFRelease( cftype );
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
    CFStringRef		bid, uti, role, url_scheme;
    CFIndex		count, index;

    int			i, rc = 0;
    int			htype = DUTI_TYPE_UTI_HANDLER;
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
	if (( url_scheme = CFDictionaryGetValue( dhentry,
				DUTI_KEY_URLSCHEME )) != NULL ) {
	    htype = DUTI_TYPE_URL_HANDLER;
	} else if (( uti = CFDictionaryGetValue( dhentry,
				DUTI_KEY_UTI )) != NULL ) {
	    if (( role = CFDictionaryGetValue( dhentry,
				DUTI_KEY_ROLE )) == NULL ) {
		fprintf( stderr, "Entry %d missing role\n", ( int )index );
		rc = 1;
		continue;
	    }
	    htype = DUTI_TYPE_UTI_HANDLER;
	} else {
	    fprintf( stderr, "Entry %d invalid\n", ( int )index );
	    rc = 1;
	    continue;
	}

	if ( htype == DUTI_TYPE_UTI_HANDLER ) {
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

	    if ( set_uti_handler( bid, uti, rtm[ i ].r_mask ) != 0 ) {
		rc = 1;
	    }
	} else if ( htype == DUTI_TYPE_URL_HANDLER ) {
	    if ( set_url_handler( bid, url_scheme ) != 0 ) {
		rc = 1;
	    }
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

    int
uti_handler_show( char *uti, int showall )
{
    CFArrayRef		cfhandlers = NULL;
    CFStringRef		cfuti = NULL;
    CFStringRef		cfhandler = NULL;
    char		dh[ MAXPATHLEN ];
    int			rc = 0;
    int			count, i;

    if ( uti == NULL ) {
	fprintf( stderr, "Invalid UTI.\n" );
	return( 2 );
    }

    if ( c2cf( uti, &cfuti ) != 0 ) {
	return( 2 );
    }

    if ( showall ) {
	if (( cfhandlers = LSCopyAllRoleHandlersForContentType(
				cfuti, kLSRolesAll )) == NULL ) {
	    fprintf( stderr, "%s: no handlers\n", uti );
	    rc = 1;
	    goto uti_show_done;
	}

	if ( verbose ) {
	    printf( "All handlers for %s:\n", uti );
	}

	count = CFArrayGetCount( cfhandlers );
	for ( i = 0; i < count; i++ ) {
	    cfhandler = CFArrayGetValueAtIndex( cfhandlers, i );
	    if ( cf2c( cfhandler, dh, sizeof( dh )) != 0 ) {
		rc = 2;
		continue;
	    }
	    printf( "%s\n", dh );
	    memset( dh, 0, sizeof( dh ));
	}
	
	cfhandler = NULL;
    } else {
	if (( cfhandler = LSCopyDefaultRoleHandlerForContentType(
				cfuti, kLSRolesAll )) == NULL ) {
	    fprintf( stderr, "%s: no default handler\n", uti );
	    rc = 1;
	    goto uti_show_done;
	}

	if ( cf2c( cfhandler, dh, MAXPATHLEN ) != 0 ) {
	    rc = 2;
	    goto uti_show_done;
	}

	if ( verbose ) {
	    printf( "Default handler for %s: ", uti );
	}
	printf( "%s\n", dh );
    }

uti_show_done:
    if ( cfhandlers ) {
	CFRelease( cfhandlers );
    }
    if ( cfuti ) {
	CFRelease( cfuti );
    }
    if ( cfhandler ) {
	CFRelease( cfhandler );
    }

    return( rc );
}
