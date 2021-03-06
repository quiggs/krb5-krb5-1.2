/*
 * lib/krb5/krb/gc_via_tgt.c
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
 * Given a tkt, and a target cred, get it.
 * Assumes that the kdc_rep has been decrypted.
 */

#include "k5-int.h"
#include "int-proto.h"

#define in_clock_skew(date, now) (labs((date)-(now)) < context->clockskew)

static krb5_error_code
krb5_kdcrep2creds(context, pkdcrep, address, psectkt, ppcreds)
    krb5_context          context;
    krb5_kdc_rep        * pkdcrep;
    krb5_address *const * address;
    krb5_data		* psectkt;
    krb5_creds         ** ppcreds;
{
    krb5_error_code retval;  
    krb5_data *pdata;
  
    if ((*ppcreds = (krb5_creds *)malloc(sizeof(krb5_creds))) == NULL) {
        return ENOMEM;
    }

    memset(*ppcreds, 0, sizeof(krb5_creds));

    if ((retval = krb5_copy_principal(context, pkdcrep->client,
                                     &(*ppcreds)->client)))
        goto cleanup;

    if ((retval = krb5_copy_principal(context, pkdcrep->enc_part2->server,
                                     &(*ppcreds)->server)))
        goto cleanup;

    if ((retval = krb5_copy_keyblock_contents(context, 
					      pkdcrep->enc_part2->session,
					      &(*ppcreds)->keyblock)))
        goto cleanup;

    if ((retval = krb5_copy_data(context, psectkt, &pdata)))
	goto cleanup;
    (*ppcreds)->second_ticket = *pdata;
    krb5_xfree(pdata);

    (*ppcreds)->ticket_flags = pkdcrep->enc_part2->flags;
    (*ppcreds)->times = pkdcrep->enc_part2->times;
    (*ppcreds)->magic = KV5M_CREDS;

    (*ppcreds)->authdata = NULL;   			/* not used */
    (*ppcreds)->is_skey = psectkt->length != 0;

    if (pkdcrep->enc_part2->caddrs) {
	if ((retval = krb5_copy_addresses(context, pkdcrep->enc_part2->caddrs,
					  &(*ppcreds)->addresses)))
	    goto cleanup_keyblock;
    } else {
	/* no addresses in the list means we got what we had */
	if ((retval = krb5_copy_addresses(context, address,
					  &(*ppcreds)->addresses)))
	    goto cleanup_keyblock;
    }

    if ((retval = encode_krb5_ticket(pkdcrep->ticket, &pdata)))
	goto cleanup_keyblock;

    (*ppcreds)->ticket = *pdata;
    free(pdata);
    return 0;

cleanup_keyblock:
    krb5_free_keyblock(context, &(*ppcreds)->keyblock);

cleanup:
    free (*ppcreds);
    return retval;
}
 
krb5_error_code
krb5_get_cred_via_tkt (context, tkt, kdcoptions, address, in_cred, out_cred)
    krb5_context 	  context;
    krb5_creds 		* tkt;
    const krb5_flags 	  kdcoptions;
    krb5_address *const * address;
    krb5_creds 		* in_cred;
    krb5_creds 	       ** out_cred;
{
    krb5_error_code retval;
    krb5_kdc_rep *dec_rep;
    krb5_error *err_reply;
    krb5_response tgsrep;
    krb5_enctype *enctypes = 0;

    /* tkt->client must be equal to in_cred->client */
    if (!krb5_principal_compare(context, tkt->client, in_cred->client))
	return KRB5_PRINC_NOMATCH;

    if (!tkt->ticket.length)
	return KRB5_NO_TKT_SUPPLIED;

    if ((kdcoptions & KDC_OPT_ENC_TKT_IN_SKEY) && 
	(!in_cred->second_ticket.length))
        return(KRB5_NO_2ND_TKT);


    /* check if we have the right TGT                    */
    /* tkt->server must be equal to                      */
    /* krbtgt/realmof(cred->server)@realmof(tgt->server) */
/*
    {
    krb5_principal tempprinc;
        if (retval = krb5_tgtname(context, 
		     krb5_princ_realm(context, in_cred->server),
		     krb5_princ_realm(context, tkt->server), &tempprinc))
    	    return(retval);

        if (!krb5_principal_compare(context, tempprinc, tkt->server)) {
            krb5_free_principal(context, tempprinc);
	    return (KRB5_PRINC_NOMATCH);
        }
    krb5_free_principal(context, tempprinc);
    }
*/

    if (in_cred->keyblock.enctype) {
	enctypes = (krb5_enctype *) malloc(sizeof(krb5_enctype)*2);
	if (!enctypes)
	    return ENOMEM;
	enctypes[0] = in_cred->keyblock.enctype;
	enctypes[1] = 0;
    }
    
    retval = krb5_send_tgs(context, kdcoptions, &in_cred->times, enctypes, 
			   in_cred->server, address, in_cred->authdata,
			   0,		/* no padata */
			   (kdcoptions & KDC_OPT_ENC_TKT_IN_SKEY) ? 
			   &in_cred->second_ticket : NULL,
			   tkt, &tgsrep);
    if (enctypes)
	free(enctypes);
    if (retval)
	return retval;

    switch (tgsrep.message_type) {
    case KRB5_TGS_REP:
	break;
    case KRB5_ERROR:
    default:
	if (krb5_is_krb_error(&tgsrep.response))
	    retval = decode_krb5_error(&tgsrep.response, &err_reply);
	else
	    retval = KRB5KRB_AP_ERR_MSG_TYPE;

	if (retval) 			/* neither proper reply nor error! */
	    goto error_4;

	retval = err_reply->error + ERROR_TABLE_BASE_krb5;

	krb5_free_error(context, err_reply);
	goto error_4;
    }

    if ((retval = krb5_decode_kdc_rep(context, &tgsrep.response,
				      &tkt->keyblock, &dec_rep)))
	goto error_4;

    if (dec_rep->msg_type != KRB5_TGS_REP) {
	retval = KRB5KRB_AP_ERR_MSG_TYPE;
	goto error_3;
    }
   
    /* make sure the response hasn't been tampered with..... */
    retval = 0;

    if (!krb5_principal_compare(context, dec_rep->client, tkt->client))
	retval = KRB5_KDCREP_MODIFIED;

    if (!krb5_principal_compare(context, dec_rep->enc_part2->server, in_cred->server))
	retval = KRB5_KDCREP_MODIFIED;

    if (!krb5_principal_compare(context, dec_rep->ticket->server, in_cred->server))
	retval = KRB5_KDCREP_MODIFIED;

    if (dec_rep->enc_part2->nonce != tgsrep.expected_nonce)
	retval = KRB5_KDCREP_MODIFIED;

    if ((kdcoptions & KDC_OPT_POSTDATED) &&
	(in_cred->times.starttime != 0) &&
    	(in_cred->times.starttime != dec_rep->enc_part2->times.starttime))
	retval = KRB5_KDCREP_MODIFIED;

    if ((in_cred->times.endtime != 0) &&
	(dec_rep->enc_part2->times.endtime > in_cred->times.endtime))
	retval = KRB5_KDCREP_MODIFIED;

    if ((kdcoptions & KDC_OPT_RENEWABLE) &&
	(in_cred->times.renew_till != 0) &&
	(dec_rep->enc_part2->times.renew_till > in_cred->times.renew_till))
	retval = KRB5_KDCREP_MODIFIED;

    if ((kdcoptions & KDC_OPT_RENEWABLE_OK) &&
	(dec_rep->enc_part2->flags & KDC_OPT_RENEWABLE) &&
	(in_cred->times.endtime != 0) &&
	(dec_rep->enc_part2->times.renew_till > in_cred->times.endtime))
 	retval = KRB5_KDCREP_MODIFIED;

    if (retval != 0)
    	goto error_3;

    if (!in_cred->times.starttime &&
	!in_clock_skew(dec_rep->enc_part2->times.starttime,
		       tgsrep.request_time)) {
	retval = KRB5_KDCREP_SKEW;
	goto error_3;
    }
    
    retval = krb5_kdcrep2creds(context, dec_rep, address, 
			       &in_cred->second_ticket,  out_cred);

error_3:;
    memset(dec_rep->enc_part2->session->contents, 0,
	   dec_rep->enc_part2->session->length);
    krb5_free_kdc_rep(context, dec_rep);

error_4:;
    free(tgsrep.response.data);
    return retval;
}
