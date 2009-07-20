
/*
 * bltBase64.c --
 *
 * This module implements base64 processing procedures for the BLT toolkit.
 *
 *	Copyright 1991-2005 George A Howlett.
 *
 *	Permission is hereby granted, free of charge, to any person obtaining
 *	a copy of this software and associated documentation files (the
 *	"Software"), to deal in the Software without restriction, including
 *	without limitation the rights to use, copy, modify, merge, publish,
 *	distribute, sublicense, and/or sell copies of the Software, and to
 *	permit persons to whom the Software is furnished to do so, subject to
 *	the following conditions:
 *
 *	The above copyright notice and this permission notice shall be
 *	included in all copies or substantial portions of the Software.
 *
 *	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *	LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *	OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "bltInt.h"
#include "bltDBuffer.h"

/*
 * Table for encoding base64.  
 */
static char encode64[64] = {
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
};

/*
 * Table for decoding base64.  
 *
 * Note that NUL and '=' also return 0.  This is so we can blindly decode 4
 * octets without requiring special handing of left-over bytes (i.e. when the
 * encoded buffer did not end on a 3-byte boundary).
 */
#define NA	127

static char decode64[256] = {
    0 /* '\0' */, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    62  /* + */, 
    NA, NA, NA, 
    63  /* / */,
    52  /* 0 */, 53  /* 1 */, 54  /* 2 */, 55  /* 3 */, 56  /* 4 */, 
    57  /* 5 */, 58  /* 6 */, 59  /* 7 */, 60  /* 8 */, 61  /* 9 */, 
    NA, NA, NA, 
    0 /* = */, 
    NA, NA, NA, 
    0   /* A */, 1   /* B */, 2   /* C */, 3   /* D */, 4   /* E */, 
    5   /* F */, 6   /* G */, 7   /* H */, 8   /* I */, 9   /* J */, 
    10  /* K */, 11  /* L */, 12  /* M */, 13  /* N */, 14  /* O */, 
    15  /* P */, 16  /* Q */, 17  /* R */, 18  /* S */, 19  /* T */, 
    20  /* U */, 21  /* V */, 22  /* W */, 23  /* X */, 24  /* Y */, 
    25  /* Z */, 
    NA, NA, NA, NA, NA, NA, 
    26  /* a */, 27  /* b */, 28  /* c */, 29  /* d */, 30  /* e */, 
    31  /* f */, 32  /* g */, 33  /* h */, 34  /* i */, 35  /* j */, 
    36  /* k */, 37  /* l */, 38  /* m */, 39  /* n */, 40  /* o */, 
    41  /* p */, 42  /* q */, 43  /* r */, 44  /* s */, 45  /* t */, 
    46  /* u */, 47  /* v */, 48  /* w */, 49  /* x */, 50  /* y */, 
    51  /* z */, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, 
    NA, NA, NA, NA, NA
};

int
Blt_IsBase64(const char *bytes, size_t nBytes)
{
    const char *bp, *bend;

    for (bp = bytes, bend = bp + nBytes; bp < bend; bp++) {
	unsigned int byte;

	byte = *bp;
	if ((decode64[byte] == NA) && (!isspace(byte))) {
	    return FALSE;
	}
    }
    return TRUE;
}

static char
NextChar(const char **bp) 
{
    char c;

    /* Skip whitespace and invalid characters. Let's see if being fault-tolerant
     * is better than erroring out here.*/
    while (decode64[(int)(**bp)] == NA) {
	(*bp)++;
    }
    c = **bp;
    if ((c != '\0') && (c != '=')) {
	(*bp)++;
    }
    return c;				/* Valid symbol */
}

unsigned char *
Blt_Base64_Decode(Tcl_Interp *interp, const char *string, size_t *lengthPtr)
{
    size_t nBytes;
    unsigned char *bp;
    unsigned char *buffer;

    nBytes = *lengthPtr;

    /* 
     * Assuming that the string contains no padding or whitespace, allocate a
     * buffer with a worst-case length.
     */
    nBytes = ((nBytes + 1) * 3)  / 4; 
    buffer = Blt_Malloc(nBytes);
    if (buffer == NULL) {
	Tcl_AppendResult(interp, "can't allocate ", Blt_Itoa(nBytes), 
			 " for buffer", (char *)NULL);
	return NULL;
    }
    bp = buffer;
    for (;;) {
	char a, b, c, d;
	unsigned int u1, u2, u3;

	a = NextChar(&string);
	b = NextChar(&string);
	c = NextChar(&string);
	d = NextChar(&string);

	if (d == '\0') {
	    if (a != '\0') {
		Tcl_AppendResult(interp, "premature end of base64 data",
			(char *)NULL);
		Blt_Free(buffer);
		return NULL;
	    }
	    break;
        }

	/*
	 * in:     a      b      c     d
	 *       ------.......-------......
	 *      |54321054|32105432|10543210|
	 * out:    u1       u2       u3
	 */

	/* a = [543210xx] | [xxxxxx54] >> 4 */
	u1 = (decode64[(int)a] << 2) | ((decode64[(int)b] & 0x30) >> 4);
	/* b = [3210xxxx] | [xxxx5432]  */
	u2 = ((decode64[(int)b] & 0x0F) << 4) |((decode64[(int)c] & 0x3C) >> 2);
	/* c = [10xxxxxx] | [xx543210]  */
	u3 = ((decode64[(int)c] & 0x03) << 6) | decode64[(int)d];

	if (d == '=') {
	    if ((a == '=') || (b == '=')) {
		break;			/* This should not be possible. */
	    }
	    if (c == '=') {
		*bp++ = (unsigned char)u1;
	    } else {
		*bp++ = (unsigned char)u1;
		*bp++ = (unsigned char)u2;
	    }
	    break;
	}
	bp[0] = (unsigned char)u1;
	bp[1] = (unsigned char)u2;
	bp[2] = (unsigned char)u3;
	bp += 3;
    }
    nBytes = bp - buffer;
    /* Reset the fill point to the number of bytes processed. */
    *lengthPtr = nBytes;
    return buffer;
}

char *
Blt_Base64_Encode(Tcl_Interp *interp, unsigned char *buffer, size_t bufsize) 
{
    char *dest, *dp;
    int count, remainder;
    size_t length;
    unsigned char *sp, *send;

    /* Compute worst-case length. */
    length = (((bufsize + 1) * 4) + 2) / 3; 
    length += (length + 59) / 60;	/* Add space for newlines. */
    length++;				/* NUL byte */

    dest = Blt_Malloc(sizeof(char) * length);
    if (dest == NULL) {
	Tcl_AppendResult(interp, "can't allocate \"", Blt_Itoa(length), 
		"\" bytes for buffer", (char *)NULL);
	return NULL;
    }
    count = 0;
    remainder = bufsize % 3;
    send = buffer + (bufsize - remainder);
    dp = dest;
    for (sp = buffer; sp < send; sp += 3) {
	int a, b, c, d;

	/*
	 * in:        0        1        2
	 *       |76543210|76543210|76543210|
	 *        ------.......-------......
	 * out:     a      b      c     d
	 */
	/* a = [xx765432] */
	a = sp[0] >> 2;
	/* b = [xx10xxxx] | [xxxx7654]  */
	b = ((sp[0] & 0x03) << 4) | ((sp[1] & 0xF0) >> 4);
	/* c = [xx3210xx] | [xxxxxx76]  */
	c = ((sp[1] & 0x0F) << 2) | ((sp[2] & 0xC0) >> 6);
	/* d = [xx543210]  */
	d = (sp[2] & 0x3F);

	dp[0] = encode64[a];
	dp[1] = encode64[b];
	dp[2] = encode64[c];
	dp[3] = encode64[d];

	dp += 4;
	count += 4;
	if (count > 60) {
	    *dp++ = '\n';
	    count = 0;
	}
    }

    if (remainder > 0) {
	int a, b, c;

	/* 
	 * Handle the two cases where the input buffer doesn't end on a 3-byte
	 * boundary.
	 */
	if (remainder == 2) {
	    a = sp[0] >> 2;
	    b = ((sp[0] & 0x03) << 4) | ((sp[1] & 0xF0) >> 4);
	    c = ((sp[1] & 0x0F) << 2);
	    dp[0] = encode64[a];
	    dp[1] = encode64[b];
	    dp[2] = encode64[c];
	    dp[3] = '=';
	} else if (remainder == 1) {
	    a = sp[0] >> 2;
	    b = ((sp[0] & 0x03) << 4);
	    dp[0] = encode64[a];
	    dp[1] = encode64[b];
	    dp[2] = dp[3] = '=';
	}
	dp += 4;
	count += 4;
	if (count > 60) {
	    *dp++ = '\n';
	}
    }
    assert((size_t)(dp - dest) < length);
    *dp = '\0';
    return dest;
}



/*ARGSUSED*/
static int
Base64Cmd(ClientData clientData, Tcl_Interp *interp, int objc, 
	  Tcl_Obj *const *objv)
{
    int option;
    static const char *args[] = {
	"decode", "encode",  NULL,
    };

    if (objc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", 
		Tcl_GetString(objv[0]), "encode|decode bytes\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], args, "qualifier", TCL_EXACT,
	    &option) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (option) {
    case 0:				/* decode */
	{
	    int length;
	    size_t nBytes;		/* # bytes in decoded string. */
	    unsigned char *bp;
	    const char *in;

	    in = Tcl_GetStringFromObj(objv[2], &length);
	    nBytes = (size_t)length;
	    bp = Blt_Base64_Decode(interp, in, &nBytes); 
	    if (bp == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_SetByteArrayObj(Tcl_GetObjResult(interp), bp, nBytes);
	}
	break;
    case 1:				/* encode */
	{
	    int length;
	    unsigned char *bp;
	    const char *out;

	    bp = Tcl_GetByteArrayFromObj(objv[2], &length);
	    out = Blt_Base64_Encode(interp, bp, length);
	    if (out == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), out, -1);
	}
	break;
    default:
	Tcl_AppendResult(interp, "bad option \"", Tcl_GetString(objv[1]), 
			 "\": should be encode or decode", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

int
Blt_Base64CmdInitProc(Tcl_Interp *interp)
{
    static Blt_InitCmdSpec cmdSpec = { "base64", Base64Cmd, };

    return Blt_InitCmd(interp, "::blt", &cmdSpec);
}

