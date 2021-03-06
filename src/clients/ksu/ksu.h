/* 
 * Copyright (c) 1994 by the University of Southern California
 *
 * EXPORT OF THIS SOFTWARE from the United States of America may
 *     require a specific license from the United States Government.
 *     It is the responsibility of any person or organization contemplating
 *     export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to copy, modify, and distribute
 *     this software and its documentation in source and binary forms is
 *     hereby granted, provided that any documentation or other materials
 *     related to such distribution or use acknowledge that the software
 *     was developed by the University of Southern California. 
 *
 * DISCLAIMER OF WARRANTY.  THIS SOFTWARE IS PROVIDED "AS IS".  The
 *     University of Southern California MAKES NO REPRESENTATIONS OR
 *     WARRANTIES, EXPRESS OR IMPLIED.  By way of example, but not
 *     limitation, the University of Southern California MAKES NO
 *     REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 *     PARTICULAR PURPOSE. The University of Southern
 *     California shall not be held liable for any liability nor for any
 *     direct, indirect, or consequential damages with respect to any
 *     claim by the user or distributor of the ksu software.
 *
 * KSU was writen by:  Ari Medvinsky, ari@isi.edu
 */

#include "k5-int.h"
#include <stdio.h>
#include "com_err.h"
#include <sys/types.h> 
#include <sys/param.h>
#include <pwd.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
/* <stdarg.h> or <varargs.h> is already included by com_err.h.  */

#define NO_TARGET_FILE '.'
#define SOURCE_USER_LOGIN "."

#define KRB5_DEFAULT_OPTIONS 0
#define KRB5_DEFAULT_TKT_LIFE 60*60*12 /* 12 hours */

#define KRB5_SECONDARY_CACHE "FILE:/tmp/krb5cc_"

#define KRB5_LOGIN_NAME ".k5login"
#define KRB5_USERS_NAME ".k5users"
#define USE_DEFAULT_REALM_NAME "."
#define PERMIT_ALL_COMMANDS "*" 
#define KRB5_SEC_BUFFSIZE 80
#define NOT_AUTHORIZED 1

#define CHUNK 3
#define CACHE_MODE 0600
#define MAX_CMD 2048 /* this is temp, should use realloc instead,          
			as done in most of the code */       
		      

extern int optind;
extern char * optarg;

/* globals */
extern char * prog_name;
extern int auth_debug;
extern int quiet;
extern char k5login_path[MAXPATHLEN];
extern char k5users_path[MAXPATHLEN];
extern char * gb_err;
/***********/

typedef struct opt_info{
	int opt;
	krb5_deltat lifetime;
	krb5_deltat rlife;
	int princ;
}opt_info;

/* krb_auth_su.c */
extern krb5_boolean krb5_auth_check
        PROTOTYPE((krb5_context, krb5_principal, char *, opt_info *,
		   char *, krb5_ccache, int *, uid_t));

extern krb5_boolean krb5_fast_auth
        PROTOTYPE((krb5_context, krb5_principal, krb5_principal, char *,
		   krb5_ccache));

extern krb5_boolean krb5_get_tkt_via_passwd 
	PROTOTYPE((krb5_context, krb5_ccache *, krb5_principal,
		   krb5_principal, opt_info *, krb5_boolean *));

extern void dump_principal 
	PROTOTYPE((krb5_context, char *, krb5_principal));

extern void plain_dump_principal 
	PROTOTYPE((krb5_context, krb5_principal));


extern krb5_error_code krb5_parse_lifetime
	PROTOTYPE((char *, long *));

extern krb5_error_code get_best_principal
	PROTOTYPE((krb5_context, char **, krb5_principal *));

/* ccache.c */
extern krb5_error_code krb5_ccache_copy
	PROTOTYPE((krb5_context, krb5_ccache, char *, krb5_principal, 
		   krb5_ccache *, krb5_boolean *, uid_t));

extern krb5_error_code krb5_store_all_creds
	PROTOTYPE((krb5_context, krb5_ccache, krb5_creds **, krb5_creds **));

extern krb5_error_code krb5_store_all_creds
	PROTOTYPE((krb5_context, krb5_ccache, krb5_creds **, krb5_creds **));

extern krb5_boolean compare_creds
	PROTOTYPE((krb5_context, krb5_creds *, krb5_creds *));

extern krb5_error_code krb5_get_nonexp_tkts
	PROTOTYPE((krb5_context, krb5_ccache, krb5_creds ***));

extern krb5_error_code krb5_check_exp
	PROTOTYPE((krb5_context, krb5_ticket_times));

extern char *flags_string PROTOTYPE((krb5_creds *));

extern krb5_error_code krb5_get_login_princ
	PROTOTYPE((const char *, char ***));

extern void show_credential
	PROTOTYPE((krb5_context, krb5_creds *, krb5_ccache));

extern int gen_sym PROTOTYPE((void));

extern krb5_error_code krb5_ccache_overwrite
	PROTOTYPE((krb5_context, krb5_ccache, krb5_ccache, krb5_principal));

extern krb5_error_code krb5_store_some_creds
	PROTOTYPE((krb5_context, krb5_ccache, krb5_creds **, krb5_creds **,
		   krb5_principal, krb5_boolean *));

extern krb5_error_code krb5_ccache_copy_restricted
	PROTOTYPE((krb5_context, krb5_ccache, char *, krb5_principal, 
		   krb5_ccache *, krb5_boolean *, uid_t));

extern krb5_error_code krb5_ccache_refresh
	PROTOTYPE((krb5_context, krb5_ccache));

extern krb5_error_code krb5_ccache_filter
	PROTOTYPE((krb5_context, krb5_ccache, krb5_principal));

extern krb5_boolean krb5_find_princ_in_cred_list
	PROTOTYPE((krb5_context, krb5_creds **, krb5_principal));

extern krb5_error_code krb5_find_princ_in_cache
	PROTOTYPE((krb5_context, krb5_ccache, krb5_principal, krb5_boolean *));

extern void printtime PROTOTYPE((time_t));

/* authorization.c */
extern krb5_boolean fowner PROTOTYPE((FILE *, int));

extern krb5_error_code krb5_authorization
	PROTOTYPE((krb5_context, krb5_principal, const char *, char *, 
		   krb5_boolean *, char **));

extern krb5_error_code k5login_lookup PROTOTYPE((FILE *, char *,
						 krb5_boolean *));

extern krb5_error_code k5users_lookup 
	PROTOTYPE((FILE *, char *, char *, krb5_boolean *, char **));

extern krb5_boolean fcmd_resolve
	PROTOTYPE((char *, char ***, char **));

extern krb5_boolean cmd_single PROTOTYPE((char *));

extern int cmd_arr_cmp_postfix PROTOTYPE((char **, char *));

extern int cmd_arr_cmp PROTOTYPE((char **, char *));

extern krb5_boolean find_first_cmd_that_exists 
	PROTOTYPE((char **, char **, char **));

extern int match_commands 
	PROTOTYPE((char *, char *, krb5_boolean *, char **, char **));

extern krb5_error_code get_line PROTOTYPE((FILE *, char **));

extern char *  get_first_token PROTOTYPE((char *, char **));

extern char *  get_next_token PROTOTYPE((char **));

extern krb5_boolean fowner PROTOTYPE((FILE *, int));

extern void init_auth_names PROTOTYPE((char *));

/* main.c */
extern void usage PROTOTYPE((void));

extern int standard_shell PROTOTYPE((char *));

extern krb5_error_code get_params PROTOTYPE((int *, int, char **, char ***));

extern char *get_dir_of_file PROTOTYPE((const char *));

/* heuristic.c */
extern krb5_error_code get_all_princ_from_file PROTOTYPE((FILE *, char ***));

extern krb5_error_code list_union PROTOTYPE((char **, char **, char ***));

extern krb5_error_code filter PROTOTYPE((FILE *, char *, char **, char ***));

extern krb5_error_code get_authorized_princ_names
	PROTOTYPE((const char *, char *, char ***));

extern krb5_error_code get_closest_principal 
	PROTOTYPE((krb5_context, char **, krb5_principal *, krb5_boolean *));

extern krb5_error_code find_either_ticket 
	PROTOTYPE((krb5_context, krb5_ccache, krb5_principal,
		krb5_principal, krb5_boolean *));

extern krb5_error_code find_ticket 
	PROTOTYPE((krb5_context, krb5_ccache, krb5_principal,
		krb5_principal, krb5_boolean *));


extern krb5_error_code find_princ_in_list
	PROTOTYPE((krb5_context, krb5_principal, char **, krb5_boolean *));

extern krb5_error_code get_best_princ_for_target
	PROTOTYPE((krb5_context, int, int, char *, char *, krb5_ccache, 
		opt_info *, char *, char *, krb5_principal *, int *));

#ifndef min
#define min(a,b) ((a) > (b) ? (b) : (a))
#endif /* min */


extern char *krb5_lname_file;  /* Note: print this out just be sure
				  that it gets set */   	    

extern void *xmalloc (), *xrealloc (), *xcalloc();
extern char *xstrdup ();
