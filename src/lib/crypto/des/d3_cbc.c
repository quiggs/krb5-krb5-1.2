/*
 * Copyright 1995 by Richard P. Basch.  All Rights Reserved.
 * Copyright 1995 by Lehman Brothers, Inc.  All Rights Reserved.
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
 * the name of Richard P. Basch, Lehman Brothers and M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Richard P. Basch,
 * Lehman Brothers and M.I.T. make no representations about the suitability
 * of this software for any purpose.  It is provided "as is" without
 * express or implied warranty.
 */

#include "des_int.h"
#include "f_tables.h"

/*
 * Triple-DES CBC encryption mode.
 */

int
mit_des3_cbc_encrypt(in, out, length, ks1, ks2, ks3, ivec, encrypt)
	const mit_des_cblock FAR *in;
	mit_des_cblock FAR *out;
	long length;
	mit_des_key_schedule ks1, ks2, ks3;
	mit_des_cblock ivec;
	int encrypt;
{
    register unsigned DES_INT32 left, right;
    register unsigned DES_INT32 temp;
    register unsigned DES_INT32 *kp1, *kp2, *kp3;
    register unsigned char *ip, *op;

    /*
     * Get key pointer here.  This won't need to be reinitialized
     */
    kp1 = (unsigned DES_INT32 *)ks1;
    kp2 = (unsigned DES_INT32 *)ks2;
    kp3 = (unsigned DES_INT32 *)ks3;

    /*
     * Deal with encryption and decryption separately.
     */
    if (encrypt) {
	/*
	 * Initialize left and right with the contents of the initial
	 * vector.
	 */
	ip = (unsigned char *)ivec;
	GET_HALF_BLOCK(left, ip);
	GET_HALF_BLOCK(right, ip);

	/*
	 * Suitably initialized, now work the length down 8 bytes
	 * at a time.
	 */
	ip = (unsigned char *)in;
	op = (unsigned char *)out;
	while (length > 0) {
	    /*
	     * Get more input, xor it in.  If the length is
	     * greater than or equal to 8 this is straight
	     * forward.  Otherwise we have to fart around.
	     */
	    if (length >= 8) {
		left  ^= ((*ip++) & FF_UINT32) << 24;
		left  ^= ((*ip++) & FF_UINT32) << 16;
		left  ^= ((*ip++) & FF_UINT32) <<  8;
		left  ^=  (*ip++) & FF_UINT32;
		right ^= ((*ip++) & FF_UINT32) << 24;
		right ^= ((*ip++) & FF_UINT32) << 16;
		right ^= ((*ip++) & FF_UINT32) <<  8;
		right ^=  (*ip++) & FF_UINT32;
		length -= 8;
	    } else {
		/*
		 * Oh, shoot.  We need to pad the
		 * end with zeroes.  Work backwards
		 * to do this.
		 */
		ip += (int) length;
		switch(length) {
		case 7:	right ^= (*(--ip) & FF_UINT32) <<  8;
		case 6:	right ^= (*(--ip) & FF_UINT32) << 16;
		case 5:	right ^= (*(--ip) & FF_UINT32) << 24;
		case 4:	left  ^=  *(--ip) & FF_UINT32;
		case 3:	left  ^= (*(--ip) & FF_UINT32) <<  8;
		case 2:	left  ^= (*(--ip) & FF_UINT32) << 16;
		case 1:	left  ^= (*(--ip) & FF_UINT32) << 24;

		}
		length = 0;
	    }

	    /*
	     * Encrypt what we have
	     */
	    DES_DO_ENCRYPT(left, right, temp, kp1);
	    DES_DO_DECRYPT(left, right, temp, kp2);
	    DES_DO_ENCRYPT(left, right, temp, kp3);

	    /*
	     * Copy the results out
	     */
	    PUT_HALF_BLOCK(left, op);
	    PUT_HALF_BLOCK(right, op);
	}
    } else {
	/*
	 * Decrypting is harder than encrypting because of
	 * the necessity of remembering a lot more things.
	 * Should think about this a little more...
	 */
	unsigned DES_INT32 ocipherl, ocipherr;
	unsigned DES_INT32 cipherl, cipherr;

	if (length <= 0)
	    return 0;

	/*
	 * Prime the old cipher with ivec.
	 */
	ip = (unsigned char *)ivec;
	GET_HALF_BLOCK(ocipherl, ip);
	GET_HALF_BLOCK(ocipherr, ip);

	/*
	 * Now do this in earnest until we run out of length.
	 */
	ip = (unsigned char *)in;
	op = (unsigned char *)out;
	for (;;) {		/* check done inside loop */
	    /*
	     * Read a block from the input into left and
	     * right.  Save this cipher block for later.
	     */
	    GET_HALF_BLOCK(left, ip);
	    GET_HALF_BLOCK(right, ip);
	    cipherl = left;
	    cipherr = right;

	    /*
	     * Decrypt this.
	     */
	    DES_DO_DECRYPT(left, right, temp, kp3);
	    DES_DO_ENCRYPT(left, right, temp, kp2);
	    DES_DO_DECRYPT(left, right, temp, kp1);

	    /*
	     * Xor with the old cipher to get plain
	     * text.  Output 8 or less bytes of this.
	     */
	    left ^= ocipherl;
	    right ^= ocipherr;
	    if (length > 8) {
		length -= 8;
		PUT_HALF_BLOCK(left, op);
		PUT_HALF_BLOCK(right, op);
		/*
		 * Save current cipher block here
		 */
		ocipherl = cipherl;
		ocipherr = cipherr;
	    } else {
		/*
		 * Trouble here.  Start at end of output,
		 * work backwards.
		 */
		op += (int) length;
		switch(length) {
		case 8: *(--op) = (unsigned char) (right & 0xff);
		case 7: *(--op) = (unsigned char) ((right >> 8) & 0xff);
		case 6: *(--op) = (unsigned char) ((right >> 16) & 0xff);
		case 5: *(--op) = (unsigned char) ((right >> 24) & 0xff);
		case 4: *(--op) = (unsigned char) (left & 0xff);
		case 3: *(--op) = (unsigned char) ((left >> 8) & 0xff);
		case 2: *(--op) = (unsigned char) ((left >> 16) & 0xff);
		case 1: *(--op) = (unsigned char) ((left >> 24) & 0xff);
		}
		break;		/* we're done */
	    }
	}
    }

    /*
     * Done, return nothing.
     */
    return 0;
}
