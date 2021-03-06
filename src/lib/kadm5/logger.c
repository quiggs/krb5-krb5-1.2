/*
 * lib/kadm/logger.c
 *
 * Copyright 1995 by the Massachusetts Institute of Technology.
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

#if !defined(_MSDOS)

/* KADM5 wants non-syslog log files to contain syslog-like entries */
#define VERBOSE_LOGS

/*
 * logger.c	- Handle logging functions for those who want it.
 */
#include "k5-int.h"
#include "adm_proto.h"
#include "com_err.h"
#include <stdio.h>
#include <ctype.h>
#ifdef	HAVE_SYSLOG_H
#include <syslog.h>
#endif	/* HAVE_SYSLOG_H */
#ifdef	HAVE_STDARG_H
#include <stdarg.h>
#else	/* HAVE_STDARG_H */
#include <varargs.h>
#endif	/* HAVE_STDARG_H */

#define	KRB5_KLOG_MAX_ERRMSG_SIZE	1024
#ifndef	MAXHOSTNAMELEN
#define	MAXHOSTNAMELEN	256
#endif	/* MAXHOSTNAMELEN */

/* This is to assure that we have at least one match in the syslog stuff */
#ifndef	LOG_AUTH
#define	LOG_AUTH	0
#endif	/* LOG_AUTH */
#ifndef	LOG_ERR
#define	LOG_ERR		0
#endif	/* LOG_ERR */

#define lspec_parse_err_1	"%s: cannot parse <%s>\n"
#define lspec_parse_err_2	"%s: warning - logging entry syntax error\n"
#define log_file_err		"%s: error writing to %s\n"
#define log_device_err		"%s: error writing to %s device\n"
#define log_ufo_string		"?\?\?" /* nb: avoid trigraphs */
#define log_emerg_string	"EMERGENCY"
#define log_alert_string	"ALERT"
#define log_crit_string		"CRITICAL"
#define log_err_string		"Error"
#define log_warning_string	"Warning"
#define log_notice_string	"Notice"
#define log_info_string		"info"
#define log_debug_string	"debug"

/*
 * Output logging.
 *
 * Output logging is now controlled by the configuration file.  We can specify
 * the following syntaxes under the [logging]->entity specification.
 *	FILE<opentype><pathname>
 *	SYSLOG[=<severity>[:<facility>]]
 *	STDERR
 *	CONSOLE
 *	DEVICE=<device-spec>
 *
 * Where:
 *	<opentype> is ":" for open/append, "=" for open/create.
 *	<pathname> is a valid path name.
 *	<severity> is one of: (default = ERR)
 *		EMERG
 *		ALERT
 *		CRIT
 *		ERR
 *		WARNING
 *		NOTICE
 *		INFO
 *		DEBUG
 *	<facility> is one of: (default = AUTH)
 *		KERN
 *		USER
 *		MAIL
 *		DAEMON
 *		AUTH
 *		LPR
 *		NEWS
 *		UUCP
 *		CRON
 *		LOCAL0..LOCAL7
 *	<device-spec> is a valid device specification.
 */
struct log_entry {
    enum log_type { K_LOG_FILE,
			K_LOG_SYSLOG,
			K_LOG_STDERR,
			K_LOG_CONSOLE,
			K_LOG_DEVICE,
			K_LOG_NONE } log_type;
    krb5_pointer log_2free;
    union log_union {
	struct log_file {
	    FILE	*lf_filep;
	    char	*lf_fname;
	} log_file;
	struct log_syslog {
	    int		ls_facility;
	    int		ls_severity;
	} log_syslog;
	struct log_device {
	    FILE	*ld_filep;
	    char	*ld_devname;
	} log_device;
    } log_union;
};
#define	lfu_filep	log_union.log_file.lf_filep
#define	lfu_fname	log_union.log_file.lf_fname
#define	lsu_facility	log_union.log_syslog.ls_facility
#define	lsu_severity	log_union.log_syslog.ls_severity
#define	ldu_filep	log_union.log_device.ld_filep
#define	ldu_devname	log_union.log_device.ld_devname

struct log_control {
    struct log_entry	*log_entries;
    int			log_nentries;
    char		*log_whoami;
    char		*log_hostname;
    krb5_boolean	log_opened;
};

static struct log_control log_control = {
    (struct log_entry *) NULL,
    0,
    (char *) NULL,
    (char *) NULL,
    0
};
static struct log_entry	def_log_entry;

/*
 * These macros define any special processing that needs to happen for
 * devices.  For unix, of course, this is hardly anything.
 */
#define	DEVICE_OPEN(d, m)	fopen(d, m)
#define	CONSOLE_OPEN(m)		fopen("/dev/console", m)
#define	DEVICE_PRINT(f, m)	((fprintf(f, "%s\r\n", m) >= 0) ? 	\
				 (fflush(f), 0) :			\
				 -1)
#define	DEVICE_CLOSE(d)		fclose(d)


/*
 * klog_com_err_proc()	- Handle com_err(3) messages as specified by the
 *			  profile.
 */
static void
klog_com_err_proc(whoami, code, format, ap)
    const char	*whoami;
    long	code;
    const char	*format;
    va_list	ap;
{
    char	outbuf[KRB5_KLOG_MAX_ERRMSG_SIZE];
    int		lindex;
    char	*actual_format;
#ifdef	HAVE_SYSLOG
    int		log_pri = -1;
#endif	/* HAVE_SYSLOG */
    char	*cp;
    char	*syslogp;

    /* Make the header */
    sprintf(outbuf, "%s: ", whoami);
    /*
     * Squirrel away address after header for syslog since syslog makes
     * a header
     */
    syslogp = &outbuf[strlen(outbuf)];

    /* If reporting an error message, separate it. */
    if (code) {
        outbuf[sizeof(outbuf) - 1] = '\0';
	strncat(outbuf, error_message(code), sizeof(outbuf) - 1 - strlen(outbuf));
	strncat(outbuf, " - ", sizeof(outbuf) - 1 - strlen(outbuf));
    }
    cp = &outbuf[strlen(outbuf)];
    
    actual_format = (char *) format;
#ifdef	HAVE_SYSLOG
    /*
     * This is an unpleasant hack.  If the first character is less than
     * 8, then we assume that it is a priority.
     *
     * Since it is not guaranteed that there is a direct mapping between
     * syslog priorities (e.g. Ultrix and old BSD), we resort to this
     * intermediate representation.
     */
    if ((((unsigned char) *format) > 0) && (((unsigned char) *format) <= 8)) {
	actual_format = (char *) (format + 1);
	switch ((unsigned char) *format) {
#ifdef	LOG_EMERG
	case 1:
	    log_pri = LOG_EMERG;
	    break;
#endif /* LOG_EMERG */
#ifdef	LOG_ALERT
	case 2:
	    log_pri = LOG_ALERT;
	    break;
#endif /* LOG_ALERT */
#ifdef	LOG_CRIT
	case 3:
	    log_pri = LOG_CRIT;
	    break;
#endif /* LOG_CRIT */
	default:
	case 4:
	    log_pri = LOG_ERR;
	    break;
#ifdef	LOG_WARNING
	case 5:
	    log_pri = LOG_WARNING;
	    break;
#endif /* LOG_WARNING */
#ifdef	LOG_NOTICE
	case 6:
	    log_pri = LOG_NOTICE;
	    break;
#endif /* LOG_NOTICE */
#ifdef	LOG_INFO
	case 7:
	    log_pri = LOG_INFO;
	    break;
#endif /* LOG_INFO */
#ifdef	LOG_DEBUG
	case 8:
	    log_pri = LOG_DEBUG;
	    break;
#endif /* LOG_DEBUG */
	}
    } 
#endif	/* HAVE_SYSLOG */

    /* Now format the actual message */
#if	HAVE_VSPRINTF
    vsprintf(cp, actual_format, ap);
#else	/* HAVE_VSPRINTF */
    sprintf(cp, actual_format, ((int *) ap)[0], ((int *) ap)[1],
	    ((int *) ap)[2], ((int *) ap)[3],
	    ((int *) ap)[4], ((int *) ap)[5]);
#endif	/* HAVE_VSPRINTF */
    
    /*
     * Now that we have the message formatted, perform the output to each
     * logging specification.
     */
    for (lindex = 0; lindex < log_control.log_nentries; lindex++) {
	switch (log_control.log_entries[lindex].log_type) {
	case K_LOG_FILE:
	case K_LOG_STDERR:
	    /*
	     * Files/standard error.
	     */
	    if (fprintf(log_control.log_entries[lindex].lfu_filep, "%s\n",
			outbuf) < 0) {
		/* Attempt to report error */
		fprintf(stderr, log_file_err, whoami,
			log_control.log_entries[lindex].lfu_fname);
	    }
	    else {
		fflush(log_control.log_entries[lindex].lfu_filep);
	    }
	    break;
	case K_LOG_CONSOLE:
	case K_LOG_DEVICE:
	    /*
	     * Devices (may need special handling)
	     */
	    if (DEVICE_PRINT(log_control.log_entries[lindex].ldu_filep,
			     outbuf) < 0) {
		/* Attempt to report error */
		fprintf(stderr, log_device_err, whoami,
			log_control.log_entries[lindex].ldu_devname);
	    }
	    break;
#ifdef	HAVE_SYSLOG
	case K_LOG_SYSLOG:
	    /*
	     * System log.
	     */
	    /*
	     * If we have specified a priority through our hackery, then
	     * use it, otherwise use the default.
	     */
	    if (log_pri >= 0)
		log_pri |= log_control.log_entries[lindex].lsu_facility;
	    else
		log_pri = log_control.log_entries[lindex].lsu_facility |
		    log_control.log_entries[lindex].lsu_severity;
					       
	    /* Log the message with our header trimmed off */
	    syslog(log_pri, "%s", syslogp);
	    break;
#endif /* HAVE_SYSLOG */
	default:
	    break;
	}
    }
}

/*
 * krb5_klog_init()	- Initialize logging.
 *
 * This routine parses the syntax described above to specify destinations for
 * com_err(3) or krb5_klog_syslog() messages generated by the caller.
 *
 * Parameters:
 *	kcontext	- Kerberos context.
 *	ename		- Entity name as it is to appear in the profile.
 *	whoami		- Entity name as it is to appear in error output.
 *	do_com_err	- Take over com_err(3) processing.
 *
 * Implicit inputs:
 *	stderr		- This is where STDERR output goes.
 *
 * Implicit outputs:
 *	log_nentries	- Number of log entries, both valid and invalid.
 *	log_control	- List of entries (log_nentries long) which contains
 *			  data for klog_com_err_proc() to use to determine
 *			  where/how to send output.
 */
krb5_error_code
krb5_klog_init(kcontext, ename, whoami, do_com_err)
    krb5_context	kcontext;
    char		*ename;
    char		*whoami;
    krb5_boolean	do_com_err;
{
    const char	*logging_profent[3];
    const char	*logging_defent[3];
    char	**logging_specs;
    int		i, ngood;
    char	*cp, *cp2;
    char	savec;
    int		error;
    int		do_openlog, log_facility;
    FILE	*f;

    /* Initialize */
    do_openlog = 0;
    log_facility = 0;

    /*
     * Look up [logging]-><ename> in the profile.  If that doesn't
     * succeed, then look for [logging]->default.
     */
    logging_profent[0] = "logging";
    logging_profent[1] = ename;
    logging_profent[2] = (char *) NULL;
    logging_defent[0] = "logging";
    logging_defent[1] = "default";
    logging_defent[2] = (char *) NULL;
    logging_specs = (char **) NULL;
    ngood = 0;
    log_control.log_nentries = 0;
    if (!profile_get_values(kcontext->profile,
			    logging_profent,
			    &logging_specs) ||
	!profile_get_values(kcontext->profile,
			    logging_defent,
			    &logging_specs)) {
	/*
	 * We have a match, so we first count the number of elements
	 */
	for (log_control.log_nentries = 0;
	     logging_specs[log_control.log_nentries];
	     log_control.log_nentries++);

	/*
	 * Now allocate our structure.
	 */
	log_control.log_entries = (struct log_entry *)
	    malloc(log_control.log_nentries * sizeof(struct log_entry));
	if (log_control.log_entries) {
	    /*
	     * Scan through the list.
	     */
	    for (i=0; i<log_control.log_nentries; i++) {
		log_control.log_entries[i].log_type = K_LOG_NONE;
		log_control.log_entries[i].log_2free = logging_specs[i];
		/*
		 * The format is:
		 *	<whitespace><data><whitespace>
		 * so, trim off the leading and trailing whitespace here.
		 */
		for (cp = logging_specs[i]; isspace(*cp); cp++);
		for (cp2 = &logging_specs[i][strlen(logging_specs[i])-1];
		     isspace(*cp2); cp2--);
		cp2++;
		*cp2 = '\0';
		/*
		 * Is this a file?
		 */
		if (!strncasecmp(cp, "FILE", 4)) {
		    /*
		     * Check for append/overwrite, then open the file.
		     */
		    if (cp[4] == ':' || cp[4] == '=') {
			f = fopen(&cp[5], (cp[4] == ':') ? "a+" : "w");
			if (f) {
			    log_control.log_entries[i].lfu_filep = f;
			    log_control.log_entries[i].log_type = K_LOG_FILE;
			    log_control.log_entries[i].lfu_fname = &cp[5];
			} else {
			    fprintf(stderr,"Couldn't open log file %s: %s\n",
				    &cp[5], error_message(errno));
			    continue;
			}
		    }
		}
#ifdef	HAVE_SYSLOG
		/*
		 * Is this a syslog?
		 */
		else if (!strncasecmp(cp, "SYSLOG", 6)) {
		    error = 0;
		    log_control.log_entries[i].lsu_facility = LOG_AUTH;
		    log_control.log_entries[i].lsu_severity = LOG_ERR;
		    /*
		     * Is there a severify specified?
		     */
		    if (cp[6] == ':') {
			/*
			 * Find the end of the severity.
			 */
			if (cp2 = strchr(&cp[7], ':')) {
			    savec = *cp2;
			    *cp2 = '\0';
			    cp2++;
			}

			/*
			 * Match a severity.
			 */
			if (!strcasecmp(&cp[7], "ERR")) {
			    log_control.log_entries[i].lsu_severity = LOG_ERR;
			}
#ifdef	LOG_EMERG
			else if (!strcasecmp(&cp[7], "EMERG")) {
			    log_control.log_entries[i].lsu_severity =
				LOG_EMERG;
			}
#endif	/* LOG_EMERG */
#ifdef	LOG_ALERT
			else if (!strcasecmp(&cp[7], "ALERT")) {
			    log_control.log_entries[i].lsu_severity =
				LOG_ALERT;
			}
#endif	/* LOG_ALERT */
#ifdef	LOG_CRIT
			else if (!strcasecmp(&cp[7], "CRIT")) {
			    log_control.log_entries[i].lsu_severity = LOG_CRIT;
			}
#endif	/* LOG_CRIT */
#ifdef	LOG_WARNING
			else if (!strcasecmp(&cp[7], "WARNING")) {
			    log_control.log_entries[i].lsu_severity =
				LOG_WARNING;
			}
#endif	/* LOG_WARNING */
#ifdef	LOG_NOTICE
			else if (!strcasecmp(&cp[7], "NOTICE")) {
			    log_control.log_entries[i].lsu_severity =
				LOG_NOTICE;
			}
#endif	/* LOG_NOTICE */
#ifdef	LOG_INFO
			else if (!strcasecmp(&cp[7], "INFO")) {
			    log_control.log_entries[i].lsu_severity = LOG_INFO;
			}
#endif	/* LOG_INFO */
#ifdef	LOG_DEBUG
			else if (!strcasecmp(&cp[7], "DEBUG")) {
			    log_control.log_entries[i].lsu_severity =
				LOG_DEBUG;
			}
#endif	/* LOG_DEBUG */
			else
			    error = 1;

			/*
			 * If there is a facility present, then parse that.
			 */
			if (cp2) {
			    if (!strcasecmp(cp2, "AUTH")) {
				log_control.log_entries[i].lsu_facility = LOG_AUTH;
			    }
#ifdef	LOG_KERN
			    else if (!strcasecmp(cp2, "KERN")) {
				log_control.log_entries[i].lsu_facility = LOG_KERN;
			    }
#endif	/* LOG_KERN */
#ifdef	LOG_USER
			    else if (!strcasecmp(cp2, "USER")) {
				log_control.log_entries[i].lsu_facility = LOG_USER;
			    }
#endif	/* LOG_USER */
#ifdef	LOG_MAIL
			    else if (!strcasecmp(cp2, "MAIL")) {
				log_control.log_entries[i].lsu_facility = LOG_MAIL;
			    }
#endif	/* LOG_MAIL */
#ifdef	LOG_DAEMON
			    else if (!strcasecmp(cp2, "DAEMON")) {
				log_control.log_entries[i].lsu_facility = LOG_DAEMON;
			    }
#endif	/* LOG_DAEMON */
#ifdef	LOG_LPR
			    else if (!strcasecmp(cp2, "LPR")) {
				log_control.log_entries[i].lsu_facility = LOG_LPR;
			    }
#endif	/* LOG_LPR */
#ifdef	LOG_NEWS
			    else if (!strcasecmp(cp2, "NEWS")) {
				log_control.log_entries[i].lsu_facility = LOG_NEWS;
			    }
#endif	/* LOG_NEWS */
#ifdef	LOG_UUCP
			    else if (!strcasecmp(cp2, "UUCP")) {
				log_control.log_entries[i].lsu_facility = LOG_UUCP;
			    }
#endif	/* LOG_UUCP */
#ifdef	LOG_CRON
			    else if (!strcasecmp(cp2, "CRON")) {
				log_control.log_entries[i].lsu_facility = LOG_CRON;
			    }
#endif	/* LOG_CRON */
#ifdef	LOG_LOCAL0
			    else if (!strcasecmp(cp2, "LOCAL0")) {
				log_control.log_entries[i].lsu_facility = LOG_LOCAL0;
			    }
#endif	/* LOG_LOCAL0 */
#ifdef	LOG_LOCAL1
			    else if (!strcasecmp(cp2, "LOCAL1")) {
				log_control.log_entries[i].lsu_facility = LOG_LOCAL1;
			    }
#endif	/* LOG_LOCAL1 */
#ifdef	LOG_LOCAL2
			    else if (!strcasecmp(cp2, "LOCAL2")) {
				log_control.log_entries[i].lsu_facility = LOG_LOCAL2;
			    }
#endif	/* LOG_LOCAL2 */
#ifdef	LOG_LOCAL3
			    else if (!strcasecmp(cp2, "LOCAL3")) {
				log_control.log_entries[i].lsu_facility = LOG_LOCAL3;
			    }
#endif	/* LOG_LOCAL3 */
#ifdef	LOG_LOCAL4
			    else if (!strcasecmp(cp2, "LOCAL4")) {
				log_control.log_entries[i].lsu_facility = LOG_LOCAL4;
			    }
#endif	/* LOG_LOCAL4 */
#ifdef	LOG_LOCAL5
			    else if (!strcasecmp(cp2, "LOCAL5")) {
				log_control.log_entries[i].lsu_facility = LOG_LOCAL5;
			    }
#endif	/* LOG_LOCAL5 */
#ifdef	LOG_LOCAL6
			    else if (!strcasecmp(cp2, "LOCAL6")) {
				log_control.log_entries[i].lsu_facility = LOG_LOCAL6;
			    }
#endif	/* LOG_LOCAL6 */
#ifdef	LOG_LOCAL7
			    else if (!strcasecmp(cp2, "LOCAL7")) {
				log_control.log_entries[i].lsu_facility = LOG_LOCAL7;
			    }
#endif	/* LOG_LOCAL7 */
			    cp2--;
			    *cp2 = savec;
			}
		    }
		    if (!error) {
			log_control.log_entries[i].log_type = K_LOG_SYSLOG;
			do_openlog = 1;
			log_facility = log_control.log_entries[i].lsu_facility;
		    }
		}
#endif	/* HAVE_SYSLOG */
		/*
		 * Is this a standard error specification?
		 */
		else if (!strcasecmp(cp, "STDERR")) {
		    if (log_control.log_entries[i].lfu_filep =
			fdopen(fileno(stderr), "a+")) {
			log_control.log_entries[i].log_type = K_LOG_STDERR;
			log_control.log_entries[i].lfu_fname =
			    "standard error";
		    }
		}
		/*
		 * Is this a specification of the console?
		 */
		else if (!strcasecmp(cp, "CONSOLE")) {
		    if (log_control.log_entries[i].ldu_filep =
			CONSOLE_OPEN("a+")) {
			log_control.log_entries[i].log_type = K_LOG_CONSOLE;
			log_control.log_entries[i].ldu_devname = "console";
		    }
		}
		/*
		 * Is this a specification of a device?
		 */
		else if (!strncasecmp(cp, "DEVICE", 6)) {
		    /*
		     * We handle devices very similarly to files.
		     */
		    if (cp[6] == '=') {
			if (log_control.log_entries[i].ldu_filep =
			    DEVICE_OPEN(&cp[7], "w")) {
			    log_control.log_entries[i].log_type = K_LOG_DEVICE;
			    log_control.log_entries[i].ldu_devname = &cp[7];
			}
		    }
		}
		/*
		 * See if we successfully parsed this specification.
		 */
		if (log_control.log_entries[i].log_type == K_LOG_NONE) {
		    fprintf(stderr, lspec_parse_err_1, whoami, cp);
		    fprintf(stderr, lspec_parse_err_2, whoami);
		}
		else
		    ngood++;
	    }
	}
	/*
	 * If we didn't find anything, then free our lists.
	 */
	if (ngood == 0) {
	    for (i=0; i<log_control.log_nentries; i++)
		free(logging_specs[i]);
	}
	free(logging_specs);
    }
    /*
     * If we didn't find anything, go for the default which is to log to
     * the system log.
     */
    if (ngood == 0) {
	if (log_control.log_entries)
	    free(log_control.log_entries);
	log_control.log_entries = &def_log_entry;
	log_control.log_entries->log_type = K_LOG_SYSLOG;
	log_control.log_entries->log_2free = (krb5_pointer) NULL;
	log_facility = log_control.log_entries->lsu_facility = LOG_AUTH;
	log_control.log_entries->lsu_severity = LOG_ERR;
	do_openlog = 1;
	log_control.log_nentries = 1;
    }
    if (log_control.log_nentries) {
	if (log_control.log_whoami = (char *) malloc(strlen(whoami)+1))
	    strcpy(log_control.log_whoami, whoami);
	if (log_control.log_hostname = (char *) malloc(MAXHOSTNAMELEN))
	    gethostname(log_control.log_hostname, MAXHOSTNAMELEN);
#ifdef	HAVE_OPENLOG
	if (do_openlog) {
	    openlog(whoami, LOG_NDELAY|LOG_PID, log_facility);
	    log_control.log_opened = 1;
	}
#endif /* HAVE_OPENLOG */
	if (do_com_err)
	    (void) set_com_err_hook(klog_com_err_proc);
    }
    return((log_control.log_nentries) ? 0 : ENOENT);
}

/*
 * krb5_klog_close()	- Close the logging context and free all data.
 */
void
krb5_klog_close(kcontext)
    krb5_context	kcontext;
{
    int lindex;
    (void) reset_com_err_hook();
    for (lindex = 0; lindex < log_control.log_nentries; lindex++) {
	switch (log_control.log_entries[lindex].log_type) {
	case K_LOG_FILE:
	case K_LOG_STDERR:
	    /*
	     * Files/standard error.
	     */
	    fclose(log_control.log_entries[lindex].lfu_filep);
	    break;
	case K_LOG_CONSOLE:
	case K_LOG_DEVICE:
	    /*
	     * Devices (may need special handling)
	     */
	    DEVICE_CLOSE(log_control.log_entries[lindex].ldu_filep);
	    break;
#ifdef	HAVE_SYSLOG
	case K_LOG_SYSLOG:
	    /*
	     * System log.
	     */
	    break;
#endif	/* HAVE_SYSLOG */
	default:
	    break;
	}
	if (log_control.log_entries[lindex].log_2free)
	    free(log_control.log_entries[lindex].log_2free);
    }
    if (log_control.log_entries != &def_log_entry)
	free(log_control.log_entries);
    log_control.log_entries = (struct log_entry *) NULL;
    log_control.log_nentries = 0;
    if (log_control.log_whoami)
	free(log_control.log_whoami);
    log_control.log_whoami = (char *) NULL;
    if (log_control.log_hostname)
	free(log_control.log_hostname);
    log_control.log_hostname = (char *) NULL;
#ifdef	HAVE_CLOSELOG
    if (log_control.log_opened)
	closelog();
#endif	/* HAVE_CLOSELOG */
}

/*
 * severity2string()	- Convert a severity to a string.
 */
static char *
severity2string(severity)
    int	severity;
{
    int s;
    const char *ss;

    s = severity & LOG_PRIMASK;
    ss = log_ufo_string;
    switch (s) {
#ifdef	LOG_EMERG
    case LOG_EMERG:
	ss = log_emerg_string;
	break;
#endif	/* LOG_EMERG */
#ifdef	LOG_ALERT
    case LOG_ALERT:
	ss = log_alert_string;
	break;
#endif	/* LOG_ALERT */
#ifdef	LOG_CRIT
    case LOG_CRIT:
	ss = log_crit_string;
	break;
#endif	/* LOG_CRIT */
    case LOG_ERR:
	ss = log_err_string;
	break;
#ifdef	LOG_WARNING
    case LOG_WARNING:
	ss = log_warning_string;
	break;
#endif	/* LOG_WARNING */
#ifdef	LOG_NOTICE
    case LOG_NOTICE:
	ss = log_notice_string;
	break;
#endif	/* LOG_NOTICE */
#ifdef	LOG_INFO
    case LOG_INFO:
	ss = log_info_string;
	break;
#endif	/* LOG_INFO */
#ifdef	LOG_DEBUG
    case LOG_DEBUG:
	ss = log_debug_string;
	break;
#endif	/* LOG_DEBUG */
    }
    return((char *) ss);
}

/*
 * krb5_klog_syslog()	- Simulate the calling sequence of syslog(3), while
 *			  also performing the logging redirection as specified
 *			  by krb5_klog_init().
 */
static int
klog_vsyslog(priority, format, arglist)
    int		priority;
    const char	*format;
    va_list	arglist;
{
    char	outbuf[KRB5_KLOG_MAX_ERRMSG_SIZE];
    int		lindex;
    char	*syslogp;
    char	*cp;
    time_t	now;
#ifdef	HAVE_STRFTIME
    size_t	soff;
#endif	/* HAVE_STRFTIME */

    /*
     * Format a syslog-esque message of the format:
     *
     * (verbose form)
     * 		<date> <hostname> <id>[<pid>](<priority>): <message>
     *
     * (short form)
     *		<date> <message>
     */
    cp = outbuf;
    (void) time(&now);
#ifdef	HAVE_STRFTIME
    /*
     * Format the date: mon dd hh:mm:ss
     */
    soff = strftime(outbuf, sizeof(outbuf), "%b %d %H:%M:%S", localtime(&now));
    if (soff > 0)
	cp += soff;
    else
	return(-1);
#else	/* HAVE_STRFTIME */
    /*
     * Format the date:
     * We ASSUME here that the output of ctime is of the format:
     *	dow mon dd hh:mm:ss tzs yyyy\n
     *  012345678901234567890123456789
     */
    strncpy(outbuf, ctime(&now) + 4, 15);
    cp += 15;
#endif	/* HAVE_STRFTIME */
#ifdef VERBOSE_LOGS
    sprintf(cp, " %s %s[%ld](%s): ",
	    log_control.log_hostname, log_control.log_whoami, (long) getpid(),
	    severity2string(priority));
#else
    sprintf(cp, " ");
#endif
    syslogp = &outbuf[strlen(outbuf)];

    /* Now format the actual message */
#ifdef	HAVE_VSPRINTF
    vsprintf(syslogp, format, arglist);
#else	/* HAVE_VSPRINTF */
    sprintf(syslogp, format, ((int *) arglist)[0], ((int *) arglist)[1],
	    ((int *) arglist)[2], ((int *) arglist)[3],
	    ((int *) arglist)[4], ((int *) arglist)[5]);
#endif	/* HAVE_VSPRINTF */

    /*
     * Now that we have the message formatted, perform the output to each
     * logging specification.
     */
    for (lindex = 0; lindex < log_control.log_nentries; lindex++) {
	switch (log_control.log_entries[lindex].log_type) {
	case K_LOG_FILE:
	case K_LOG_STDERR:
	    /*
	     * Files/standard error.
	     */
	    if (fprintf(log_control.log_entries[lindex].lfu_filep, "%s\n",
			outbuf) < 0) {
		/* Attempt to report error */
		fprintf(stderr, log_file_err, log_control.log_whoami,
			log_control.log_entries[lindex].lfu_fname);
	    }
	    else {
		fflush(log_control.log_entries[lindex].lfu_filep);
	    }
	    break;
	case K_LOG_CONSOLE:
	case K_LOG_DEVICE:
	    /*
	     * Devices (may need special handling)
	     */
	    if (DEVICE_PRINT(log_control.log_entries[lindex].ldu_filep,
			     outbuf) < 0) {
		/* Attempt to report error */
		fprintf(stderr, log_device_err, log_control.log_whoami,
			log_control.log_entries[lindex].ldu_devname);
	    }
	    break;
#ifdef	HAVE_SYSLOG
	case K_LOG_SYSLOG:
	    /*
	     * System log.
	     */
					       
	    /* Log the message with our header trimmed off */
	    syslog(priority, "%s", syslogp);
	    break;
#endif /* HAVE_SYSLOG */
	default:
	    break;
	}
    }
    return(0);
}

#ifdef	HAVE_STDARG_H
int
krb5_klog_syslog(int priority, const char *format, ...)
#else	/* HAVE_STDARG_H */
int
krb5_klog_syslog(priority, format, va_alist)
    int		priority;
    const char	*format;
    va_dcl
#endif	/* HAVE_STDARG_H */
{
    int		retval;
    va_list	pvar;

#ifdef	HAVE_STDARG_H
    va_start(pvar, format);
#else	/* HAVE_STDARG_H */
    va_start(pvar);
#endif	/* HAVE_STDARG_H */
    retval = klog_vsyslog(priority, format, pvar);
    va_end(pvar);
    return(retval);
}

/*
 * krb5_klog_reopen() - Close and reopen any open (non-syslog) log files.
 *                      This function is called when a SIGHUP is received
 *                      so that external log-archival utilities may
 *                      alert the Kerberos daemons that they should get
 *                      a new file descriptor for the give filename.
 */
void
krb5_klog_reopen(kcontext)
krb5_context kcontext;
{
    int lindex;
    FILE *f;

    /*
     * Only logs which are actually files need to be closed
     * and reopened in response to a SIGHUP
     */
    for (lindex = 0; lindex < log_control.log_nentries; lindex++) {
	if (log_control.log_entries[lindex].log_type == K_LOG_FILE) {
	    fclose(log_control.log_entries[lindex].lfu_filep);
	    /*
	     * In case the old logfile did not get moved out of the
	     * way, open for append to prevent squashing the old logs.
	     */
	    f = fopen(log_control.log_entries[lindex].lfu_fname, "a+");
	    if (f) {
		log_control.log_entries[lindex].lfu_filep = f;
	    } else {
		fprintf(stderr, "Couldn't open log file %s: %s\n",
			log_control.log_entries[lindex].lfu_fname,
			error_message(errno));
	    }
	}
    }
}

#endif /* !defined(_MSDOS) */

