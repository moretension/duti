/* -*- mode:c; tab-width: 8 -*- */

struct roles {
    const char		*r_role;
    LSRolesMask		r_mask;
};

extern int		nroles;

#define DUTI_TYPE_URL_HANDLER		2
#define DUTI_TYPE_UTI_HANDLER		3

int		set_uti_handler( CFStringRef, CFStringRef, LSRolesMask );
int		set_url_handler( CFStringRef, CFStringRef );
int		fsethandler( char * );
int		psethandler( char * );
int		dirsethandler( char * );

int		uti_handler_show( char *uti, int showall );
int		url_handler_show( char *url_scheme );
int		duti_handler_set( char *, char *, char * );
int		duti_default_app_for_extension( char * );
int		duti_is_conformant_uti( CFStringRef );
int		duti_utis( char * );
int		duti_utis_for_extension( char * );
