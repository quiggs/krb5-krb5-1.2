/*
 * klog.c
 *
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 *
 * For copying and distribution information, please see the file
 * <mit-copyright.h>.
 */

#include "mit-copyright.h"
#include "krb.h"
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#if !defined(VMS) && !defined(_WINDOWS)
#include <sys/time.h>
#endif
#include <stdio.h>

#include "krb.h"
#include <klog.h>

static char *log_name = KRBLOG;
static int is_open;
static char logtxt[1000];

/*
 * This file contains two logging routines: kset_logfile()
 * to determine the file to which log entries should be written;
 * and klog() to write log entries to the file.
 */

/*
 * klog() is used to add entries to the logfile (see kset_logfile()
 * below).  Note that it is probably not portable since it makes
 * assumptions about what the compiler will do when it is called
 * with less than the correct number of arguments which is the
 * way it is usually called.
 *
 * The log entry consists of a timestamp and the given arguments
 * printed according to the given "format" string.
 *
 * The log file is opened and closed for each log entry.
 *
 * If the given log type "type" is unknown, or if the log file
 * cannot be opened, no entry is made to the log file.
 *
 * The return value is always a pointer to the formatted log
 * text string "logtxt".
 */

char * klog(type,format,a1,a2,a3,a4,a5,a6,a7,a8,a9,a0)
    int type;
    char *format;
    char *a1,*a2,*a3,*a4,*a5,*a6,*a7,*a8,*a9,*a0;
{
    FILE *logfile;
    time_t now;
    char *month_sname();
    struct tm *tm;
    static int logtype_array[NLOGTYPE];
    static int array_initialized;

    if (!(array_initialized++)) {
        logtype_array[L_NET_ERR] = 1;
        logtype_array[L_KRB_PERR] = 1;
        logtype_array[L_KRB_PWARN] = 1;
        logtype_array[L_APPL_REQ] = 1;
        logtype_array[L_INI_REQ] = 1;
        logtype_array[L_DEATH_REQ] = 1;
        logtype_array[L_NTGT_INTK] = 1;
        logtype_array[L_ERR_SEXP] = 1;
        logtype_array[L_ERR_MKV] = 1;
        logtype_array[L_ERR_NKY] = 1;
        logtype_array[L_ERR_NUN] = 1;
        logtype_array[L_ERR_UNK] = 1;
    }

    (void) sprintf(logtxt,format,a1,a2,a3,a4,a5,a6,a7,a8,a9,a0);

    if (!logtype_array[type])
	return(logtxt);

    if ((logfile = fopen(log_name,"a")) == NULL)
        return(logtxt);

    (void) time(&now);
    tm = localtime(&now);

    fprintf(logfile,"%2d-%s-%d %02d:%02d:%02d ",tm->tm_mday,
            month_sname(tm->tm_mon + 1),1900+tm->tm_year,
            tm->tm_hour, tm->tm_min, tm->tm_sec);
    fprintf(logfile,"%s\n",logtxt);
    (void) fclose(logfile);
    return(logtxt);
}

/*
 * kset_logfile() changes the name of the file to which
 * messages are logged.  If kset_logfile() is not called,
 * the logfile defaults to KRBLOG, defined in "krb.h".
 */

kset_logfile(filename)
    char *filename;
{
    log_name = filename;
    is_open = 0;
}
