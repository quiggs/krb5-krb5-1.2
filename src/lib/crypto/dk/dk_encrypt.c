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

#include "k5-int.h"
#include "dk.h"

#define K5CLENGTH 5 /* 32 bit net byte order integer + one byte seed */

/* the spec says that the confounder size and padding are specific to
   the encryption algorithm.  This code (dk_encrypt_length and
   dk_encrypt) assume the confounder is always the blocksize, and the
   padding is always zero bytes up to the blocksize.  If these
   assumptions ever fails, the keytype table should be extended to
   include these bits of info. */

void
krb5_dk_encrypt_length(enc, hash, inputlen, length)
     krb5_const struct krb5_enc_provider *enc;
     krb5_const struct krb5_hash_provider *hash;
     size_t inputlen;
     size_t *length;
{
    size_t blocksize, hashsize;

    (*(enc->block_size))(&blocksize);
    (*(hash->hash_size))(&hashsize);

    *length = krb5_roundup(blocksize+inputlen, blocksize) + hashsize;
}

krb5_error_code
krb5_dk_encrypt(enc, hash, key, usage, ivec, input, output)
     krb5_const struct krb5_enc_provider *enc;
     krb5_const struct krb5_hash_provider *hash;
     krb5_const krb5_keyblock *key;
     krb5_keyusage usage;
     krb5_const krb5_data *ivec;
     krb5_const krb5_data *input;
     krb5_data *output;
{
    size_t blocksize, keybytes, keylength, plainlen, enclen;
    krb5_error_code ret;
    unsigned char constantdata[K5CLENGTH];
    krb5_data d1, d2;
    unsigned char *plaintext, *kedata, *kidata, *cn;
    krb5_keyblock ke, ki;

    /* allocate and set up plaintext and to-be-derived keys */

    (*(enc->block_size))(&blocksize);
    (*(enc->keysize))(&keybytes, &keylength);
    plainlen = krb5_roundup(blocksize+input->length, blocksize);

    krb5_dk_encrypt_length(enc, hash, input->length, &enclen);

    /* key->length, ivec will be tested in enc->encrypt */

    if (output->length < enclen)
	return(KRB5_BAD_MSIZE);

    if ((kedata = (unsigned char *) malloc(keylength)) == NULL)
	return(ENOMEM);
    if ((kidata = (unsigned char *) malloc(keylength)) == NULL) {
	free(kedata);
	return(ENOMEM);
    }
    if ((plaintext = (unsigned char *) malloc(plainlen)) == NULL) {
	free(kidata);
	free(kedata);
	return(ENOMEM);
    }

    ke.contents = kedata;
    ke.length = keylength;
    ki.contents = kidata;
    ki.length = keylength;

    /* derive the keys */

    d1.data = constantdata;
    d1.length = K5CLENGTH;

    d1.data[0] = (usage>>24)&0xff;
    d1.data[1] = (usage>>16)&0xff;
    d1.data[2] = (usage>>8)&0xff;
    d1.data[3] = usage&0xff;

    d1.data[4] = 0xAA;

    if ((ret = krb5_derive_key(enc, key, &ke, &d1)))
	goto cleanup;

    d1.data[4] = 0x55;

    if ((ret = krb5_derive_key(enc, key, &ki, &d1)))
	goto cleanup;

    /* put together the plaintext */

    d1.length = blocksize;
    d1.data = plaintext;

    if ((ret = krb5_c_random_make_octets(/* XXX */ 0, &d1)))
	goto cleanup;

    memcpy(plaintext+blocksize, input->data, input->length);

    memset(plaintext+blocksize+input->length, 0,
	   plainlen - (blocksize+input->length));

    /* encrypt the plaintext */

    d1.length = plainlen;
    d1.data = plaintext;

    d2.length = plainlen;
    d2.data = output->data;

    if ((ret = ((*(enc->encrypt))(&ke, ivec, &d1, &d2))))
	goto cleanup;

    if (ivec != NULL && ivec->length == blocksize)
	cn = d2.data + d2.length - blocksize;
    else
	cn = NULL;

    /* hash the plaintext */

    d2.length = enclen - plainlen;
    d2.data = output->data+plainlen;

    output->length = enclen;

    if ((ret = krb5_hmac(hash, &ki, 1, &d1, &d2))) {
	memset(d2.data, 0, d2.length);
	goto cleanup;
    }

    /* update ivec */
    if (cn != NULL)
	memcpy(ivec->data, cn, blocksize);

    /* ret is set correctly by the prior call */

cleanup:
    memset(kedata, 0, keylength);
    memset(kidata, 0, keylength);
    memset(plaintext, 0, plainlen);

    free(plaintext);
    free(kidata);
    free(kedata);

    return(ret);
}

#ifdef ATHENA_DES3_KLUDGE
void
krb5_marc_dk_encrypt_length(enc, hash, inputlen, length)
     krb5_const struct krb5_enc_provider *enc;
     krb5_const struct krb5_hash_provider *hash;
     size_t inputlen;
     size_t *length;
{
    size_t blocksize, hashsize;

    (*(enc->block_size))(&blocksize);
    (*(hash->hash_size))(&hashsize);

    *length = krb5_roundup(blocksize+4+inputlen, blocksize) + hashsize;
}

krb5_error_code
krb5_marc_dk_encrypt(enc, hash, key, usage, ivec, input, output)
     krb5_const struct krb5_enc_provider *enc;
     krb5_const struct krb5_hash_provider *hash;
     krb5_const krb5_keyblock *key;
     krb5_keyusage usage;
     krb5_const krb5_data *ivec;
     krb5_const krb5_data *input;
     krb5_data *output;
{
    size_t blocksize, keybytes, keylength, plainlen, enclen;
    krb5_error_code ret;
    unsigned char constantdata[K5CLENGTH];
    krb5_data d1, d2;
    unsigned char *plaintext, *kedata, *kidata, *cn;
    krb5_keyblock ke, ki;

    /* allocate and set up plaintext and to-be-derived keys */

    (*(enc->block_size))(&blocksize);
    (*(enc->keysize))(&keybytes, &keylength);
    plainlen = krb5_roundup(blocksize+4+input->length, blocksize);

    krb5_marc_dk_encrypt_length(enc, hash, input->length, &enclen);

    /* key->length, ivec will be tested in enc->encrypt */

    if (output->length < enclen)
	return(KRB5_BAD_MSIZE);

    if ((kedata = (unsigned char *) malloc(keylength)) == NULL)
	return(ENOMEM);
    if ((kidata = (unsigned char *) malloc(keylength)) == NULL) {
	free(kedata);
	return(ENOMEM);
    }
    if ((plaintext = (unsigned char *) malloc(plainlen)) == NULL) {
	free(kidata);
	free(kedata);
	return(ENOMEM);
    }

    ke.contents = kedata;
    ke.length = keylength;
    ki.contents = kidata;
    ki.length = keylength;

    /* derive the keys */

    d1.data = constantdata;
    d1.length = K5CLENGTH;

    d1.data[0] = (usage>>24)&0xff;
    d1.data[1] = (usage>>16)&0xff;
    d1.data[2] = (usage>>8)&0xff;
    d1.data[3] = usage&0xff;

    d1.data[4] = 0xAA;

    if ((ret = krb5_derive_key(enc, key, &ke, &d1)))
	goto cleanup;

    d1.data[4] = 0x55;

    if ((ret = krb5_derive_key(enc, key, &ki, &d1)))
	goto cleanup;

    /* put together the plaintext */

    d1.length = blocksize;
    d1.data = plaintext;

    if ((ret = krb5_c_random_make_octets(/* XXX */ 0, &d1)))
	goto cleanup;

    (plaintext+blocksize)[0] = (input->length>>24)&0xff;
    (plaintext+blocksize)[1] = (input->length>>16)&0xff;
    (plaintext+blocksize)[2] = (input->length>>8)&0xff;
    (plaintext+blocksize)[3] = input->length&0xff;

    memcpy(plaintext+blocksize+4, input->data, input->length);

    memset(plaintext+blocksize+4+input->length, 0,
	   plainlen - (blocksize+4+input->length));

    /* encrypt the plaintext */

    d1.length = plainlen;
    d1.data = plaintext;

    d2.length = plainlen;
    d2.data = output->data;

    if ((ret = ((*(enc->encrypt))(&ke, ivec, &d1, &d2))))
	goto cleanup;

    if (ivec != NULL && ivec->length == blocksize)
	cn = d2.data + d2.length - blocksize;
    else
	cn = NULL;

    /* hash the plaintext */

    d2.length = enclen - plainlen;
    d2.data = output->data+plainlen;

    output->length = enclen;

    if ((ret = krb5_hmac(hash, &ki, 1, &d1, &d2))) {
	memset(d2.data, 0, d2.length);
	goto cleanup;
    }

    /* update ivec */
    if (cn != NULL)
	memcpy(ivec->data, cn, blocksize);

    /* ret is set correctly by the prior call */

cleanup:
    memset(kedata, 0, keylength);
    memset(kidata, 0, keylength);
    memset(plaintext, 0, plainlen);

    free(plaintext);
    free(kidata);
    free(kedata);

    return(ret);
}
#endif /* ATHENA_DES3_KLUDGE */
