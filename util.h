/* -*- mode:c; tab-width: 8 -*- */

struct ll {
    char	*l_path;
    struct ll	*l_next;
};

int		parseline( char *, char *** );
int		c2cf( char *, CFStringRef * );
int		cf2c( CFStringRef, char *, int );
int		cfurl2path( CFURLRef, char *, int );
void		lladd( char *, struct ll ** );
