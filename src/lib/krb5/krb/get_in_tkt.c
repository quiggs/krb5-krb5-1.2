/*
 * lib/krb5/krb/get_in_tkt.c
 *
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
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 *
 * krb5_get_in_tkt()
 */

#include <string.h>

#include "k5-int.h"

/*
 All-purpose initial ticket routine, usually called via
 krb5_get_in_tkt_with_password or krb5_get_in_tkt_with_skey.

 Attempts to get an initial ticket for creds->client to use server
 creds->server, (realm is taken from creds->client), with options
 options, and using creds->times.starttime, creds->times.endtime,
 creds->times.renew_till as from, till, and rtime.  
 creds->times.renew_till is ignored unless the RENEWABLE option is requested.

 key_proc is called to fill in the key to be used for decryption.
 keyseed is passed on to key_proc.

 decrypt_proc is called to perform the decryption of the response (the
 encrypted part is in dec_rep->enc_part; the decrypted part should be
 allocated and filled into dec_rep->enc_part2
 arg is passed on to decrypt_proc.

 If addrs is non-NULL, it is used for the addresses requested.  If it is
 null, the system standard addresses are used.

 A succesful call will place the ticket in the credentials cache ccache
 and fill in creds with the ticket information used/returned..

 returns system errors, encryption errors

 */


/* some typedef's for the function args to make things look a bit cleaner */

typedef krb5_error_code (*git_key_proc) PROTOTYPE((krb5_context,
						   const krb5_enctype,
						   krb5_data *,
						   krb5_const_pointer,
						   krb5_keyblock **));

typedef krb5_error_code (*git_decrypt_proc) PROTOTYPE((krb5_context,
						       const krb5_keyblock *,
						       krb5_const_pointer,
						       krb5_kdc_rep * ));

static krb5_error_code make_preauth_list PROTOTYPE((krb5_context, 
						    krb5_preauthtype *,
						    int, krb5_pa_data ***));
/*
 * This function sends a request to the KDC, and gets back a response;
 * the response is parsed into ret_err_reply or ret_as_reply if the
 * reponse is a KRB_ERROR or a KRB_AS_REP packet.  If it is some other
 * unexpected response, an error is returned.
 */
static krb5_error_code
send_as_request(context, request, time_now, ret_err_reply, ret_as_reply,
		use_master)
    krb5_context 		context;
    krb5_kdc_req		*request;
    krb5_timestamp 		*time_now;
    krb5_error ** 		ret_err_reply;
    krb5_kdc_rep ** 		ret_as_reply;
    int 			use_master;
{
    krb5_kdc_rep *as_reply = 0;
    krb5_error_code retval;
    krb5_data *packet;
    krb5_data reply;
    char k4_version;		/* same type as *(krb5_data::data) */

    reply.data = 0;
    
    if ((retval = krb5_timeofday(context, time_now)))
	goto cleanup;

    /*
     * XXX we know they are the same size... and we should do
     * something better than just the current time
     */
    request->nonce = (krb5_int32) *time_now;

    /* encode & send to KDC */
    if ((retval = encode_krb5_as_req(request, &packet)) != 0)
	goto cleanup;

    k4_version = packet->data[0];
    retval = krb5_sendto_kdc(context, packet, 
			     krb5_princ_realm(context, request->client),
			     &reply, use_master);
    krb5_free_data(context, packet);
    if (retval)
	goto cleanup;

    /* now decode the reply...could be error or as_rep */
    if (krb5_is_krb_error(&reply)) {
	krb5_error *err_reply;

	if ((retval = decode_krb5_error(&reply, &err_reply)))
	    /* some other error code--??? */	    
	    goto cleanup;
    
	if (ret_err_reply)
	    *ret_err_reply = err_reply;
	else
	    krb5_free_error(context, err_reply);
	goto cleanup;
    }

    /*
     * Check to make sure it isn't a V4 reply.
     */
    if (!krb5_is_as_rep(&reply)) {
/* these are in <kerberosIV/prot.h> as well but it isn't worth including. */
#define V4_KRB_PROT_VERSION	4
#define V4_AUTH_MSG_ERR_REPLY	(5<<1)
	/* check here for V4 reply */
	unsigned int t_switch;

	/* From v4 g_in_tkt.c: This used to be
	   switch (pkt_msg_type(rpkt) & ~1) {
	   but SCO 3.2v4 cc compiled that incorrectly.  */
	t_switch = reply.data[1];
	t_switch &= ~1;

	if (t_switch == V4_AUTH_MSG_ERR_REPLY
	    && (reply.data[0] == V4_KRB_PROT_VERSION
		|| reply.data[0] == k4_version)) {
	    retval = KRB5KRB_AP_ERR_V4_REPLY;
	} else {
	    retval = KRB5KRB_AP_ERR_MSG_TYPE;
	}
	goto cleanup;
    }

    /* It must be a KRB_AS_REP message, or an bad returned packet */
    if ((retval = decode_krb5_as_rep(&reply, &as_reply)))
	/* some other error code ??? */
	goto cleanup;

    if (as_reply->msg_type != KRB5_AS_REP) {
	retval = KRB5KRB_AP_ERR_MSG_TYPE;
	krb5_free_kdc_rep(context, as_reply);
	goto cleanup;
    }

    if (ret_as_reply)
	*ret_as_reply = as_reply;
    else
	krb5_free_kdc_rep(context, as_reply);

cleanup:
    if (reply.data)
	free(reply.data);
    return retval;
}

static krb5_error_code
decrypt_as_reply(context, request, as_reply, key_proc, keyseed, key,
		 decrypt_proc, decryptarg)
    krb5_context 		context;
    krb5_kdc_req		*request;
    krb5_kdc_rep		*as_reply;
    git_key_proc 		key_proc;
    krb5_const_pointer 		keyseed;
    krb5_keyblock *		key;	
    git_decrypt_proc 		decrypt_proc;
    krb5_const_pointer 		decryptarg;
{
    krb5_error_code		retval;
    krb5_keyblock *		decrypt_key = 0;
    krb5_data 			salt;
    
    if (as_reply->enc_part2)
	return 0;

    if (key)
	    decrypt_key = key;
    else {
	if ((retval = krb5_principal2salt(context, request->client, &salt)))
	    return(retval);
    
	retval = (*key_proc)(context, as_reply->enc_part.enctype,
			     &salt, keyseed, &decrypt_key);
	krb5_xfree(salt.data);
	if (retval)
	    goto cleanup;
    }
    
    if ((retval = (*decrypt_proc)(context, decrypt_key, decryptarg, as_reply)))
	goto cleanup;

cleanup:
    if (!key && decrypt_key)
	krb5_free_keyblock(context, decrypt_key);
    return (retval);
}

static krb5_error_code
verify_as_reply(context, time_now, request, as_reply)
    krb5_context 		context;
    krb5_timestamp 		time_now;
    krb5_kdc_req		*request;
    krb5_kdc_rep		*as_reply;
{
    krb5_error_code		retval;
    
    /* check the contents for sanity: */
    if (!as_reply->enc_part2->times.starttime)
	as_reply->enc_part2->times.starttime =
	    as_reply->enc_part2->times.authtime;
    
    if (!krb5_principal_compare(context, as_reply->client, request->client)
	|| !krb5_principal_compare(context, as_reply->enc_part2->server, request->server)
	|| !krb5_principal_compare(context, as_reply->ticket->server, request->server)
	|| (request->nonce != as_reply->enc_part2->nonce)
	/* XXX check for extraneous flags */
	/* XXX || (!krb5_addresses_compare(context, addrs, as_reply->enc_part2->caddrs)) */
	|| ((request->kdc_options & KDC_OPT_POSTDATED) &&
	    (request->from != 0) &&
	    (request->from != as_reply->enc_part2->times.starttime))
	|| ((request->till != 0) &&
	    (as_reply->enc_part2->times.endtime > request->till))
	|| ((request->kdc_options & KDC_OPT_RENEWABLE) &&
	    (request->rtime != 0) &&
	    (as_reply->enc_part2->times.renew_till > request->rtime))
	|| ((request->kdc_options & KDC_OPT_RENEWABLE_OK) &&
	    (as_reply->enc_part2->flags & KDC_OPT_RENEWABLE) &&
	    (request->till != 0) &&
	    (as_reply->enc_part2->times.renew_till > request->till))
	)
	return KRB5_KDCREP_MODIFIED;

    if (context->library_options & KRB5_LIBOPT_SYNC_KDCTIME) {
	retval = krb5_set_real_time(context,
				    as_reply->enc_part2->times.authtime, 0);
	if (retval)
	    return retval;
    } else {
	if ((request->from == 0) &&
	    (labs(as_reply->enc_part2->times.starttime - time_now)
	     > context->clockskew))
	    return (KRB5_KDCREP_SKEW);
    }
    return 0;
}

static krb5_error_code
stash_as_reply(context, time_now, request, as_reply, creds, ccache)
    krb5_context 		context;
    krb5_timestamp 		time_now;
    krb5_kdc_req		*request;
    krb5_kdc_rep		*as_reply;
    krb5_creds * 		creds;
    krb5_ccache 		ccache;
{
    krb5_error_code 		retval;
    krb5_data *			packet;
    krb5_principal		client;
    krb5_principal		server;

    client = NULL;
    server = NULL;

    if (!creds->client)
	if (retval = krb5_copy_principal(context, as_reply->client, &client))
	    goto cleanup;

    if (!creds->server)
	if (retval = krb5_copy_principal(context, as_reply->enc_part2->server,
					 &server))
	    goto cleanup;

    /* fill in the credentials */
    if ((retval = krb5_copy_keyblock_contents(context, 
					      as_reply->enc_part2->session,
					      &creds->keyblock)))
	goto cleanup;

    creds->times = as_reply->enc_part2->times;
    creds->is_skey = FALSE;		/* this is an AS_REQ, so cannot
					   be encrypted in skey */
    creds->ticket_flags = as_reply->enc_part2->flags;
    if ((retval = krb5_copy_addresses(context, as_reply->enc_part2->caddrs,
				      &creds->addresses)))
	goto cleanup;

    creds->second_ticket.length = 0;
    creds->second_ticket.data = 0;

    if ((retval = encode_krb5_ticket(as_reply->ticket, &packet)))
	goto cleanup;

    creds->ticket = *packet;
    krb5_xfree(packet);

    /* store it in the ccache! */
    if (ccache)
	if ((retval = krb5_cc_store_cred(context, ccache, creds)))
	    goto cleanup;

    if (!creds->client)
	creds->client = client;
    if (!creds->server)
	creds->server = server;

cleanup:
    if (retval) {
	if (client)
	    krb5_free_principal(context, client);
	if (server)
	    krb5_free_principal(context, server);
	if (creds->keyblock.contents) {
	    memset((char *)creds->keyblock.contents, 0,
		   creds->keyblock.length);
	    krb5_xfree(creds->keyblock.contents);
	    creds->keyblock.contents = 0;
	    creds->keyblock.length = 0;
	}
	if (creds->ticket.data) {
	    krb5_xfree(creds->ticket.data);
	    creds->ticket.data = 0;
	}
	if (creds->addresses) {
	    krb5_free_addresses(context, creds->addresses);
	    creds->addresses = 0;
	}
    }
    return (retval);
}

static krb5_error_code
make_preauth_list(context, ptypes, nptypes, ret_list)
    krb5_context	context;
    krb5_preauthtype *	ptypes;
    int			nptypes;
    krb5_pa_data ***	ret_list;
{
    krb5_preauthtype *		ptypep;
    krb5_pa_data **		preauthp;
    krb5_pa_data **		preauth_to_use;
    int				i;

    if (nptypes < 0) {
 	for (nptypes=0, ptypep = ptypes; *ptypep; ptypep++, nptypes++)
 	    ;
    }
 
    /* allocate space for a NULL to terminate the list */
 
    if ((preauthp =
 	 (krb5_pa_data **) malloc((nptypes+1)*sizeof(krb5_pa_data *))) == NULL)
 	return(ENOMEM);
 
    for (i=0; i<nptypes; i++) {
 	if ((preauthp[i] =
 	     (krb5_pa_data *) malloc(sizeof(krb5_pa_data))) == NULL) {
 	    for (; i>=0; i++)
 		free(preauthp[i]);
 	    free(preauthp);
	    return (ENOMEM);
	}
 	preauthp[i]->magic = KV5M_PA_DATA;
 	preauthp[i]->pa_type = ptypes[i];
 	preauthp[i]->length = 0;
 	preauthp[i]->contents = 0;
    }
     
    /* fill in the terminating NULL */
 
    preauthp[nptypes] = NULL;
 
    *ret_list = preauthp;
    return 0;
}

#define MAX_IN_TKT_LOOPS 16

KRB5_DLLIMP krb5_error_code KRB5_CALLCONV
krb5_get_in_tkt(context, options, addrs, ktypes, ptypes, key_proc, keyseed,
		decrypt_proc, decryptarg, creds, ccache, ret_as_reply)
    krb5_context context;
    const krb5_flags options;
    krb5_address FAR * const FAR * addrs;
    krb5_enctype FAR * ktypes;
    krb5_preauthtype FAR * ptypes;
    git_key_proc key_proc;
    krb5_const_pointer keyseed;
    git_decrypt_proc decrypt_proc;
    krb5_const_pointer decryptarg;
    krb5_creds FAR * creds;
    krb5_ccache ccache;
    krb5_kdc_rep FAR * FAR * ret_as_reply;
{
    krb5_error_code	retval;
    krb5_timestamp	time_now;
    krb5_keyblock *	decrypt_key = 0;
    krb5_kdc_req	request;
    krb5_pa_data	**padata = 0;
    krb5_error *	err_reply;
    krb5_kdc_rep *	as_reply = 0;
    krb5_pa_data  **	preauth_to_use = 0;
    int			loopcount = 0;
    krb5_int32		do_more = 0;

    if (! krb5_realm_compare(context, creds->client, creds->server))
	return KRB5_IN_TKT_REALM_MISMATCH;

    if (ret_as_reply)
	*ret_as_reply = 0;
    
    /*
     * Set up the basic request structure
     */
    request.magic = KV5M_KDC_REQ;
    request.msg_type = KRB5_AS_REQ;
    request.addresses = 0;
    request.ktype = 0;
    request.padata = 0;
    if (addrs)
	request.addresses = (krb5_address **) addrs;
    else
	if ((retval = krb5_os_localaddr(context, &request.addresses)))
	    goto cleanup;
    request.kdc_options = options;
    request.client = creds->client;
    request.server = creds->server;
    request.from = creds->times.starttime;
    request.till = creds->times.endtime;
    request.rtime = creds->times.renew_till;
    if ((retval = krb5_get_default_in_tkt_ktypes(context, &request.ktype)))
	goto cleanup;
    for (request.nktypes = 0;request.ktype[request.nktypes];request.nktypes++);
    if (ktypes) {
	int i, req, next = 0;
	for (req = 0; ktypes[req]; req++) {
	    if (ktypes[req] == request.ktype[next]) {
		next++;
		continue;
	    }
	    for (i = next + 1; i < request.nktypes; i++)
		if (ktypes[req] == request.ktype[i]) {
		    /* Found the enctype we want, but not in the
		       position we want.  Move it, but keep the old
		       one from the desired slot around in case it's
		       later in our requested-ktypes list.  */
		    krb5_enctype t;
		    t = request.ktype[next];
		    request.ktype[next] = request.ktype[i];
		    request.ktype[i] = t;
		    next++;
		    break;
		}
	    /* If we didn't find it, don't do anything special, just
	       drop it.  */
	}
	request.ktype[next] = 0;
	request.nktypes = next;
    }
    request.authorization_data.ciphertext.length = 0;
    request.authorization_data.ciphertext.data = 0;
    request.unenc_authdata = 0;
    request.second_ticket = 0;

    /*
     * If a list of preauth types are passed in, convert it to a
     * preauth_to_use list.
     */
    if (ptypes) {
	retval = make_preauth_list(context, ptypes, -1, &preauth_to_use);
	if (retval)
	    goto cleanup;
    }
	    
    while (1) {
	if (loopcount++ > MAX_IN_TKT_LOOPS) {
	    retval = KRB5_GET_IN_TKT_LOOP;
	    goto cleanup;
	}

	if ((retval = krb5_obtain_padata(context, preauth_to_use, key_proc,
					 keyseed, creds, &request)) != 0)
	    goto cleanup;
	if (preauth_to_use)
	    krb5_free_pa_data(context, preauth_to_use);
	preauth_to_use = 0;
	
	err_reply = 0;
	as_reply = 0;
	if ((retval = send_as_request(context, &request, &time_now, &err_reply,
				      &as_reply, NULL)))
	    goto cleanup;

	if (err_reply) {
	    if (err_reply->error == KDC_ERR_PREAUTH_REQUIRED &&
		err_reply->e_data.length > 0) {
		retval = decode_krb5_padata_sequence(&err_reply->e_data,
						     &preauth_to_use);
		krb5_free_error(context, err_reply);
		if (retval)
		    goto cleanup;
		continue;
	    } else {
		retval = err_reply->error + ERROR_TABLE_BASE_krb5;
		krb5_free_error(context, err_reply);
		goto cleanup;
	    }
	} else if (!as_reply) {
	    retval = KRB5KRB_AP_ERR_MSG_TYPE;
	    goto cleanup;
	}
	if ((retval = krb5_process_padata(context, &request, as_reply,
					  key_proc, keyseed, decrypt_proc, 
					  &decrypt_key, creds,
					  &do_more)) != 0)
	    goto cleanup;

	if (!do_more)
	    break;
    }
    
    if ((retval = decrypt_as_reply(context, &request, as_reply, key_proc,
				   keyseed, decrypt_key, decrypt_proc,
				   decryptarg)))
	goto cleanup;

    if ((retval = verify_as_reply(context, time_now, &request, as_reply)))
	goto cleanup;

    if ((retval = stash_as_reply(context, time_now, &request, as_reply,
				 creds, ccache)))
	goto cleanup;

cleanup:
    if (request.ktype)
	free(request.ktype);
    if (!addrs && request.addresses)
	krb5_free_addresses(context, request.addresses);
    if (request.padata)
	krb5_free_pa_data(context, request.padata);
    if (padata)
	krb5_free_pa_data(context, padata);
    if (preauth_to_use)
	krb5_free_pa_data(context, preauth_to_use);
    if (decrypt_key)
    	krb5_free_keyblock(context, decrypt_key);
    if (as_reply) {
	if (ret_as_reply)
	    *ret_as_reply = as_reply;
	else
	    krb5_free_kdc_rep(context, as_reply);
    }
    return (retval);
}

/* begin libdefaults parsing code.  This should almost certainly move
   somewhere else, but I don't know where the correct somewhere else
   is yet. */

/* XXX Duplicating this is annoying; try to work on a better way.*/
static char *conf_yes[] = {
    "y", "yes", "true", "t", "1", "on",
    0,
};

static char *conf_no[] = {
    "n", "no", "false", "nil", "0", "off",
    0,
};

int
_krb5_conf_boolean(s)
     char *s;
{
    char **p;

    for(p=conf_yes; *p; p++) {
	if (!strcasecmp(*p,s))
	    return 1;
    }

    for(p=conf_no; *p; p++) {
	if (!strcasecmp(*p,s))
	    return 0;
    }

    /* Default to "no" */
    return 0;
}

static krb5_error_code
krb5_libdefault_string(context, realm, option, ret_value)
     krb5_context context;
     const krb5_data *realm;
     const char *option;
     char **ret_value;
{
    profile_t profile;
    const char *names[5];
    char **nameval = NULL;
    krb5_error_code retval;
    char realmstr[1024];
    char **cpp;

    if (realm->length > sizeof(realmstr)-1)
	return(EINVAL);

    strncpy(realmstr, realm->data, realm->length);
    realmstr[realm->length] = '\0';

    if (!context || (context->magic != KV5M_CONTEXT)) 
	return KV5M_CONTEXT;

    profile = context->profile;
	    
    names[0] = "libdefaults";

    /*
     * Try number one:
     *
     * [libdefaults]
     *		REALM = {
     *			option = <boolean>
     *		}
     */

    names[1] = realmstr;
    names[2] = option;
    names[3] = 0;
    retval = profile_get_values(profile, names, &nameval);
    if (retval == 0 && nameval && nameval[0])
	goto goodbye;

    /*
     * Try number two:
     *
     * [libdefaults]
     *		option = <boolean>
     */
    
    names[1] = option;
    names[2] = 0;
    retval = profile_get_values(profile, names, &nameval);
    if (retval == 0 && nameval && nameval[0])
	goto goodbye;

goodbye:
    if (!nameval) 
	return(ENOENT);

    if (!nameval[0]) {
        retval = ENOENT;
    } else {
        *ret_value = malloc(strlen(nameval[0]) + 1);
        if (!*ret_value)
            retval = ENOMEM;
        else
            strcpy(*ret_value, nameval[0]);
    }

    profile_free_list(nameval);

    return retval;
}

/* not static so verify_init_creds() can call it */
/* as well as the DNS code */

krb5_error_code
krb5_libdefault_boolean(context, realm, option, ret_value)
     krb5_context context;
     const char *option;
     const krb5_data *realm;
     int *ret_value;
{
    char *string = NULL;
    krb5_error_code retval;

    retval = krb5_libdefault_string(context, realm, option, &string);

    if (retval)
	return(retval);

    *ret_value = _krb5_conf_boolean(string);
    free(string);

    return(0);
}

KRB5_DLLIMP krb5_error_code KRB5_CALLCONV
krb5_get_init_creds(context, creds, client, prompter, prompter_data,
		    start_time, in_tkt_service, options, gak_fct, gak_data,
		    use_master, as_reply)
     krb5_context context;
     krb5_creds *creds;
     krb5_principal client;
     krb5_prompter_fct prompter;
     void *prompter_data;
     krb5_deltat start_time;
     char *in_tkt_service;
     krb5_get_init_creds_opt *options;
     krb5_gic_get_as_key_fct gak_fct;
     void *gak_data;
     int  use_master;
     krb5_kdc_rep **as_reply;
{
    krb5_error_code ret;
    krb5_kdc_req request;
    krb5_pa_data **padata;
    int tempint;
    char *tempstr;
    krb5_deltat renew_life;
    int loopcount;
    krb5_data salt;
    krb5_keyblock as_key;
    krb5_error *err_reply;
    krb5_kdc_rep *local_as_reply;
    krb5_timestamp time_now;
    krb5_enctype etype = 0;

    /* initialize everything which will be freed at cleanup */

    request.server = NULL;
    request.ktype = NULL;
    request.addresses = NULL;
    request.padata = NULL;
    padata = NULL;
    as_key.length = 0;
    salt.length = 0;
    salt.data = NULL;

	local_as_reply = 0;

    /*
     * Set up the basic request structure
     */
    request.magic = KV5M_KDC_REQ;
    request.msg_type = KRB5_AS_REQ;

    /* request.padata is filled in later */

    request.kdc_options = 0;

    /* forwardable */

    if (options && (options->flags & KRB5_GET_INIT_CREDS_OPT_FORWARDABLE))
	tempint = options->forwardable;
    else if ((ret = krb5_libdefault_boolean(context, &client->realm,
					    "forwardable", &tempint)) == 0)
	    ;
    else
	tempint = 0;
    if (tempint)
	request.kdc_options |= KDC_OPT_FORWARDABLE;

    /* proxiable */

    if (options && (options->flags & KRB5_GET_INIT_CREDS_OPT_PROXIABLE))
	tempint = options->proxiable;
    else if ((ret = krb5_libdefault_boolean(context, &client->realm,
					    "proxiable", &tempint)) == 0)
	    ;
    else
	tempint = 0;
    if (tempint)
	request.kdc_options |= KDC_OPT_PROXIABLE;

    /* renewable */

    if (options && (options->flags & KRB5_GET_INIT_CREDS_OPT_RENEW_LIFE)) {
	renew_life = options->renew_life;
    } else if ((ret = krb5_libdefault_string(context, &client->realm,
					     "renew_lifetime", &tempstr))
	       == 0) {
	if (ret = krb5_string_to_deltat(tempstr, &renew_life)) {
	    free(tempstr);
	    goto cleanup;
	}
    } else {
	renew_life = 0;
    }
    if (renew_life > 0)
	request.kdc_options |= KDC_OPT_RENEWABLE;

    /* allow_postdate */

    if (start_time > 0)
	request.kdc_options |= (KDC_OPT_ALLOW_POSTDATE|KDC_OPT_POSTDATED);

    /* client */

    request.client = client;

    if (in_tkt_service) {
	/* this is ugly, because so are the data structures involved.  I'm
	   in the library, so I'm going to manipulate the data structures
	   directly, otherwise, it will be worse. */

	if (ret = krb5_parse_name(context, in_tkt_service, &request.server))
	    goto cleanup;

	/* stuff the client realm into the server principal.
	   realloc if necessary */
	if (request.server->realm.length < request.client->realm.length)
	    if ((request.server->realm.data =
		 (char *) realloc(request.server->realm.data,
				  request.client->realm.length)) == NULL) {
		ret = ENOMEM;
		goto cleanup;
	    }

	request.server->realm.length = request.client->realm.length;
	memcpy(request.server->realm.data, request.client->realm.data,
	       request.client->realm.length);
    } else {
	if (ret = krb5_build_principal_ext(context, &request.server,
					   request.client->realm.length,
					   request.client->realm.data,
					   KRB5_TGS_NAME_SIZE,
					   KRB5_TGS_NAME,
					   request.client->realm.length,
					   request.client->realm.data,
					   0))
	    goto cleanup;
    }

    if (ret = krb5_timeofday(context, &request.from))
	goto cleanup;
    request.from += start_time;

    request.till = request.from;
    if (options && (options->flags & KRB5_GET_INIT_CREDS_OPT_TKT_LIFE))
	request.till += options->tkt_life;
    else
	request.till += 10*60*60; /* this used to be hardcoded in kinit.c */

    if (renew_life > 0) {
	request.rtime = request.from;
	request.rtime += renew_life;
    } else {
	request.rtime = 0;
    }

    /* nonce is filled in by send_as_request */

    if (options && (options->flags & KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST)) {
	request.ktype = options->etype_list;
	request.nktypes = options->etype_list_length;
    } else if ((ret = krb5_get_default_in_tkt_ktypes(context,
						     &request.ktype)) == 0) {
	for (request.nktypes = 0;
	     request.ktype[request.nktypes];
	     request.nktypes++)
	    ;
    } else {
	/* there isn't any useful default here.  ret is set from above */
	goto cleanup;
    }

    if (options && (options->flags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST)) {
	request.addresses = options->address_list;
    }
    /* it would be nice if this parsed out an address list, but
       that would be work. */
    else if (((ret = krb5_libdefault_boolean(context, &client->realm,
					    "noaddresses", &tempint)) == 0)
	     && tempint) {
	    ;
    } else {
	if ((ret = krb5_os_localaddr(context, &request.addresses)))
	    goto cleanup;
    }

    request.authorization_data.ciphertext.length = 0;
    request.authorization_data.ciphertext.data = 0;
    request.unenc_authdata = 0;
    request.second_ticket = 0;

    /* set up the other state.  */

    if (options && (options->flags & KRB5_GET_INIT_CREDS_OPT_PREAUTH_LIST)) {
	if (ret = make_preauth_list(context, options->preauth_list,
				    options->preauth_list_length, 
				    &padata))
	    goto cleanup;
    }

    /* the salt is allocated from somewhere, unless it is from the caller,
       then it is a reference */

    if (options && (options->flags & KRB5_GET_INIT_CREDS_OPT_SALT)) {
	salt = *options->salt;
    } else {
	salt.length = -1;
	salt.data = NULL;
    }

    /* now, loop processing preauth data and talking to the kdc */

    for (loopcount = 0; loopcount < MAX_IN_TKT_LOOPS; loopcount++) {
	if (request.padata) {
	    krb5_free_pa_data(context, request.padata);
	    request.padata = NULL;
	}

	if (ret = krb5_do_preauth(context, &request,
				  padata, &request.padata,
				  &salt, &etype, &as_key, prompter,
				  prompter_data, gak_fct, gak_data))
	    goto cleanup;

	if (padata) {
	    krb5_free_pa_data(context, padata);
	    padata = 0;
	}

	err_reply = 0;
	local_as_reply = 0;
	if ((ret = send_as_request(context, &request, &time_now, &err_reply,
				   &local_as_reply, use_master)))
	    goto cleanup;

	if (err_reply) {
	    if (err_reply->error == KDC_ERR_PREAUTH_REQUIRED &&
		err_reply->e_data.length > 0) {
		ret = decode_krb5_padata_sequence(&err_reply->e_data,
						  &padata);
		krb5_free_error(context, err_reply);
		if (ret)
		    goto cleanup;
	    } else {
		ret = err_reply->error + ERROR_TABLE_BASE_krb5;
		krb5_free_error(context, err_reply);
		goto cleanup;
	    }
	} else if (local_as_reply) {
	    break;
	} else {
	    ret = KRB5KRB_AP_ERR_MSG_TYPE;
	    goto cleanup;
	}
    }

    if (loopcount == MAX_IN_TKT_LOOPS) {
	ret = KRB5_GET_IN_TKT_LOOP;
	goto cleanup;
    }

    /* process any preauth data in the as_reply */

    if (ret = krb5_do_preauth(context, &request,
			      local_as_reply->padata, &padata,
			      &salt, &etype, &as_key, prompter,
			      prompter_data, gak_fct, gak_data))
	goto cleanup;

    /* XXX if there's padata on output, something is wrong, but it's
       not obviously an error */

    /* XXX For 1.1.1 and prior KDC's, when SAM is used w/ USE_SAD_AS_KEY,
       the AS_REP comes back encrypted in the user's longterm key
       instead of in the SAD. If there was a SAM preauth, there
       will be an as_key here which will be the SAD. If that fails,
       use the gak_fct to get the password, and try again. */
      
    /* XXX because etypes are handled poorly (particularly wrt SAM,
       where the etype is fixed by the kdc), we may want to try
       decrypt_as_reply twice.  If there's an as_key available, try
       it.  If decrypting the as_rep fails, or if there isn't an
       as_key at all yet, then use the gak_fct to get one, and try
       again.  */

    if (as_key.length)
	ret = decrypt_as_reply(context, NULL, local_as_reply, NULL,
			       NULL, &as_key, krb5_kdc_rep_decrypt_proc,
			       NULL);
    else
	ret = -1;
	   
    if (ret) {
	/* if we haven't get gotten a key, get it now */

	if (ret = ((*gak_fct)(context, request.client,
			      local_as_reply->enc_part.enctype,
			      prompter, prompter_data, &salt,
			      &as_key, gak_data)))
	    goto cleanup;

	if (ret = decrypt_as_reply(context, NULL, local_as_reply, NULL,
				   NULL, &as_key, krb5_kdc_rep_decrypt_proc,
				   NULL))
	    goto cleanup;
    }

    if (ret = verify_as_reply(context, time_now, &request, local_as_reply))
	goto cleanup;

    /* XXX this should be inside stash_as_reply, but as long as
       get_in_tkt is still around using that arg as an in/out, I can't
       do that */
    memset(creds, 0, sizeof(*creds));

    if (ret = stash_as_reply(context, time_now, &request, local_as_reply,
			     creds, NULL))
	goto cleanup;

    /* success */

    ret = 0;

cleanup:
    if (request.server)
	krb5_free_principal(context, request.server);
    if (request.ktype &&
	(!(options && (options->flags & KRB5_GET_INIT_CREDS_OPT_ETYPE_LIST))))
	free(request.ktype);
    if (request.addresses &&
	(!(options &&
	   (options->flags & KRB5_GET_INIT_CREDS_OPT_ADDRESS_LIST))))
	krb5_free_addresses(context, request.addresses);
    if (padata)
	krb5_free_pa_data(context, padata);
    if (request.padata)
	krb5_free_pa_data(context, request.padata);
    if (as_key.length)
	krb5_free_keyblock_contents(context, &as_key);
    if (salt.data &&
	(!(options && (options->flags & KRB5_GET_INIT_CREDS_OPT_SALT))))
	krb5_xfree(salt.data);
    if (as_reply)
	*as_reply = local_as_reply;
    else if (local_as_reply)
	krb5_free_kdc_rep(context, local_as_reply);

    return(ret);
}
