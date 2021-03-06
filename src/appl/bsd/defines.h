#define OPTS_FORWARD_CREDS           0x00000020
#define OPTS_FORWARDABLE_CREDS       0x00000010
#define RCMD_BUFSIZ	5120

enum kcmd_proto {
  /* Old protocol: DES encryption only.  No subkeys.  No protection
     for cleartext length.  No ivec supplied.  OOB hacks used for
     rlogin.  Checksum may be omitted at connection startup.  */
  KCMD_OLD_PROTOCOL = 1,
  /* New protocol: Any encryption scheme.  Client-generated subkey
     required.  Prepend cleartext-length to cleartext data (but don't
     include it in count).  Starting ivec defined, chained.  In-band
     signalling.  Checksum required.  */
  KCMD_NEW_PROTOCOL,
  /* Hack: Get credentials, and use the old protocol iff the session
     key type is single-DES.  */
  KCMD_PROTOCOL_COMPAT_HACK,
  /* Using Kerberos version 4.  */
  KCMD_V4_PROTOCOL,
  /* ??? */
  KCMD_UNKNOWN_PROTOCOL
};

extern int kcmd (int *sock, char **ahost, int /* u_short */ rport,
		 char *locuser, char *remuser, char *cmd,
		 int *fd2p, char *service, char *realm,
		 krb5_creds **cred,
		 krb5_int32 *seqno, krb5_int32 *server_seqno,
		 struct sockaddr_in *laddr,
		 struct sockaddr_in *faddr,
		 krb5_auth_context *authconp,
		 krb5_flags authopts,
		 int anyport, int suppress_err,
		 enum kcmd_proto *protonum /* input and output */
		 );

extern int rcmd_stream_read (int fd, char *buf, int len, int secondary);
extern int rcmd_stream_write (int fd, char *buf, int len, int secondary);
extern int getport (int *);

extern void rcmd_stream_init_krb5 (krb5_keyblock *in_keyblock,
				   int encrypt_flag, int lencheck,
				   int am_client, enum kcmd_proto protonum);
