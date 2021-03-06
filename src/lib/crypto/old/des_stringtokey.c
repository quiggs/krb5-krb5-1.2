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
#include "old.h"

/* XXX */
extern krb5_error_code mit_des_string_to_key_int
KRB5_PROTOTYPE ((krb5_keyblock FAR * keyblock,
		 const krb5_data FAR * data,
		 const krb5_data FAR * salt));

krb5_error_code
krb5_des_string_to_key(enc, string, salt, key)
     krb5_const struct krb5_enc_provider *enc;
     krb5_const krb5_data *string;
     krb5_const krb5_data *salt;
     krb5_keyblock *key;
{
    return(mit_des_string_to_key_int(key, string, salt));
}
