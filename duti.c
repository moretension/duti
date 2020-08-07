/* duti: set default handlers for document types based on a settings file. */

#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "handler.h"

extern char		*duti_version;
extern int		nroles;
int			verbose = 0;

struct roles		rtm[] = {
    { "none",   kLSRolesNone },
    { "viewer", kLSRolesViewer },
    { "editor", kLSRolesEditor },
    { "shell",  kLSRolesShell },
    { "all",    kLSRolesAll },
};

    int
main( int ac, char *av[] )
{
    struct stat		st;
    int			c, rc, err = 0;
    int			set = 0;
    int			( *handler_f )( char * );
    char		*path = NULL;
    char		*p;

    extern int		optind;
    extern char		*optarg;

    while (( c = getopt( ac, av, "b:d:e:hl:o:st:u:Vvx:" )) != -1 ) {
	switch ( c ) {
	case 'b':	/* show bundle URLs for identifier */
	    rc = duti_urls_for_bundle( optarg );
	    if ( rc ) {
	        exit( 0 );
	    }
	    break;

	case 'd':	/* show default handler for UTI */
	    rc = uti_handler_show( optarg, 0 );
	    if ( rc ) {
	        exit( 0 );
	    }
	    break;

	case 'e':	/* UTI declarations for extension */
	    rc = duti_utis_for_extension( optarg );
	    if ( rc ) {
	        exit( 0 );
	    }
	    break;

	case 'h':	/* help */
	default:
	    err++;
	    break;

	case 'l':	/* list all handlers for UTI */
	    rc = uti_handler_show( optarg, 1 );
	    if ( rc ) {
	        exit( 0 );
	    }
	    break;

	case 'o':	/* show all bundles able to open path */
	    rc = duti_urls_for_url( optarg );
	    if ( rc ) {
	        exit( 0 );
	    }
	    break;

	case 's':	/* set handler */
	    set = 1;
	    break;

	case 't':	/* info for type */
	    rc = duti_default_app_for_type( optarg );
	    if ( rc ) {
	        exit( 0 );
	    }
	    break;

	case 'u':	/* UTI declarations */
	    rc = duti_utis( optarg );
	    if ( rc ) {
	        exit( 0 );
	    }
	    break;

	case 'V':	/* version */
	    printf( "%s\n", duti_version );
	    break;

	case 'v':	/* verbose */
	    verbose = 1;
	    break;

	case 'x':	/* info for extension */
	    rc = duti_default_app_for_extension( optarg );
	    if ( rc ) {
	        exit( 0 );
	    }
	}
    }

    if ( !set && !err) {
	exit( 0 );
    }

    nroles = sizeof( rtm ) / sizeof( rtm[ 0 ] );

    switch (( ac - optind )) {
    case 0 :	/* read from stdin */
	if ( set ) {
	    err++;
	}
	break;

    case 1 :	/* read from file or directory */
	if ( set ) {
	    err++;
	}
	path = av[ optind ];
	break;

    case 2 :	/* set URI handler */
	if ( set ) {
	    return( duti_handler_set( av[ optind ],
		    av[ optind + 1 ], NULL ));
	}
	/* this fallthrough works because set == 0 */
	
    case 3 :	/* set UTI handler */
	if ( set ) {
	    return( duti_handler_set( av[ optind ],
		    av[ optind + 1 ], av[ optind + 2 ] ));
	}
	/* fallthrough to error */
    
    default :	/* error */
	err++;
	break;
    }

    if ( err ) {
	fprintf( stderr, "usage: %s [ -hvV ] [ -b bundle_id ] [ -d uti ] [ -e ext ] [ -l uti ] [ -o path ] [ -t type ] [ -u uti ] [ -x ext ]"
		 "[ settings_path ]\n", av[ 0 ] );
	fprintf( stderr, "	-b print the URLs to all bundles with the identifier\n" );
	fprintf( stderr, "	-d print the default handler for the UTI\n" );
	fprintf( stderr, "	-e print the UTI declaration for the file extension\n" );
	fprintf( stderr, "	-l print the handlers for the UTI\n" );
	fprintf( stderr, "	-o print the URLs for all bundles able to open path\n" );
	fprintf( stderr, "	-t print the UTI declaration for the file type\n" );
	fprintf( stderr, "	-u print the UTI declaration for the UTI\n" );
	fprintf( stderr, "	-x print bundle claiming the file extension\n" );
	fprintf( stderr, "usage: %s -s bundle_id url_scheme\n", av[ 0 ] );
	fprintf( stderr, "usage: %s -s bundle_id uti role\n", av[ 0 ] );
	exit( 1 );
    }

    /* by default, read from a FILE stream */
    handler_f = fsethandler;

    if ( path ) {
	if ( stat( path, &st ) != 0 ) {
	    fprintf( stderr, "stat %s: %s\n", path, strerror( errno ));
	    exit( 2 );
	}
	switch ( st.st_mode & S_IFMT ) {
	case S_IFDIR:	/* directory of settings files */
	    handler_f = dirsethandler;
	    break;

	case S_IFREG:	/* settings file or plist */
	    if (( p = strrchr( path, '.' )) != NULL ) {
		p++;
		if ( strcmp( p, "plist" ) == 0 ) {
		    handler_f = psethandler;
		}
	    }
	    break;

	default:
	    fprintf( stderr, "%s: not a supported settings path\n", path );
	    exit( 1 );
	}
    }

    return( handler_f( path ));
}
