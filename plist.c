#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/param.h>
#include <errno.h>
#include <string.h>

#include "plist.h"

    int
read_plist( char *plpath, CFDictionaryRef *dr )
{
    CFURLRef		cfurl = NULL;
    CFReadStreamRef	cfrs = NULL;
    CFDictionaryRef	cfdict = NULL;
    CFPropertyListFormat	fmt = kCFPropertyListXMLFormat_v1_0;
    CFStreamError	err;

    int			rc = 0;
    char		respath[ MAXPATHLEN ];
    
    if ( plpath == NULL ) {
	fprintf( stderr, "%s: Invalid plist path\n", __FUNCTION__ );
	return( -1 );
    }

    if ( realpath( plpath, respath ) == NULL ) {
	fprintf( stderr, "%s: realpath failed: %s\n",
		__FUNCTION__, strerror( errno ));
	return( -1 );
    }

    /*
     * must convert C string path to CFURL to read the plist
     * from disk. no convenience methods here like Cocoa's
     * -dictionaryWithContentsOfFile:
     */
    if (( cfurl = CFURLCreateFromFileSystemRepresentation(
			kCFAllocatorDefault, ( const UInt8 * )respath,
			( CFIndex )strlen( respath ), false )) == NULL ) {
	fprintf( stderr, "%s: failed to create URL for %s\n",
			__FUNCTION__, respath );
	return( -1 );
    }

    if (( cfrs = CFReadStreamCreateWithFile( kCFAllocatorDefault,
			cfurl )) == NULL ) {
	fprintf( stderr, "%s: failed to create read stream\n", __FUNCTION__ );
	rc = -1;
	goto cleanup;
    }
    if ( CFReadStreamOpen( cfrs ) == false ) {
	err = CFReadStreamGetError( cfrs );
	fprintf( stderr, "%s: failed to open read stream\n", __FUNCTION__ );
	if ( err.domain == kCFStreamErrorDomainPOSIX ) {
	    fprintf( stderr, "%s: %s\n", plpath, strerror( errno ));
	} else {
	    fprintf( stderr, "domain %d, error %d\n",
			( int )err.domain, ( int )err.error );
	}
	rc = -1;
	goto cleanup;
    }

    if (( cfdict = CFPropertyListCreateFromStream( kCFAllocatorDefault, cfrs,
			0, kCFPropertyListImmutable, &fmt, NULL )) == NULL ) {
	fprintf( stderr, "%s: failed to read plist\n", __FUNCTION__ );
	rc = -1;
	goto cleanup;
    }

    if ( !CFPropertyListIsValid( cfdict, fmt )) {
	fprintf( stderr, "%s: invalid plist\n", plpath );
	CFRelease( cfdict );
	cfdict = NULL;
	rc = -1;
    }

cleanup:
    if ( cfurl != NULL ) {
	CFRelease( cfurl );
    }
    if ( cfrs != NULL ) {
	CFReadStreamClose( cfrs );
	CFRelease( cfrs );
    }

    *dr = cfdict;

    return( rc );
}
