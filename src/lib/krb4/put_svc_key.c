/* lib/krb/put_svc_key.c */
/* Copyright 1994 Cygnus Support */
/* Mark W. Eichin */
/*
 * Permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation.
 * Cygnus Support makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * put_svc_key is a simple version of what 'ksrvutil add' provides, for some
 *    circumstances when service keys are distributed by applictions.
 *
 * Caveats: currently uses UNIX I/O (open, read) rather than stdio - this 
 *    should be fixed.
 *          It could probably be made more general (and then actually be used
 *    by ksrvutil.) This version supports just enough to be useful.
 */

#include "krb.h"

#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

extern char *krb__get_srvtabname();

#define KEYSZ sizeof(C_Block)
/* strict put_svc_key.
   The srvtab must already exist;
   The key (exact match) must already be in the file;
   version numbers are not checked.
 */
KRB5_DLLIMP int KRB5_CALLCONV
put_svc_key(sfile,name,inst,realm,newvno,key)
	char FAR *sfile;
	char FAR *name;
	char FAR *inst;
	char FAR *realm;
	int newvno;
	char FAR *key;
{
	int fd;
	char fname[SNAME_SZ], finst[INST_SZ], frlm[REALM_SZ];
	unsigned char fvno;
	char fkey[KEYSZ];

	if (!sfile)
		sfile = KEYFILE;

	if ((fd = open(sfile, O_RDWR)) < 0)
		return KFAILURE;

	while(getst(fd,fname,SNAME_SZ) > 0) {
		getst(fd,finst,INST_SZ);
		getst(fd,frlm,REALM_SZ);
		if (!strcmp(fname,name)
		    && !strcmp(finst,inst)
		    && !strcmp(frlm,realm)) {
			/* all matched, so write new data */
			fvno = newvno;
			lseek(fd,0,SEEK_CUR);
			if (write(fd,&fvno,1) != 1) {
				close(fd);
				return KFAILURE;
			}
			if (write(fd,key,KEYSZ) != KEYSZ) {
				close(fd);
				return KFAILURE;
			}
			close(fd);
			return KSUCCESS;
		}
                if (read(fd,&fvno,1) != 1) {
                        close(fd);
                        return KFAILURE;
                }
                if (read(fd,fkey,KEYSZ) != KEYSZ) {
                        close(fd);
                        return KFAILURE;
                }
	}
	/* never found it */
	close(fd);
	return KFAILURE;
}
