int	parseline( char *, char *** );
int	sethandler( CFStringRef, CFStringRef, LSRolesMask );
int	fsethandler( FILE * );
int	psethandler( CFDictionaryRef );

struct roles {
    const char		*r_role;
    LSRolesMask		r_mask;
};

struct roles		rtm[] = {
    { "none",	kLSRolesNone },
    { "viewer",	kLSRolesViewer },
    { "editor",	kLSRolesEditor },
    { "shell",	kLSRolesShell },
    { "all",	kLSRolesAll },
};
