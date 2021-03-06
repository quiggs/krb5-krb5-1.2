/*
 * lib/krb5/os/init_ctx.c
 *
 * Copyright 1994 by the Massachusetts Institute of Technology.
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
 * krb5_init_contex()
 */

#define NEED_WINDOWS
#include "k5-int.h"

#ifdef TARGET_OS_MAC
#include <KerberosPreferences/KerberosPreferences.h>
#endif /* TARGET_OS_MAC */

#if defined(_MSDOS) || defined(_WIN32)

static krb5_error_code
get_from_windows_dir(
    char **pname
    )
{
    UINT size = GetWindowsDirectory(0, 0);
    *pname = malloc(size + 1 +
                    strlen(DEFAULT_PROFILE_FILENAME) + 1);
    if (*pname)
    {
        GetWindowsDirectory(*pname, size);
        strcat(*pname, "\\");
        strcat(*pname, DEFAULT_PROFILE_FILENAME);
        return 0;
    } else {
        return KRB5_CONFIG_CANTOPEN;
    }
}

static krb5_error_code
get_from_module_dir(
    char **pname
    )
{
    const DWORD size = 1024; /* fixed buffer */
    int found = 0;
    char *p;
    char *name;
    struct _stat s;

    *pname = 0;

    name = malloc(size);
    if (!name)
        return ENOMEM;

    if (!GetModuleFileName(GetModuleHandle("krb5_32"), name, size))
        goto cleanup;

    p = name + strlen(name);
    while ((p >= name) && (*p != '\\') && (*p != '/')) p--;
    if (p < name)
        goto cleanup;
    p++;
    strncpy(p, DEFAULT_PROFILE_FILENAME, size - (p - name));
    name[size - 1] = 0;
    found = !_stat(name, &s);

 cleanup:
    if (found)
        *pname = name;
    else
        if (name) free(name);
    return 0;
}

/*
 * get_from_registry
 *
 * This will find a profile in the registry.  *pbuffer != 0 if we
 * found something.  Make sure to free(*pbuffer) when done.  It will
 * return an error code if there is an error the user should know
 * about.  We maintain the invariant: return value != 0 => 
 * *pbuffer == 0.
 */
static krb5_error_code
get_from_registry(
    char** pbuffer,
    HKEY hBaseKey
    )
{
    HKEY hKey = 0;
    LONG rc = 0;
    DWORD size = 0;
    krb5_error_code retval = 0;
    const char *key_path = "Software\\MIT\\Kerberos5";
    const char *value_name = "config";

    /* a wannabe assertion */
    if (!pbuffer)
    {
        /*
         * We have a programming error!  For now, we segfault :)
         * There is no good mechanism to deal.
         */
    }
    *pbuffer = 0;

    if ((rc = RegOpenKeyEx(hBaseKey, key_path, 0, KEY_QUERY_VALUE, 
                           &hKey)) != ERROR_SUCCESS)
    {
        /* not a real error */
        goto cleanup;
    }
    rc = RegQueryValueEx(hKey, value_name, 0, 0, 0, &size);
    if ((rc != ERROR_SUCCESS) &&  (rc != ERROR_MORE_DATA))
    {
        /* not a real error */
        goto cleanup;
    }
    *pbuffer = malloc(size);
    if (!*pbuffer)
    {
        retval = ENOMEM;
        goto cleanup;
    }
    if ((rc = RegQueryValueEx(hKey, value_name, 0, 0, *pbuffer, &size)) != 
        ERROR_SUCCESS)
    {
        /*
         * Let's not call it a real error in case it disappears, but
         * we need to free so that we say we did not find anything.
         */
        free(*pbuffer);
        *pbuffer = 0;
        goto cleanup;
    }
 cleanup:
    if (hKey)
        RegCloseKey(hKey);
    if (retval && *pbuffer)
    {
        free(*pbuffer);
        /* Let's say we did not find anything: */
        *pbuffer = 0;
    }
    return retval;
}

#endif /* _MSDOS || _WIN32 */

static void
free_filespecs(files)
	profile_filespec_t *files;
{
#if !TARGET_OS_MAC
    char **cp;

    if (files == 0)
        return;
    
    for (cp = files; *cp; cp++)
	free(*cp);
#endif
    free(files);
}

static krb5_error_code
os_get_default_config_files(pfiles, secure)
	profile_filespec_t ** pfiles;
	krb5_boolean secure;
{
#ifdef TARGET_OS_MAC
        FSSpec*	files = nil;
	FSSpec*	preferencesFiles = nil;
	UInt32	numPreferencesFiles;
	FSSpec*	preferencesFilesToInit = nil;
	UInt32	numPreferencesFilesToInit;
	UInt32 i;
	Boolean foundPreferences = false;
	Boolean writtenPreferences = false;
	SInt16 refNum = -1;
	SInt32 length = 0;
	
	OSErr err = KPGetListOfPreferencesFiles (
		secure ? kpSystemPreferences : kpUserPreferences | kpSystemPreferences,
		&preferencesFiles,
		&numPreferencesFiles);

	if (err == noErr) {		
		/* After we get the list of files, check whether any of them contain any useful information */
		for (i = 0; i < numPreferencesFiles; i++) {
			if (KPPreferencesFileIsReadable (&preferencesFiles [i]) == noErr) {
				/* It's readable, check if it has anything in the data fork */
				err = FSpOpenDF (&preferencesFiles [i], fsRdPerm, &refNum);
				if (err == noErr) {
					err = GetEOF (refNum, &length);
				}
				
				if (refNum != -1) {
					FSClose (refNum);
				}
				
				if (length != 0) {
					foundPreferences = true;
					break;
				}
			}
		}

		if (!foundPreferences) {
			/* We found no profile data in any of those files; try to initialize one */
			/* If we are running "secure" do not try to initialize preferences */
			if (!secure) {
				err = KPGetListOfPreferencesFiles (kpUserPreferences, &preferencesFilesToInit, &numPreferencesFilesToInit);
				if (err == noErr) {
					for (i = 0; i < numPreferencesFilesToInit; i++) {
						if (KPPreferencesFileIsWritable (&preferencesFilesToInit [i]) == noErr) {
							err = noErr;
							/* If not readable, create it */
							if (KPPreferencesFileIsReadable (&preferencesFilesToInit [i]) != noErr) {
								err = KPCreatePreferencesFile (&preferencesFilesToInit [i]);
							}
							/* Initialize it */
							if (err == noErr) {
								err = KPInitializeWithDefaultKerberosLibraryPreferences (&preferencesFilesToInit [i]);
							}
							break;
						}
					}
				}
			}
		}
	}
	
	if (err == noErr) {
		files = malloc ((numPreferencesFiles + 1) * sizeof (FSSpec));
		if (files == NULL)
			err = memFullErr;
	}
	
	if (err == noErr) {
    	for (i = 0; i < numPreferencesFiles; i++) {
    		files [i] = preferencesFiles [i];
    	}
    	
    	files [numPreferencesFiles].vRefNum = 0;
    	files [numPreferencesFiles].parID = 0;
    	files [numPreferencesFiles].name[0] = '\0';
	}
	
	if (preferencesFiles != nil)
		KPFreeListOfPreferencesFiles (preferencesFiles);
	
	if (preferencesFilesToInit != nil) 
		KPFreeListOfPreferencesFiles (preferencesFilesToInit);
		
	if (err == memFullErr)
		return ENOMEM;
	else if (err != noErr)
		return ENOENT;
	
#else /* !macintosh */
    profile_filespec_t* files;
#if defined(_MSDOS) || defined(_WIN32)
    krb5_error_code retval = 0;
    char *name = 0;

    if (!secure)
    {
        char *env = getenv("KRB5_CONFIG");
        if (env)
        {
            name = malloc(strlen(env) + 1);
            if (!name) return ENOMEM;
            strcpy(name, env);
        }
    }
    if (!name && !secure)
    {
        /* HKCU */
        retval = get_from_registry(&name, HKEY_CURRENT_USER);
        if (retval) return retval;
    }
    if (!name)
    {
        /* HKLM */
        retval = get_from_registry(&name, HKEY_LOCAL_MACHINE);
        if (retval) return retval;
    }
    if (!name && !secure)
    {
        /* module dir */
        retval = get_from_module_dir(&name);
        if (retval) return retval;
    }
    if (!name)
    {
        /* windows dir */
        retval = get_from_windows_dir(&name);
    }
    if (retval)
        return retval;
    if (!name)
        return KRB5_CONFIG_CANTOPEN; /* should never happen */
    
    files = malloc(2 * sizeof(char *));
    files[0] = name;
    files[1] = 0;
#else /* !_MSDOS && !_WIN32 */
    char* filepath = 0;
    int n_entries, i;
    int ent_len;
    const char *s, *t;

    if (!secure) filepath = getenv("KRB5_CONFIG");
    if (!filepath) filepath = DEFAULT_PROFILE_PATH;

    /* count the distinct filename components */
    for(s = filepath, n_entries = 1; *s; s++) {
        if (*s == ':')
            n_entries++;
    }

    /* the array is NULL terminated */
    files = (char**) malloc((n_entries+1) * sizeof(char*));
    if (files == 0)
        return ENOMEM;

    /* measure, copy, and skip each one */
    for(s = filepath, i=0; (t = strchr(s, ':')) || (t=s+strlen(s)); s=t+1, i++)
    {
        ent_len = t-s;
        files[i] = (char*) malloc(ent_len + 1);
        if (files[i] == 0) {
            /* if malloc fails, free the ones that worked */
            while(--i >= 0) free(files[i]);
            free(files);
            return ENOMEM;
        }
        strncpy(files[i], s, ent_len);
        files[i][ent_len] = 0;
        if (*t == 0) {
            i++;
            break;
        }
    }
    /* cap the array */
    files[i] = 0;
#endif /* !_MSDOS && !_WIN32 */
#endif /* !macintosh */
    *pfiles = files;
    return 0;
}


/* Set the profile paths in the context. If secure is set to TRUE then 
   do not include user paths (from environment variables, etc.)
*/
static krb5_error_code
os_init_paths(ctx)
	krb5_context ctx;
{
    krb5_error_code	retval = 0;
    profile_filespec_t *files = 0;
    krb5_boolean secure = ctx->profile_secure;

#ifdef KRB5_DNS_LOOKUP
    ctx->profile_in_memory = 0;
#endif /* KRB5_DNS_LOOKUP */

    retval = os_get_default_config_files(&files, secure);
    
    if (!retval) {
#if TARGET_OS_MAC
        retval = FSp_profile_init_path(files,
			      &ctx->profile);
#else
        retval = profile_init((const_profile_filespec_t *) files,
			      &ctx->profile);
#endif

#ifdef KRB5_DNS_LOOKUP
        /* if none of the filenames can be opened use an empty profile */
        if (retval == ENOENT) {
            retval = profile_init(NULL, &ctx->profile);
            if (!retval)
                ctx->profile_in_memory = 1;
        }   
#endif /* KRB5_DNS_LOOKUP */
    }

    if (files)
        free_filespecs(files);

    if (retval)
        ctx->profile = 0;

    if (retval == ENOENT)
        return KRB5_CONFIG_CANTOPEN;

    if ((retval == PROF_SECTION_NOTOP) ||
        (retval == PROF_SECTION_SYNTAX) ||
        (retval == PROF_RELATION_SYNTAX) ||
        (retval == PROF_EXTRA_CBRACE) ||
        (retval == PROF_MISSING_OBRACE))
        return KRB5_CONFIG_BADFORMAT;

    return retval;
}

krb5_error_code
krb5_os_init_context(ctx)
	krb5_context ctx;
{
	krb5_os_context os_ctx;
	krb5_error_code	retval = 0;

	if (ctx->os_context)
		return 0;

	os_ctx = malloc(sizeof(struct _krb5_os_context));
	if (!os_ctx)
		return ENOMEM;
	memset(os_ctx, 0, sizeof(struct _krb5_os_context));
	os_ctx->magic = KV5M_OS_CONTEXT;

	ctx->os_context = (void *) os_ctx;

	os_ctx->time_offset = 0;
	os_ctx->usec_offset = 0;
	os_ctx->os_flags = 0;
	os_ctx->default_ccname = 0;
	os_ctx->default_ccprincipal = 0;

	krb5_cc_set_default_name(ctx, NULL);

	retval = os_init_paths(ctx);

	/*
	 * If there's an error in the profile, return an error.  Just
	 * ignoring the error is a Bad Thing (tm).
	 */

	return retval;
}

KRB5_DLLIMP krb5_error_code KRB5_CALLCONV
krb5_get_profile (ctx, profile)
	krb5_context ctx;
	profile_t* profile;
{
    krb5_error_code	retval = 0;
    profile_filespec_t *files = 0;

    retval = os_get_default_config_files(&files, ctx->profile_secure);

    if (!retval) {
#if TARGET_OS_MAC
        retval = FSp_profile_init_path(files,
			      profile);
#else
        retval = profile_init((const_profile_filespec_t *) files,
			      profile);
#endif
    }

    if (files)
        free_filespecs(files);

    if (retval == ENOENT)
        return KRB5_CONFIG_CANTOPEN;

    if ((retval == PROF_SECTION_NOTOP) ||
        (retval == PROF_SECTION_SYNTAX) ||
        (retval == PROF_RELATION_SYNTAX) ||
        (retval == PROF_EXTRA_CBRACE) ||
        (retval == PROF_MISSING_OBRACE))
        return KRB5_CONFIG_BADFORMAT;

    return retval;
}	

#if !TARGET_OS_MAC

krb5_error_code
krb5_set_config_files(ctx, filenames)
	krb5_context ctx;
	const char **filenames;
{
	krb5_error_code retval;
	profile_t	profile;
	
	retval = profile_init(filenames, &profile);
	if (retval)
		return retval;

	if (ctx->profile)
		profile_release(ctx->profile);
	ctx->profile = profile;

	return 0;
}

KRB5_DLLIMP krb5_error_code KRB5_CALLCONV
krb5_get_default_config_files(pfilenames)
	char ***pfilenames;
{
    if (!pfilenames)
        return EINVAL;
    return os_get_default_config_files(pfilenames, FALSE);
}

KRB5_DLLIMP void KRB5_CALLCONV
krb5_free_config_files(filenames)
	char **filenames;
{
    free_filespecs(filenames);
}

#endif /* macintosh */

krb5_error_code
krb5_secure_config_files(ctx)
	krb5_context ctx;
{
	/* Obsolete interface; always return an error.

	   This function should be removed next time a major version
	   number change happens.  */
	krb5_error_code retval;
	
	if (ctx->profile) {
		profile_release(ctx->profile);
		ctx->profile = 0;
	}

	ctx->profile_secure = TRUE;
	retval = os_init_paths(ctx);
	if (retval)
		return retval;

	return KRB5_OBSOLETE_FN;
}

void
krb5_os_free_context(ctx)
	krb5_context	ctx;
{
	krb5_os_context os_ctx;

	os_ctx = ctx->os_context;
	
	if (!os_ctx)
		return;

        if (os_ctx->default_ccname) {
		free(os_ctx->default_ccname);
                os_ctx->default_ccname = 0;
        }

	if (os_ctx->default_ccprincipal) {
		krb5_free_principal (ctx, os_ctx->default_ccprincipal);
		os_ctx->default_ccprincipal = 0;
	}

	os_ctx->magic = 0;
	free(os_ctx);
	ctx->os_context = 0;

        if (ctx->profile) {
	    profile_release(ctx->profile);
            ctx->profile = 0;
        }
}
