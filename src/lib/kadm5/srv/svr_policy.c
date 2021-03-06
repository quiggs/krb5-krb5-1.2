/*
 * Copyright 1993 OpenVision Technologies, Inc., All Rights Reserved
 *
 * $Header$
 */

#if !defined(lint) && !defined(__CODECENTER__)
static char *rcsid = "$Header$";
#endif

#include	<sys/types.h>
#include	<kadm5/admin.h>
#include	"adb.h"
#include	"server_internal.h"
#include	<stdlib.h>

#define MAX_PW_HISTORY	10
#define MIN_PW_HISTORY	1
#define	MIN_PW_CLASSES	1
#define MAX_PW_CLASSES	5
#define	MIN_PW_LENGTH	1

/*
 * Function: kadm5_create_policy
 * 
 * Purpose: Create Policies in the policy DB.
 *
 * Arguments:
 *	entry	(input) The policy entry to be written out to the DB.
 *	mask	(input)	Specifies which fields in entry are to ge written out
 *			and which get default values.
 *	<return value> 0 if sucsessfull otherwise an error code is returned.
 *
 * Requires:
 *	Entry must be a valid principal entry, and mask have a valid value.
 * 
 * Effects:
 *	Verifies that mask does not specify that the refcount should
 *	be set as part of the creation, and calls
 *	kadm5_create_policy_internal.  If the refcount *is*
 *	specified, returns KADM5_BAD_MASK.
 */

kadm5_ret_t
kadm5_create_policy(void *server_handle,
			 kadm5_policy_ent_t entry, long mask)
{
    CHECK_HANDLE(server_handle);

    if (mask & KADM5_REF_COUNT)
	return KADM5_BAD_MASK;
    else
	return kadm5_create_policy_internal(server_handle, entry, mask);
}

/*
 * Function: kadm5_create_policy_internal
 * 
 * Purpose: Create Policies in the policy DB.
 *
 * Arguments:
 *	entry	(input) The policy entry to be written out to the DB.
 *	mask	(input)	Specifies which fields in entry are to ge written out
 *			and which get default values.
 *	<return value> 0 if sucsessfull otherwise an error code is returned.
 *
 * Requires:
 *	Entry must be a valid principal entry, and mask have a valid value.
 * 
 * Effects:
 *	Writes the data to the database, and does a database sync if
 *	sucsessfull.
 *
 */

kadm5_ret_t
kadm5_create_policy_internal(void *server_handle,
				  kadm5_policy_ent_t entry, long mask)
{
    kadm5_server_handle_t handle = server_handle;
    osa_policy_ent_rec	pent;
    int			ret;
    char		*p;

    CHECK_HANDLE(server_handle);

    if ((entry == (kadm5_policy_ent_t) NULL) || (entry->policy == NULL))
	return EINVAL;
    if(strlen(entry->policy) == 0)
	return KADM5_BAD_POLICY;
    if (!(mask & KADM5_POLICY))
	return KADM5_BAD_MASK;
	
    pent.name = entry->policy;
    p = entry->policy;
    while(*p != '\0') {
	if(*p < ' ' || *p > '~')
	    return KADM5_BAD_POLICY;
	else
	    p++;
    }
    if (!(mask & KADM5_PW_MAX_LIFE))
	pent.pw_max_life = 0;
    else
	pent.pw_max_life = entry->pw_max_life;
    if (!(mask & KADM5_PW_MIN_LIFE))
	pent.pw_min_life = 0;
    else {
	if((mask & KADM5_PW_MAX_LIFE)) {
	    if(entry->pw_min_life > entry->pw_max_life && entry->pw_max_life != 0)
		return KADM5_BAD_MIN_PASS_LIFE;
	}
	pent.pw_min_life = entry->pw_min_life;
    }
    if (!(mask & KADM5_PW_MIN_LENGTH))
	pent.pw_min_length = MIN_PW_LENGTH;
    else {
	if(entry->pw_min_length < MIN_PW_LENGTH)
	    return KADM5_BAD_LENGTH;
	pent.pw_min_length = entry->pw_min_length;
    }
    if (!(mask & KADM5_PW_MIN_CLASSES))
	pent.pw_min_classes = MIN_PW_CLASSES;
    else {
	if(entry->pw_min_classes > MAX_PW_CLASSES || entry->pw_min_classes < MIN_PW_CLASSES)
	    return KADM5_BAD_CLASS;
	pent.pw_min_classes = entry->pw_min_classes;
    }
    if (!(mask & KADM5_PW_HISTORY_NUM))
	pent.pw_history_num = MIN_PW_HISTORY;
    else {
	if(entry->pw_history_num < MIN_PW_HISTORY ||
	   entry->pw_history_num > MAX_PW_HISTORY)
	    return KADM5_BAD_HISTORY;
	else
	    pent.pw_history_num = entry->pw_history_num;
    }
    if (!(mask & KADM5_REF_COUNT))
	pent.policy_refcnt = 0;
    else
	pent.policy_refcnt = entry->policy_refcnt;
    if ((ret = osa_adb_create_policy(handle->policy_db, &pent)) == OSA_ADB_OK)
	return KADM5_OK;
    else
	return ret;
}
	  
kadm5_ret_t
kadm5_delete_policy(void *server_handle, kadm5_policy_t name)
{
    kadm5_server_handle_t handle = server_handle;
    osa_policy_ent_t		entry;
    int				ret;

    CHECK_HANDLE(server_handle);

    if(name == (kadm5_policy_t) NULL)
	return EINVAL;
    if(strlen(name) == 0)
	return KADM5_BAD_POLICY;
    if ((ret = osa_adb_get_policy(handle->policy_db, name, &entry)) != OSA_ADB_OK)
	return ret;
    if(entry->policy_refcnt != 0) {
	osa_free_policy_ent(entry);
	return KADM5_POLICY_REF;
    }
    osa_free_policy_ent(entry);
    if ((ret = osa_adb_destroy_policy(handle->policy_db, name)) == OSA_ADB_OK)
	return KADM5_OK;
    else
	return ret;
}

kadm5_ret_t
kadm5_modify_policy(void *server_handle,
			 kadm5_policy_ent_t entry, long mask)
{
    CHECK_HANDLE(server_handle);

    if (mask & KADM5_REF_COUNT)
	return KADM5_BAD_MASK;
    else
	return kadm5_modify_policy_internal(server_handle, entry, mask);
}

kadm5_ret_t
kadm5_modify_policy_internal(void *server_handle,
				  kadm5_policy_ent_t entry, long mask)
{
    kadm5_server_handle_t handle = server_handle;
    osa_policy_ent_t	p;
    int			ret;

    CHECK_HANDLE(server_handle);

    if((entry == (kadm5_policy_ent_t) NULL) || (entry->policy == NULL))
	return EINVAL;
    if(strlen(entry->policy) == 0)
	return KADM5_BAD_POLICY;
    if((mask & KADM5_POLICY))
	return KADM5_BAD_MASK;
		
    switch ((ret = osa_adb_get_policy(handle->policy_db, entry->policy, &p))) {
    case OSA_ADB_OK:
	break;
    case OSA_ADB_NOENT:
	return KADM5_UNK_POLICY;
    default:
	break;
    }
    if ((mask & KADM5_PW_MAX_LIFE))
	p->pw_max_life = entry->pw_max_life;
    if ((mask & KADM5_PW_MIN_LIFE)) {
	if(entry->pw_min_life > p->pw_max_life && p->pw_max_life != 0)	{
	     osa_free_policy_ent(p);
	     return KADM5_BAD_MIN_PASS_LIFE;
	}
	p->pw_min_life = entry->pw_min_life;
    }
    if ((mask & KADM5_PW_MIN_LENGTH)) {
	if(entry->pw_min_length < MIN_PW_LENGTH) {
	      osa_free_policy_ent(p);
	      return KADM5_BAD_LENGTH;
	 }
	p->pw_min_length = entry->pw_min_length;
    }
    if ((mask & KADM5_PW_MIN_CLASSES)) {
	if(entry->pw_min_classes > MAX_PW_CLASSES ||
	   entry->pw_min_classes < MIN_PW_CLASSES) {
	     osa_free_policy_ent(p);
	     return KADM5_BAD_CLASS;
	}
	p->pw_min_classes = entry->pw_min_classes;
    }
    if ((mask & KADM5_PW_HISTORY_NUM)) {
	if(entry->pw_history_num < MIN_PW_HISTORY ||
	   entry->pw_history_num > MAX_PW_HISTORY) {
	     osa_free_policy_ent(p);
	     return KADM5_BAD_HISTORY;
	}
	p->pw_history_num = entry->pw_history_num;
    }
    if ((mask & KADM5_REF_COUNT))
	p->policy_refcnt = entry->policy_refcnt;
    switch ((ret = osa_adb_put_policy(handle->policy_db, p))) {
    case OSA_ADB_OK:
	ret = KADM5_OK;
	break;
    case OSA_ADB_NOENT:	/* this should not happen here ... */
	ret = KADM5_UNK_POLICY;
	break;
    }
    osa_free_policy_ent(p);
    return ret;
}

kadm5_ret_t
kadm5_get_policy(void *server_handle, kadm5_policy_t name,
		 kadm5_policy_ent_t entry) 
{
    osa_policy_ent_t		t;
    kadm5_policy_ent_rec	entry_local, **entry_orig, *new;
    int				ret;
    kadm5_server_handle_t handle = server_handle;

    CHECK_HANDLE(server_handle);

    /*
     * In version 1, entry is a pointer to a kadm5_policy_ent_t that
     * should be filled with allocated memory.
     */
    if (handle->api_version == KADM5_API_VERSION_1) {
	 entry_orig = (kadm5_policy_ent_rec **) entry;
	 *entry_orig = NULL;
	 entry = &entry_local;
    }
    
    if (name == (kadm5_policy_t) NULL)
	return EINVAL;
    if(strlen(name) == 0)
	return KADM5_BAD_POLICY;
    switch((ret = osa_adb_get_policy(handle->policy_db, name, &t))) {
    case OSA_ADB_OK:
	break;
    case OSA_ADB_NOENT:
	return KADM5_UNK_POLICY;
    default:
	return ret;
    }
    if ((entry->policy = (char *) malloc(strlen(t->name) + 1)) == NULL) {
	 osa_free_policy_ent(t);
	 return ENOMEM;
    }
    strcpy(entry->policy, t->name);
    entry->pw_min_life = t->pw_min_life;
    entry->pw_max_life = t->pw_max_life;
    entry->pw_min_length = t->pw_min_length;
    entry->pw_min_classes = t->pw_min_classes;
    entry->pw_history_num = t->pw_history_num;
    entry->policy_refcnt = t->policy_refcnt;
    osa_free_policy_ent(t);

    if (handle->api_version == KADM5_API_VERSION_1) {
	 new = (kadm5_policy_ent_t) malloc(sizeof(kadm5_policy_ent_rec));
	 if (new == NULL) {
	      free(entry->policy);
	      osa_free_policy_ent(t);
	      return ENOMEM;
	 }
	 *new = *entry;
	 *entry_orig = new;
    }
    
    return KADM5_OK;
}
