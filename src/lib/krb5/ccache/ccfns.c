/*
 * lib/krb5/ccache/ccfns.c
 *
 * Copyright 2000 by the Massachusetts Institute of Technology.
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
 */

/*
 * Dispatch methods for credentials cache code.
 */

#include "k5-int.h"

const char FAR * KRB5_CALLCONV
krb5_cc_get_name (krb5_context context, krb5_ccache cache)
{
    return cache->ops->get_name(context, cache);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_gen_new (krb5_context context, krb5_ccache FAR *cache)
{
    return (*cache)->ops->gen_new(context, cache);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_initialize(krb5_context context, krb5_ccache cache,
		   krb5_principal principal)
{
    return cache->ops->init(context, cache, principal);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_destroy (krb5_context context, krb5_ccache cache)
{
    return cache->ops->destroy(context, cache);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_close (krb5_context context, krb5_ccache cache)
{
    return cache->ops->close(context, cache);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_store_cred (krb5_context context, krb5_ccache cache,
		    krb5_creds FAR *creds)
{
    return cache->ops->store(context, cache, creds);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_retrieve_cred (krb5_context context, krb5_ccache cache,
		       krb5_flags flags, krb5_creds FAR *mcreds,
		       krb5_creds FAR *creds)
{
    return cache->ops->retrieve(context, cache, flags, mcreds, creds);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_get_principal (krb5_context context, krb5_ccache cache,
		       krb5_principal FAR *principal)
{
    return cache->ops->get_princ(context, cache, principal);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_start_seq_get (krb5_context context, krb5_ccache cache,
		       krb5_cc_cursor FAR *cursor)
{
    return cache->ops->get_first(context, cache, cursor);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_next_cred (krb5_context context, krb5_ccache cache,
		   krb5_cc_cursor FAR *cursor, krb5_creds FAR *creds)
{
    return cache->ops->get_next(context, cache, cursor, creds);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_end_seq_get (krb5_context context, krb5_ccache cache,
		     krb5_cc_cursor FAR *cursor)
{
    return cache->ops->end_get(context, cache, cursor);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_remove_cred (krb5_context context, krb5_ccache cache, krb5_flags flags,
		     krb5_creds FAR *creds)
{
    return cache->ops->remove_cred(context, cache, flags, creds);
}

krb5_error_code KRB5_CALLCONV
krb5_cc_set_flags (krb5_context context, krb5_ccache cache, krb5_flags flags)
{
    return cache->ops->set_flags(context, cache, flags);
}

const char FAR * KRB5_CALLCONV
krb5_cc_get_type (krb5_context context, krb5_ccache cache)
{
    return cache->ops->prefix;
}
