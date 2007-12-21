struct roles {
    const char		*r_role;
    LSRolesMask		r_mask;
};

extern int		nroles;

int		sethandler( CFStringRef, CFStringRef, LSRolesMask );
int		fsethandler( char * );
int		psethandler( char * );
int		dirsethandler( char * );
int		( *handler_f )( char * );
