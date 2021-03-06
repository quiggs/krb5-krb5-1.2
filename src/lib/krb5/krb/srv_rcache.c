/*
 * lib/krb5/krb/srv_rcache.c
 *
 * Copyright 1991 by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * Allocate & prepare a default replay cache for a server.
 */

#include "k5-int.h"
#include <ctype.h>
#include <stdio.h>

KRB5_DLLIMP krb5_error_code KRB5_CALLCONV
krb5_get_server_rcache(context, piece, rcptr)
    krb5_context context;
    const krb5_data *piece;
    krb5_rcache *rcptr;
{
    krb5_rcache rcache = 0;
    char *cachename = 0;
    char tmp[4];
    krb5_error_code retval;
    int len, p, i;

#ifdef HAVE_GETEUID
    unsigned long tens;
    unsigned long uid = geteuid();
#endif
    
    rcache = (krb5_rcache) malloc(sizeof(*rcache));
    if (!rcache)
	return ENOMEM;
    
    retval = krb5_rc_resolve_type(context, &rcache, 
				  krb5_rc_default_type(context));
    if (retval) goto cleanup;

    len = piece->length + 3 + 1;
    for (i = 0; i < piece->length; i++) {
	if (piece->data[i] == '\\')
	    len++;
	else if (!isgraph(piece->data[i]))
	    len += 3;
    }

#ifdef HAVE_GETEUID
    len += 2;	/* _<uid> */
    for (tens = 1; (uid / tens) > 9 ; tens *= 10)
	len++;
#endif
    
    cachename = malloc(len);
    if (!cachename) {
	retval = ENOMEM;
	goto cleanup;
    }
    strcpy(cachename, "rc_");
    p = 3;
    for (i = 0; i < piece->length; i++) {
	if (piece->data[i] == '\\') {
	    cachename[p++] = '\\';
	    cachename[p++] = '\\';
	    continue;
	}
	if (!isgraph(piece->data[i])) {
	    sprintf(tmp, "%03o", piece->data[i]);
	    cachename[p++] = '\\';
	    cachename[p++] = tmp[0];
	    cachename[p++] = tmp[1];
	    cachename[p++] = tmp[2];
	    continue;
	}
	cachename[p++] = piece->data[i];
    }

#ifdef HAVE_GETEUID
    cachename[p++] = '_';
    while (tens) {
	cachename[p++] = '0' + ((uid / tens) % 10);
	tens /= 10;
    }
#endif

    cachename[p++] = '\0';

    if ((retval = krb5_rc_resolve(context, rcache, cachename)))
	goto cleanup;
    
    /*
     * First try to recover the replay cache; if that doesn't work,
     * initialize it.
     */
    if (krb5_rc_recover(context, rcache)) {
	if ((retval = krb5_rc_initialize(context, rcache,
					 context->clockskew))) {
	    krb5_rc_close(context, rcache);
	    rcache = 0;
	    goto cleanup;
	}
    }

    *rcptr = rcache;
    rcache = 0;
    retval = 0;

cleanup:
    if (rcache)
	krb5_xfree(rcache);
    if (cachename)
	krb5_xfree(cachename);
    return retval;
}
