/*
 * lib/krb5/krb/princ_comp.c
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
 * compare two principals, returning a krb5_boolean true if equal, false if
 * not.
 */

#include "k5-int.h"

krb5_boolean
krb5_realm_compare(context, princ1, princ2)
    krb5_context context;
    krb5_const_principal princ1;
    krb5_const_principal princ2;
{
    if (krb5_princ_realm(context, princ1)->length != 
	krb5_princ_realm(context, princ2)->length ||
	memcmp (krb5_princ_realm(context, princ1)->data, 
	 	krb5_princ_realm(context, princ2)->data,
		krb5_princ_realm(context, princ2)->length))
	return FALSE;

    return TRUE;
}

KRB5_DLLIMP krb5_boolean KRB5_CALLCONV
krb5_principal_compare(context, princ1, princ2)
    krb5_context context;
    krb5_const_principal princ1;
    krb5_const_principal princ2;
{
    register int i;
    krb5_int32 nelem;

    nelem = krb5_princ_size(context, princ1);
    if (nelem != krb5_princ_size(context, princ2))
	return FALSE;

    if (! krb5_realm_compare(context, princ1, princ2))
	return FALSE;

    for (i = 0; i < (int) nelem; i++) {
	register const krb5_data *p1 = krb5_princ_component(context, princ1, i);
	register const krb5_data *p2 = krb5_princ_component(context, princ2, i);
	if (p1->length != p2->length ||
	    memcmp(p1->data, p2->data, p1->length))
	    return FALSE;
    }
    return TRUE;
}
