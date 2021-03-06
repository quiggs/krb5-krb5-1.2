#include "rpc_test.h"
#include <stdio.h>
#include <stdlib.h> /* getenv, exit */
#include <sys/types.h>
#include <syslog.h>

/* States a server can be in wrt request */

#define	_IDLE 0
#define	_SERVED 1

static int _rpcsvcstate = _IDLE;	/* Set when a request is serviced */
static int _rpcsvccount = 0;		/* Number of requests being serviced */

static
void _msgout(msg)
	char *msg;
{
	syslog(LOG_ERR, msg);
}

void
rpc_test_prog_1(rqstp, transp)
	struct svc_req *rqstp;
	register SVCXPRT *transp;
{
	union {
		char *rpc_test_echo_1_arg;
	} argument;
	char *result;
	bool_t (*xdr_argument)(), (*xdr_result)();
	char *(*local)();

	_rpcsvccount++;
	switch (rqstp->rq_proc) {
	case NULLPROC:
		(void) svc_sendreply(transp, xdr_void,
			(char *)NULL);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;

	case RPC_TEST_ECHO:
		xdr_argument = xdr_wrapstring;
		xdr_result = xdr_wrapstring;
		local = (char *(*)()) rpc_test_echo_1;
		break;

	default:
		svcerr_noproc(transp);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;
	}
	(void) memset((char *)&argument, 0, sizeof (argument));
	if (!svc_getargs(transp, xdr_argument, &argument)) {
		svcerr_decode(transp);
		_rpcsvccount--;
		_rpcsvcstate = _SERVED;
		return;
	}
	result = (*local)(&argument, rqstp);
	if (result != NULL && !svc_sendreply(transp, xdr_result, result)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_argument, &argument)) {
		_msgout("unable to free arguments");
		exit(1);
	}
	_rpcsvccount--;
	_rpcsvcstate = _SERVED;
	return;
}
