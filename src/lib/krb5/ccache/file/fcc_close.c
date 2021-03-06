/*
 * lib/krb5/ccache/file/fcc_close.c
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
 * This file contains the source code for krb5_fcc_close.
 */



#include "fcc.h"
/*
 * Modifies:
 * id
 *
 * Effects:
 * Closes the file cache, invalidates the id, and frees any resources
 * associated with the cache.
 */
krb5_error_code KRB5_CALLCONV
krb5_fcc_close(context, id)
   krb5_context context;
   krb5_ccache id;
{
     register int closeval = KRB5_OK;

     if (((krb5_fcc_data *) id->data)->fd >= 0)
	     krb5_fcc_close_file(context, id);

     krb5_xfree(((krb5_fcc_data *) id->data)->filename);
     krb5_xfree(((krb5_fcc_data *) id->data));
     krb5_xfree(id);

     return closeval;
}
