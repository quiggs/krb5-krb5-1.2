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
#include <string.h>
#include <signal.h>
#include <gssrpc/rpc.h>
#include <gssrpc/pmap_clnt.h>
#include <arpa/inet.h>  /* inet_ntoa */
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_generic.h>
#include <gssrpc/auth_gssapi.h>
#include <sys/param.h>	/* MAXHOSTNAMELEN */
#include "rpc_test.h"

extern void rpc_test_prog_1();

extern int svc_debug_gssapi, misc_debug_gssapi;

void rpc_test_badauth(OM_uint32 major, OM_uint32 minor,
		 struct sockaddr_in *addr, void *data);
void log_miscerr(struct svc_req *rqst, struct rpc_msg *msg, char
		 *error, char *data);
void log_badauth_display_status(OM_uint32 major, OM_uint32 minor);
void log_badauth_display_status_1(OM_uint32 code, int type, int rec);
static void rpc_test_badverf(gss_name_t client, gss_name_t server,
			     struct svc_req *rqst, struct rpc_msg *msg,
			     caddr_t data);

#ifndef SERVICE_NAME
#define SERVICE_NAME "server"
#endif

void usage()
{
     fprintf(stderr, "Usage: server {-t|-u} [svc-debug] [misc-debug]\n");
     exit(1);
}

void handlesig(void)
{
    exit(0);
}

main(int argc, char **argv)
{
     int c, prot;
     auth_gssapi_name names[2];
     register SVCXPRT *transp;
     extern int optind;
#ifdef POSIX_SIGNALS
     struct sigaction sa;
#endif     

     names[0].name = SERVICE_NAME;
     names[0].type = (gss_OID) gss_nt_service_name;
     names[1].name = 0;
     names[1].type = 0;

     prot = 0;
     while ((c = getopt(argc, argv, "tu")) != -1) {
	  switch (c) {
	  case 't':
	       prot = IPPROTO_TCP;
	       break;
	  case 'u':
	       prot = IPPROTO_UDP;
	       break;
	  case '?':
	       usage();
	       break;
	  }
     }
     if (prot == 0)
	  usage();

     argv += optind;
     argc -= optind;

     switch (argc) {
     case 2:
	  misc_debug_gssapi = atoi(argv[1]);
     case 1:
	  svc_debug_gssapi = atoi(argv[0]);
     case 0:
	  break;
     default:
	  usage();
	  exit(1);
     }

     (void) pmap_unset(RPC_TEST_PROG, RPC_TEST_VERS_1);

     if (prot == IPPROTO_TCP)
	  transp = svctcp_create(RPC_ANYSOCK, 0, 0);
     else
	  transp = svcudp_create(RPC_ANYSOCK);
     if (transp == NULL) {
	  fprintf(stderr, "cannot create tcp service.");
	  exit(1);
     }
     if (!svc_register(transp, RPC_TEST_PROG, RPC_TEST_VERS_1,
		       rpc_test_prog_1, prot)) { 
	  fprintf(stderr,
		  "unable to register (RPC_TEST_PROG, RPC_TEST_VERS_1, %s).",
		  prot == IPPROTO_TCP ? "tcp" : "udp");
	  exit(1);
     }
     
     if (_svcauth_gssapi_set_names(names, 0) == FALSE) {
	  fprintf(stderr, "unable to set gssapi names\n");
	  exit(1);
     }

     _svcauth_gssapi_set_log_badauth_func(rpc_test_badauth, NULL);
     _svcauth_gssapi_set_log_badverf_func(rpc_test_badverf, NULL);
     _svcauth_gssapi_set_log_miscerr_func(log_miscerr, NULL);

#ifdef POSIX_SIGNALS
     (void) sigemptyset(&sa.sa_mask);
     sa.sa_flags = 0;
     sa.sa_handler = handlesig;
     (void) sigaction(SIGHUP, &sa, NULL);
     (void) sigaction(SIGINT, &sa, NULL);
     (void) sigaction(SIGTERM, &sa, NULL);
#else
     signal(SIGHUP, handlesig);
     signal(SIGINT, handlesig);
     signal(SIGTERM, handlesig);
#endif
     printf("running\n");
     
     svc_run();
     fprintf(stderr, "svc_run returned");
     exit(1);
     /* NOTREACHED */
}

char **rpc_test_echo_1(char **arg, struct svc_req *h)
{
     static char *res = NULL;

     if (res)
	  free(res);
     res = (char *) malloc(strlen(*arg) + strlen("Echo: ") + 1);
     sprintf(res, "Echo: %s", *arg);
     return &res;
}

static void rpc_test_badverf(gss_name_t client, gss_name_t server,
			     struct svc_req *rqst, struct rpc_msg *msg,
			     caddr_t data)
{
     OM_uint32 minor_stat;
     gss_OID type;
     gss_buffer_desc client_name, server_name;

     (void) gss_display_name(&minor_stat, client, &client_name, &type);
     (void) gss_display_name(&minor_stat, server, &server_name, &type);

     printf("rpc_test server: bad verifier from %s at %s:%d for %s\n",
	    client_name.value,
	    inet_ntoa(rqst->rq_xprt->xp_raddr.sin_addr), 
	    ntohs(rqst->rq_xprt->xp_raddr.sin_port),
	    server_name.value);

     (void) gss_release_buffer(&minor_stat, &client_name);
     (void) gss_release_buffer(&minor_stat, &server_name);
}

/*
 * Function: log_badauth
 *
 * Purpose: Callback from GSS-API Sun RPC for authentication
 * failures/errors.
 *
 * Arguments:
 * 	major 		(r) GSS-API major status
 * 	minor		(r) GSS-API minor status
 * 	addr		(r) originating address
 * 	data		(r) arbitrary data (NULL), not used
 *
 * Effects:
 *
 * Logs the GSS-API error to stdout.
 */
void rpc_test_badauth(OM_uint32 major, OM_uint32 minor,
		 struct sockaddr_in *addr, void *data)
{
     char *a;
     
     /* Authentication attempt failed: <IP address>, <GSS-API error */
     /* strings> */

     a = inet_ntoa(addr->sin_addr);

     printf("rpc_test server: Authentication attempt failed: %s", a);
     log_badauth_display_status(major, minor);
     printf("\n");
}

void log_miscerr(struct svc_req *rqst, struct rpc_msg *msg,
		 char *error, char *data)
{
     char *a;
     
     a = inet_ntoa(rqst->rq_xprt->xp_raddr.sin_addr);
     printf("Miscellaneous RPC error: %s, %s\n", a, error);
}

void log_badauth_display_status(OM_uint32 major, OM_uint32 minor)
{
     log_badauth_display_status_1(major, GSS_C_GSS_CODE, 0);
     log_badauth_display_status_1(minor, GSS_C_MECH_CODE, 0);
}

void log_badauth_display_status_1(OM_uint32 code, int type, int rec)
{
     OM_uint32 gssstat, minor_stat;
     gss_buffer_desc msg;
     int msg_ctx;

     msg_ctx = 0;
     while (1) {
	  gssstat = gss_display_status(&minor_stat, code,
				       type, GSS_C_NULL_OID,
				       &msg_ctx, &msg);
	  if (gssstat != GSS_S_COMPLETE) {
 	       if (!rec) {
		    log_badauth_display_status_1(gssstat,GSS_C_GSS_CODE,1); 
		    log_badauth_display_status_1(minor_stat,
						 GSS_C_MECH_CODE, 1);
	       } else
		    printf("GSS-API authentication error %s: "
			   "recursive failure!\n", msg);
	       return;
	  }
	  
	  printf(", %s", (char *)msg.value); 
	  (void) gss_release_buffer(&minor_stat, &msg);
	  
	  if (!msg_ctx)
	       break;
     }
}


#if 0

/* this hack is no longer necessary, since the library supports it
   internally */

/* This is a hack to change the default keytab name */

#include <krb5/krb5.h>
extern char *krb5_defkeyname;

krb5_error_code
krb5_kt_default_name(char *name, int namesize)
{
   char *ktname;

   if ((ktname = getenv("KRB5KTNAME")) == NULL)
      ktname = krb5_defkeyname;

   if (namesize < strlen(ktname)+1)
      return(KRB5_CONFIG_NOTENUFSPACE);

   strcpy(name, ktname);

   return(0);
}

#endif
