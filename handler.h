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
