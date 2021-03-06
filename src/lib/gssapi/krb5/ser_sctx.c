/*
 * lib/gssapi/krb5/ser_sctx.c
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
 *
 */

/*
 * ser_sctx.c - Handle [de]serialization of GSSAPI security context.
 */
#include "k5-int.h"
#include "gssapiP_krb5.h"

/*
 * This module contains routines to [de]serialize 
 *	krb5_gss_enc_desc and krb5_gss_ctx_id_t.
 * XXX This whole serialization abstraction is unnecessary in a
 * non-messaging environment, which krb5 is.  Someday, this should
 * all get redone without the extra level of indirection. I've done
 * some of this work here, since adding new serializers is an internal
 * krb5 interface, and I won't use those.  There is some more
 * deobfuscation (no longer anonymizing pointers, mostly) which could
 * still be done. --marc
 */

static krb5_error_code
kg_oid_externalize(kcontext, arg, buffer, lenremain)
    krb5_context	kcontext;
    krb5_pointer	arg;
    krb5_octet		**buffer;
    size_t		*lenremain;
{
     gss_OID oid = (gss_OID) arg;
     
     (void) krb5_ser_pack_int32(KV5M_GSS_OID, buffer, lenremain);
     (void) krb5_ser_pack_int32((krb5_int32) oid->length,
				buffer, lenremain);
     (void) krb5_ser_pack_bytes((krb5_octet *) oid->elements,
				oid->length, buffer, lenremain);
     (void) krb5_ser_pack_int32(KV5M_GSS_OID, buffer, lenremain);
     return 0;
}

static krb5_error_code
kg_oid_internalize(kcontext, argp, buffer, lenremain)
    krb5_context	kcontext;
    krb5_pointer	*argp;
    krb5_octet		**buffer;
    size_t		*lenremain;
{
     krb5_error_code	kret;
     gss_OID oid;
     krb5_int32 ibuf;
     krb5_octet		*bp;
     size_t		remain;

     bp = *buffer;
     remain = *lenremain;

     /* Read in and check our magic number */
     if ((kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain)))
	return (EINVAL);

     if (ibuf != KV5M_GSS_OID)
	 return (EINVAL);

     oid = (gss_OID) malloc(sizeof(gss_OID_desc));
     if (oid == NULL)
	  return ENOMEM;
     (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
     oid->length = ibuf;
     oid->elements = malloc(ibuf);
     if (oid->elements == 0) {
	     free(oid);
	     return ENOMEM;
     }
     (void) krb5_ser_unpack_bytes((krb5_octet *) oid->elements,
				  oid->length, &bp, &remain);
     
     /* Read in and check our trailing magic number */
     if ((kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain)))
	return (EINVAL);

     if (ibuf != KV5M_GSS_OID)
	 return (EINVAL);

     *buffer = bp;
     *lenremain = remain;
     *argp = (krb5_pointer) oid;
     return 0;
}

krb5_error_code
kg_oid_size(kcontext, arg, sizep)
    krb5_context	kcontext;
    krb5_pointer	arg;
    size_t		*sizep;
{
   krb5_error_code kret;
   gss_OID oid;
   size_t required;

   kret = EINVAL;
   if ((oid = (gss_OID) arg)) {
      required = 2*sizeof(krb5_int32); /* For the header and trailer */
      required += sizeof(krb5_int32);
      required += oid->length;

      kret = 0;

      *sizep += required;
   }

   return(kret);
}

static krb5_error_code
kg_queue_externalize(kcontext, arg, buffer, lenremain)
    krb5_context	kcontext;
    krb5_pointer	arg;
    krb5_octet		**buffer;
    size_t		*lenremain;
{
     (void) krb5_ser_pack_int32(KV5M_GSS_QUEUE, buffer, lenremain);
     g_queue_externalize(arg, buffer, lenremain);
     (void) krb5_ser_pack_int32(KV5M_GSS_QUEUE, buffer, lenremain);
     return 0;
}

static krb5_error_code
kg_queue_internalize(kcontext, argp, buffer, lenremain)
    krb5_context	kcontext;
    krb5_pointer	*argp;
    krb5_octet		**buffer;
    size_t		*lenremain;
{
     krb5_error_code	kret;
     gss_OID oid;
     krb5_int32 ibuf;
     krb5_octet		*bp;
     size_t		remain;

     bp = *buffer;
     remain = *lenremain;

     /* Read in and check our magic number */
     if ((kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain)))
	return (EINVAL);

     if (ibuf != KV5M_GSS_QUEUE)
	 return (EINVAL);

     g_queue_internalize(argp, &bp, &remain);

     /* Read in and check our trailing magic number */
     if ((kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain)))
	return (EINVAL);

     if (ibuf != KV5M_GSS_QUEUE)
	 return (EINVAL);

     *buffer = bp;
     *lenremain = remain;
     return 0;
}

krb5_error_code
kg_queue_size(kcontext, arg, sizep)
    krb5_context	kcontext;
    krb5_pointer	arg;
    size_t		*sizep;
{
   krb5_error_code kret;
   size_t required;

   kret = EINVAL;
   if (arg) {
      required = 2*sizeof(krb5_int32); /* For the header and trailer */
      g_queue_size(arg, &required);

      kret = 0;
      *sizep += required;
   }
   return(kret);
}

/*
 * Determine the size required for this krb5_gss_ctx_id_rec.
 */
krb5_error_code
kg_ctx_size(kcontext, arg, sizep)
    krb5_context	kcontext;
    krb5_pointer	arg;
    size_t		*sizep;
{
    krb5_error_code	kret;
    krb5_gss_ctx_id_rec	*ctx;
    size_t		required;

    /*
     * krb5_gss_ctx_id_rec requires:
     *	krb5_int32	for KG_CONTEXT
     *	krb5_int32	for initiate.
     *	krb5_int32	for mutual.
     *	krb5_int32	for seed_init.
     *	sizeof(seed)	for seed
     *  krb5_int32	for signalg.
     *  krb5_int32	for cksum_size.
     *  krb5_int32	for sealalg.
     *	krb5_int32	for endtime.
     *	krb5_int32	for flags.
     *	krb5_int32	for seq_send.
     *	krb5_int32	for seq_recv.
     *	krb5_int32	for established.
     *	krb5_int32	for big_endian.
     *	krb5_int32	for nctypes.
     *	krb5_int32	for trailer.
     */
    kret = EINVAL;
    if ((ctx = (krb5_gss_ctx_id_rec *) arg)) {
	required = 16*sizeof(krb5_int32);
	required += sizeof(ctx->seed);
	required += ctx->nctypes*sizeof(krb5_int32);

	kret = 0;
	if (!kret && ctx->here)
	    kret = krb5_size_opaque(kcontext,
				    KV5M_PRINCIPAL,
				    (krb5_pointer) ctx->here,
				    &required);

	if (!kret && ctx->there)
	    kret = krb5_size_opaque(kcontext,
				    KV5M_PRINCIPAL,
				    (krb5_pointer) ctx->there,
				    &required);

	if (!kret && ctx->subkey)
	    kret = krb5_size_opaque(kcontext,
				    KV5M_KEYBLOCK,
				    (krb5_pointer) ctx->subkey,
				    &required);

	if (!kret && ctx->enc)
	    kret = krb5_size_opaque(kcontext,
				    KV5M_KEYBLOCK,
				    (krb5_pointer) ctx->enc,
				    &required);

	if (!kret && ctx->seq)
	    kret = krb5_size_opaque(kcontext,
				    KV5M_KEYBLOCK,
				    (krb5_pointer) ctx->seq,
				    &required);

	if (!kret)
	    kret = kg_oid_size(kcontext,
			       (krb5_pointer) ctx->mech_used,
			       &required);

	if (!kret && ctx->seqstate)
	    kret = kg_queue_size(kcontext, ctx->seqstate, &required);

	if (!kret)
	    kret = krb5_size_opaque(kcontext,
				    KV5M_AUTH_CONTEXT,
				    (krb5_pointer) ctx->auth_context,
				    &required);
	if (!kret)
	    *sizep += required;
    }
    return(kret);
}

/*
 * Externalize this krb5_gss_ctx_id_ret.
 */
krb5_error_code
kg_ctx_externalize(kcontext, arg, buffer, lenremain)
    krb5_context	kcontext;
    krb5_pointer	arg;
    krb5_octet		**buffer;
    size_t		*lenremain;
{
    krb5_error_code	kret;
    krb5_gss_ctx_id_rec	*ctx;
    size_t		required;
    krb5_octet		*bp;
    size_t		remain;
    int			i;

    required = 0;
    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    if ((ctx = (krb5_gss_ctx_id_rec *) arg)) {
	kret = ENOMEM;
	if (!kg_ctx_size(kcontext, arg, &required) &&
	    (required <= remain)) {
	    /* Our identifier */
	    (void) krb5_ser_pack_int32(KG_CONTEXT, &bp, &remain);

	    /* Now static data */
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->initiate,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->gss_flags,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->seed_init,
				       &bp, &remain);
	    (void) krb5_ser_pack_bytes((krb5_octet *) ctx->seed,
				       sizeof(ctx->seed),
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->signalg,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->cksum_size,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->sealalg,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->endtime,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->krb_flags,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->seq_send,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->seq_recv,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->established,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->big_endian,
				       &bp, &remain);
	    (void) krb5_ser_pack_int32((krb5_int32) ctx->nctypes,
				       &bp, &remain);

	    /* Now dynamic data */
	    kret = 0;

	    if (!kret && ctx->mech_used)
		 kret = kg_oid_externalize(kcontext, ctx->mech_used,
					   &bp, &remain); 
	    
	    if (!kret && ctx->here)
		kret = krb5_externalize_opaque(kcontext,
					       KV5M_PRINCIPAL,
					       (krb5_pointer) ctx->here,
					       &bp, &remain);

	    if (!kret && ctx->there)
		kret = krb5_externalize_opaque(kcontext,
					       KV5M_PRINCIPAL,
					       (krb5_pointer) ctx->there,
					       &bp, &remain);

	    if (!kret && ctx->subkey)
		kret = krb5_externalize_opaque(kcontext,
					       KV5M_KEYBLOCK,
					       (krb5_pointer) ctx->subkey,
					       &bp, &remain);

	    if (!kret && ctx->enc)
		kret = krb5_externalize_opaque(kcontext,
					       KV5M_KEYBLOCK,
					       (krb5_pointer) ctx->enc,
					       &bp, &remain);

	    if (!kret && ctx->seq)
		kret = krb5_externalize_opaque(kcontext,
					       KV5M_KEYBLOCK,
					       (krb5_pointer) ctx->seq,
					       &bp, &remain);

	    if (!kret && ctx->seqstate)
		kret = kg_queue_externalize(kcontext,
					    ctx->seqstate, &bp, &remain);

	    if (!kret)
		kret = krb5_externalize_opaque(kcontext,
					       KV5M_AUTH_CONTEXT,
					       (krb5_pointer) ctx->auth_context,
					       &bp, &remain);

	    for (i=0; i<ctx->nctypes; i++) {
		if (!kret) {
		    kret = krb5_ser_pack_int32((krb5_int32) ctx->ctypes[i],
					       &bp, &remain);
		}
	    }
	    
	    if (!kret) {
		(void) krb5_ser_pack_int32(KG_CONTEXT, &bp, &remain);
		*buffer = bp;
		*lenremain = remain;
	    }
	}
    }
    return(kret);
}

/*
 * Internalize this krb5_gss_ctx_id_t.
 */
krb5_error_code
kg_ctx_internalize(kcontext, argp, buffer, lenremain)
    krb5_context	kcontext;
    krb5_pointer	*argp;
    krb5_octet		**buffer;
    size_t		*lenremain;
{
    krb5_error_code	kret;
    krb5_gss_ctx_id_rec	*ctx;
    krb5_int32		ibuf;
    krb5_octet		*bp;
    size_t		remain;
    int			i;

    bp = *buffer;
    remain = *lenremain;
    kret = EINVAL;
    /* Read our magic number */
    if (krb5_ser_unpack_int32(&ibuf, &bp, &remain))
	ibuf = 0;
    if (ibuf == KG_CONTEXT) {
	kret = ENOMEM;

	/* Get a context */
	if ((remain >= ((10*sizeof(krb5_int32))+sizeof(ctx->seed))) &&
	    (ctx = (krb5_gss_ctx_id_rec *)
	     xmalloc(sizeof(krb5_gss_ctx_id_rec)))) {
	    memset(ctx, 0, sizeof(krb5_gss_ctx_id_rec));

	    /* Get static data */
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->initiate = (int) ibuf;
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->gss_flags = (int) ibuf;
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->seed_init = (int) ibuf;
	    (void) krb5_ser_unpack_bytes((krb5_octet *) ctx->seed,
					 sizeof(ctx->seed),
					 &bp, &remain);
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->signalg = (int) ibuf;
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->cksum_size = (int) ibuf;
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->sealalg = (int) ibuf;
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->endtime = (krb5_timestamp) ibuf;
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->krb_flags = (krb5_flags) ibuf;
	    (void) krb5_ser_unpack_int32(&ctx->seq_send, &bp, &remain);
	    (void) krb5_ser_unpack_int32(&ctx->seq_recv, &bp, &remain);
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->established = (int) ibuf;
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->big_endian = (int) ibuf;
	    (void) krb5_ser_unpack_int32(&ibuf, &bp, &remain);
	    ctx->nctypes = (int) ibuf;

	    if ((kret = kg_oid_internalize(kcontext, &ctx->mech_used, &bp,
					   &remain))) {
		 if (kret == EINVAL)
		      kret = 0;
	    }
	    /* Now get substructure data */
	    if ((kret = krb5_internalize_opaque(kcontext,
						KV5M_PRINCIPAL,
						(krb5_pointer *) &ctx->here,
						&bp, &remain))) {
		if (kret == EINVAL)
		    kret = 0;
	    }
	    if (!kret &&
		(kret = krb5_internalize_opaque(kcontext,
						KV5M_PRINCIPAL,
						(krb5_pointer *) &ctx->there,
						&bp, &remain))) {
		if (kret == EINVAL)
		    kret = 0;
	    }
	    if (!kret &&
		(kret = krb5_internalize_opaque(kcontext,
						KV5M_KEYBLOCK,
						(krb5_pointer *) &ctx->subkey,
						&bp, &remain))) {
		if (kret == EINVAL)
		    kret = 0;
	    }
	    if (!kret &&
		(kret = krb5_internalize_opaque(kcontext,
						KV5M_KEYBLOCK,
						(krb5_pointer *) &ctx->enc,
						&bp, &remain))) {
		if (kret == EINVAL)
		    kret = 0;
	    }
	    if (!kret &&
		(kret = krb5_internalize_opaque(kcontext,
						KV5M_KEYBLOCK,
						(krb5_pointer *) &ctx->seq,
						&bp, &remain))) {
		if (kret == EINVAL)
		    kret = 0;
	    }

	    if (!kret) {
		kret = kg_queue_internalize(kcontext, &ctx->seqstate,
					    &bp, &remain);
		if (kret == EINVAL)
		    kret = 0;
	    }
		
	    if (!kret)
		kret = krb5_internalize_opaque(kcontext,
					       KV5M_AUTH_CONTEXT,
				       (krb5_pointer *) &ctx->auth_context,
					       &bp, &remain);
		
	    if (!kret) {
		if (ctx->nctypes) {
		    if ((ctx->ctypes = (krb5_cksumtype *)
			 malloc(ctx->nctypes*sizeof(krb5_cksumtype))) == NULL){
			kret = ENOMEM;
		    }

		    for (i=0; i<ctx->nctypes; i++) {
			if (!kret) {
			    kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain);
			    ctx->ctypes[i] = (krb5_cksumtype) ibuf;
			}
		    }
		}
	    }

	    /* Get trailer */
	    if (!kret &&
		!(kret = krb5_ser_unpack_int32(&ibuf, &bp, &remain)) &&
		(ibuf == KG_CONTEXT)) {
		*buffer = bp;
		*lenremain = remain;
		*argp = (krb5_pointer) ctx;
	    }
	    else {
		if (!kret && (ibuf != KG_CONTEXT))
		    kret = EINVAL;
		if (ctx->seq)
		    krb5_free_keyblock(kcontext, ctx->seq);
		if (ctx->enc)
		    krb5_free_keyblock(kcontext, ctx->enc);
		if (ctx->subkey)
		    krb5_free_keyblock(kcontext, ctx->subkey);
		if (ctx->there)
		    krb5_free_principal(kcontext, ctx->there);
		if (ctx->here)
		    krb5_free_principal(kcontext, ctx->here);
		xfree(ctx);
	    }
	}
    }
    return(kret);
}
