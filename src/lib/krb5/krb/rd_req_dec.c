/*
 * lib/krb5/krb/rd_req_dec.c
 *
 * Copyright (c) 1994 CyberSAFE Corporation.
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
 * Neither M.I.T., the Open Computing Security Group, nor
 * CyberSAFE Corporation make any representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * krb5_rd_req_decoded()
 */

#include "k5-int.h"
#include "auth_con.h"

/*
 * essentially the same as krb_rd_req, but uses a decoded AP_REQ as
 * the input rather than an encoded input.
 */
/*
 *  Parses a KRB_AP_REQ message, returning its contents.
 * 
 *  server specifies the expected server's name for the ticket; if NULL, then
 *  any server will be accepted if the key can be found, and the caller should
 *  verify that the principal is something it trusts.
 * 
 *  rcache specifies a replay detection cache used to store authenticators and
 *  server names
 * 
 *  keyproc specifies a procedure to generate a decryption key for the
 *  ticket.  If keyproc is non-NULL, keyprocarg is passed to it, and the result
 *  used as a decryption key. If keyproc is NULL, then fetchfrom is checked;
 *  if it is non-NULL, it specifies a parameter name from which to retrieve the
 *  decryption key.  If fetchfrom is NULL, then the default key store is
 *  consulted.
 * 
 *  authdat is set to point at allocated storage structures; the caller
 *  should free them when finished. 
 * 
 *  returns system errors, encryption errors, replay errors
 */

static krb5_error_code decrypt_authenticator
	PROTOTYPE((krb5_context, const krb5_ap_req *, krb5_authenticator **,
		   int));

#define in_clock_skew(date) (labs((date)-currenttime) < context->clockskew)

static krb5_error_code
krb5_rd_req_decrypt_tkt_part(context, req, keytab)
    krb5_context          context;
    const krb5_ap_req 	* req;
    krb5_keytab           keytab;

{
    krb5_error_code 	  retval;
    krb5_enctype 	  enctype;
    krb5_keytab_entry 	  ktent;

    enctype = req->ticket->enc_part.enctype;

    if ((retval = krb5_kt_get_entry(context, keytab, req->ticket->server,
				    req->ticket->enc_part.kvno,
				    enctype, &ktent)))
	return retval;

    retval = krb5_decrypt_tkt_part(context, &ktent.key, req->ticket);
    /* Upon error, Free keytab entry first, then return */

    (void) krb5_kt_free_entry(context, &ktent);
    return retval;
}

static krb5_error_code
krb5_rd_req_decoded_opt(context, auth_context, req, server, keytab, 
			ap_req_options, ticket, check_valid_flag)
    krb5_context 	  context;
    krb5_auth_context   * auth_context;
    const krb5_ap_req 	* req;
    krb5_const_principal  server;
    krb5_keytab           keytab;
    krb5_flags          * ap_req_options;
    krb5_ticket	       ** ticket;
    int			  check_valid_flag;
{
    krb5_error_code 	  retval = 0;
    krb5_timestamp 	  currenttime;

    if (server && !krb5_principal_compare(context, server, req->ticket->server))
	return KRB5KRB_AP_WRONG_PRINC;

    /* if (req->ap_options & AP_OPTS_USE_SESSION_KEY)
       do we need special processing here ?	*/

    /* decrypt the ticket */
    if ((*auth_context)->keyblock) { /* User to User authentication */
    	if ((retval = krb5_decrypt_tkt_part(context, (*auth_context)->keyblock,
					    req->ticket)))
	    return retval;
	krb5_free_keyblock(context, (*auth_context)->keyblock);
	(*auth_context)->keyblock = NULL;
    } else {
    	if ((retval = krb5_rd_req_decrypt_tkt_part(context, req, keytab)))
	    return retval;
    }

    /* XXX this is an evil hack.  check_valid_flag is set iff the call
       is not from inside the kdc.  we can use this to determine which
       key usage to use */
    if ((retval = decrypt_authenticator(context, req, 
					&((*auth_context)->authentp),
					check_valid_flag)))
	goto cleanup;

    if (!krb5_principal_compare(context, (*auth_context)->authentp->client,
				req->ticket->enc_part2->client)) {
	retval = KRB5KRB_AP_ERR_BADMATCH;
	goto cleanup;
    }

    if ((*auth_context)->remote_addr && 
      !krb5_address_search(context, (*auth_context)->remote_addr, 
			   req->ticket->enc_part2->caddrs)) {
	retval = KRB5KRB_AP_ERR_BADADDR;
	goto cleanup;
    }

    /* okay, now check cross-realm policy */

#if defined(_SINGLE_HOP_ONLY)

    /* Single hop cross-realm tickets only */

    { 
	krb5_transited *trans = &(req->ticket->enc_part2->transited);

      	/* If the transited list is empty, then we have at most one hop */
      	if (trans->tr_contents.data && trans->tr_contents.data[0])
            retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
    }

#elif defined(_NO_CROSS_REALM)

    /* No cross-realm tickets */

    { 
	char		* lrealm;
      	krb5_data      	* realm;
      	krb5_transited 	* trans;
  
	realm = krb5_princ_realm(context, req->ticket->enc_part2->client);
	trans = &(req->ticket->enc_part2->transited);

	/*
      	 * If the transited list is empty, then we have at most one hop 
      	 * So we also have to check that the client's realm is the local one 
	 */
      	krb5_get_default_realm(context, &lrealm);
      	if ((trans->tr_contents.data && trans->tr_contents.data[0]) ||
          strlen(lrealm) != realm->length ||
          memcmp(lrealm, realm->data, strlen(lrealm))) {
            retval = KRB5KRB_AP_ERR_ILL_CR_TKT;
      	}
      	free(lrealm);
    }

#else

    /* Hierarchical Cross-Realm */
  
    {
    	krb5_data        lrealm;
      	krb5_data      * realm;
      	krb5_transited * trans;
  
	realm = krb5_princ_realm(context, req->ticket->enc_part2->client);
	trans = &(req->ticket->enc_part2->transited);

	/*
      	 * If the transited list is not empty, then check that all realms 
      	 * transited are within the hierarchy between the client's realm  
      	 * and the local realm.                                        
  	 */
      	if (trans->tr_contents.data && trans->tr_contents.data[0]) {
            krb5_get_default_realm(context, &(lrealm.data));
            lrealm.length = strlen(lrealm.data);
            retval = krb5_check_transited_list(context, &(trans->tr_contents), 
					       realm, &lrealm);
            free(lrealm.data);
      	}
    }

#endif

    if (retval)  goto cleanup;

    /* only check rcache if sender has provided one---some services
       may not be able to use replay caches (such as datagram servers) */

    if ((*auth_context)->rcache) {
	krb5_donot_replay  rep;
        krb5_tkt_authent   tktauthent;

	tktauthent.ticket = req->ticket;	
	tktauthent.authenticator = (*auth_context)->authentp;
	if (!(retval = krb5_auth_to_rep(context, &tktauthent, &rep))) {
	    retval = krb5_rc_store(context, (*auth_context)->rcache, &rep);
	    krb5_xfree(rep.server);
	    krb5_xfree(rep.client);
	}

	if (retval)
	    goto cleanup;
    }

    retval = krb5_validate_times(context, &req->ticket->enc_part2->times);
    if (retval != 0)
	    goto cleanup;

    if ((retval = krb5_timeofday(context, &currenttime)))
	goto cleanup;

    if (!in_clock_skew((*auth_context)->authentp->ctime)) {
	retval = KRB5KRB_AP_ERR_SKEW;
	goto cleanup;
    }

    if (check_valid_flag) {
      if (req->ticket->enc_part2->flags & TKT_FLG_INVALID) {
	retval = KRB5KRB_AP_ERR_TKT_INVALID;
	goto cleanup;
      }
    }

    /* check if the various etypes are permitted */

    if ((*auth_context)->auth_context_flags & KRB5_AUTH_CONTEXT_PERMIT_ALL) {
	/* no etype check needed */;
    } else if ((*auth_context)->permitted_etypes == NULL) {
	/* check against the default set */
	if ((!krb5_is_permitted_enctype(context,
					req->ticket->enc_part.enctype)) ||
	    (!krb5_is_permitted_enctype(context,
					req->ticket->enc_part2->session->enctype)) ||
	    (((*auth_context)->authentp->subkey) &&
	     !krb5_is_permitted_enctype(context,
					(*auth_context)->authentp->subkey->enctype))) {
	    retval = KRB5_NOPERM_ETYPE;
	    goto cleanup;
	}
    } else {
	/* check against the set in the auth_context */
	int i;

	for (i=0; (*auth_context)->permitted_etypes[i]; i++)
	    if ((*auth_context)->permitted_etypes[i] ==
		req->ticket->enc_part.enctype)
		break;
	if (!(*auth_context)->permitted_etypes[i]) {
	    retval = KRB5_NOPERM_ETYPE;
	    goto cleanup;
	}
	
	for (i=0; (*auth_context)->permitted_etypes[i]; i++)
	    if ((*auth_context)->permitted_etypes[i] ==
		req->ticket->enc_part2->session->enctype)
		break;
	if (!(*auth_context)->permitted_etypes[i]) {
	    retval = KRB5_NOPERM_ETYPE;
	    goto cleanup;
	}
	
	if ((*auth_context)->authentp->subkey) {
	    for (i=0; (*auth_context)->permitted_etypes[i]; i++)
		if ((*auth_context)->permitted_etypes[i] ==
		    (*auth_context)->authentp->subkey->enctype)
		    break;
	    if (!(*auth_context)->permitted_etypes[i]) {
		retval = KRB5_NOPERM_ETYPE;
		goto cleanup;
	    }
	}
    }

    (*auth_context)->remote_seq_number = (*auth_context)->authentp->seq_number;
    if ((*auth_context)->authentp->subkey) {
	if ((retval = krb5_copy_keyblock(context,
					 (*auth_context)->authentp->subkey,
					 &((*auth_context)->remote_subkey))))
	    goto cleanup;
    } else {
	(*auth_context)->remote_subkey = 0;
    }

    if ((retval = krb5_copy_keyblock(context, req->ticket->enc_part2->session,
				     &((*auth_context)->keyblock))))
	goto cleanup;

    /*
     * If not AP_OPTS_MUTUAL_REQUIRED then and sequence numbers are used 
     * then the default sequence number is the one's complement of the
     * sequence number sent ot us.
     */
    if ((!(req->ap_options & AP_OPTS_MUTUAL_REQUIRED)) && 
      (*auth_context)->remote_seq_number) {
	(*auth_context)->local_seq_number ^= 
	  (*auth_context)->remote_seq_number;
    }

    if (ticket)
   	if ((retval = krb5_copy_ticket(context, req->ticket, ticket)))
	    goto cleanup;
    if (ap_req_options)
    	*ap_req_options = req->ap_options;
    retval = 0;
    
cleanup:
    if (retval) {
	/* only free if we're erroring out...otherwise some
	   applications will need the output. */
        krb5_free_enc_tkt_part(context, req->ticket->enc_part2);
	req->ticket->enc_part2 = NULL;
    }
    return retval;
}

krb5_error_code
krb5_rd_req_decoded(context, auth_context, req, server, keytab, 
		    ap_req_options, ticket)
    krb5_context 	  context;
    krb5_auth_context   * auth_context;
    const krb5_ap_req 	* req;
    krb5_const_principal  server;
    krb5_keytab           keytab;
    krb5_flags          * ap_req_options;
    krb5_ticket	       ** ticket;
{
  krb5_error_code retval;
  retval = krb5_rd_req_decoded_opt(context, auth_context,
				   req, server, keytab, 
				   ap_req_options, ticket,
				   1); /* check_valid_flag */
  return retval;
}

krb5_error_code
krb5_rd_req_decoded_anyflag(context, auth_context, req, server, keytab, 
			ap_req_options, ticket)
    krb5_context 	  context;
    krb5_auth_context   * auth_context;
    const krb5_ap_req 	* req;
    krb5_const_principal  server;
    krb5_keytab           keytab;
    krb5_flags          * ap_req_options;
    krb5_ticket	       ** ticket;
{
  krb5_error_code retval;
  retval = krb5_rd_req_decoded_opt(context, auth_context,
				   req, server, keytab, 
				   ap_req_options, ticket,
				   0); /* don't check_valid_flag */
  return retval;
}

static krb5_error_code
decrypt_authenticator(context, request, authpp, is_ap_req)
    krb5_context context;
    const krb5_ap_req *request;
    krb5_authenticator **authpp;
    int is_ap_req;
{
    krb5_authenticator *local_auth;
    krb5_error_code retval;
    krb5_data scratch;
    krb5_keyblock *sesskey;

    sesskey = request->ticket->enc_part2->session;

    scratch.length = request->authenticator.ciphertext.length;
    if (!(scratch.data = malloc(scratch.length)))
	return(ENOMEM);

    if ((retval = krb5_c_decrypt(context, sesskey,
				 is_ap_req?KRB5_KEYUSAGE_AP_REQ_AUTH:
				 KRB5_KEYUSAGE_TGS_REQ_AUTH, 0,
				 &request->authenticator, &scratch))) {
	free(scratch.data);
	return(retval);
    }

#define clean_scratch() {memset(scratch.data, 0, scratch.length); \
free(scratch.data);}

    /*  now decode the decrypted stuff */
    if (!(retval = decode_krb5_authenticator(&scratch, &local_auth))) {
	*authpp = local_auth;
    }
    clean_scratch();
    return retval;
}

