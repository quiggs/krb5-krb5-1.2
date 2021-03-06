/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved.
 *
 * $Id$
 * $Source$
 */

#if !defined(lint) && !defined(__CODECENTER__)
static char *rcsid = "$Header$";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <com_err.h>
#include <kadm5/admin.h>
#include <krb5.h>
#include "server_internal.h"

/*
 * Function check_handle
 *
 * Purpose: Check a server handle and return a com_err code if it is
 * invalid or 0 if it is valid.
 *
 * Arguments:
 *
 * 	handle		The server handle.
 */

static int check_handle(void *handle)
{
     CHECK_HANDLE(handle);
     return 0;
}

kadm5_ret_t kadm5_init_with_password(char *client_name, char *pass,
				     char *service_name,
				     kadm5_config_params *params,
				     krb5_ui_4 struct_version,
				     krb5_ui_4 api_version,
				     void **server_handle)
{
     return kadm5_init(client_name, pass, service_name, params,
		       struct_version, api_version,
		       server_handle);
}

kadm5_ret_t kadm5_init_with_creds(char *client_name,
				  krb5_ccache ccache,
				  char *service_name,
				  kadm5_config_params *params,
				  krb5_ui_4 struct_version,
				  krb5_ui_4 api_version,
				  void **server_handle)
{
     /*
      * A program calling init_with_creds *never* expects to prompt the
      * user.  Therefore, always pass a dummy password in case this is
      * KADM5_API_VERSION_1.  If this is KADM5_API_VERSION_2 and
      * MKEY_FROM_KBD is non-zero, return an error.
      */
     if (api_version == KADM5_API_VERSION_2 && params &&
	 (params->mask & KADM5_CONFIG_MKEY_FROM_KBD) &&
	 params->mkey_from_kbd)
	  return KADM5_BAD_SERVER_PARAMS;
     return kadm5_init(client_name, NULL, service_name, params,
		       struct_version, api_version,
		       server_handle);
}


kadm5_ret_t kadm5_init_with_skey(char *client_name, char *keytab,
				 char *service_name,
				 kadm5_config_params *params,
				 krb5_ui_4 struct_version,
				 krb5_ui_4 api_version,
				 void **server_handle)
{
     /*
      * A program calling init_with_skey *never* expects to prompt the
      * user.  Therefore, always pass a dummy password in case this is
      * KADM5_API_VERSION_1.  If this is KADM5_API_VERSION_2 and
      * MKEY_FROM_KBD is non-zero, return an error.
      */
     if (api_version == KADM5_API_VERSION_2 && params &&
	 (params->mask & KADM5_CONFIG_MKEY_FROM_KBD) &&
	 params->mkey_from_kbd)
	  return KADM5_BAD_SERVER_PARAMS;
     return kadm5_init(client_name, NULL, service_name, params,
		       struct_version, api_version,
		       server_handle);
}

kadm5_ret_t kadm5_init(char *client_name, char *pass,
		       char *service_name,
		       kadm5_config_params *params_in,
		       krb5_ui_4 struct_version,
		       krb5_ui_4 api_version,
		       void **server_handle)
{
     int ret;
     kadm5_server_handle_t handle;
     kadm5_config_params params_local; /* for v1 compat */

    if (! server_handle)
	 return EINVAL;

    if (! client_name)
	 return EINVAL;
    
    if (! (handle = (kadm5_server_handle_t) malloc(sizeof *handle)))
	 return ENOMEM;
    memset(handle, 0, sizeof(*handle));

    if (ret = (int) krb5_init_context(&(handle->context))) {
	 free(handle);
	 return(ret);
    }

    initialize_ovk_error_table();
    initialize_adb_error_table();
    initialize_ovku_error_table();

    handle->magic_number = KADM5_SERVER_HANDLE_MAGIC;
    handle->struct_version = struct_version;
    handle->api_version = api_version;

     /*
      * Verify the version numbers before proceeding; we can't use
      * CHECK_HANDLE because not all fields are set yet.
      */
     GENERIC_CHECK_HANDLE(handle, KADM5_OLD_SERVER_API_VERSION,
			  KADM5_NEW_SERVER_API_VERSION);
     
     /*
      * Acquire relevant profile entries.  In version 2, merge values
      * in params_in with values from profile, based on
      * params_in->mask.
      *
      * In version 1, we've given a realm (which may be NULL) instead
      * of params_in.  So use that realm, make params_in contain an
      * empty mask, and behave like version 2.
      */
     memset((char *) &params_local, 0, sizeof(params_local));
     if (api_version == KADM5_API_VERSION_1) {
	  params_local.realm = (char *) params_in;
	  if (params_in)
	       params_local.mask = KADM5_CONFIG_REALM;
	  params_in = &params_local;
     }

#define ILLEGAL_PARAMS (KADM5_CONFIG_ADMIN_SERVER)
     if (params_in && (params_in->mask & ILLEGAL_PARAMS)) {
	  krb5_free_context(handle->context);
	  free(handle);
	  return KADM5_BAD_SERVER_PARAMS;
     }

     if (ret = kadm5_get_config_params(handle->context,
				       (char *) NULL,
				       (char *) NULL,
				       params_in,
				       &handle->params)) {
	  krb5_free_context(handle->context);
	  free(handle);
	  return(ret);
     }

#define REQUIRED_PARAMS (KADM5_CONFIG_REALM | KADM5_CONFIG_DBNAME | \
			 KADM5_CONFIG_ADBNAME | \
			 KADM5_CONFIG_ADB_LOCKFILE | \
			 KADM5_CONFIG_ENCTYPE | \
			 KADM5_CONFIG_FLAGS | \
			 KADM5_CONFIG_MAX_LIFE | KADM5_CONFIG_MAX_RLIFE | \
			 KADM5_CONFIG_EXPIRATION | KADM5_CONFIG_ENCTYPES) 

     if ((handle->params.mask & REQUIRED_PARAMS) != REQUIRED_PARAMS) {
	  krb5_free_context(handle->context);
	  free(handle);
	  return KADM5_MISSING_CONF_PARAMS;
     }

    /*
     * Set the db_name based on configuration before calling
     * krb5_db_init, so it will get used.
     */
    if (ret = krb5_db_set_name(handle->context,
				   handle->params.dbname)) {
	 free(handle);
	 return(ret);
    }

    if (ret = krb5_db_init(handle->context)) {
	 krb5_free_context(handle->context);
	 free(handle);
	 return(ret);
    }

    if ((ret = krb5_parse_name(handle->context, client_name,
			       &handle->current_caller))) {
	 krb5_db_fini(handle->context);
	 krb5_free_context(handle->context);
	 free(handle);
	 return ret;
    }

     if (! (handle->lhandle = malloc(sizeof(*handle)))) {
	  krb5_db_fini(handle->context);
	  krb5_free_context(handle->context);
	  free(handle);
	  return ENOMEM;
     }
     *handle->lhandle = *handle;
     handle->lhandle->api_version = KADM5_API_VERSION_2;
     handle->lhandle->struct_version = KADM5_STRUCT_VERSION;
     handle->lhandle->lhandle = handle->lhandle;

     /* can't check the handle until current_caller is set */
    if (ret = check_handle((void *) handle)) {
	 free(handle);
	 return ret;
    }

     /*
      * The KADM5_API_VERSION_1 spec said "If pass (or keytab) is NULL
      * or an empty string, reads the master password from [the stash
      * file].  Otherwise, the non-NULL password is ignored and the
      * user is prompted for it via the tty."  However, the code was
      * implemented the other way: when a non-NULL password was
      * provided, the stash file was used.  This is somewhat more
      * sensible, as then a local or remote client that provides a
      * password does not prompt the user.  This code maintains the
      * previous actual behavior, and not the old spec behavior,
      * because that is how the unit tests are written.
      *
      * In KADM5_API_VERSION_2, this decision is controlled by
      * params.
      *
      * kdb_init_master's third argument is "from_keyboard".
      */ 
    if (ret = kdb_init_master(handle, handle->params.realm,
			      (handle->api_version == KADM5_API_VERSION_1 ?
			       ((pass == NULL) || !(strlen(pass))) :
			       ((handle->params.mask &
				 KADM5_CONFIG_MKEY_FROM_KBD) &&
				handle->params.mkey_from_kbd))
			      )) {
	 krb5_db_fini(handle->context);
	 krb5_free_context(handle->context);
	 free(handle);
	 return ret;
    }

    if ((ret = kdb_init_hist(handle, handle->params.realm))) {
	 krb5_db_fini(handle->context);
	 krb5_free_context(handle->context);
	 free(handle);
	 return ret;
    }

    if (ret = init_dict(&handle->params)) {
	 krb5_db_fini(handle->context);
	 krb5_free_principal(handle->context, handle->current_caller);
	 krb5_free_context(handle->context);
	 free(handle);
	 return ret;
    }
    
    if (ret = adb_policy_init(handle)) {
	 krb5_db_fini(handle->context);
	 krb5_free_principal(handle->context, handle->current_caller);
	 krb5_free_context(handle->context);
	 free(handle);
	 return ret;
    }
    handle->lhandle->policy_db = handle->policy_db;

    *server_handle = (void *) handle;
    
    return KADM5_OK;
}

kadm5_ret_t kadm5_destroy(void *server_handle)
{
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    destroy_dict();

    adb_policy_close(handle);
    krb5_db_fini(handle->context);
    krb5_free_principal(handle->context, handle->current_caller);
    kadm5_free_config_params(handle->context, &handle->params);
    krb5_free_context(handle->context);
    handle->magic_number = 0;
    free(handle->lhandle);
    free(handle);
 
    return KADM5_OK;
}

kadm5_ret_t kadm5_flush(void *server_handle)
{
     kadm5_server_handle_t handle = server_handle;
     kadm5_ret_t ret;

     CHECK_HANDLE(server_handle);

     if ((ret = krb5_db_fini(handle->context)) ||
	 /*
	  * Set the db_name based on configuration before calling
	  * krb5_db_init, so it will get used.
	  */
	 (ret = krb5_db_set_name(handle->context,
				     handle->params.dbname)) ||
	 (ret = krb5_db_init(handle->context)) ||
	 (ret = adb_policy_close(handle)) ||
	 (ret = adb_policy_init(handle))) {
	  (void) kadm5_destroy(server_handle);
	  return ret;
     }
     return KADM5_OK;
}

int _kadm5_check_handle(void *handle)
{
     CHECK_HANDLE(handle);
     return 0;
}
