/*
 * lib/krb5/krb/copy_key.c
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
 * krb5_copy_keyblock()
 */

#include "k5-int.h"

/*
 * Copy a keyblock, including alloc'ed storage.
 */
KRB5_DLLIMP krb5_error_code KRB5_CALLCONV
krb5_copy_keyblock(context, from, to)
    krb5_context context;
    const krb5_keyblock FAR *from;
    krb5_keyblock FAR * FAR *to;
{
	krb5_keyblock	*new_key;

	if (!(new_key = (krb5_keyblock *) malloc(sizeof(krb5_keyblock))))
		return ENOMEM;
#ifdef HAVE_C_STRUCTURE_ASSIGNMENT
	*new_key = *from;
#else
	memcpy(new_key, from, sizeof(krb5_keyblock));
#endif
	if (!(new_key->contents = (krb5_octet *)malloc(new_key->length))) {
		krb5_xfree(new_key);
		return(ENOMEM);
	}
	memcpy((char *)new_key->contents, (char *)from->contents,
	       new_key->length);
	*to = new_key;
	return 0;
}
