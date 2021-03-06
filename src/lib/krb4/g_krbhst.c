/*
 * g_krbhst.c
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#include "mit-copyright.h"
#include <stdio.h>
#include "krb.h"
#include <string.h>

/*
 * Given a Kerberos realm, find a host on which the Kerberos authenti-
 * cation server can be found.
 *
 * krb_get_krbhst takes a pointer to be filled in, a pointer to the name
 * of the realm for which a server is desired, and an integer, n, and
 * returns (in h) the nth entry from the configuration file (KRB_CONF,
 * defined in "krb.h") associated with the specified realm.
 *
 * On end-of-file, krb_get_krbhst returns KFAILURE.  If n=1 and the
 * configuration file does not exist, krb_get_krbhst will return KRB_HOST
 * (also defined in "krb.h").  If all goes well, the routine returnes
 * KSUCCESS.
 *
 * The KRB_CONF file contains the name of the local realm in the first
 * line (not used by this routine), followed by lines indicating realm/host
 * entries.  The words "admin server" following the hostname indicate that 
 * the host provides an administrative database server.
 * This will also look in KRB_FB_CONF if ATHENA_CONF_FALLBACK is defined.
 *
 * For example:
 *
 *	ATHENA.MIT.EDU
 *	ATHENA.MIT.EDU kerberos-1.mit.edu admin server
 *	ATHENA.MIT.EDU kerberos-2.mit.edu
 *	LCS.MIT.EDU kerberos.lcs.mit.edu admin server
 *
 * This is a temporary hack to allow us to find the nearest system running
 * kerberos.  In the long run, this functionality will be provided by a
 * nameserver.
 */

static int
get_krbhst_default(h, r, n)
     char *h;
     char *r;
     int n;
{
    if (n==1) {
        (void) strncpy(h,KRB_HOST,MAXHOSTNAMELEN-1);
	h[MAXHOSTNAMELEN-1] = '\0';
	(void) strncat(h,".",MAXHOSTNAMELEN-1-strlen(h));
	(void) strncat(h,r,MAXHOSTNAMELEN-1-strlen(h));
				/* KRB_HOST.REALM (ie. kerberos.CYGNUS.COM) */
	return(KSUCCESS);
    }
    else
        return(KFAILURE);
}

KRB5_DLLIMP int KRB5_CALLCONV
krb_get_krbhst(h,r,n)
    char FAR *h;
    char FAR *r;
    int n;
{
    FILE *cnffile, *krb__get_cnffile();
    char tr[REALM_SZ];
    char linebuf[BUFSIZ];
    register int i;

    cnffile = krb__get_cnffile();
    if (!cnffile)
        return get_krbhst_default(h, r, n);
    if (fscanf(cnffile,"%39s",tr) == EOF) /* XXX assumes REALM_SZ == 40 */
        return get_krbhst_default(h, r, n);
    /* run through the file, looking for the nth server for this realm */
    for (i = 1; i <= n;) {
	if (fgets(linebuf, BUFSIZ, cnffile) == NULL) {
            (void) fclose(cnffile);
            return get_krbhst_default(h, r, n);
        }
	if (sscanf(linebuf, "%39s %1023s", tr, h) != 2)	/* REALM_SZ == 40 */
	    continue;
        if (!strcmp(tr,r))
            i++;
    }
    (void) fclose(cnffile);
    return(KSUCCESS);
}
