/*
 * Copyright 1994 by OpenVision Technologies, Inc.
 * 
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 * 
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(lint) && !defined(__CODECENTER__)
static char *rcsid = "$Header$";
#endif

#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>
#include <ctype.h>

#include <gssapi/gssapi_generic.h>
#include "gss-misc.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

void usage()
{
     fprintf(stderr, "Usage: gss-server [-port port] [-verbose] [-once]\n");
     fprintf(stderr, "       [-inetd] [-export] [-logfile file] [service_name]\n");
     exit(1);
}

FILE *log;

int verbose = 0;

/*
 * Function: server_acquire_creds
 *
 * Purpose: imports a service name and acquires credentials for it
 *
 * Arguments:
 *
 * 	service_name	(r) the ASCII service name
 * 	server_creds	(w) the GSS-API service credentials
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * The service name is imported with gss_import_name, and service
 * credentials are acquired with gss_acquire_cred.  If either opertion
 * fails, an error message is displayed and -1 is returned; otherwise,
 * 0 is returned.
 */
int server_acquire_creds(service_name, server_creds)
     char *service_name;
     gss_cred_id_t *server_creds;
{
     gss_buffer_desc name_buf;
     gss_name_t server_name;
     OM_uint32 maj_stat, min_stat;

     name_buf.value = service_name;
     name_buf.length = strlen(name_buf.value) + 1;
     maj_stat = gss_import_name(&min_stat, &name_buf, 
				(gss_OID) gss_nt_service_name, &server_name);
     if (maj_stat != GSS_S_COMPLETE) {
	  display_status("importing name", maj_stat, min_stat);
	  return -1;
     }

     maj_stat = gss_acquire_cred(&min_stat, server_name, 0,
				 GSS_C_NULL_OID_SET, GSS_C_ACCEPT,
				 server_creds, NULL, NULL);
     if (maj_stat != GSS_S_COMPLETE) {
	  display_status("acquiring credentials", maj_stat, min_stat);
	  return -1;
     }

     (void) gss_release_name(&min_stat, &server_name);

     return 0;
}

/*
 * Function: server_establish_context
 *
 * Purpose: establishses a GSS-API context as a specified service with
 * an incoming client, and returns the context handle and associated
 * client name
 *
 * Arguments:
 *
 * 	s		(r) an established TCP connection to the client
 * 	service_creds	(r) server credentials, from gss_acquire_cred
 * 	context		(w) the established GSS-API context
 * 	client_name	(w) the client's ASCII name
 *
 * Returns: 0 on success, -1 on failure
 *
 * Effects:
 *
 * Any valid client request is accepted.  If a context is established,
 * its handle is returned in context and the client name is returned
 * in client_name and 0 is returned.  If unsuccessful, an error
 * message is displayed and -1 is returned.
 */
int server_establish_context(s, server_creds, context, client_name, ret_flags)
     int s;
     gss_cred_id_t server_creds;
     gss_ctx_id_t *context;
     gss_buffer_t client_name;
     OM_uint32 *ret_flags;
{
     gss_buffer_desc send_tok, recv_tok;
     gss_name_t client;
     gss_OID doid;
     OM_uint32 maj_stat, min_stat, acc_sec_min_stat;
     gss_buffer_desc	oid_name;
     int token_flags;

     if (recv_token(s, &token_flags, &recv_tok) < 0)
       return -1;

     (void) gss_release_buffer(&min_stat, &recv_tok);
     if (! (token_flags & TOKEN_NOOP)) {
       if (log)
	 fprintf(log, "Expected NOOP token, got %d token instead\n",
		 token_flags);
       return -1;
     }

     *context = GSS_C_NO_CONTEXT;

     if (token_flags & TOKEN_CONTEXT_NEXT) {
       do {
	 if (recv_token(s, &token_flags, &recv_tok) < 0)
	   return -1;

	 if (verbose && log) {
	   fprintf(log, "Received token (size=%d): \n", recv_tok.length);
	   print_token(&recv_tok);
	 }

	 maj_stat =
	   gss_accept_sec_context(&acc_sec_min_stat,
				  context,
				  server_creds,
				  &recv_tok,
				  GSS_C_NO_CHANNEL_BINDINGS,
				  &client,
				  &doid,
				  &send_tok,
				  ret_flags,
				  NULL, 	/* ignore time_rec */
				  NULL); 	/* ignore del_cred_handle */

	 (void) gss_release_buffer(&min_stat, &recv_tok);

	 if (send_tok.length != 0) {
	   if (verbose && log) {
	     fprintf(log,
		     "Sending accept_sec_context token (size=%d):\n",
		     send_tok.length);
	     print_token(&send_tok);
	   }
	   if (send_token(s, TOKEN_CONTEXT, &send_tok) < 0) {
	     if (log)
	       fprintf(log, "failure sending token\n");
	     return -1;
	   }

	   (void) gss_release_buffer(&min_stat, &send_tok);
	 }
	 if (maj_stat!=GSS_S_COMPLETE && maj_stat!=GSS_S_CONTINUE_NEEDED) {
	      display_status("accepting context", maj_stat,
			     acc_sec_min_stat);
	      if (*context == GSS_C_NO_CONTEXT)
		      gss_delete_sec_context(&min_stat, context,
					     GSS_C_NO_BUFFER);
	      return -1;
	 }
 
	 if (verbose && log) {
	   if (maj_stat == GSS_S_CONTINUE_NEEDED)
	     fprintf(log, "continue needed...\n");
	   else
	     fprintf(log, "\n");
	   fflush(log);
	 }
       } while (maj_stat == GSS_S_CONTINUE_NEEDED);

       /* display the flags */
       display_ctx_flags(*ret_flags);

       if (verbose && log) {
	 maj_stat = gss_oid_to_str(&min_stat, doid, &oid_name);
	 if (maj_stat != GSS_S_COMPLETE) {
	   display_status("converting oid->string", maj_stat, min_stat);
	   return -1;
	 }
	 fprintf(log, "Accepted connection using mechanism OID %.*s.\n",
		 (int) oid_name.length, (char *) oid_name.value);
	 (void) gss_release_buffer(&min_stat, &oid_name);
       }

       maj_stat = gss_display_name(&min_stat, client, client_name, &doid);
       if (maj_stat != GSS_S_COMPLETE) {
	 display_status("displaying name", maj_stat, min_stat);
	 return -1;
       }
       maj_stat = gss_release_name(&min_stat, &client);
       if (maj_stat != GSS_S_COMPLETE) {
	 display_status("releasing name", maj_stat, min_stat);
	 return -1;
       }
     }
     else {
       client_name->length = *ret_flags = 0;

       if (log)
	 fprintf(log, "Accepted unauthenticated connection.\n");
     }

     return 0;
}

/*
 * Function: create_socket
 *
 * Purpose: Opens a listening TCP socket.
 *
 * Arguments:
 *
 * 	port		(r) the port number on which to listen
 *
 * Returns: the listening socket file descriptor, or -1 on failure
 *
 * Effects:
 *
 * A listening socket on the specified port and created and returned.
 * On error, an error message is displayed and -1 is returned.
 */
int create_socket(port)
     u_short port;
{
     struct sockaddr_in saddr;
     int s;
     int on = 1;
     
     saddr.sin_family = AF_INET;
     saddr.sin_port = htons(port);
     saddr.sin_addr.s_addr = INADDR_ANY;

     if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	  perror("creating socket");
	  return -1;
     }
     /* Let the socket be reused right away */
     (void) setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
     if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
	  perror("binding socket");
	  (void) close(s);
	  return -1;
     }
     if (listen(s, 5) < 0) {
	  perror("listening on socket");
	  (void) close(s);
	  return -1;
     }
     return s;
}

static float timeval_subtract(tv1, tv2)
	struct timeval *tv1, *tv2;
{
	return ((tv1->tv_sec - tv2->tv_sec) +
		((float) (tv1->tv_usec - tv2->tv_usec)) / 1000000);
}

/*
 * Yes, yes, this isn't the best place for doing this test.
 * DO NOT REMOVE THIS UNTIL A BETTER TEST HAS BEEN WRITTEN, THOUGH.
 * 					-TYT
 */
int test_import_export_context(context)
	gss_ctx_id_t *context;
{
	OM_uint32	min_stat, maj_stat;
	gss_buffer_desc context_token, copied_token;
	struct timeval tm1, tm2;
	
	/*
	 * Attempt to save and then restore the context.
	 */
	gettimeofday(&tm1, (struct timezone *)0);
	maj_stat = gss_export_sec_context(&min_stat, context, &context_token);
	if (maj_stat != GSS_S_COMPLETE) {
		display_status("exporting context", maj_stat, min_stat);
		return 1;
	}
	gettimeofday(&tm2, (struct timezone *)0);
	if (verbose && log)
		fprintf(log, "Exported context: %d bytes, %7.4f seconds\n",
			context_token.length, timeval_subtract(&tm2, &tm1));
	copied_token.length = context_token.length;
	copied_token.value = malloc(context_token.length);
	if (copied_token.value == 0) {
	  if (log)
	    fprintf(log, "Couldn't allocate memory to copy context token.\n");
	  return 1;
	}
	memcpy(copied_token.value, context_token.value, copied_token.length);
	maj_stat = gss_import_sec_context(&min_stat, &copied_token, context);
	if (maj_stat != GSS_S_COMPLETE) {
		display_status("importing context", maj_stat, min_stat);
		return 1;
	}
	free(copied_token.value);
	gettimeofday(&tm1, (struct timezone *)0);
	if (verbose && log)
		fprintf(log, "Importing context: %7.4f seconds\n",
			timeval_subtract(&tm1, &tm2));
	(void) gss_release_buffer(&min_stat, &context_token);
	return 0;
}

/*
 * Function: sign_server
 *
 * Purpose: Performs the "sign" service.
 *
 * Arguments:
 *
 * 	s		(r) a TCP socket on which a connection has been
 *			accept()ed
 * 	service_name	(r) the ASCII name of the GSS-API service to
 * 			establish a context as
 *	export		(r) whether to test context exporting
 * 
 * Returns: -1 on error
 *
 * Effects:
 *
 * sign_server establishes a context, and performs a single sign request.
 *
 * A sign request is a single GSS-API sealed token.  The token is
 * unsealed and a signature block, produced with gss_sign, is returned
 * to the sender.  The context is the destroyed and the connection
 * closed.
 *
 * If any error occurs, -1 is returned.
 */
int sign_server(s, server_creds, export)
     int s;
     gss_cred_id_t server_creds;
     int export;
{
     gss_buffer_desc client_name, xmit_buf, msg_buf;
     gss_ctx_id_t context;
     OM_uint32 maj_stat, min_stat;
     int i, conf_state, ret_flags;
     char	*cp;
     int token_flags;

     /* Establish a context with the client */
     if (server_establish_context(s, server_creds, &context,
				  &client_name, &ret_flags) < 0)
	return(-1);

     if (context == GSS_C_NO_CONTEXT) {
       printf("Accepted unauthenticated connection.\n");
     }
     else {
       printf("Accepted connection: \"%.*s\"\n",
	      (int) client_name.length, (char *) client_name.value);
       (void) gss_release_buffer(&min_stat, &client_name);

       if (export) {
	 for (i=0; i < 3; i++)
	   if (test_import_export_context(&context))
	     return -1;
       }
     }

     do {
       /* Receive the message token */
       if (recv_token(s, &token_flags, &xmit_buf) < 0)
	 return(-1);

       if (token_flags & TOKEN_NOOP) {
	 if (log)
	   fprintf(log, "NOOP token\n");
	 (void) gss_release_buffer(&min_stat, &xmit_buf);
	 break;
       }

       if (verbose && log) {
	 fprintf(log, "Message token (flags=%d):\n", token_flags);
	 print_token(&xmit_buf);
       }

       if ((context == GSS_C_NO_CONTEXT) &&
	   (token_flags & (TOKEN_WRAPPED|TOKEN_ENCRYPTED|TOKEN_SEND_MIC))) {
	 if (log)
	   fprintf(log,
		   "Unauthenticated client requested authenticated services!\n");
	 (void) gss_release_buffer(&min_stat, &xmit_buf);
	 return(-1);
       }

       if (token_flags & TOKEN_WRAPPED) {
	 maj_stat = gss_unwrap(&min_stat, context, &xmit_buf, &msg_buf,
			       &conf_state, (gss_qop_t *) NULL);
	 if (maj_stat != GSS_S_COMPLETE) {
	   display_status("unsealing message", maj_stat, min_stat);
	   (void) gss_release_buffer(&min_stat, &xmit_buf);
	   return(-1);
	 } else if (! conf_state && (token_flags & TOKEN_ENCRYPTED)) {
	   fprintf(stderr, "Warning!  Message not encrypted.\n");
	 }

	 (void) gss_release_buffer(&min_stat, &xmit_buf);
       }
       else {
	 msg_buf = xmit_buf;
       }

       if (log) {
	 fprintf(log, "Received message: ");
	 cp = msg_buf.value;
	 if ((isprint(cp[0]) || isspace(cp[0])) &&
	    (isprint(cp[1]) || isspace(cp[1]))) {
	   fprintf(log, "\"%.*s\"\n", msg_buf.length, msg_buf.value);
	 } else {
	   fprintf(log, "\n");
	   print_token(&msg_buf);
	 }
       }

       if (token_flags & TOKEN_SEND_MIC) {
	 /* Produce a signature block for the message */
	 maj_stat = gss_get_mic(&min_stat, context, GSS_C_QOP_DEFAULT,
				&msg_buf, &xmit_buf);
	 if (maj_stat != GSS_S_COMPLETE) {
	   display_status("signing message", maj_stat, min_stat);
	   return(-1);
	 }

	 (void) gss_release_buffer(&min_stat, &msg_buf);

	 /* Send the signature block to the client */
	 if (send_token(s, TOKEN_MIC, &xmit_buf) < 0)
	   return(-1);

	 (void) gss_release_buffer(&min_stat, &xmit_buf);
       }
       else {
	 (void) gss_release_buffer(&min_stat, &msg_buf);
	 if (send_token(s, TOKEN_NOOP, empty_token) < 0)
	   return(-1);
       }
     } while (1 /* loop will break if NOOP received */);

     if (context != GSS_C_NO_CONTEXT) {
       /* Delete context */
       maj_stat = gss_delete_sec_context(&min_stat, &context, NULL);
       if (maj_stat != GSS_S_COMPLETE) {
	 display_status("deleting context", maj_stat, min_stat);
	 return(-1);
       }
     }

     if (log)
       fflush(log);

     return(0);
}

int
main(argc, argv)
     int argc;
     char **argv;
{
     char *service_name;
     gss_cred_id_t server_creds;
     OM_uint32 min_stat;
     u_short port = 4444;
     int s;
     int once = 0;
     int do_inetd = 0;
     int export = 0;

     log = stdout;
     display_file = stdout;
     argc--; argv++;
     while (argc) {
	  if (strcmp(*argv, "-port") == 0) {
	       argc--; argv++;
	       if (!argc) usage();
	       port = atoi(*argv);
	  } else if (strcmp(*argv, "-verbose") == 0) {
	      verbose = 1;
	  } else if (strcmp(*argv, "-once") == 0) {
	      once = 1;
	  } else if (strcmp(*argv, "-inetd") == 0) {
	      do_inetd = 1;
	  } else if (strcmp(*argv, "-export") == 0) {
	      export = 1;
	  } else if (strcmp(*argv, "-logfile") == 0) {
	      argc--; argv++;
	      if (!argc) usage();
	      /* Gross hack, but it makes it unnecessary to add an
                 extra argument to disable logging, and makes the code
                 more efficient because it doesn't actually write data
                 to /dev/null. */
	      if (! strcmp(*argv, "/dev/null")) {
		log = display_file = NULL;
	      }
	      else {
		log = fopen(*argv, "a");
		display_file = log;
		if (!log) {
		  perror(*argv);
		  exit(1);
		}
	      }
	  } else
	       break;
	  argc--; argv++;
     }
     if (argc != 1)
	  usage();

     if ((*argv)[0] == '-')
	  usage();

     service_name = *argv;

     if (server_acquire_creds(service_name, &server_creds) < 0)
	 return -1;
     
     if (do_inetd) {
	 close(1);
	 close(2);

	 sign_server(0, server_creds, export);
	 close(0);
     } else {
	 int stmp;

 	 if ((stmp = create_socket(port)) >= 0) {
 	     do {
 		 /* Accept a TCP connection */
 		 if ((s = accept(stmp, NULL, 0)) < 0) {
 		     perror("accepting connection");
 		     continue;
 		 }
 		 /* this return value is not checked, because there's
 		    not really anything to do if it fails */
 		 sign_server(s, server_creds);
 		 close(s);
 	     } while (!once);
 
 	     close(stmp);
 	 }
     }

     (void) gss_release_cred(&min_stat, &server_creds);

     return 0;
}
