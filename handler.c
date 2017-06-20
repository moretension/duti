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

static void dump_cf_array( const void *value, void *context );
static void dump_cf_dictionary( const void *key, const void *value, void *context );


    int
duti_is_conformant_uti( CFStringRef uti )
{
    /*
     * this is gross, but the kUTType CFStringRefs aren't constant, so
     * there's no convenient way to make a simple CFStringRef array (not
     * a CFArrayRef) with the base types we're checking. creating a
     * CFArrayRef with the kUTType CFStringRefs would be worthwhile if
     * we weren't running as a one-shot utility.
     */

    if ( UTTypeConformsTo( uti, kUTTypeItem ) ||
	 UTTypeConformsTo( uti, kUTTypeContent ) ||
	 UTTypeConformsTo( uti, kUTTypeMessage ) ||
	 UTTypeConformsTo( uti, kUTTypeContact ) ||
	 UTTypeConformsTo( uti, kUTTypeArchive )) {
        return( 1 );
    }

    return( 0 );
}

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

    
    if ( !duti_is_conformant_uti( type )) {
	fprintf( stderr, "%s does not conform to any UTI hierarchy\n", ct );
	return( 1 );
    }
    
    /* don't set handler if it's already set */
    CFStringRef cur_bid = LSCopyDefaultRoleHandlerForContentType(type, mask);
    if (cur_bid) {
	if (CFStringCompare(bid, cur_bid, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
	    CFRelease(cur_bid);
	    return noErr;
	}
	CFRelease(cur_bid);
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
    
    /* don't set handler if it's already set */
    CFStringRef cur_bid = LSCopyDefaultHandlerForURLScheme(url_scheme);
    if (cur_bid) {
	if (CFStringCompare(bid, cur_bid, kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
	    CFRelease(cur_bid);
	    return noErr;
	}
	CFRelease(cur_bid);
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
    CFArrayRef		cfschemes = NULL;
    CFStringRef		cfuti = NULL;
    CFStringRef		cfhandler = NULL;
    CFStringRef		cfscheme = NULL;
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
	cfhandlers = LSCopyAllRoleHandlersForContentType( cfuti, kLSRolesAll );
	cfschemes = LSCopyAllHandlersForURLScheme( cfuti );
	if (( cfhandlers == NULL ) && ( cfschemes == NULL )) {
	    fprintf( stderr, "%s: no handlers\n", uti );
	    rc = 1;
	    goto uti_show_done;
	}

	if ( cfhandlers ) {
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
	}

	if ( cfschemes ) {
	    if ( verbose ) {
		printf( "All handlers for scheme %s:\n", uti );
	    }

	    count = CFArrayGetCount( cfschemes );
	    for ( i = 0; i < count; i++ ) {
		cfscheme = CFArrayGetValueAtIndex( cfschemes, i );
		if ( cf2c( cfscheme, dh, sizeof( dh )) != 0 ) {
		    rc = 2;
		    continue;
		}
		printf( "%s\n", dh );
		memset( dh, 0, sizeof( dh ));
	    }
	
	    cfscheme = NULL;
	}
    } else {
	cfhandler = LSCopyDefaultRoleHandlerForContentType( cfuti, kLSRolesAll );
	cfscheme = LSCopyDefaultHandlerForURLScheme( cfuti );
	if (( cfhandler == NULL ) && ( cfscheme == NULL )) {
	    fprintf( stderr, "%s: no default handler\n", uti );
	    rc = 1;
	    goto uti_show_done;
	}

	if ( cfhandler ) {
	    if ( cf2c( cfhandler, dh, MAXPATHLEN ) != 0 ) {
		rc = 2;
		goto uti_show_done;
	    }

	    if ( verbose ) {
		printf( "Default handler for UTI %s: ", uti );
	    }
	    printf( "%s\n", dh );
	}

	if ( cfscheme ) {
	    if ( cf2c( cfscheme, dh, MAXPATHLEN ) != 0 ) {
		rc = 2;
		goto uti_show_done;
	    }

	    if ( verbose ) {
		printf( "Default handler for scheme %s: ", uti );
	    }
	    printf( "%s\n", dh );
	}
    }

uti_show_done:
    if ( cfhandlers ) {
	CFRelease( cfhandlers );
    }
    if ( cfschemes ) {
	CFRelease( cfschemes );
    }
    if ( cfuti ) {
	CFRelease( cfuti );
    }
    if ( cfhandler ) {
	CFRelease( cfhandler );
    }
    if ( cfscheme ) {
	CFRelease( cfscheme );
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
    CFStringRef		cf_uti = NULL;
    CFStringRef		cf_app_bundle_id = NULL;
    CFStringRef		cf_app_name = NULL;
    CFStringRef		cf_error_description = NULL;
    CFURLRef		cf_app_url = NULL;
    CFErrorRef		cf_error = NULL;
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

    cf_uti = UTTypeCreatePreferredIdentifierForTag( kUTTagClassFilenameExtension, cf_ext, NULL );
    if ( cf_uti == NULL ) {
	fprintf( stderr, "Failed to get default application for extension \'%s\'\n", rext );
	goto duti_extension_cleanup;
    }
    cf_app_url = UTTypeCopyDeclaringBundleURL( cf_uti );
    if ( cf_app_url == NULL ) {
	fprintf( stderr, "Failed to get default application for extension \'%s\'\n", rext );
	goto duti_extension_cleanup;
    }

    if ( !CFURLCopyResourcePropertyForKey( cf_app_url, kCFURLLocalizedNameKey, &cf_app_name, &cf_error )) {
	cf_error_description = CFErrorCopyDescription( cf_error );
	if ( cf_error_description != NULL ) {
	    if ( cf2c( cf_error_description, tmp, sizeof( tmp )) != 0 ) {
		goto duti_extension_cleanup;
	    }
	    fprintf( stderr, "Failed to get display name: %s\n", tmp );
	}
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
    if ( cf_uti != NULL ) {
	CFRelease( cf_uti );
    }
    if ( cf_app_url != NULL ) {
	CFRelease( cf_app_url );
    }
    if ( cf_info_dict != NULL ) {
	CFRelease( cf_info_dict );
    }
    if ( cf_error_description != NULL ) {
	CFRelease( cf_error_description );
    }
    if ( cf_app_name != NULL ) {
	CFRelease( cf_app_name );
    }
    if ( cf_error != NULL ) {
	CFRelease( cf_error );
    }

    return( rc );
}

int
duti_default_app_for_type( char *osType ) {
    union {
	OSType		typeAsOSType;
	char		typeAsString[4];
    }			u;
    CFStringRef		cf_type = NULL;
    CFArrayRef		cf_array = NULL;
    CFStringRef		cf_uti_description = NULL;
    CFDictionaryRef	cf_uti_declaration = NULL;
    CFIndex		index;
    CFIndex		count;
    char		tmp[ MAXPATHLEN ];
    int			rc = 2;

    u.typeAsOSType = 0;
    strncpy( u.typeAsString, osType, sizeof( u.typeAsOSType ));
    cf_type = UTCreateStringForOSType( htonl( u.typeAsOSType ));

    cf_array = UTTypeCreateAllIdentifiersForTag(kUTTagClassOSType, cf_type, nil);
    for (index = 0, count = CFArrayGetCount(cf_array); index < count; index++) {
	CFStringRef cf_uti_identifier = CFArrayGetValueAtIndex(cf_array, index);
	if ( cf2c( cf_uti_identifier, tmp, sizeof( tmp )) != 0 ) {
	    goto duti_default_app_for_type_cleanup;
	}
	printf( "identifier: %s\n", tmp );

	cf_uti_description = UTTypeCopyDescription(cf_uti_identifier);
	if ( cf2c( cf_uti_description, tmp, sizeof( tmp )) != 0 ) {
	    goto duti_default_app_for_type_cleanup;
	}
	CFRelease(cf_uti_description); cf_uti_description = NULL;
	printf( "description: %s\n", tmp );

	cf_uti_declaration = UTTypeCopyDeclaration(cf_uti_identifier);
	printf( "declaration: {\n" );
	CFDictionaryApplyFunction(cf_uti_declaration, dump_cf_dictionary, "\t");
	CFRelease(cf_uti_declaration); cf_uti_declaration = NULL;
	printf( "}\n" );
    }

    /* success */
    rc = 0;

duti_default_app_for_type_cleanup:
    if ( cf_type != NULL ) {
	CFRelease( cf_type );
    }
    return( rc );
}

int
duti_urls_for_url( char *pathOrURL ) {
    char		*url;
    CFURLRef		cf_url = NULL;
    CFURLRef		cf_url_default = NULL;
    CFURLRef		cf_other_url = NULL;
    CFArrayRef		cf_urls = NULL;
    CFStringRef		cf_url_path = NULL;
    CFStringRef		cf_error_description = NULL;
    CFErrorRef		cf_error = NULL;
    CFIndex		count;
    CFIndex		index;
    char		tmp[ MAXPATHLEN ];
    int			rc = 2;

    url = pathOrURL;
    if ( strncmp( pathOrURL, "file://", 7 ) != 0 ) {
	url = tmp;
	strncpy( url, "file://", MAXPATHLEN );
	if ( strncmp( pathOrURL, "/", 1 ) != 0 ) {
	    getcwd( url + 7, MAXPATHLEN - 7 );
	    url[MAXPATHLEN - 1] = '\0';
	}
	count = strlen( url );
	strncat( url, "/", MAXPATHLEN - count );
	if ( count < MAXPATHLEN ) {
	    count++;
	}
	strncpy( url + count, pathOrURL, MAXPATHLEN - count );
	url[MAXPATHLEN - 1] = '\0';
    }

    cf_url = CFURLCreateAbsoluteURLWithBytes(kCFAllocatorDefault, (UInt8*) url, strlen(url), kCFStringEncodingUTF8, NULL, TRUE );
    if ( cf_url == NULL ) {
	goto duti_urls_for_url_cleanup;
    }
	
    cf_url_default = LSCopyDefaultApplicationURLForURL(cf_url, kLSRolesAll, &cf_error);
    if ( cf_url_default == NULL ) {
	cf_error_description = CFErrorCopyDescription( cf_error );
	if ( cf_error_description != NULL ) {
	    if ( cf2c( cf_error_description, tmp, sizeof( tmp )) != 0 ) {
		goto duti_urls_for_url_cleanup;
	    }
	    fprintf( stderr, "%s\n", tmp );
	}
	goto duti_urls_for_url_cleanup;
    }
	
    cf_url_path = CFURLGetString( cf_url_default );
    if ( cf2c( cf_url_path, tmp, sizeof( tmp )) != 0 ) {
	goto duti_urls_for_url_cleanup;
    }
    printf( "default:\n" );
    printf( "%s\n", tmp );

    cf_urls = LSCopyApplicationURLsForURL(cf_url, kLSRolesEditor);
    if ( cf_urls != NULL ) {
	printf( "\neditor:\n" );
	count = CFArrayGetCount( cf_urls );
	for ( index = 0; index < count; index++ ) {
	    cf_other_url = CFArrayGetValueAtIndex( cf_urls, index );
	    cf_url_path = CFURLGetString( cf_other_url );
	    if ( cf2c( cf_url_path, tmp, sizeof( tmp )) != 0 ) {
		goto duti_urls_for_url_cleanup;
	    }
	    printf( "%s\n", tmp );
	}	
	CFRelease( cf_urls ); cf_urls = NULL;
    }

    cf_urls = LSCopyApplicationURLsForURL(cf_url, kLSRolesViewer);
    if ( cf_urls != NULL ) {
	printf( "\nviewer:\n" );
	count = CFArrayGetCount( cf_urls );
	for ( index = 0; index < count; index++ ) {
	    cf_other_url = CFArrayGetValueAtIndex( cf_urls, index );
	    cf_url_path = CFURLGetString( cf_other_url );
	    if ( cf2c( cf_url_path, tmp, sizeof( tmp )) != 0 ) {
		goto duti_urls_for_url_cleanup;
	    }
	    printf( "%s\n", tmp );
	}
	CFRelease( cf_urls ); cf_urls = NULL;
    }

    cf_urls = LSCopyApplicationURLsForURL(cf_url, kLSRolesShell);
    if ( cf_urls != NULL ) {
	printf( "\nshell:\n" );
	count = CFArrayGetCount( cf_urls );
	for ( index = 0; index < count; index++ ) {
	    cf_other_url = CFArrayGetValueAtIndex( cf_urls, index );
	    cf_url_path = CFURLGetString( cf_other_url );
	    if ( cf2c( cf_url_path, tmp, sizeof( tmp )) != 0 ) {
		goto duti_urls_for_url_cleanup;
	    }
	    printf( "%s\n", tmp );
	}	
	CFRelease( cf_urls ); cf_urls = NULL;
    }

    cf_urls = LSCopyApplicationURLsForURL(cf_url, kLSRolesNone);
    if ( cf_urls != NULL ) {
	printf( "\nnone:\n" );
	count = CFArrayGetCount( cf_urls );
	for ( index = 0; index < count; index++ ) {
	    cf_other_url = CFArrayGetValueAtIndex( cf_urls, index );
	    cf_url_path = CFURLGetString( cf_other_url );
	    if ( cf2c( cf_url_path, tmp, sizeof( tmp )) != 0 ) {
		goto duti_urls_for_url_cleanup;
	    }
	    printf( "%s\n", tmp );
	}	
	CFRelease( cf_urls ); cf_urls = NULL;
    }

    /* success */
    rc = 0;

duti_urls_for_url_cleanup:
    if ( cf_url != NULL ) {
	CFRelease( cf_url );
    }
    if ( cf_url_default != NULL ) {
	CFRelease( cf_url_default );
    }
    if ( cf_urls != NULL ) {
	CFRelease( cf_urls );
    }
    if ( cf_error_description != NULL ) {
	CFRelease( cf_error_description );
    }
    if ( cf_error != NULL ) {
	CFRelease( cf_error );
    }

    return rc;
}

int
duti_utis( char *uti ) {
    CFStringRef		cf_uti_identifier = NULL;
    CFStringRef		cf_uti_description = NULL;
    CFDictionaryRef	cf_uti_declaration = NULL;
    char		tmp[ MAXPATHLEN ];
    int			rc = 2;

    if ( uti == NULL ) {
	fprintf( stderr, "Invalid UTI.\n" );
	return( rc );
    }

    if ( c2cf( uti, &cf_uti_identifier ) != 0 ) {
	return( rc );
    }

    cf_uti_description = UTTypeCopyDescription(cf_uti_identifier);
    if ( cf2c( cf_uti_description, tmp, sizeof( tmp )) != 0 ) {
	goto duti_utis_cleanup;
    }
    CFRelease(cf_uti_description); cf_uti_description = NULL;
    printf( "description: %s\n", tmp );
	
    cf_uti_declaration = UTTypeCopyDeclaration(cf_uti_identifier);
    printf( "declaration: {\n" );
    CFDictionaryApplyFunction(cf_uti_declaration, dump_cf_dictionary, "\t");
    CFRelease(cf_uti_declaration); cf_uti_declaration = NULL;
    printf( "}\n" );

    /* success */
    rc = 0;

duti_utis_cleanup:
    if ( cf_uti_identifier ) {
	CFRelease( cf_uti_identifier );
    }
    if ( cf_uti_description != NULL ) {
	CFRelease( cf_uti_description );
    }
    if ( cf_uti_declaration != NULL ) {
	CFRelease( cf_uti_declaration );
    }

    return rc;
}

int
duti_urls_for_bundle(char *bundle_id) {
    CFStringRef		cf_bundle_id = NULL;
    CFArrayRef		cf_array = NULL;
    CFErrorRef		cf_error = NULL;
    int			rc = 2;

    if ( bundle_id == NULL ) {
	fprintf( stderr, "Invalid bundle identifier.\n" );
	return( rc );
    }

    if ( c2cf( bundle_id, &cf_bundle_id ) != 0 ) {
	return( rc );
    }

    cf_array = LSCopyApplicationURLsForBundleIdentifier(cf_bundle_id, &cf_error);
    if (cf_array) {
	CFArrayApplyFunction(cf_array, CFRangeMake( 0, CFArrayGetCount( cf_array )), dump_cf_array, "url: ");
    }

    /* success */
    rc = 0;

    if ( cf_bundle_id ) {
	CFRelease( cf_bundle_id );
    }
    if ( cf_array != NULL ) {
	CFRelease( cf_array );
    }
    if ( cf_error != NULL ) {
	CFRelease( cf_error );
    }

    return rc;
}

int
duti_utis_for_extension(char *ext) {
    CFStringRef		cf_ext = NULL;
    CFArrayRef		cf_array = NULL;
    CFStringRef		cf_uti_description = NULL;
    CFDictionaryRef	cf_uti_declaration = NULL;
    CFIndex		index;
    CFIndex		count;
    char   	        *rext;
    char        	tmp[ MAXPATHLEN ];
    int	            	rc = 2;

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

    cf_array = UTTypeCreateAllIdentifiersForTag(kUTTagClassFilenameExtension, cf_ext, nil);
    for (index = 0, count = CFArrayGetCount(cf_array); index < count; index++) {
	CFStringRef cf_uti_identifier = CFArrayGetValueAtIndex(cf_array, index);
	if ( cf2c( cf_uti_identifier, tmp, sizeof( tmp )) != 0 ) {
	    goto duti_utis_cleanup;
	}
	printf( "identifier: %s\n", tmp );

	cf_uti_description = UTTypeCopyDescription(cf_uti_identifier);
	if ( cf2c( cf_uti_description, tmp, sizeof( tmp )) != 0 ) {
	    goto duti_utis_cleanup;
	}
	CFRelease(cf_uti_description); cf_uti_description = NULL;
	printf( "description: %s\n", tmp );

	cf_uti_declaration = UTTypeCopyDeclaration(cf_uti_identifier);
	printf( "declaration: {\n" );
	CFDictionaryApplyFunction(cf_uti_declaration, dump_cf_dictionary, "\t");
	CFRelease(cf_uti_declaration); cf_uti_declaration = NULL;
	printf( "}\n" );
    }

    /* success */
    rc = 0;

duti_utis_cleanup:
    if ( cf_ext != NULL ) {
	CFRelease( cf_ext );
    }
    if ( cf_array != NULL ) {
	CFRelease( cf_array );
    }
    if ( cf_uti_description != NULL ) {
	CFRelease( cf_uti_description );
    }
    if ( cf_uti_declaration != NULL ) {
	CFRelease( cf_uti_declaration );
    }

    return( rc );
}

static void
dump_cf_array( const void *value, void *context ) {
    CFTypeID	typeID;
    char	tmp[ MAXPATHLEN ];

    typeID = CFGetTypeID( value );
    if (typeID == CFStringGetTypeID()) {
	if ( cf2c( value, tmp, sizeof( tmp )) != 0 ) {
	    return;
	}
	printf( "%s%s\n", (char *) context, tmp );
    } if (typeID == CFURLGetTypeID()) {
	CFURLRef cf_url = (CFURLRef) value;
	CFStringRef cf_url_path = CFURLGetString( cf_url );
	if ( cf2c( cf_url_path, tmp, sizeof( tmp )) != 0 ) {
	    return;
	}
	printf( "%s%s\n", (char *) context, tmp );
    } else {
	printf( "%sunhandled value\n", (char *) context );
    }
}

static void
dump_cf_dictionary( const void *key, const void *value, void *context ) {
    CFTypeID	typeID;
    char	tmp[ MAXPATHLEN ];

    if ( cf2c( key, tmp, sizeof( tmp )) != 0 ) {
	return;
    }
    printf( "%s%s = ", (char *) context, tmp );

    typeID = CFGetTypeID( value );
    if (typeID == CFStringGetTypeID()) {
	if ( cf2c( value, tmp, sizeof( tmp )) != 0 ) {
	    return;
	}
	printf( "%s\n", tmp );
    } else if (typeID == CFArrayGetTypeID()) {
	printf( "[\n" );
	sprintf(tmp, "%s\t", (char *) context );
	CFArrayApplyFunction(value, CFRangeMake( 0, CFArrayGetCount( value )), dump_cf_array, tmp);
	printf( "%s]\n", (char *) context );
    } else if (typeID == CFDictionaryGetTypeID()) {
	printf( "{\n" );
	sprintf(tmp, "%s\t", (char *) context );
	CFDictionaryApplyFunction(value, dump_cf_dictionary, tmp);
	printf( "%s}\n", (char *) context );
    } else {
	printf( "unhandled key\n");
    }
}
