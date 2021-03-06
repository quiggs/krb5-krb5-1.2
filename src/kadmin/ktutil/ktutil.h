/*
 * kadmin/ktutil/ktutil.h
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

typedef struct _krb5_kt_list {
    struct _krb5_kt_list *next;
    krb5_keytab_entry *entry;
} *krb5_kt_list;

krb5_error_code ktutil_free_kt_list
	KRB5_PROTOTYPE((krb5_context,
			krb5_kt_list));

krb5_error_code ktutil_delete
	KRB5_PROTOTYPE((krb5_context,
			krb5_kt_list *,
			int));

krb5_error_code ktutil_add
	KRB5_PROTOTYPE((krb5_context,
			krb5_kt_list *,
			char *,
			krb5_kvno,
			char *,
			int));

krb5_error_code ktutil_read_keytab
	KRB5_PROTOTYPE((krb5_context,
			char *,
			krb5_kt_list *));

krb5_error_code ktutil_write_keytab
	KRB5_PROTOTYPE((krb5_context,
			krb5_kt_list,
			char *));

#ifdef KRB5_KRB4_COMPAT
krb5_error_code ktutil_read_srvtab
	KRB5_PROTOTYPE((krb5_context,
			char *,
			krb5_kt_list *));
krb5_error_code ktutil_write_srvtab
	KRB5_PROTOTYPE((krb5_context,
			krb5_kt_list,
			char *));
#endif
