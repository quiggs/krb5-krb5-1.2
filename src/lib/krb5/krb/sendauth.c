/*
 * lib/krb5/krb/sendauth.c
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
 * convenience sendauth/recvauth functions
 */

#define NEED_SOCKETS

#include "k5-int.h"
#include "auth_con.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static char *sendauth_version = "KRB5_SENDAUTH_V1.0";

KRB5_DLLIMP krb5_error_code KRB5_CALLCONV
krb5_sendauth(context, auth_context,
	      /* IN */
	      fd, appl_version, client, server, ap_req_options, in_data,
	      in_creds,
	      /* IN/OUT */
	      ccache,
	      /* OUT */
	      error, rep_result, out_creds)
    	krb5_context 		  context;
    	krb5_auth_context       FAR * auth_context;
	krb5_pointer		  fd;
	char			FAR * appl_version;
	krb5_principal		  client;
	krb5_principal		  server;
	krb5_flags		  ap_req_options;
	krb5_data		FAR * in_data;
	krb5_creds		FAR * in_creds;
	krb5_ccache	  	  ccache;
	krb5_error             FAR * FAR * error;
	krb5_ap_rep_enc_part   FAR * FAR * rep_result;
	krb5_creds	       FAR * FAR * out_creds;
{
	krb5_octet		result;
	krb5_creds 		creds;
	krb5_creds		FAR * credsp = NULL;
	krb5_creds		FAR * credspout = NULL;
	krb5_error_code		retval = 0;
	krb5_data		inbuf, outbuf;
	int			len;
	krb5_ccache		use_ccache = 0;

	if (error)
	    *error = 0;

	/*
	 * First, send over the length of the sendauth version string;
	 * then, we send over the sendauth version.  Next, we send
	 * over the length of the application version strings followed
	 * by the string itself.  
	 */
	outbuf.length = strlen(sendauth_version) + 1;
	outbuf.data = sendauth_version;
	if ((retval = krb5_write_message(context, fd, &outbuf)))
		return(retval);
	outbuf.length = strlen(appl_version) + 1;
	outbuf.data = appl_version;
	if ((retval = krb5_write_message(context, fd, &outbuf)))
		return(retval);
	/*
	 * Now, read back a byte: 0 means no error, 1 means bad sendauth
	 * version, 2 means bad application version
	 */
#ifndef ECONNABORTED
#define ECONNABORTED WSAECONNABORTED
#endif
    if ((len = krb5_net_read(context, *((int *) fd), (char *)&result, 1)) != 1)
	return((len < 0) ? errno : ECONNABORTED);
    if (result == 1)
	return(KRB5_SENDAUTH_BADAUTHVERS);
    else if (result == 2)
	return(KRB5_SENDAUTH_BADAPPLVERS);
    else if (result != 0)
	return(KRB5_SENDAUTH_BADRESPONSE);
	/*
	 * We're finished with the initial negotiations; let's get and
	 * send over the authentication header.  (The AP_REQ message)
	 */

	/*
	 * If no credentials were provided, try getting it from the
	 * credentials cache.
	 */
	memset((char *)&creds, 0, sizeof(creds));

	/*
	 * See if we need to access the credentials cache
	 */
	if (!in_creds || !in_creds->ticket.length) {
		if (ccache)
			use_ccache = ccache;
		else if ((retval = krb5int_cc_default(context, &use_ccache)))
			goto error_return;
	}
	if (!in_creds) {
		if ((retval = krb5_copy_principal(context, server,
						  &creds.server)))
			goto error_return;
		if (client)
			retval = krb5_copy_principal(context, client, 
						     &creds.client);
		else
			retval = krb5_cc_get_principal(context, use_ccache,
						       &creds.client);
		if (retval) {
			krb5_free_principal(context, creds.server);
			goto error_return;
		}
		/* creds.times.endtime = 0; -- memset 0 takes care of this
					zero means "as long as possible" */
		/* creds.keyblock.enctype = 0; -- as well as this.
					zero means no session enctype
					preference */
		in_creds = &creds;
	}
	if (!in_creds->ticket.length) {
	    if ((retval = krb5_get_credentials(context, 0,
					       use_ccache, in_creds, &credsp)))
		    goto error_return;
	    credspout = credsp;
	} else {
	    credsp = in_creds;
	}

	if (ap_req_options & AP_OPTS_USE_SUBKEY) {
	    /* Provide some more fodder for random number code.
	       This isn't strong cryptographically; the point here is
	       not to guarantee randomness, but to make it less likely
	       that multiple sessions could pick the same subkey.  */
	    char rnd_data[1024];
	    size_t len;
	    krb5_data d;
	    d.length = sizeof (rnd_data);
	    d.data = rnd_data;
	    len = sizeof (rnd_data);
	    if (getpeername (*(int*)fd, (struct sockaddr *) rnd_data, &len) == 0) {
		d.length = len;
		(void) krb5_c_random_seed (context, &d);
	    }
	    len = sizeof (rnd_data);
	    if (getsockname (*(int*)fd, (struct sockaddr *) rnd_data, &len) == 0) {
		d.length = len;
		(void) krb5_c_random_seed (context, &d);
	    }
	}

	if ((retval = krb5_mk_req_extended(context, auth_context,
					   ap_req_options, in_data, credsp,
					   &outbuf)))
	    goto error_return;

	/*
	 * First write the length of the AP_REQ message, then write
	 * the message itself.
	 */
	retval = krb5_write_message(context, fd, &outbuf);
	free(outbuf.data);
	if (retval)
	    goto error_return;

	/*
	 * Now, read back a message.  If it was a null message (the
	 * length was zero) then there was no error.  If not, we the
	 * authentication was rejected, and we need to return the
	 * error structure.
	 */
	if ((retval = krb5_read_message(context, fd, &inbuf)))
	    goto error_return;

	if (inbuf.length) {
		if (error) {
		    if ((retval = krb5_rd_error(context, &inbuf, error))) {
			krb5_xfree(inbuf.data);
			goto error_return;
		    }
		}
		retval = KRB5_SENDAUTH_REJECTED;
		krb5_xfree(inbuf.data);
		goto error_return;
	}
	
	/*
	 * If we asked for mutual authentication, we should now get a
	 * length field, followed by a AP_REP message
	 */
	if ((ap_req_options & AP_OPTS_MUTUAL_REQUIRED)) {
	    krb5_ap_rep_enc_part	*repl = 0;
		
	    if ((retval = krb5_read_message(context, fd, &inbuf)))
		goto error_return;

	    if ((retval = krb5_rd_rep(context, *auth_context, &inbuf,
				      &repl))) {
		if (repl)
		    krb5_free_ap_rep_enc_part(context, repl);
	        krb5_xfree(inbuf.data);
		goto error_return;
	    }

	    krb5_xfree(inbuf.data);
	    /*
	     * If the user wants to look at the AP_REP message,
	     * copy it for him
	     */
	    if (rep_result) 
		*rep_result = repl;
	    else
		krb5_free_ap_rep_enc_part(context, repl);
	}
	retval = 0;		/* Normal return */
	if (out_creds) {
	    *out_creds = credsp;
	    credspout = NULL;
	}

error_return:
    krb5_free_cred_contents(context, &creds);
    if (credspout != NULL)
	krb5_free_creds(context, credspout); 
    if (!ccache && use_ccache)
	krb5_cc_close(context, use_ccache);
    return(retval);
}


