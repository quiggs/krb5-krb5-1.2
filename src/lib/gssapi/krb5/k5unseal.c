/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "gssapiP_krb5.h"
#include <memory.h>

/*
 * $Id$
 */

/* message_buffer is an input if SIGN, output if SEAL, and ignored if DEL_CTX
   conf_state is only valid if SEAL. */

OM_uint32
kg_unseal_v1(context, minor_status, ctx, ptr, bodysize, message_buffer,
	     conf_state, qop_state, toktype)
    krb5_context context;
    OM_uint32 *minor_status;
    krb5_gss_ctx_id_rec *ctx;
    unsigned char *ptr;
    int bodysize;
    gss_buffer_t message_buffer;
    int *conf_state;
    int *qop_state;
    int toktype;
{
    krb5_error_code code;
    int tmsglen;
    int conflen = 0;
    int signalg;
    int sealalg;
    gss_buffer_desc token;
    krb5_checksum cksum;
    krb5_checksum md5cksum;
    krb5_data plaind;
    char *data_ptr;
    krb5_timestamp now;
    unsigned char *plain;
    int cksum_len = 0;
    int plainlen;
    int direction;
    krb5_int32 seqnum;
    OM_uint32 retval;
    size_t sumlen;

    if (toktype == KG_TOK_SEAL_MSG) {
	message_buffer->length = 0;
	message_buffer->value = NULL;
    }

    /* get the sign and seal algorithms */

    signalg = ptr[0] + (ptr[1]<<8);
    sealalg = ptr[2] + (ptr[3]<<8);

    /* Sanity checks */

    if ((ptr[4] != 0xff) || (ptr[5] != 0xff)) {
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }

    if ((toktype != KG_TOK_SEAL_MSG) &&
	(sealalg != 0xffff)) {
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }

    /* in the current spec, there is only one valid seal algorithm per
       key type, so a simple comparison is ok */

    if ((toktype == KG_TOK_SEAL_MSG) &&
	!((sealalg == 0xffff) ||
	  (sealalg == ctx->sealalg))) {
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }

    /* there are several mappings of seal algorithms to sign algorithms,
       but few enough that we can try them all. */

    if ((ctx->sealalg == SEAL_ALG_NONE && signalg > 1) ||
	(ctx->sealalg == SEAL_ALG_1 && signalg != SGN_ALG_3) ||
	(ctx->sealalg == SEAL_ALG_DES3KD &&
	 signalg != SGN_ALG_HMAC_SHA1_DES3_KD)) {
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }

    switch (signalg) {
    case SGN_ALG_DES_MAC_MD5:
    case SGN_ALG_MD2_5:
	cksum_len = 8;
	break;
    case SGN_ALG_3:
	cksum_len = 16;
	break;
    case SGN_ALG_HMAC_SHA1_DES3_KD:
	cksum_len = 20;
	break;
    default:
	*minor_status = 0;
	return GSS_S_DEFECTIVE_TOKEN;
    }

    if (toktype == KG_TOK_SEAL_MSG)
	tmsglen = bodysize-(14+cksum_len);

    /* get the token parameters */

    /* decode the message, if SEAL */

    if (toktype == KG_TOK_SEAL_MSG) {
	if (sealalg != 0xffff) {
	    if ((plain = (unsigned char *) xmalloc(tmsglen)) == NULL) {
		*minor_status = ENOMEM;
		return(GSS_S_FAILURE);
	    }

	    if ((code = kg_decrypt(context, ctx->enc, KG_USAGE_SEAL, NULL,
				   ptr+14+cksum_len, plain, tmsglen))) {
		xfree(plain);
		*minor_status = code;
		return(GSS_S_FAILURE);
	    }
	} else {
	    plain = ptr+14+cksum_len;
	}

	plainlen = tmsglen;

	if ((sealalg == 0xffff) && ctx->big_endian) {
	    token.length = tmsglen;
	} else {
	    conflen = kg_confounder_size(context, ctx->enc);
	    token.length = tmsglen - conflen - plain[tmsglen-1];
	}

	if (token.length) {
	    if ((token.value = (void *) xmalloc(token.length)) == NULL) {
		if (sealalg != 0xffff)
		    xfree(plain);
		*minor_status = ENOMEM;
		return(GSS_S_FAILURE);
	    }
	    memcpy(token.value, plain+conflen, token.length);
	}
    } else if (toktype == KG_TOK_SIGN_MSG) {
	token = *message_buffer;
	plain = token.value;
	plainlen = token.length;
    } else {
	token.length = 0;
	token.value = NULL;
	plain = token.value;
	plainlen = token.length;
    }

    /* compute the checksum of the message */

    /* initialize the the cksum */
    switch (signalg) {
    case SGN_ALG_DES_MAC_MD5:
    case SGN_ALG_MD2_5:
    case SGN_ALG_HMAC_MD5:
    case SGN_ALG_DES_MAC:
    case SGN_ALG_3:
	md5cksum.checksum_type = CKSUMTYPE_RSA_MD5;
	break;
    case SGN_ALG_HMAC_SHA1_DES3_KD:
	md5cksum.checksum_type = CKSUMTYPE_HMAC_SHA1_DES3;
	break;
    default:
	abort ();
    }

    if (code = krb5_c_checksum_length(context, md5cksum.checksum_type, &sumlen))
	return(code);
    md5cksum.length = sumlen;

    switch (signalg) {
    case SGN_ALG_DES_MAC_MD5:
    case SGN_ALG_3:
	/* compute the checksum of the message */

	/* 8 = bytes of token body to be checksummed according to spec */

	if (! (data_ptr = (void *)
	       xmalloc(8 + (ctx->big_endian ? token.length : plainlen)))) {
	    if (sealalg != 0xffff)
		xfree(plain);
	    if (toktype == KG_TOK_SEAL_MSG)
		xfree(token.value);
	    *minor_status = ENOMEM;
	    return(GSS_S_FAILURE);
	}

	(void) memcpy(data_ptr, ptr-2, 8);

	if (ctx->big_endian)
	    (void) memcpy(data_ptr+8, token.value, token.length);
	else
	    (void) memcpy(data_ptr+8, plain, plainlen);

	plaind.length = 8 + (ctx->big_endian ? token.length : plainlen);
	plaind.data = data_ptr;
	code = krb5_c_make_checksum(context, md5cksum.checksum_type,
				    ctx->seq, KG_USAGE_SIGN,
				    &plaind, &md5cksum);
	xfree(data_ptr);

	if (code) {
	    if (toktype == KG_TOK_SEAL_MSG)
		xfree(token.value);
	    *minor_status = code;
	    return(GSS_S_FAILURE);
	}

	if ((code = kg_encrypt(context, ctx->seq, KG_USAGE_SEAL,
			       (g_OID_equal(ctx->mech_used, gss_mech_krb5_old) ?
				ctx->seq->contents : NULL),
			       md5cksum.contents, md5cksum.contents, 16))) {
	    xfree(md5cksum.contents);
	    if (toktype == KG_TOK_SEAL_MSG)
		xfree(token.value);
	    *minor_status = code;
	    return GSS_S_FAILURE;
	}

	if (signalg == 0)
	    cksum.length = 8;
	else
	    cksum.length = 16;
	cksum.contents = md5cksum.contents + 16 - cksum.length;

	code = memcmp(cksum.contents, ptr+14, cksum.length);
	break;

    case SGN_ALG_MD2_5:
	if (!ctx->seed_init &&
	    (code = kg_make_seed(context, ctx->subkey, ctx->seed))) {
	    xfree(md5cksum.contents);
	    if (sealalg != 0xffff)
		xfree(plain);
	    if (toktype == KG_TOK_SEAL_MSG)
		xfree(token.value);
	    *minor_status = code;
	    return GSS_S_FAILURE;
	}

	if (! (data_ptr = (void *)
	       xmalloc(sizeof(ctx->seed) + 8 +
		       (ctx->big_endian ? token.length : plainlen)))) {
	    xfree(md5cksum.contents);
	    if (sealalg == 0)
		xfree(plain);
	    if (toktype == KG_TOK_SEAL_MSG)
		xfree(token.value);
	    *minor_status = ENOMEM;
	    return(GSS_S_FAILURE);
	}
	(void) memcpy(data_ptr, ptr-2, 8);
	(void) memcpy(data_ptr+8, ctx->seed, sizeof(ctx->seed));
	if (ctx->big_endian)
	    (void) memcpy(data_ptr+8+sizeof(ctx->seed),
			  token.value, token.length);
	else
	    (void) memcpy(data_ptr+8+sizeof(ctx->seed),
			  plain, plainlen);
	plaind.length = 8 + sizeof(ctx->seed) +
	    (ctx->big_endian ? token.length : plainlen);
	plaind.data = data_ptr;
	xfree(md5cksum.contents);
	code = krb5_c_make_checksum(context, md5cksum.checksum_type,
				    ctx->seq, KG_USAGE_SIGN,
				    &plaind, &md5cksum);
	xfree(data_ptr);

	if (code) {
	    if (sealalg == 0)
		xfree(plain);
	    if (toktype == KG_TOK_SEAL_MSG)
		xfree(token.value);
	    *minor_status = code;
	    return(GSS_S_FAILURE);
	}

	code = memcmp(md5cksum.contents, ptr+14, 8);
	/* Falls through to defective-token??  */

    default:
	*minor_status = 0;
	return(GSS_S_DEFECTIVE_TOKEN);

    case SGN_ALG_HMAC_SHA1_DES3_KD:
	/* compute the checksum of the message */

	/* 8 = bytes of token body to be checksummed according to spec */

	if (! (data_ptr = (void *)
	       xmalloc(8 + (ctx->big_endian ? token.length : plainlen)))) {
	    if (sealalg != 0xffff)
		xfree(plain);
	    if (toktype == KG_TOK_SEAL_MSG)
		xfree(token.value);
	    *minor_status = ENOMEM;
	    return(GSS_S_FAILURE);
	}

	(void) memcpy(data_ptr, ptr-2, 8);

	if (ctx->big_endian)
	    (void) memcpy(data_ptr+8, token.value, token.length);
	else
	    (void) memcpy(data_ptr+8, plain, plainlen);

	plaind.length = 8 + (ctx->big_endian ? token.length : plainlen);
	plaind.data = data_ptr;
	code = krb5_c_make_checksum(context, md5cksum.checksum_type,
				    ctx->seq, KG_USAGE_SIGN,
				    &plaind, &md5cksum);
	xfree(data_ptr);

	if (code) {
	    if (toktype == KG_TOK_SEAL_MSG)
		xfree(token.value);
	    *minor_status = code;
	    return(GSS_S_FAILURE);
	}

	code = memcmp(md5cksum.contents, ptr+14, md5cksum.length);
	break;
    }

    xfree(md5cksum.contents);
    if (sealalg != 0xffff)
	xfree(plain);

    /* compare the computed checksum against the transmitted checksum */

    if (code) {
	if (toktype == KG_TOK_SEAL_MSG)
	    xfree(token.value);
	*minor_status = 0;
	return(GSS_S_BAD_SIG);
    }


    /* it got through unscathed.  Make sure the context is unexpired */

    if (toktype == KG_TOK_SEAL_MSG)
	*message_buffer = token;

    if (conf_state)
	*conf_state = (sealalg != 0xffff);

    if (qop_state)
	*qop_state = GSS_C_QOP_DEFAULT;

    if ((code = krb5_timeofday(context, &now))) {
	*minor_status = code;
	return(GSS_S_FAILURE);
    }

    if (now > ctx->endtime) {
	*minor_status = 0;
	return(GSS_S_CONTEXT_EXPIRED);
    }

    /* do sequencing checks */

    if ((code = kg_get_seq_num(context, ctx->seq, ptr+14, ptr+6, &direction,
			       &seqnum))) {
	if (toktype == KG_TOK_SEAL_MSG)
	    xfree(token.value);
	*minor_status = code;
	return(GSS_S_BAD_SIG);
    }

    if ((ctx->initiate && direction != 0xff) ||
	(!ctx->initiate && direction != 0)) {
	if (toktype == KG_TOK_SEAL_MSG)
	    xfree(token.value);
	*minor_status = G_BAD_DIRECTION;
	return(GSS_S_BAD_SIG);
    }

    retval = g_order_check(&(ctx->seqstate), seqnum);

    /* success or ordering violation */

    *minor_status = 0;
    return(retval);
}

/* message_buffer is an input if SIGN, output if SEAL, and ignored if DEL_CTX
   conf_state is only valid if SEAL. */

OM_uint32
kg_unseal(context, minor_status, context_handle, input_token_buffer,
	  message_buffer, conf_state, qop_state, toktype)
    krb5_context context;
    OM_uint32 *minor_status;
    gss_ctx_id_t context_handle;
    gss_buffer_t input_token_buffer;
    gss_buffer_t message_buffer;
    int *conf_state;
    int *qop_state;
    int toktype;
{
    krb5_gss_ctx_id_rec *ctx;
    unsigned char *ptr;
    int bodysize;
    int err;

    /* validate the context handle */
    if (! kg_validate_ctx_id(context_handle)) {
	*minor_status = (OM_uint32) G_VALIDATE_FAILED;
	return(GSS_S_NO_CONTEXT);
    }

    ctx = (krb5_gss_ctx_id_rec *) context_handle;

    if (! ctx->established) {
	*minor_status = KG_CTX_INCOMPLETE;
	return(GSS_S_NO_CONTEXT);
    }

    /* parse the token, leave the data in message_buffer, setting conf_state */

    /* verify the header */

    ptr = (unsigned char *) input_token_buffer->value;

    if (!(err = g_verify_token_header((gss_OID) ctx->mech_used,
				      &bodysize, &ptr, toktype,
				      input_token_buffer->length))) {
	return(kg_unseal_v1(context, minor_status, ctx, ptr, bodysize,
			    message_buffer, conf_state, qop_state,
			    toktype));
    }

    *minor_status = err;
    return(GSS_S_DEFECTIVE_TOKEN);
}
