/*
 * lib/crypto/des/destest.c
 *
 * Copyright 1990,1991 by the Massachusetts Institute of Technology.
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
 * Test a DES implementation against known inputs & outputs
 */


/*
 * Copyright (C) 1998 by the FundsXpress, INC.
 * 
 * All rights reserved.
 * 
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 * 
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 * 
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "des_int.h"
#include "com_err.h"

#include <stdio.h>

void convert PROTOTYPE((char *, unsigned char []));

void des_cblock_print_file PROTOTYPE((mit_des_cblock, FILE *));

krb5_octet zeroblock[8] = {0,0,0,0,0,0,0,0};

void
main(argc, argv)
int argc;
char *argv[];
{
    char block1[17], block2[17], block3[17];

    mit_des_cblock key, input, output, output2;
    mit_des_key_schedule sched;
    int num = 0;
    int retval;

    int error = 0;

    while (scanf("%16s %16s %16s", block1, block2, block3) == 3) {
	convert(block1, key);
	convert(block2, input);
	convert(block3, output);

	if (retval = mit_des_key_sched(key, sched)) {
            fprintf(stderr, "des test: can't process key");
            exit(1);
        }
	mit_des_cbc_encrypt(&input, &output2, 8, sched, zeroblock, 1);

	if (memcmp((char *)output2, (char *)output, 8)) {
	    fprintf(stderr, 
		    "DES ENCRYPT ERROR, key %s, text %s, real cipher %s, computed cyphertext %02X%02X%02X%02X%02X%02X%02X%02X\n",
		    block1, block2, block3,
		    output2[0],output2[1],output2[2],output2[3],
		    output2[4],output2[5],output2[6],output2[7]);
	    error++;
	}

	/*
	 * Now try decrypting....
	 */
	mit_des_cbc_encrypt(&output, &output2, 8, sched, zeroblock, 0);

	if (memcmp((char *)output2, (char *)input, 8)) {
	    fprintf(stderr, 
		    "DES DECRYPT ERROR, key %s, text %s, real cipher %s, computed cleartext %02X%02X%02X%02X%02X%02X%02X%02X\n",
		    block1, block2, block3,
		    output2[0],output2[1],output2[2],output2[3],
		    output2[4],output2[5],output2[6],output2[7]);
	    error++;
	}

	num++;
    }

    if (error) 
	printf("destest: failed to pass the test\n");
    else
	printf("destest: %d tests passed successfully\n", num);

    exit( (error > 256 && error % 256) ? 1 : error);
}

unsigned int value[128] = {
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
0, 1, 2, 3, 4, 5, 6, 7,
8, 9, -1, -1, -1, -1, -1, -1,
-1, 10, 11, 12, 13, 14, 15, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
-1, -1, -1, -1, -1, -1, -1, -1,
};

void
convert(text, cblock)
char *text;
unsigned char cblock[];
{
    register int i;
    for (i = 0; i < 8; i++) {
	if (value[text[i*2]] == -1 || value[text[i*2+1]] == -1) {
	    printf("Bad value byte %d in %s\n", i, text);
	    exit(1);
	}
	cblock[i] = 16*value[text[i*2]] + value[text[i*2+1]];
    }
    return;
}

/*
 * Fake out the DES library, for the purposes of testing.
 */

#include "des_int.h"

int
mit_des_is_weak_key(key)
    mit_des_cblock key;
{
    return 0;				/* fake it out for testing */
}

void
des_cblock_print_file(x, fp)
    mit_des_cblock x;
    FILE *fp;
{
    unsigned char *y = (unsigned char *) x;
    register int i = 0;
    fprintf(fp," 0x { ");

    while (i++ < 8) {
        fprintf(fp,"%x",*y++);
        if (i < 8)
            fprintf(fp,", ");
    }
    fprintf(fp," }");
}


#define smask(step) ((1<<step)-1)
#define pstep(x,step) (((x)&smask(step))^(((x)>>step)&smask(step)))
#define parity_char(x) pstep(pstep(pstep((x),4),2),1)

/*
 * des_check_key_parity: returns true iff key has the correct des parity.
 *                       See des_fix_key_parity for the definition of
 *                       correct des parity.
 */
int
mit_des_check_key_parity(key)
     register mit_des_cblock key;
{
    int i;
    
    for (i=0; i<sizeof(mit_des_cblock); i++) {
	if ((key[i] & 1) == parity_char(0xfe&key[i])) {
	    printf("warning: bad parity key:");
	    des_cblock_print_file(key, stdout); 
	    putchar('\n');
	      
	    return 1;
	}
    }

    return(1);
}

void
mit_des_fixup_key_parity(key)
     register mit_des_cblock key;
{
    int i;
    for (i=0; i<sizeof(mit_des_cblock); i++) 
      {
	key[i] &= 0xfe;
	key[i] |= 1^parity_char(key[i]);
      }
  
    return;
}
