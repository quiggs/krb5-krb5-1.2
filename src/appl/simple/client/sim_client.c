/*
 * appl/simple/client/sim_client.c
 *
 * Copyright 1989,1991 by the Massachusetts Institute of Technology.
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
 *
 * Simple UDP-based sample client program.  For demonstration.
 * This program performs no useful function.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <krb5.h>
#include "com_err.h"

#include "simple.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#else
extern char *malloc();
#endif

/* for old Unixes and friends ... */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

#define MSG "hi there!"			/* message text */

void
usage(name)
    char *name;
{
	fprintf(stderr, "usage: %s [-p port] [-h host] [-m message] [-s service] [host]\n", name);
}	

int
main(argc, argv)
    int argc;
    char *argv[];
{
    int sock, i;
    int flags = 0;			/* flags for sendto() */
    struct servent *serv;
    struct hostent *host;
    char *cp;
    char full_hname[MAXHOSTNAMELEN];
#ifdef BROKEN_STREAMS_SOCKETS
    char my_hostname[MAXHOSTNAMELEN];
#endif
    struct sockaddr_in s_sock;		/* server address */
    struct sockaddr_in c_sock;		/* client address */
    extern int opterr, optind;
    extern char * optarg;
    int	ch;
    
    short port = 0;
    char *message = MSG;
    char *hostname = 0;
    char *service = SIMPLE_SERVICE;
    char *progname = 0;

    krb5_error_code retval;
    krb5_data packet, inbuf;
    krb5_ccache ccdef;
    krb5_address addr, *portlocal_addr;
    krb5_rcache rcache;
    krb5_data	rcache_name;

    krb5_context 	  context;
    krb5_auth_context 	  auth_context = NULL;
    krb5_replay_data 	  replaydata;

    retval = krb5_init_context(&context);
    if (retval) {
	    com_err(argv[0], retval, "while initializing krb5");
	    exit(1);
    }

    progname = argv[0];

    /*
     * Parse command line arguments
     *  
     */
    opterr = 0;
    while ((ch = getopt(argc, argv, "p:m:h:s:")) != -1)
    switch (ch) {
    case 'p':
	port = atoi(optarg);
	break;
    case 'm':
	message = optarg;
	break;
    case 'h':
	hostname = optarg;
	break;
    case 's':
	service = optarg;
	break;
    case '?':
    default:
	usage(progname);
	exit(1);
	break;
    }
    argc -= optind;
    argv += optind;
    if (argc > 0) {
	if (hostname)
	    usage(progname);
	hostname = argv[0];
    }

    if (hostname == 0) {
	fprintf(stderr, "You must specify a hostname to contact.\n\n");
	usage(progname);
	exit(1);
    }

    if (!valid_cksumtype(CKSUMTYPE_CRC32)) {
	com_err(progname, KRB5_PROG_SUMTYPE_NOSUPP, "while using CRC-32");
	exit(1);
    }

    /* Look up server host */
    if ((host = gethostbyname(hostname)) == (struct hostent *) 0) {
	fprintf(stderr, "%s: unknown host\n", hostname);
	exit(1);
    }
    strncpy(full_hname, host->h_name, sizeof(full_hname)-1);
    full_hname[sizeof(full_hname)-1] = '\0';

    /* lower-case to get name for "instance" part of service name */
    for (cp = full_hname; *cp; cp++)
        if (isupper(*cp))
            *cp = tolower(*cp);

    /* Set server's address */
    (void) memset((char *)&s_sock, 0, sizeof(s_sock));

    memcpy((char *)&s_sock.sin_addr, host->h_addr, sizeof(s_sock.sin_addr));
#ifdef DEBUG
    printf("s_sock.sin_addr is %s\n", inet_ntoa(s_sock.sin_addr));
#endif
    s_sock.sin_family = AF_INET;

    if (port == 0) {
	/* Look up service */
	if ((serv = getservbyname(SIMPLE_PORT, "udp")) == NULL) {
	    fprintf(stderr, "service unknown: %s/udp\n", SIMPLE_PORT);
	    exit(1);
	}
	s_sock.sin_port = serv->s_port;
    } else {
	s_sock.sin_port = htons(port);
    }

    /* Open a socket */
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	com_err(progname, errno, "opening datagram socket");
	exit(1);
    }

    memset((char *)&c_sock, 0, sizeof(c_sock));
    c_sock.sin_family = AF_INET;
#ifdef BROKEN_STREAMS_SOCKETS
    if (gethostname(my_hostname, sizeof(my_hostname)) < 0) {
	perror("gethostname");
	exit(1);
    }

    if ((host = gethostbyname(my_hostname)) == (struct hostent *)0) {
	fprintf(stderr, "%s: unknown host\n", hostname);
	exit(1);
    }
    memcpy((char *)&c_sock.sin_addr, host->h_addr, sizeof(c_sock.sin_addr));
#endif
    

    /* Bind it to set the address; kernel will fill in port # */
    if (bind(sock, (struct sockaddr *)&c_sock, sizeof(c_sock)) < 0) {
	com_err(progname, errno, "while binding datagram socket");
	exit(1);
    }
	
    /* PREPARE KRB_AP_REQ MESSAGE */

    inbuf.data = hostname;
    inbuf.length = strlen(hostname);

    /* Get credentials for server */
    if ((retval = krb5_cc_default(context, &ccdef))) {
	com_err(progname, retval, "while getting default ccache");
	exit(1);
    }

    if ((retval = krb5_mk_req(context, &auth_context, 0, service, full_hname,
			      &inbuf, ccdef, &packet))) {
	com_err(progname, retval, "while preparing AP_REQ");
	exit(1);
    }
    printf("Got credentials for %s.\n", service);

    /* "connect" the datagram socket; this is necessary to get a local address
       properly bound for getsockname() below. */

    if (connect(sock, (struct sockaddr *)&s_sock, sizeof(s_sock)) == -1) {
	com_err(progname, errno, "while connecting to server");
	exit(1);
    }
    /* Send authentication info to server */
    if ((i = send(sock, (char *)packet.data, packet.length, flags)) < 0) 
	com_err(progname, errno, "while sending KRB_AP_REQ message");
    printf("Sent authentication data: %d bytes\n", i);
    krb5_free_data_contents(context, &packet);

    /* PREPARE KRB_SAFE MESSAGE */

    /* Get my address */
    memset((char *) &c_sock, 0, sizeof(c_sock));
    i = sizeof(c_sock);
    if (getsockname(sock, (struct sockaddr *)&c_sock, &i) < 0) {
	com_err(progname, errno, "while getting socket name");
	exit(1);
    }

    addr.addrtype = ADDRTYPE_IPPORT;
    addr.length = sizeof(c_sock.sin_port);
    addr.contents = (krb5_octet *)&c_sock.sin_port;
    if ((retval = krb5_auth_con_setports(context, auth_context,
					 &addr, NULL))) {
	com_err(progname, retval, "while setting local port\n");
	exit(1);
    }

    addr.addrtype = ADDRTYPE_INET;
    addr.length = sizeof(c_sock.sin_addr);
    addr.contents = (krb5_octet *)&c_sock.sin_addr;
    if ((retval = krb5_auth_con_setaddrs(context, auth_context,
					 &addr, NULL))) {
	com_err(progname, retval, "while setting local addr\n");
	exit(1);
    }

    /* THIS IS UGLY */
    if ((retval = krb5_gen_portaddr(context, &addr,
				    (krb5_pointer) &c_sock.sin_port,
				    &portlocal_addr))) {
	com_err(progname, retval, "while generating port address");
	exit(1);
    }
    
    if ((retval = krb5_gen_replay_name(context,portlocal_addr,
				       "_sim_clt",&cp))) {
	com_err(progname, retval, "while generating replay cache name");
	exit(1);
    }

    rcache_name.length = strlen(cp);
    rcache_name.data = cp;

    if ((retval = krb5_get_server_rcache(context, &rcache_name, &rcache))) {
	com_err(progname, retval, "while getting server rcache");
	exit(1);
    }

    /* set auth_context rcache */
    krb5_auth_con_setrcache(context, auth_context, rcache);

    /* Make the safe message */
    inbuf.data = message;
    inbuf.length = strlen(message);

    if ((retval = krb5_mk_safe(context, auth_context, &inbuf, &packet, NULL))){
	com_err(progname, retval, "while making KRB_SAFE message");
	exit(1);
    }

    /* Send it */
    if ((i = send(sock, (char *)packet.data, packet.length, flags)) < 0)
	com_err(progname, errno, "while sending SAFE message");
    printf("Sent checksummed message: %d bytes\n", i);
    krb5_free_data_contents(context, &packet);

    /* PREPARE KRB_PRIV MESSAGE */

    /* Make the encrypted message */
    if ((retval = krb5_mk_priv(context, auth_context, &inbuf,
			       &packet, NULL))) {
	com_err(progname, retval, "while making KRB_PRIV message");
	exit(1);
    }

    /* Send it */
    if ((i = send(sock, (char *)packet.data, packet.length, flags)) < 0)
	com_err(progname, errno, "while sending PRIV message");
    printf("Sent encrypted message: %d bytes\n", i);
    krb5_free_data_contents(context, &packet);

    krb5_auth_con_free(context, auth_context);
    krb5_free_context(context);
    
    exit(0);
}
