/*
 * lib/krb5/krb/get_in_tkt.c
 *
 * Copyright 1995 by the Massachusetts Institute of Technology.
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
 */

#define NEED_SOCKETS
#include "k5-int.h"
#include <memory.h>

/* helper function: convert flags to necessary KDC options */
#define flags2options(flags) (flags & KDC_TKT_COMMON_MASK)

/* Get a TGT for use at the remote host */
KRB5_DLLIMP krb5_error_code KRB5_CALLCONV
krb5_fwd_tgt_creds(context, auth_context, rhost, client, server, cc,
		   forwardable, outbuf)
    krb5_context context;
    krb5_auth_context auth_context;
    char FAR *rhost;
    krb5_principal client;
    krb5_principal server;
    krb5_ccache cc;
    int forwardable;      /* Should forwarded TGT also be forwardable? */
    krb5_data FAR *outbuf;
{
    krb5_replay_data replaydata;
    krb5_data FAR * scratch = 0;
    krb5_address FAR * FAR *addrs = 0;
    krb5_error_code retval;
    krb5_creds creds, tgt;
    krb5_creds FAR *pcreds;
    krb5_flags kdcoptions;
    int close_cc = 0;
    int free_rhost = 0;

    memset((char *)&creds, 0, sizeof(creds));
    memset((char *)&tgt, 0, sizeof(creds));

    if (rhost == NULL) {
	if (krb5_princ_type(context, server) != KRB5_NT_SRV_HST)
	    return(KRB5_FWD_BAD_PRINCIPAL);

	if (krb5_princ_size(context, server) < 2)
	    return (KRB5_CC_BADNAME);
	
	rhost = malloc(server->data[1].length+1);
	if (!rhost)
	    return ENOMEM;
	free_rhost = 1;
	memcpy(rhost, server->data[1].data, server->data[1].length);
	rhost[server->data[1].length] = '\0';
    }

    retval = krb5_os_hostaddr(context, rhost, &addrs);
    if (retval)
	goto errout;

    if ((retval = krb5_copy_principal(context, client, &creds.client)))
	goto errout;
    
    if ((retval = krb5_build_principal_ext(context, &creds.server,
					   client->realm.length,
					   client->realm.data,
					   KRB5_TGS_NAME_SIZE,
					   KRB5_TGS_NAME,
					   client->realm.length,
					   client->realm.data,
					   0)))
	goto errout;
	
    if (cc == 0) {
	if ((retval = krb5int_cc_default(context, &cc)))
	    goto errout;
	close_cc = 1;
    }

    /* fetch tgt directly from cache */
    retval = krb5_cc_retrieve_cred (context, cc, KRB5_TC_SUPPORTED_KTYPES,
				    &creds, &tgt);
    if (retval)
	goto errout;

    /* tgt->client must be equal to creds.client */
    if (!krb5_principal_compare(context, tgt.client, creds.client)) {
	retval = KRB5_PRINC_NOMATCH;
	goto errout;
    }

    if (!tgt.ticket.length) {
	retval = KRB5_NO_TKT_SUPPLIED;
	goto errout;
    }

    creds.times = tgt.times;
    creds.times.starttime = 0;
    kdcoptions = flags2options(tgt.ticket_flags)|KDC_OPT_FORWARDED;

    if (!forwardable) /* Reset KDC_OPT_FORWARDABLE */
      kdcoptions &= ~(KDC_OPT_FORWARDABLE);

    if ((retval = krb5_get_cred_via_tkt(context, &tgt, kdcoptions,
					addrs, &creds, &pcreds)))
        goto errout;

    retval = krb5_mk_1cred(context, auth_context, pcreds,
                           &scratch, &replaydata);
    krb5_free_creds(context, pcreds);

    if (retval) {
	if (scratch)
	    krb5_free_data(context, scratch);
    } else {
	*outbuf = *scratch;
	krb5_xfree(scratch);
    }
        
errout:
    if (addrs)
	krb5_free_addresses(context, addrs);
    if (close_cc)
	krb5_cc_close(context, cc);
    if (free_rhost)
	free(rhost);
    krb5_free_cred_contents(context, &creds);
    krb5_free_cred_contents(context, &tgt);
    return retval;
}
