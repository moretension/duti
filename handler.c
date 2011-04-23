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

    if ( !UTTypeConformsTo( type, kUTTypeItem ) &&
		!UTTypeConformsTo( type, kUTTypeContent )) {
	fprintf( stderr, "%s does not conform to any UTI hierarchy\n", ct );
	return( 1 );
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
    int		linenum = 0, rc = 0;
    int		len, htype;

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

	if ( htype == DUTI_TYPE_UTI_HANDLER ) {
	    duti_handler_set( handler, type, role );
	} else if ( htype == DUTI_TYPE_URL_HANDLER ) {
	    duti_handler_set( handler, type, NULL );
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

    int			rc = 0;
    int			htype = DUTI_TYPE_UTI_HANDLER;
    char		handler[ MAXPATHLEN ], type[ MAXPATHLEN ];
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
	    if ( cf2c( bid, handler, sizeof( handler )) != 0 ) {
		rc = 1;
		continue;
	    }
	    if ( cf2c( uti, type, sizeof( type )) != 0 ) {
		rc = 1;
		continue;
	    }
	    if ( cf2c( role, crole, sizeof( crole )) != 0 ) {
		rc = 1;
		continue;
	    }
	    if ( duti_handler_set( handler, type, crole ) != 0 ) {
		rc = 1;
	    }
	} else if ( htype == DUTI_TYPE_URL_HANDLER ) {
	    if ( cf2c( bid, handler, sizeof( handler )) != 0 ) {
		rc = 1;
		continue;
	    }
	    if ( cf2c( url_scheme, type, sizeof( type )) != 0 ) {
		rc = 1;
		continue;
	    }
	    if ( duti_handler_set( handler, type, NULL ) != 0 ) {
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
	    if (( cfhandlers = LSCopyAllHandlersForURLScheme(
					cfuti )) == NULL ) {
		fprintf( stderr, "%s: no handlers\n", uti );
		rc = 1;
		goto uti_show_done;
	    }
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
	    if (( cfhandler = LSCopyDefaultHandlerForURLScheme(
					cfuti )) == NULL ) {
		fprintf( stderr, "%s: no default handler\n", uti );
		rc = 1;
		goto uti_show_done;
	    }
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

    int
duti_handler_set( char *bid, char *type, char *role )
{
    CFStringRef		cf_bid = NULL;
    CFStringRef		cf_type = NULL;
    CFStringRef		tagClass = NULL, preferredUTI = NULL;
    int			rc = 0;
    int			i = 0;

    if ( role != NULL ) {
	for ( i = 0; i < nroles; i++ ) {
	    if ( strcasecmp( role, rtm[ i ].r_role ) == 0 ) {
		break;
	    }
	}
	if ( i >= nroles ) {
	    fprintf( stderr, "role \"%s\" unrecognized\n", role );
	    return( 2 );
	}
    }

    if ( role != NULL ) {
	if ( *type == '.' ) {
	    type++;
	    if ( *type == '\0' ) {
		fprintf( stderr, "duti_handler_set: invalid empty type" );
		rc = 2;
		goto duti_set_cleanup;
	    }
	    tagClass = kUTTagClassFilenameExtension;
	} else {
	    if ( strchr( type, '/' ) != NULL ) {
		tagClass = kUTTagClassMIMEType;
	    } else if ( strchr( type, '.' ) == NULL ) {
		tagClass = kUTTagClassFilenameExtension;
	    }
	}
	if ( tagClass != NULL ) {
	    /*
	     * if no UTI defined for the extension, the system creates a
	     * dynamic local UTI with a "dyn." prefix & an encoded value.
	     */
	    if ( c2cf( type, &cf_type ) != 0 ) {
		rc = 2;
		goto duti_set_cleanup;
	    }

	    preferredUTI = UTTypeCreatePreferredIdentifierForTag(
				tagClass, cf_type, kUTTypeContent );
	    CFRelease( cf_type );
	    cf_type = NULL;

	    if ( preferredUTI == NULL ) {
		fprintf( stderr, "failed to create preferred "
				 "identifier for type %s", type );
		rc = 2;
		goto duti_set_cleanup;
	    }
	}
    }
    if ( c2cf( bid, &cf_bid ) != 0 ) {
	rc = 2;
	goto duti_set_cleanup;
    }
    if ( preferredUTI != NULL ) {
	if (( cf_type = CFStringCreateCopy( kCFAllocatorDefault,
			    preferredUTI )) == NULL ) {
	    fprintf( stderr, "failed to copy preferred UTI" );
	    rc = 2;
	    goto duti_set_cleanup;
	}
    } else {
	if ( c2cf( type, &cf_type ) != 0 ) {
	    rc = 1;
	    goto duti_set_cleanup;
	}
    }

    if ( role != NULL ) {
	/* set UTI handler */
	if (( rc = set_uti_handler( cf_bid, cf_type,
					rtm[ i ].r_mask )) != 0 ) {
	    rc = 2;
	}
    } else {
	/* set URL scheme handler */
	if (( rc = set_url_handler( cf_bid, cf_type )) != 0 ) {
	    rc = 2;
	}
    }

duti_set_cleanup:
    if ( cf_bid != NULL ) {
	CFRelease( cf_bid );
    }
    if ( cf_type != NULL ) {
	CFRelease( cf_type );
    }
    if ( preferredUTI != NULL ) {
	CFRelease( preferredUTI );
    }

    return( rc );
}

/*
 * print default app info for a given extension. based on
 * public domain source posted on the heliumfoot.com blog
 * by Keith Alperin.
 *
 * http://www.heliumfoot.com/blog/77
 */
    int
duti_default_app_for_extension( char *ext )
{
    CFDictionaryRef	cf_info_dict = NULL;
    CFStringRef		cf_ext = NULL;
    CFStringRef		cf_app_bundle_id = NULL;
    CFStringRef		cf_app_name = NULL;
    CFURLRef		cf_app_url = NULL;
    OSStatus		err;
    char		*rext;
    char		tmp[ MAXPATHLEN ];
    int			rc = 2;

    if (( rext = strrchr( ext, '.' )) == NULL ) {
	rext = ext;
    } else {
	rext++;
	if ( *rext == '\0' ) {
	    fprintf( stderr, "no extension provided\n" );
	    return( rc );
	}
    }
    if ( c2cf( rext, &cf_ext ) != 0 ) {
	return( rc );
    }

    err = LSGetApplicationForInfo( kLSUnknownType, kLSUnknownCreator, cf_ext,
					kLSRolesAll, NULL, &cf_app_url );
    if ( err != noErr ) {
	fprintf( stderr, "Failed to get default application for "
			 "extension \'%s\'\n", rext );
	goto duti_extension_cleanup;
    }

    err = LSCopyDisplayNameForURL( cf_app_url, &cf_app_name );
    if ( err != noErr ) {
	fprintf( stderr, "Failed to get display name\n" );
	goto duti_extension_cleanup;
    }
    if ( cf2c( cf_app_name, tmp, sizeof( tmp )) != 0 ) {
	goto duti_extension_cleanup;
    }
    printf( "%s\n", tmp );

    if ( cfurl2path( cf_app_url, tmp, sizeof( tmp )) != 0 ) {
	goto duti_extension_cleanup;
    }
    printf( "%s\n", tmp );

    cf_info_dict = CFBundleCopyInfoDictionaryInDirectory( cf_app_url );
    if ( cf_info_dict == NULL ) {
	fprintf( stderr, "Failed to copy info dictionary from %s\n", tmp );
	goto duti_extension_cleanup;
    }

    cf_app_bundle_id = CFDictionaryGetValue( cf_info_dict,
					     kCFBundleIdentifierKey );
    if ( cf_app_bundle_id == NULL ) {
	fprintf( stderr, "Failed to get bundle identifier for %s\n", tmp );
	goto duti_extension_cleanup;
    }

    if ( cf2c( cf_app_bundle_id, tmp, sizeof( tmp )) != 0 ) { 
	goto duti_extension_cleanup;
    }
    printf( "%s\n", tmp );

    /* success */
    rc = 0;

duti_extension_cleanup:
    if ( cf_ext != NULL ) {
	CFRelease( cf_ext );
    }
    if ( cf_app_url != NULL ) {
	CFRelease( cf_app_url );
    }
    if ( cf_info_dict != NULL ) {
	CFRelease( cf_info_dict );
    }
    if ( cf_app_name != NULL ) {
	CFRelease( cf_app_name );
    }

    return( rc );
}
