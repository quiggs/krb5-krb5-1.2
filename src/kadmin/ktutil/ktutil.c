/*
 * kadmin/ktutil/ktutil.c
 *
 * Copyright 1995, 1996 by the Massachusetts Institute of Technology.
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
 * SS user interface for ktutil.
 */

#include "k5-int.h"
#include "ktutil.h"
#include <com_err.h>
#include "adm_proto.h"
#include <ss/ss.h>
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

extern ss_request_table ktutil_cmds;
krb5_context kcontext;
krb5_kt_list ktlist = NULL;

int main(argc, argv)
    int argc;
    char *argv[];
{
    krb5_error_code retval;
    extern krb5_kt_ops krb5_ktf_writable_ops;
    int sci_idx;

    retval = krb5_init_context(&kcontext);
    if (retval) {
        com_err(argv[0], retval, "while initializing krb5");
	exit(1);
    }
    retval = krb5_kt_register(kcontext, &krb5_ktf_writable_ops);
    if (retval) {
	com_err(argv[0], retval,
		"while registering writable key table functions");
	exit(1);
    }
    sci_idx = ss_create_invocation("ktutil", "5.0", (char *)NULL,
				   &ktutil_cmds, &retval);
    if (retval) {
	ss_perror(sci_idx, retval, "creating invocation");
	exit(1);
    }
    ss_listen(sci_idx, &retval);
    ktutil_free_kt_list(kcontext, ktlist);
    exit(0);
}

void ktutil_clear_list(argc, argv)
    int argc;
    char *argv[];
{
    krb5_error_code retval;

    if (argc != 1) {
	fprintf(stderr, "%s: invalid arguments\n", argv[0]);
	return;
    }
    retval = ktutil_free_kt_list(kcontext, ktlist);
    if (retval)
	com_err(argv[0], retval, "while freeing ktlist");
    ktlist = NULL;
}

void ktutil_read_v5(argc, argv)
    int argc;
    char *argv[];
{
    krb5_error_code retval;

    if (argc != 2) {
	fprintf(stderr, "%s: must specify keytab to read\n", argv[0]);
	return;
    }
    retval = ktutil_read_keytab(kcontext, argv[1], &ktlist);
    if (retval)
	com_err(argv[0], retval, "while reading keytab \"%s\"", argv[1]);
}

void ktutil_read_v4(argc, argv)
    int argc;
    char *argv[];
{
#ifdef KRB5_KRB4_COMPAT
    krb5_error_code retval;

    if (argc != 2) {
	fprintf(stderr, "%s: must specify the srvtab to read\n", argv[0]);
	return;
    }
    retval = ktutil_read_srvtab(kcontext, argv[1], &ktlist);
    if (retval)
	com_err(argv[0], retval, "while reading srvtab \"%s\"", argv[1]);
#else
    fprintf(stderr, "%s: krb4 support not configured\n", argv[0]);
#endif
}

void ktutil_write_v5(argc, argv)
    int argc;
    char *argv[];
{
    krb5_error_code retval;

    if (argc != 2) {
	fprintf(stderr, "%s: must specify keytab to write\n", argv[0]);
	return;
    }
    retval = ktutil_write_keytab(kcontext, ktlist, argv[1]);
    if (retval)
	com_err(argv[0], retval, "while writing keytab \"%s\"", argv[1]);
}

void ktutil_write_v4(argc, argv)
    int argc;
    char *argv[];
{
#ifdef KRB5_KRB4_COMPAT
    krb5_error_code retval;

    if (argc != 2) {
	fprintf(stderr, "%s: must specify srvtab to write\n", argv[0]);
	return;
    }
    retval = ktutil_write_srvtab(kcontext, ktlist, argv[1]);
    if (retval)
	com_err(argv[0], retval, "while writing srvtab \"%s\"", argv[1]);
#else
    fprintf(stderr, "%s: krb4 support not configured\n", argv[0]);
#endif
}

void ktutil_add_entry(argc, argv)
    int argc;
    char *argv[];
{
    krb5_error_code retval;
    char *princ = NULL;
    char *enctype = NULL;
    krb5_kvno kvno = 0;
    int use_pass = 0, use_key = 0, i;    

    for (i = 1; i < argc; i++) {
	if ((strlen(argv[i]) == 2) && !strncmp(argv[i], "-p", 2)) {
	    princ = argv[++i];
	    continue;
	}
	if ((strlen(argv[i]) == 2) && !strncmp(argv[i], "-k", 2)) {
	    kvno = (krb5_kvno) atoi(argv[++i]);
	    continue;
	}
	if ((strlen(argv[i]) == 2) && !strncmp(argv[i], "-e", 2)) {
	    enctype = argv[++i];
	    continue;
	}
	if ((strlen(argv[i]) == 9) && !strncmp(argv[i], "-password", 9)) {
	    use_pass++;
	    continue;
	}
	if ((strlen(argv[i]) == 4) && !strncmp(argv[i], "-key", 4)) {
	    use_key++;
	    continue;
	}
    }

    if (argc != 8 || !(princ && kvno && enctype) || (use_pass+use_key != 1)) {
        fprintf(stderr, "usage: %s (-key | -password) -p principal "
		"-k kvno -e enctype\n", argv[0]);
	return;
    }

    retval = ktutil_add(kcontext, &ktlist, princ, kvno, enctype, use_pass);
    if (retval)
        com_err(argv[0], retval, "while adding new entry");
}

void ktutil_delete_entry(argc, argv)
    int argc;
    char *argv[];
{
    krb5_error_code retval;

    if (argc != 2) {
	fprintf(stderr, "%s: must specify entry to delete\n", argv[0]);
	return;
    }
    retval = ktutil_delete(kcontext, &ktlist, atoi(argv[1]));
    if (retval)
	com_err(argv[0], retval, "while deleting entry %d", atoi(argv[1]));
}

void ktutil_list(argc, argv)
    int argc;
    char *argv[];
{
    krb5_error_code retval;
    krb5_kt_list lp;
    struct tm *stime;
    int show_time = 0, show_keys = 0, show_enctype = 0;
    int i, j;
    char *pname;

    for (i = 1; i < argc; i++) {
	if ((strlen(argv[i]) == 2) && !strncmp(argv[i], "-t", 2)) {
	    show_time++;
	    continue;
	}
	if ((strlen(argv[i]) == 2) && !strncmp(argv[i], "-k", 2)) {
	    show_keys++;
	    continue;
	}
	if ((strlen(argv[i]) == 2) && !strncmp(argv[i], "-e", 2)) {
	    show_enctype++;
	    continue;
	}
	
	fprintf(stderr, "%s: illegal arguments\n", argv[0]);
	return;
    }
    if (show_time) {
	printf("slot KVNO Timestamp         Principal\n");
	printf("---- ---- ----------------- ---------------------------------------------------\n");
    } else {
	printf("slot KVNO Principal\n");
	printf("---- ---- ---------------------------------------------------------------------\n");
    }
    for (i = 1, lp = ktlist; lp; i++, lp = lp->next) {
	retval = krb5_unparse_name(kcontext, lp->entry->principal, &pname);
	if (retval) {
	    com_err(argv[0], retval, "while unparsing principal name");
	    return;
	}
	printf("%4d %4d ", i, lp->entry->vno);
	if (show_time) {
	    char fmtbuf[18];
	    char fill;

	    stime = localtime((time_t *)&lp->entry->timestamp);
	    fill = ' ';
	    if (!krb5_timestamp_to_sfstring((krb5_timestamp)lp->entry->
					    	timestamp,
					    fmtbuf,
					    sizeof(fmtbuf),
					    &fill))
		printf("%s ", fmtbuf);
	}
	printf("%40s", pname);
	if (show_enctype) {
	    static char buf[256];
		if ((retval = krb5_enctype_to_string(
		    lp->entry->key.enctype, buf, 256))) {
		    com_err(argv[0], retval, "While converting enctype to string");
		    return;
		}
	    printf(" (%s) ", buf);
	}
	
	if (show_keys) {
	    printf(" (0x");
	    for (j = 0; j < lp->entry->key.length; j++)
		printf("%02x", lp->entry->key.contents[j]);
	    printf(")");
	}
	printf("\n");
	krb5_xfree(pname);
    }
}










