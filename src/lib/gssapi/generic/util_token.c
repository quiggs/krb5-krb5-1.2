/*
 * Copyright 1993 by OpenVision Technologies, Inc.
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

#include "gssapiP_generic.h"
#include <memory.h>

/*
 * $Id$
 */

/* XXXX this code currently makes the assumption that a mech oid will
   never be longer than 127 bytes.  This assumption is not inherent in
   the interfaces, so the code can be fixed if the OSI namespace
   balloons unexpectedly. */

/* Each token looks like this:

0x60				tag for APPLICATION 0, SEQUENCE
					(constructed, definite-length)
	<length>		possible multiple bytes, need to parse/generate
	0x06			tag for OBJECT IDENTIFIER
		<moid_length>	compile-time constant string (assume 1 byte)
		<moid_bytes>	compile-time constant string
	<inner_bytes>		the ANY containing the application token
					bytes 0,1 are the token type
					bytes 2,n are the token data

For the purposes of this abstraction, the token "header" consists of
the sequence tag and length octets, the mech OID DER encoding, and the
first two inner bytes, which indicate the token type.  The token
"body" consists of everything else.

*/

static int der_length_size(length)
     int length;
{
   if (length < (1<<7))
      return(1);
   else if (length < (1<<8))
      return(2);
#if (SIZEOF_INT == 2)
   else
       return(3);
#else
   else if (length < (1<<16))
      return(3);
   else if (length < (1<<24))
      return(4);
   else
      return(5);
#endif
}

static void der_write_length(buf, length)
     unsigned char **buf;
     int length;
{
   if (length < (1<<7)) {
      *(*buf)++ = (unsigned char) length;
   } else {
      *(*buf)++ = (unsigned char) (der_length_size(length)+127);
#if (SIZEOF_INT > 2)
      if (length >= (1<<24))
	 *(*buf)++ = (unsigned char) (length>>24);
      if (length >= (1<<16))
	 *(*buf)++ = (unsigned char) ((length>>16)&0xff);
#endif
      if (length >= (1<<8))
	 *(*buf)++ = (unsigned char) ((length>>8)&0xff);
      *(*buf)++ = (unsigned char) (length&0xff);
   }
}

/* returns decoded length, or < 0 on failure.  Advances buf and
   decrements bufsize */

static int der_read_length(buf, bufsize)
     unsigned char **buf;
     int *bufsize;
{
   unsigned char sf;
   int ret;

   if (*bufsize < 1)
      return(-1);
   sf = *(*buf)++;
   (*bufsize)--;
   if (sf & 0x80) {
      if ((sf &= 0x7f) > ((*bufsize)-1))
	 return(-1);
      if (sf > SIZEOF_INT)
	  return (-1);
      ret = 0;
      for (; sf; sf--) {
	 ret = (ret<<8) + (*(*buf)++);
	 (*bufsize)--;
      }
   } else {
      ret = sf;
   }

   return(ret);
}

/* returns the length of a token, given the mech oid and the body size */

int g_token_size(mech, body_size)
     gss_OID mech;
     unsigned int body_size;
{
   /* set body_size to sequence contents size */
   body_size += 4 + (int) mech->length;         /* NEED overflow check */
   return(1 + der_length_size(body_size) + body_size);
}

/* fills in a buffer with the token header.  The buffer is assumed to
   be the right size.  buf is advanced past the token header */

void g_make_token_header(mech, body_size, buf, tok_type)
     gss_OID mech;
     int body_size;
     unsigned char **buf;
     int tok_type;
{
   *(*buf)++ = 0x60;
   der_write_length(buf, 4 + mech->length + body_size);
   *(*buf)++ = 0x06;
   *(*buf)++ = (unsigned char) mech->length;
   TWRITE_STR(*buf, mech->elements, ((int) mech->length));
   *(*buf)++ = (unsigned char) ((tok_type>>8)&0xff);
   *(*buf)++ = (unsigned char) (tok_type&0xff);
}

/*
 * Given a buffer containing a token, reads and verifies the token,
 * leaving buf advanced past the token header, and setting body_size
 * to the number of remaining bytes.  Returns 0 on success,
 * G_BAD_TOK_HEADER for a variety of errors, and G_WRONG_MECH if the
 * mechanism in the token does not match the mech argument.  buf and
 * *body_size are left unmodified on error.
 */
gss_int32 g_verify_token_header(mech, body_size, buf_in, tok_type, toksize)
     gss_OID mech;
     int *body_size;
     unsigned char **buf_in;
     int tok_type;
     int toksize;
{
   unsigned char *buf = *buf_in;
   int seqsize;
   gss_OID_desc toid;
   int ret = 0;

   if ((toksize-=1) < 0)
      return(G_BAD_TOK_HEADER);
   if (*buf++ != 0x60)
      return(G_BAD_TOK_HEADER);

   if ((seqsize = der_read_length(&buf, &toksize)) < 0)
      return(G_BAD_TOK_HEADER);

   if (seqsize != toksize)
      return(G_BAD_TOK_HEADER);

   if ((toksize-=1) < 0)
      return(G_BAD_TOK_HEADER);
   if (*buf++ != 0x06)
      return(G_BAD_TOK_HEADER);
 
   if ((toksize-=1) < 0)
      return(G_BAD_TOK_HEADER);
   toid.length = *buf++;

   if ((toksize-=toid.length) < 0)
      return(G_BAD_TOK_HEADER);
   toid.elements = buf;
   buf+=toid.length;

   if (! g_OID_equal(&toid, mech)) 
      ret = G_WRONG_MECH;
 
   /* G_WRONG_MECH is not returned immediately because it's more important
      to return G_BAD_TOK_HEADER if the token header is in fact bad */

   if ((toksize-=2) < 0)
      return(G_BAD_TOK_HEADER);

   if (ret)
       return(ret);

   if ((*buf++ != ((tok_type>>8)&0xff)) ||
       (*buf++ != (tok_type&0xff))) 
      return(G_WRONG_TOKID);

   if (!ret) {
	*buf_in = buf;
	*body_size = toksize;
   }

   return(ret);
}
