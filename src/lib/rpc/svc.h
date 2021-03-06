/* @(#)svc.h	2.2 88/07/29 4.0 RPCSRC; from 1.20 88/02/08 SMI */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * svc.h, Server-side remote procedure call interface.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#ifndef __SVC_HEADER__
#define __SVC_HEADER__

/*
 * This interface must manage two items concerning remote procedure calling:
 *
 * 1) An arbitrary number of transport connections upon which rpc requests
 * are received.  The two most notable transports are TCP and UDP;  they are
 * created and registered by routines in svc_tcp.c and svc_udp.c, respectively;
 * they in turn call xprt_register and xprt_unregister.
 *
 * 2) An arbitrary number of locally registered services.  Services are
 * described by the following four data: program number, version number,
 * "service dispatch" function, a transport handle, and a boolean that
 * indicates whether or not the exported program should be registered with a
 * local binder service;  if true the program's number and version and the
 * port number from the transport handle are registered with the binder.
 * These data are registered with the rpc svc system via svc_register.
 *
 * A service's dispatch function is called whenever an rpc request comes in
 * on a transport.  The request's program and version numbers must match
 * those of the registered service.  The dispatch function is passed two
 * parameters, struct svc_req * and SVCXPRT *, defined below.
 */

enum xprt_stat {
	XPRT_DIED,
	XPRT_MOREREQS,
	XPRT_IDLE
};

/*
 * Server side transport handle
 */
typedef struct {
	int		xp_sock;
	unsigned short		xp_port;	 /* associated port number */
	struct xp_ops {
	    bool_t	(*xp_recv)();	 /* receive incomming requests */
	    enum xprt_stat (*xp_stat)(); /* get transport status */
	    bool_t	(*xp_getargs)(); /* get arguments */
	    bool_t	(*xp_reply)();	 /* send reply */
	    bool_t	(*xp_freeargs)();/* free mem allocated for args */
	    void	(*xp_destroy)(); /* destroy this struct */
	} *xp_ops;
	int		xp_addrlen;	 /* length of remote address */
	struct sockaddr_in xp_raddr;	 /* remote address */
	struct opaque_auth xp_verf;	 /* raw response verifier */
	SVCAUTH		*xp_auth;	 /* auth flavor of current req */
	caddr_t		xp_p1;		 /* private */
	caddr_t		xp_p2;		 /* private */
	int		xp_laddrlen;	 /* lenght of local address */
	struct sockaddr_in xp_laddr;	 /* local address */
} SVCXPRT;

/*
 *  Approved way of getting address of caller
 */
#define svc_getcaller(x) (&(x)->xp_raddr)

/*
 * Operations defined on an SVCXPRT handle
 *
 * SVCXPRT		*xprt;
 * struct rpc_msg	*msg;
 * xdrproc_t		 xargs;
 * caddr_t		 argsp;
 */
#define SVC_RECV(xprt, msg)				\
	(*(xprt)->xp_ops->xp_recv)((xprt), (msg))
#define svc_recv(xprt, msg)				\
	(*(xprt)->xp_ops->xp_recv)((xprt), (msg))

#define SVC_STAT(xprt)					\
	(*(xprt)->xp_ops->xp_stat)(xprt)
#define svc_stat(xprt)					\
	(*(xprt)->xp_ops->xp_stat)(xprt)

#define SVC_GETARGS(xprt, xargs, argsp)			\
	(*(xprt)->xp_ops->xp_getargs)((xprt), (xargs), (argsp))
#define svc_getargs(xprt, xargs, argsp)			\
	(*(xprt)->xp_ops->xp_getargs)((xprt), (xargs), (argsp))

#define SVC_GETARGS_REQ(xprt, req, xargs, argsp)	\
	(*(xprt)->xp_ops->xp_getargs_req)((xprt), (req), (xargs), (argsp))
#define svc_getargs_req(xprt, req, xargs, argsp)	\
	(*(xprt)->xp_ops->xp_getargs_req)((xprt), (req), (xargs), (argsp))

#define SVC_REPLY(xprt, msg)				\
	(*(xprt)->xp_ops->xp_reply) ((xprt), (msg))
#define svc_reply(xprt, msg)				\
	(*(xprt)->xp_ops->xp_reply) ((xprt), (msg))

#define SVC_REPLY_REQ(xprt, req, msg)			\
	(*(xprt)->xp_ops->xp_reply_req) ((xprt), (req), (msg))
#define svc_reply_req(xprt, msg)			\
	(*(xprt)->xp_ops->xp_reply_req) ((xprt), (req), (msg))

#define SVC_FREEARGS(xprt, xargs, argsp)		\
	(*(xprt)->xp_ops->xp_freeargs)((xprt), (xargs), (argsp))
#define svc_freeargs(xprt, xargs, argsp)		\
	(*(xprt)->xp_ops->xp_freeargs)((xprt), (xargs), (argsp))

#define SVC_DESTROY(xprt)				\
	(*(xprt)->xp_ops->xp_destroy)(xprt)
#define svc_destroy(xprt)				\
	(*(xprt)->xp_ops->xp_destroy)(xprt)


/*
 * Service request
 */
struct svc_req {
	rpc_u_int32		rq_prog;	/* service program number */
	rpc_u_int32		rq_vers;	/* service protocol version */
	rpc_u_int32		rq_proc;	/* the desired procedure */
	struct opaque_auth rq_cred;	/* raw creds from the wire */
	caddr_t		rq_clntcred;	/* read only cooked client cred */
	caddr_t		rq_svccred;	/* read only cooked svc cred */
	SVCXPRT		*rq_xprt;	/* associated transport */

	/* The request's auth flavor *should* be here, but the svc_req 	*/
	/* isn't passed around everywhere it is necessary.  The 	*/
	/* transport *is* passed around, so the auth flavor it stored 	*/
	/* there.  This means that the transport must be single 	*/
	/* threaded, but other parts of SunRPC already require that. 	*/
	/*SVCAUTH		*rq_auth;	 associated auth flavor */
};


/*
 * Service registration
 *
 * svc_register(xprt, prog, vers, dispatch, protocol)
 *	SVCXPRT *xprt;
 *	rpc_u_int32 prog;
 *	rpc_u_int32 vers;
 *	void (*dispatch)();
 *	int protocol;  like TCP or UDP, zero means do not register 
 */
#define svc_register		gssrpc_svc_register
extern bool_t	svc_register();

/*
 * Service un-registration
 *
 * svc_unregister(prog, vers)
 *	rpc_u_int32 prog;
 *	rpc_u_int32 vers;
 */
#define svc_unregister		gssrpc_svc_unregister
extern void	svc_unregister();

/*
 * Transport registration.
 *
 * xprt_register(xprt)
 *	SVCXPRT *xprt;
 */
#define xprt_register		gssrpc_xprt_register
extern void	xprt_register();

/*
 * Transport un-register
 *
 * xprt_unregister(xprt)
 *	SVCXPRT *xprt;
 */
#define xprt_unregister		gssrpc_xprt_unregister
extern void	xprt_unregister();




/*
 * When the service routine is called, it must first check to see if
 * it knows about the procedure; if not, it should call svcerr_noproc
 * and return.  If so, it should deserialize its arguments via
 * SVC_GETARGS or the new SVC_GETARGS_REQ (both defined above).  If
 * the deserialization does not work, svcerr_decode should be called
 * followed by a return.  Successful decoding of the arguments should
 * be followed the execution of the procedure's code and a call to
 * svc_sendreply or the new svc_sendreply_req.
 *
 * Also, if the service refuses to execute the procedure due to too-
 * weak authentication parameters, svcerr_weakauth should be called.
 * Note: do not confuse access-control failure with weak authentication!
 *
 * NB: In pure implementations of rpc, the caller always waits for a reply
 * msg.  This message is sent when svc_sendreply is called.  
 * Therefore pure service implementations should always call
 * svc_sendreply even if the function logically returns void;  use
 * xdr.h - xdr_void for the xdr routine.  HOWEVER, tcp based rpc allows
 * for the abuse of pure rpc via batched calling or pipelining.  In the
 * case of a batched call, svc_sendreply should NOT be called since
 * this would send a return message, which is what batching tries to avoid.
 * It is the service/protocol writer's responsibility to know which calls are
 * batched and which are not.  Warning: responding to batch calls may
 * deadlock the caller and server processes!
 */

#define svc_sendreply		gssrpc_svc_sendreply
#define svcerr_decode		gssrpc_svcerr_decode
#define svcerr_weakauth		gssrpc_svcerr_weakauth
#define svcerr_noproc		gssrpc_svcerr_noproc
#define svcerr_progvers		gssrpc_svcerr_progvers
#define svcerr_auth		gssrpc_svcerr_auth
#define svcerr_noprog		gssrpc_svcerr_noprog
#define svcerr_systemerr	gssrpc_svcerr_systemerr

extern bool_t	svc_sendreply();
extern void	svcerr_decode();
extern void	svcerr_weakauth();
extern void	svcerr_noproc();
extern void	svcerr_progvers();
extern void	svcerr_auth();
extern void	svcerr_noprog();
extern void	svcerr_systemerr();
    
/*
 * Lowest level dispatching -OR- who owns this process anyway.
 * Somebody has to wait for incoming requests and then call the correct
 * service routine.  The routine svc_run does infinite waiting; i.e.,
 * svc_run never returns.
 * Since another (co-existant) package may wish to selectively wait for
 * incoming calls or other events outside of the rpc architecture, the
 * routine svc_getreq is provided.  It must be passed readfds, the
 * "in-place" results of a select system call (see select, section 2).
 */

/*
 * Global keeper of rpc service descriptors in use
 * dynamic; must be inspected before each call to select 
 */
#define svc_fdset	gssrpc_svc_fdset
#define svc_fds		gssrpc_svc_fds
#ifdef FD_SETSIZE
extern fd_set gssrpc_svc_fdset;
#define gssrpc_svc_fds gsssrpc_svc_fdset.fds_bits[0]	/* compatibility */
#else
extern int gssrpc_svc_fds;
#endif /* def FD_SETSIZE */

/*
 * a small program implemented by the svc_rpc implementation itself;
 * also see clnt.h for protocol numbers.
 */
#define rpctest_service		gssrpc_rpctest_service
extern void rpctest_service();

#define svc_getreq	gssrpc_svc_getreq
#define svc_getreqset	gssrpc_svc_getreqset
#define svc_run		gssrpc_svc_run

extern void	svc_getreq();
extern void	svc_getreqset();	/* takes fdset instead of int */
extern void	svc_run(); 	 /* never returns */

/*
 * Socket to use on svcxxx_create call to get default socket
 */
#define	RPC_ANYSOCK	-1

/*
 * These are the existing service side transport implementations
 */

/*
 * Memory based rpc for testing and timing.
 */
#define svcraw_create	gssrpc_svcraw_create
extern SVCXPRT *svcraw_create();

/*
 * Udp based rpc.
 */
#define svcudp_create		gssrpc_svcudp_create
#define svcudp_bufcreate	gssrpc_svcudp_bufcreate
extern SVCXPRT *svcudp_create();
extern SVCXPRT *svcudp_bufcreate();

/*
 * Tcp based rpc.
 */
#define svctcp_create		gssrpc_svctcp_create
extern SVCXPRT *svctcp_create();

#endif /* !__SVC_HEADER__ */
