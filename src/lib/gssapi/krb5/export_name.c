/*
 * lib/gssapi/krb5/export_name.c
 *
 * Copyright 1997 by the Massachusetts Institute of Technology.
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

#include "gssapiP_krb5.h"

OM_uint32 krb5_gss_export_name(OM_uint32  *minor_status,
			       const gss_name_t input_name,
			       gss_buffer_t exported_name)
{
	krb5_context context;
	krb5_error_code code;
	int length;
	char *str, *cp;

	if (GSS_ERROR(kg_get_context(minor_status, &context)))
		return(GSS_S_FAILURE);

	exported_name->length = 0;
	exported_name->value = NULL;
	
	if (! kg_validate_name(input_name)) {
		if (minor_status)
			*minor_status = (OM_uint32) G_VALIDATE_FAILED;
		return(GSS_S_CALL_BAD_STRUCTURE|GSS_S_BAD_NAME);
	}

	if ((code = krb5_unparse_name(context, (krb5_principal) input_name, 
				      &str))) {
		*minor_status = code;
		return(GSS_S_FAILURE);
	}

	length = strlen(str);
	exported_name->length = 10 + length + gss_mech_krb5->length;
	exported_name->value = malloc(exported_name->length);
	if (!exported_name->value) {
		free(str);
		*minor_status = ENOMEM;
		return(GSS_S_FAILURE);
	}
	cp = exported_name->value;

	/* Note: we assume the OID will be less than 128 bytes... */
	*cp++ = 0x04; *cp++ = 0x01;
	*cp++ = (gss_mech_krb5->length+2) >> 8;
	*cp++ = (gss_mech_krb5->length+2) & 0xFF;
	*cp++ = 0x06;
	*cp++ = (gss_mech_krb5->length) & 0xFF;
	memcpy(cp, gss_mech_krb5->elements, gss_mech_krb5->length);
	cp += gss_mech_krb5->length;
	*cp++ = length >> 24;
	*cp++ = length >> 16;
	*cp++ = length >> 8;
	*cp++ = length & 0xFF;
	memcpy(cp, str, length);

	free(str);

	return(GSS_S_COMPLETE);
}






