/* $Header$ */
/*
 * ed.chared.c: Character editing functions.
 */
/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "config.h"
RCSID("$Id$")

#include "sh.h"
#include "ed.h"
#include "tw.h"
#include "ed.defns.h"

/* #define SDEBUG */

#define NOP    0
#define DELETE 1
#define INSERT 2
#define CHANGE 3
#define UP_SEARCH 4
#define DN_SEARCH 5

static Char *InsertPos = InputBuf; /* Where insertion starts */
static Char *ActionPos = 0;	   /* Where action begins  */
static int  ActionFlag = 0;	   /* What delayed action to take */
static int  searchdir = UP_SEARCH; /* Direction of last search */

/* all routines that start with c_ are private to this set of routines */
static	void	 c_alternativ_key_map	__P((int));
static	void	 c_insert		__P((int));
static	void	 c_delafter		__P((int));
static	void	 c_delbefore		__P((int));
static	Char	*c_prev_word		__P((Char *, Char *, int));
static	Char	*c_next_word		__P((Char *, Char *, int));
static	Char	*c_beg_next_word	__P((Char *, Char *, int));
static	void	 c_copy			__P((Char *, Char *, int));
static	Char	*c_number		__P((Char *, int *, int));
static	Char	*c_expand		__P((Char *));
static	void	 c_excl			__P((Char *));
static	void	 c_substitute		__P((void));
static	int	 c_hmatch		__P((Char *));
static	void	 c_hsetpat		__P((void));
#ifdef COMMENT
static	void	 c_get_word		__P((Char **, Char **));
#endif
static	void	 c_delafter_save	__P((int));
static	void	 c_delbefore_save	__P((int));
static	Char	*c_preword		__P((Char *, Char *, int));
static	Char	*c_endword		__P((Char *, Char *, int));
static	Char	*c_eword		__P((Char *, Char *, int));
static  CCRETVAL v_repeat_srch		__P((int));

static void
c_alternativ_key_map(state)
    int     state;
{
    switch ( state ) {
    case 0:
	CurrentKeyMap = CcKeyMap;
	break;
    case 1:
	CurrentKeyMap = CcAltMap;
	break;
    default:
	return;
    }

    AltKeyMap = state;
}

static void
c_insert(num)
    register int num;
{
    register Char *cp;

    if (LastChar + num >= InputLim)
	return;			/* can't go past end of buffer */

    if (Cursor < LastChar) {	/* if I must move chars */
	for (cp = LastChar; cp >= Cursor; cp--)
	    cp[num] = *cp;
    }
    LastChar += num;
}

static void
c_delafter_save(num)	
    register int num;
{
    register Char *cp, *kp;

    if (Cursor + num > LastChar)
	num = LastChar - Cursor;/* bounds check */

    if (num > 0) {		/* if I can delete anything */
	kp = UndoBuf;		/* Set Up for VI undo command */
	UndoAction = INSERT;
	UndoSize = num;
	UndoPtr  = Cursor;

	for (cp = Cursor; cp <= LastChar; cp++) {
	    *kp++ = *cp;	/* Save deleted chars into undobuf */
	    *cp = cp[num];
	}
	LastChar -= num;
    }
    else
	replacemode = 0;
}

static void
c_delafter(num)			/* delete after dot, with bounds checking */
    register int num;
{
    register Char *cp;

    if (Cursor + num > LastChar)
	num = LastChar - Cursor;/* bounds check */

    if (num > 0) 		/* if I can delete anything */
    {
	for (cp = Cursor; cp <= LastChar; cp++)
	    *cp = cp[num];
	LastChar -= num;
    }
    else
	replacemode = 0;
}

static void
c_delbefore_save(num)
    register int num;
{
    register Char *cp, *kp;

    if (Cursor - num < InputBuf)
	num = Cursor - InputBuf;/* bounds check */

    if (num > 0) {		/* if I can delete anything */
	kp = UndoBuf;		/* Set Up for VI undo command */
	UndoAction = INSERT;
	UndoSize = num;
	UndoPtr  = Cursor - num;

	for (cp = Cursor - num; cp <= LastChar; cp++) {
	    *kp++ = *cp;
	    *cp = cp[num];
	}
	LastChar -= num;
    }
}

static void
c_delbefore(num)		/* delete before dot, with bounds checking */
    register int num;
{
    register Char *cp;

    if (Cursor - num < InputBuf)
	num = Cursor - InputBuf;/* bounds check */

    if (num > 0) 		/* if I can delete anything */
    {
	for (cp = Cursor - num; cp <= LastChar; cp++)
	    *cp = cp[num];
	LastChar -= num;
    }
}

static Char *
c_preword(p, low, n)
    register Char *p, *low;
    register int n;
{
    p--;
    while (n--) {
	while ((p >= low) && Isspace(*p)) 
	    p--;

	if (Isalnum(*p))
	    while ((p >= low) && Isalnum(*p)) 
		p--;
	else
	    while ((p >= low) && !Isspace(*p) || Isalnum(*p)) 
		p--;

    }
    /* cp now points to one character before the word */
    p++;
    if (p < low)
	p = low;
    /* cp now points where we want it */
    return (p);
}

static Char *
c_prev_word(p, low, n)
    register Char *p, *low;
    register int n;
{
    if (VImode) {
	if (!Isspace(*p) && Isspace(p[-1])) 
	    p--; 

	while ( n-- ) {
	    while ((p >= low) && Isspace(*p)) 
		p--;
	    while ((p >= low) && !Isspace(*p)) 
		p--;
	}
	/* cp now points to one character before the word */
	p++;
	if (p < low)
	    p = low;
	/* cp now points where we want it */
	return (p);
    }
    else {
	/* to the beginning of the PREVIOUS word, not this one */
	p--;

	while (n--) {
	    while ((p >= low) && !isword(*p)) 
		p--;
	    while ((p >= low) && isword(*p)) 
		p--;
	}

	/* cp now points to one character before the word */
	p++;
	if (p < low)
	    p = low;
	/* cp now points where we want it */
	return (p);
    }
}

static Char *
c_next_word(p, high, n)
    register Char *p, *high;
    register int n;
{
    if (VImode) 
	while (n--) {
	    while ((p < high) && !Isspace(*p)) 
		p++;
	    while ((p < high) && Isspace(*p)) 
		p++;
	}
    else
	while (n--) {
	    while ((p < high) && !isword(*p)) 
		p++;
	    while ((p < high) && isword(*p)) 
		p++;
	}
    if (p > high)
	p = high;
    /* p now points where we want it */
    return (p);
}

static Char *
c_beg_next_word(p, high, n)
    register Char *p, *high;
    register int n;
{
    if (VImode)
	while (n--) { 
	    if (Isalnum(*p))
		while ((p < high) && Isalnum(*p)) 
		    p++;
	    else
		while ((p < high) && !Isspace(*p) || Isalnum(*p))
		    p++;

	    while ((p < high) && (Isspace(*p)))
		p++;
	}
    else
	while (n--) {
	    while ((p < high) && isword(*p))
		p++;
	    while ((p < high) && !isword(*p))
		p++;
	}

    if (p > high)
	p = high;
    /* p now points where we want it */
    return (p);
}

/*
 * Expand-History (originally "Magic-Space") code added by
 * Ray Moody <ray@gibbs.physics.purdue.edu>
 * this is a neat, but odd, addition.
 */

/*
 * c_copy is sorta like bcopy() except that we handle overlap between
 * source and destination memory
 */

static void
c_copy(src, dst, length)
    register Char *src, *dst;
    register int length;
{
    if (src > dst) {
	while (length--) {
	    *dst++ = *src++;
	}
    }
    else {
	src += length;
	dst += length;
	while (length--) {
	    *--dst = *--src;
	}
    }
}

/*
 * c_number: Ignore character p points to, return number appearing after that.
 * A '$' by itself means a big number; "$-" is for negative; '^' means 1.
 * Return p pointing to last char used.
 */

/*
 * dval is the number to subtract from for things like $-3
 */

static Char *
c_number(p, num, dval)
    register Char *p;
    register int *num;
    register int dval;
{
    register int i;
    register int sign = 1;

    if (*++p == '^') {
	*num = 1;
	return (p);
    }
    if (*p == '$') {
	if (*++p != '-') {
	    *num = NCARGS;	/* Handle $ */
	    return (--p);
	}
	sign = -1;		/* Handle $- */
	++p;
    }
    for (i = 0; *p >= '0' && *p <= '9'; i = 10 * i + *p++ - '0');
    *num = (sign < 0 ? dval - i : i);
    return (--p);
}

/*
 * excl_expand: There is an excl to be expanded to p -- do the right thing
 * with it and return a version of p advanced over the expanded stuff.  Also,
 * update tsh_cur and related things as appropriate...
 */

static Char *
c_expand(p)
    register Char *p;
{
    register Char *q;
    register struct Hist *h = Histlist.Hnext;
    register struct wordent *l;
    int     i, from, to, dval;
    bool    all_dig;
    bool    been_once = 0;
    Char   *op = p;
    Char    buf[INBUFSIZ];
    Char   *bend = buf;
    Char   *modbuf, *omodbuf;

    if (!h)
	goto excl_err;
excl_sw:
    switch (*(q = p + 1)) {

    case '^':
	bend = expand_lex(buf, INBUFSIZ, &h->Hlex, 1, 1);
	break;

    case '$':
	if ((l = (h->Hlex).prev))
	    bend = expand_lex(buf, INBUFSIZ, l->prev->prev, 0, 0);
	break;

    case '*':
	bend = expand_lex(buf, INBUFSIZ, &h->Hlex, 1, NCARGS);
	break;

    default:
	if (been_once) {	/* unknown argument */
	    /* assume it's a modifier, e.g. !foo:h, and get whole cmd */
	    bend = expand_lex(buf, INBUFSIZ, &h->Hlex, 0, NCARGS);
	    q -= 2;
	    break;
	}
	been_once = 1;

	if (*q == ':')		/* short form: !:arg */
	    --q;

	if (*q != HIST) {
	    /*
	     * Search for a space, tab, or colon.  See if we have a number (as
	     * in !1234:xyz).  Remember the number.
	     */
	    for (i = 0, all_dig = 1; 
		 *q != ' ' && *q != '\t' && *q != ':' && q < Cursor; q++) {
		/*
		 * PWP: !-4 is a valid history argument too, therefore the test
		 * is if not a digit, or not a - as the first character.
		 */
		if ((*q < '0' || *q > '9') && (*q != '-' || q != p + 1))
		    all_dig = 0;
		else if (*q == '-')
		    all_dig = 2;/* we are sneeky about this */
		else
		    i = 10 * i + *q - '0';
	    }
	    --q;

	    /*
	     * If we have a number, search for event i.  Otherwise, search for
	     * a named event (as in !foo).  (In this case, I is the length of
	     * the named event).
	     */
	    if (all_dig) {
		if (all_dig == 2)
		    i = -i;	/* make it negitive */
		if (i < 0)	/* if !-4 (for example) */
		    i = eventno + 1 + i;	/* remember: i is < 0 */
		for (; h; h = h->Hnext) {
		    if (h->Hnum == i)
			break;
		}
	    }
	    else {
		for (i = q - p; h; h = h->Hnext) {
		    if ((l = &h->Hlex)) {
			if (!Strncmp(p + 1, l->next->word, i))
			    break;
		    }
		}
	    }
	}
	if (!h)
	    goto excl_err;
	if (q[1] == ':' || q[1] == '-' || q[1] == '*' ||
	    q[1] == '$' || q[1] == '^') {	/* get some args */
	    p = q[1] == ':' ? ++q : q;
	    /*
	     * Go handle !foo:*
	     */
	    if ((q[1] < '0' || q[1] > '9') &&
		q[1] != '-' && q[1] != '$' && q[1] != '^')
		goto excl_sw;
	    /*
	     * Go handle !foo:$
	     */
	    if (q[1] == '$' && (q[2] != '-' || q[3] < '0' || q[3] > '9'))
		goto excl_sw;
	    /*
	     * Count up the number of words in this event.  Store it in dval.
	     * Dval will be fed to number.
	     */
	    dval = 0;
	    if ((l = h->Hlex.prev)) {
		for (l = l->prev; l != h->Hlex.next; l = l->prev, dval++);
	    }
	    if (!dval)
		goto excl_err;
	    if (q[1] == '-')
		from = 0;
	    else
		q = c_number(q, &from, dval);
	    if (q[1] == '-') {
		++q;
		if ((q[1] < '0' || q[1] > '9') && q[1] != '$')
		    to = dval - 1;
		else
		    q = c_number(q, &to, dval);
	    }
	    else if (q[1] == '*') {
		++q;
		to = NCARGS;
	    }
	    else {
		to = from;
	    }
	    if (from < 0 || to < from)
		goto excl_err;
	    bend = expand_lex(buf, INBUFSIZ, &h->Hlex, from, to);
	}
	else {			/* get whole cmd */
	    bend = expand_lex(buf, INBUFSIZ, &h->Hlex, 0, NCARGS);
	}
	break;
    }

    /*
     * Apply modifiers, if any.
     */
    if (q[1] == ':') {
	*bend = '\0';
	omodbuf = buf;
	while (q[1] == ':' && (modbuf = domod(omodbuf, (int) q[2])) != NOSTR) {
	    if (omodbuf != buf)
		xfree((ptr_t) omodbuf);
	    omodbuf = modbuf;
	    q += 2;
	}
	if (omodbuf != buf) {
	    (void) Strcpy(buf, omodbuf);
	    xfree((ptr_t) omodbuf);
	    bend = Strend(buf);
	}
    }

    /*
     * Now replace the text from op to q inclusive with the text from buf to
     * bend.
     */
    q++;

    /*
     * Now replace text non-inclusively like a real CS major!
     */
    if (LastChar + (bend - buf) - (q - op) >= InputLim)
	goto excl_err;
    c_copy(q, q + (bend - buf) - (q - op), LastChar - q);
    LastChar += (bend - buf) - (q - op);
    Cursor += (bend - buf) - (q - op);
    c_copy(buf, op, (bend - buf));
    return (op + (bend - buf));
excl_err:
    Beep();
    return (op + 1);
}

/*
 * c_excl: An excl has been found at point p -- back up and find some white
 * space (or the beginning of the buffer) and properly expand all the excl's
 * from there up to the current cursor position. We also avoid (trying to)
 * expanding '>!'
 */

static void
c_excl(p)
    register Char *p;
{
    register int i;
    register Char *q;

    /*
     * if />[SPC TAB]*![SPC TAB]/, back up p to just after the >. otherwise,
     * back p up to just before the current word.
     */
    if ((p[1] == ' ' || p[1] == '\t') &&
	(p[-1] == ' ' || p[-1] == '\t' || p[-1] == '>')) {
	for (q = p - 1; q > InputBuf && (*q == ' ' || *q == '\t'); --q);
	if (*q == '>')
	    ++p;
    }
    else {
	while (*p != ' ' && *p != '\t' && p > InputBuf)
	    --p;
    }

    /*
     * Forever: Look for history char.  (Stop looking when we find the cursor.)
     * Count backslashes.  Of odd, skip history char. Return if all done.
     * Expand if even number of backslashes.
     */
    for (;;) {
	while (*p != HIST && p < Cursor)
	    ++p;
	for (i = 1; (p - i) >= InputBuf && p[-i] == '\\'; i++);
	if (i % 2 == 0)
	    ++p;
	if (p >= Cursor)
	    return;
	if (i % 2 == 1)
	    p = c_expand(p);
    }
}


static void
c_substitute()
{
    register Char *p;

    /*
     * Start p out one character before the cursor.  Move it backwards looking
     * for white space, the beginning of the line, or a history character.
     */
    for (p = Cursor - 1; 
	 p > InputBuf && *p != ' ' && *p != '\t' && *p != HIST; --p);

    /*
     * If we found a history character, go expand it.
     */
    if (*p == HIST)
	c_excl(p);
    Refresh();
}

static void
c_delfini()		/* Finish up delete action */
{
    register int Size;
    ActionFlag = 0;

    if (ActionPos == 0) 
	return;

    UndoAction = INSERT;

    if (Cursor > ActionPos) {
	Size = (int) (Cursor-ActionPos);
	c_delbefore_save(Size); 
	Cursor = ActionPos;
	RefCursor();
    }
    else if ( Cursor < ActionPos ) {
	Size = (int)(ActionPos-Cursor);
	c_delafter_save(Size);
    }
    else  {
	Size = 1;
	c_delafter_save (Size);
    }
    UndoPtr = Cursor;
    UndoSize = Size;
}

static Char *
c_endword(p, high, n)
    register Char *p, *high;
    register int n;
{
    p++;

    while (n--) {
	while ((p < high) && Isspace(*p))
	    p++;
	while ((p < high) && !Isspace(*p)) 
	    p++;
    }

    p--;
    return(p);
}


static Char *
c_eword(p, high, n)
    register Char *p, *high;
    register int n;
{
    p++;

    while (n--) {
	while ((p < high) && Isspace(*p)) 
	    p++;

	if (Isalnum(*p))
	    while ((p < high) && Isalnum(*p)) 
		p++;
	else
	    while ((p < high) && !(Isspace(*p) || Isalnum(*p)))
		p++;
    }

    p--;
    return(p);
}

/*
 * semi-PUBLIC routines.  Any routine that is of type CCRETVAL is an
 * entry point, called from the CcKeyMap indirected into the
 * CcFuncTbl array.
 */

/*ARGSUSED*/
CCRETVAL
v_cmd_mode(c)
    int c;
{

    InsertPos = 0;
    ActionFlag = 0;	/* [Esc] cancels pending action */
    ActionPos = 0;
    if ( UndoPtr > Cursor )
	UndoSize = (int)(UndoPtr - Cursor);
    else
	UndoSize = (int)( Cursor - UndoPtr );

    replacemode = 0;
    c_alternativ_key_map(1);
    if (Cursor > InputBuf)
	Cursor--;
    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_unassigned(c)
    int c;
{				/* bound to keys that arn't really assigned */
    Beep();
    flush();
    return (CC_NORM);
}

CCRETVAL
e_insert(c)
    register int c;
{
    register int i;
#ifndef SHORT_STRINGS
    c &= ASCII;			/* no meta chars ever */
#endif

    if (!c)
	return (CC_ERROR);	/* no NULs in the input ever!! */

    if (LastChar + Argument >= InputLim)
	return (CC_ERROR);	/* end of buffer space */

    if (Argument == 1)  	/* How was this optimized ???? */
    {

	if ( ( replacemode == 1 ) || ( replacemode == 2) )
	{
	    UndoBuf[UndoSize++] = *Cursor;
	    UndoBuf[UndoSize] = '\0';
	    c_delafter(1);   /* Do NOT use the saving ONE */
    	}

        c_insert(1);

	*Cursor++ = c;
	DoingArg = 0;		/* just in case */
	RefPlusOne();		/* fast refresh for one char. */
    }
    else 
    {
	if ( ( replacemode == 1 ) || ( replacemode == 2) )
	{
	    for(i=0;i<Argument;i++)
	    {
		UndoBuf[UndoSize++] = *(Cursor+i);
	    }
	    UndoBuf[UndoSize] = '\0';
	    c_delafter(Argument);   /* Do NOT use the saving ONE */
    	}

        c_insert(Argument);

	while (Argument--)
	    *Cursor++ = c;
	Refresh();
    }

    if (replacemode == 2)
	(void) v_cmd_mode(0);

    return (CC_NORM);
}

int
InsertStr(s)			/* insert ASCIZ s at cursor (for complete) */
    Char   *s;
{
    register int len;

    if ((len = Strlen(s)) <= 0)
	return -1;
    if (LastChar + len >= InputLim)
	return -1;		/* end of buffer space */

    c_insert(len);
    while (len--)
	*Cursor++ = *s++;
    return 0;
}

void
DeleteBack(n)			/* delete the n characters before . */
    int     n;
{
    if (n <= 0)
	return;
    if (Cursor >= &InputBuf[n]) {
	c_delbefore(n);		/* delete before dot */
	Cursor -= n;
	if (Cursor < InputBuf)
	    Cursor = InputBuf;	/* bounds check */
    }
}

CCRETVAL
e_digit(c)			/* gray magic here */
    register int c;
{
    if (!Isdigit(c))
	return (CC_ERROR);	/* no NULs in the input ever!! */

    if (DoingArg) {		/* if doing an arg, add this in... */
	if (LastCmd == F_ARGFOUR)	/* if last command was ^U */
	    Argument = c - '0';
	else {
	    if (Argument > 1000000)
		return CC_ERROR;
	    Argument = (Argument * 10) + (c - '0');
	}
	return (CC_ARGHACK);
    }
    else {
	if (LastChar + 1 >= InputLim)
	    return CC_ERROR;	/* end of buffer space */

	c_insert(1);
	*Cursor++ = c;
	DoingArg = 0;		/* just in case */
	RefPlusOne();		/* fast refresh for one char. */
    }
    return (CC_NORM);
}

CCRETVAL
e_argdigit(c)			/* for ESC-n */
    register int c;
{
    c &= ASCII;

    if (!Isdigit(c))
	return (CC_ERROR);	/* no NULs in the input ever!! */

    if (DoingArg) {		/* if doing an arg, add this in... */
	if (Argument > 1000000)
	    return CC_ERROR;
	Argument = (Argument * 10) + (c - '0');
    }
    else {			/* else starting an argument */
	Argument = c - '0';
	DoingArg = 1;
    }
    return (CC_ARGHACK);
}

CCRETVAL
v_zero(c)			/* command mode 0 for vi */
    register int c;
{
    if (DoingArg) {		/* if doing an arg, add this in... */
	if (Argument > 1000000)
	    return CC_ERROR;
	Argument = (Argument * 10) + (c - '0');
	return (CC_ARGHACK);
    }
    else {			/* else starting an argument */
	Cursor = InputBuf;
	if ( ActionFlag == DELETE ) {
	   c_delfini();
	   return(CC_REFRESH);
        }
	RefCursor();		/* move the cursor */
	return (CC_NORM);
    }
}

/*ARGSUSED*/
CCRETVAL
e_newline(c)
    int c;
{				/* always ignore argument */

    if (VImode) { /* VI mode searching ONLY */
	if ( ActionFlag == UP_SEARCH ) {
	    c_alternativ_key_map(1);
	    if (Cursor != InputBuf + 1)
		c_hsetpat();
	    LastCmd = F_UP_SEARCH_HIST;  /* Hack to stop c_hsetpat */
/*	    LastChar = InputBuf; */
	    return( e_up_search_hist(c));
	}
	else if ( ActionFlag == DN_SEARCH ) {
	    c_alternativ_key_map(1);
	    if (Cursor != InputBuf + 1) 
		c_hsetpat();
	    LastCmd = F_DOWN_SEARCH_HIST;  /* Hack to stop c_hsetpat */
/*	    LastChar = InputBuf; */
	    return( e_down_search_hist(c));
	}
    }

    PastBottom();
    *LastChar++ = '\n';		/* for the benefit of CSH */
    *LastChar = '\0';		/* just in case */
    if (VImode)
	InsertPos = InputBuf;	/* Reset editing position */
    return (CC_NEWLINE);	/* we must do a ResetInLine later */
}

/*ARGSUSED*/
CCRETVAL
e_send_eof(c)
    int c;
{				/* for when ^D is ONLY send-eof */
    PastBottom();
    *LastChar = '\0';		/* just in case */
#ifdef notdef
    ResetInLine();		/* reset the input pointers */
#endif
    return (CC_EOF);
}

/*ARGSUSED*/
CCRETVAL
e_complete(c)
    int c;
{
    *LastChar = '\0';		/* just in case */
    return (CC_COMPLETE);
}

/*ARGSUSED*/
CCRETVAL
v_cm_complete(c)
    int c;
{
    if (Cursor < LastChar)
	Cursor++;
    *LastChar = '\0';		/* just in case */
    return (CC_COMPLETE);
}

/*ARGSUSED*/
CCRETVAL
e_toggle_hist(c)
    int c;
{
    struct Hist *hp;
    int     h;

    *LastChar = '\0';		/* just in case */

    if (Hist_num <= 0) {
	return CC_ERROR;
    }

    hp = Histlist.Hnext;
    if (hp == NULL) {	/* this is only if no history */
	return (CC_ERROR);
    }

    for (h = 1; h < Hist_num; h++)
	hp = hp->Hnext;

    if (!CurrentHistLit) {
	if (hp->histline) {
	    copyn(InputBuf, hp->histline, INBUFSIZ);
	    CurrentHistLit = 1;
	}
	else {
	    return CC_ERROR;
	}
    }
    else {
	(void) sprlex(InputBuf, &hp->Hlex);
	CurrentHistLit = 0;
    }

    LastChar = InputBuf + Strlen(InputBuf);
    if (LastChar > InputBuf) {
	if (LastChar[-1] == '\n')
	    LastChar--;
	if (LastChar[-1] == ' ')
	    LastChar--;
	if (LastChar < InputBuf)
	    LastChar = InputBuf;
    }

    if (VImode)
	Cursor = InputBuf;
    else
	Cursor = LastChar;

    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_up_hist(c)
    int c;
{
    struct Hist *hp;
    int     hnumcntr;
    Char    beep = 0;

    UndoAction = 0;
    *LastChar = '\0';		/* just in case */

    if (Hist_num == 0) {	/* save the current buffer away */
	copyn(HistBuf, InputBuf, INBUFSIZ);
	LastHist = HistBuf + (LastChar - InputBuf);
    }

    hp = Histlist.Hnext;
    if (hp == NULL) {	/* this is only if no history */
	return (CC_ERROR);
    }

    Hist_num += Argument;
    for (hnumcntr = 1; hnumcntr < Hist_num; hnumcntr++) {
	if ((hp->Hnext) == NULL) {
	    Hist_num = hnumcntr;
	    beep = 1;
	    break;
	}
	hp = hp->Hnext;
    }

    if (HistLit && hp->histline) {
	copyn(InputBuf, hp->histline, INBUFSIZ);
	CurrentHistLit = 1;
    }
    else {
	(void) sprlex(InputBuf, &hp->Hlex);
	CurrentHistLit = 0;
    }
    LastChar = InputBuf + Strlen(InputBuf);
    if (LastChar > InputBuf) {
	if (LastChar[-1] == '\n')
	    LastChar--;
	if (LastChar[-1] == ' ')
	    LastChar--;
	if (LastChar < InputBuf)
	    LastChar = InputBuf;
    }

    if (VImode)
	Cursor = InputBuf;
    else
	Cursor = LastChar;

    Refresh();
    if (beep)
	return (CC_ERROR);
    else
	return (CC_NORM);	/* was CC_UP_HIST */
}

/*ARGSUSED*/
CCRETVAL
e_down_hist(c)
    int c;
{
    struct Hist *hp;
    int     hnumcntr;

    UndoAction = NOP;
    *LastChar = '\0';		/* just in case */

    Hist_num -= Argument;

    if (Hist_num < 0) {
	Hist_num = 0;
	return (CC_ERROR);	/* make it beep */
    }

    if (Hist_num == 0) {	/* if really the current line */
	copyn(InputBuf, HistBuf, INBUFSIZ);
	LastChar = InputBuf + (LastHist - HistBuf);

    if (VImode)
	Cursor = InputBuf;
    else
	Cursor = LastChar;

	return (CC_REFRESH);
    }

    hp = Histlist.Hnext;
    if (hp == NULL)
	return (CC_ERROR);

    for (hnumcntr = 1; hnumcntr < Hist_num; hnumcntr++) {
	if ((hp->Hnext) == NULL) {
	    Hist_num = hnumcntr;
	    return (CC_ERROR);
	}
	hp = hp->Hnext;
    }

    if (HistLit && hp->histline) {
	copyn(InputBuf, hp->histline, INBUFSIZ);
	CurrentHistLit = 1;
    }
    else {
	(void) sprlex(InputBuf, &hp->Hlex);
	CurrentHistLit = 0;
    }
    LastChar = InputBuf + Strlen(InputBuf);

    if (LastChar > InputBuf) {
	if (LastChar[-1] == '\n')
	    LastChar--;
	if (LastChar[-1] == ' ')
	    LastChar--;
	if (LastChar < InputBuf)
	    LastChar = InputBuf;
    }

    if (VImode)
	Cursor = InputBuf;
    else
	Cursor = LastChar;

    return (CC_REFRESH);
}



static Char patbuf[INBUFSIZ];
static int patlen = 0;
/*
 * c_hmatch() return True if the pattern matches the prefix
 */
static int
c_hmatch(str)
Char *str;
{
    if (Strncmp(patbuf, str, patlen) == 0)
	return 1;
    return Gmatch(str, patbuf);
}

/*
 * c_hsetpat(): Set the history seatch pattern
 */
static void
c_hsetpat()
{
    register Char *cp;	/* Used all the time */

    if (LastCmd != F_UP_SEARCH_HIST && LastCmd != F_DOWN_SEARCH_HIST)
    {
	if (VImode)
	    cp = InputBuf+1;  /* Ignore leading ? or /  */
	else
	    cp = InputBuf;

	patlen = Cursor - cp;
	if (patlen >= INBUFSIZ) patlen = INBUFSIZ -1;
	if (patlen >= 0)  {
	    (void) Strncpy(patbuf, cp, patlen);
	    patbuf[patlen] = '\0';
	}
	else
	    patlen = Strlen(patbuf);
    }
#ifdef SDEBUG
    xprintf("\nHist_num = %d\n", Hist_num);
    xprintf("patlen = %d\n", patlen);
    xprintf("patbuf = \"%s\"\n", short2str(patbuf));
#endif
}

/*ARGSUSED*/
CCRETVAL
e_up_search_hist(c)
    int c;
{
    struct Hist *hp;
    int h;
    bool    found = 0;

    UndoAction = 0;
    *LastChar = '\0';		/* just in case */
    if (Hist_num < 0) {
	xprintf("tcsh: e_up_search_hist(): Hist_num < 0; resetting.\n");
	Hist_num = 0;
	return (CC_ERROR);
    }

    if (Hist_num == 0)
    {
	copyn(HistBuf, InputBuf, INBUFSIZ);
	LastHist = HistBuf + (LastChar - InputBuf);
    }

    hp = Histlist.Hnext;
    if (hp == NULL)
	return (CC_ERROR);

    c_hsetpat();		/* Set search pattern !! */

    for (h = 1; h <= Hist_num; h++)
	hp = hp->Hnext;

    while (hp != NULL) {
	if (hp->histline == NULL) {
	    Char sbuf[BUFSIZ];
	    hp->histline = Strsave(sprlex(sbuf, &hp->Hlex));
	}
#ifdef SDEBUG
	xprintf("Comparing with \"%s\"\n", short2str(hp->histline));
#endif
	if (c_hmatch(hp->histline)) {
	    found++;
	    break;
	}
	h++;
	hp = hp->Hnext;
    }

    if (VImode)
	ActionFlag = 0;
    if (!found) {
	if (VImode) {
	    LastChar = InputBuf;
	    Cursor   = InputBuf;
	    Refresh();
	}
#ifdef SDEBUG
	xprintf("not found\n"); 
#endif
	return (CC_ERROR);
    }

    Hist_num = h;

    if (HistLit && hp->histline) {
	copyn(InputBuf, hp->histline, INBUFSIZ);
	CurrentHistLit = 1;
    }
    else {
	(void) sprlex(InputBuf, &hp->Hlex);
	CurrentHistLit = 0;
    }
    LastChar = InputBuf + Strlen(InputBuf);
    if (LastChar > InputBuf) {
	if (LastChar[-1] == '\n')
	    LastChar--;
	if (LastChar[-1] == ' ')
	    LastChar--;
	if (LastChar < InputBuf)
	    LastChar = InputBuf;
    }

    if (VImode)
	Cursor = InputBuf;
    else
	Cursor = LastChar;

    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_down_search_hist(c)
    int c;
{
    struct Hist *hp, *hpt = NULL;
    int h;
    bool    found = 0;

    UndoAction = 0;
    *LastChar = '\0';		/* just in case */

    if (Hist_num == 0)
	return (CC_ERROR);

    hp = Histlist.Hnext;
    if (hp == 0)
	return (CC_ERROR);

    c_hsetpat();		/* Set search pattern !! */

    for (h = 1; h < Hist_num && hp; h++) {
	if (hp->histline == NULL) {
	    Char sbuf[BUFSIZ];
	    hp->histline = Strsave(sprlex(sbuf, &hp->Hlex));
	}
#ifdef SDEBUG
	xprintf("Comparing with \"%s\"\n", short2str(hp->histline));
#endif
	if (c_hmatch(hp->histline)) {
	    found = h;
	    hpt = hp;
	}
	hp = hp->Hnext;
    }

    if (VImode)
	ActionFlag = 0;

    if (!found) {		/* is it the current history number? */
	if (c_hmatch(HistBuf)) {
	    copyn(InputBuf, HistBuf, INBUFSIZ);
	    LastChar = InputBuf + (LastHist - HistBuf);
	    Hist_num = 0;

            if (VImode)
		Cursor = InputBuf;
	    else
		Cursor = LastChar;
	    return (CC_REFRESH);
	}
	else {
#ifdef SDEBUG
	    xprintf("not found\n"); 
#endif
	    return (CC_ERROR);
	}
    }

    Hist_num = found;
    hp = hpt;

    if (HistLit && hp->histline) {
	copyn(InputBuf, hp->histline, INBUFSIZ);
	CurrentHistLit = 1;
    }
    else {
	(void) sprlex(InputBuf, &hp->Hlex);
	CurrentHistLit = 0;
    }
    LastChar = InputBuf + Strlen(InputBuf);
    if (LastChar > InputBuf) {
	if (LastChar[-1] == '\n')
	    LastChar--;
	if (LastChar[-1] == ' ')
	    LastChar--;
	if (LastChar < InputBuf)
	    LastChar = InputBuf;
    }

    if (VImode)
	Cursor = InputBuf;
    else
	Cursor = LastChar;

    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_helpme(c)
    int c;
{
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return (CC_HELPME);
}

/*ARGSUSED*/
CCRETVAL
e_correct(c)
    int c;
{
    *LastChar = '\0';		/* just in case */
    return (CC_CORRECT);
}

/*ARGSUSED*/
CCRETVAL
e_correctl(c)
    int c;
{
    *LastChar = '\0';		/* just in case */
    return (CC_CORRECT_L);
}

/*ARGSUSED*/
CCRETVAL
e_run_fg_editor(c)
    int c;
{
    register struct process *pp;
    extern bool tellwhat;

    if ((pp = find_stop_ed()) != PNULL) {
	/* save our editor state so we can restore it */
	tellwhat = 1;
	copyn(WhichBuf, InputBuf, INBUFSIZ);
	LastWhich = WhichBuf + (LastChar - InputBuf);
	CursWhich = WhichBuf + (Cursor - InputBuf);
	HistWhich = Hist_num;
	Hist_num = 0;		/* for the history commands */

	/* put the tty in a sane mode */
	PastBottom();
	(void) Cookedmode();	/* make sure the tty is set up correctly */

	/* do it! */
	fg_proc_entry(pp);

	(void) Rawmode();	/* go on */
	Refresh();
	tellwhat = 0;
    }
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_list_choices(c)
    int c;
{
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return (CC_LIST_CHOICES);
}

/*ARGSUSED*/
CCRETVAL
e_list_glob(c)
    int c;
{
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return (CC_LIST_GLOB);
}

/*ARGSUSED*/
CCRETVAL
e_expand_glob(c)
    int c;
{
    *LastChar = '\0';		/* just in case */
    return (CC_EXPAND_GLOB);
}

/*ARGSUSED*/
CCRETVAL
e_expand_vars(c)
    int c;
{
    *LastChar = '\0';		/* just in case */
    return (CC_EXPAND_VARS);
}

/*ARGSUSED*/
CCRETVAL
e_which(c)
    int c;
{				/* do a fast command line which(1) */
    PastBottom();
    *LastChar = '\0';		/* just in case */
    return (CC_WHICH);
}

/*ARGSUSED*/
CCRETVAL
e_last_item(c)
    int c;
{				/* insert the last element of the prev. cmd */
    register Char *cp;
    register struct Hist *hp;
    register struct wordent *wp, *firstp;
    register int i;

    if (Argument <= 0)
	return (CC_ERROR);

    hp = Histlist.Hnext;
    if (hp == NULL) {	/* this is only if no history */
	return (CC_ERROR);
    }

    wp = (hp->Hlex).prev;

    if (wp->prev == (struct wordent *) NULL)
	return (CC_ERROR);	/* an empty history entry */

    firstp = (hp->Hlex).next;

    for (i = 0; i < Argument; i++) {	/* back up arg words in lex */
	wp = wp->prev;
	if (wp == firstp)
	    break;
    }

    while (i > 0) {
	cp = wp->word;

	if (!cp)
	    return (CC_ERROR);

	if (InsertStr(cp))
	    return (CC_ERROR);

	wp = wp->next;
	i--;
    }

    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_yank_kill(c)
    int c;
{				/* almost like GnuEmacs */
    register Char *kp, *cp;

    if (LastKill == KillBuf)	/* if zero content */
	return (CC_ERROR);

    if (LastChar + (LastKill - KillBuf) >= InputLim)
	return (CC_ERROR);	/* end of buffer space */

    /* else */
    Mark = Cursor;		/* set the mark */
    cp = Cursor;		/* for speed */

    c_insert(LastKill - KillBuf);	/* open the space, */
    for (kp = KillBuf; kp < LastKill; kp++)	/* copy the chars */
	*cp++ = *kp;

    if (Argument == 1)		/* if an arg, cursor at beginning */
	Cursor = cp;		/* else cursor at end */

    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_delprev(c) 		/* Backspace key in insert mode */
    int c;
{
    int rc;

    rc = CC_ERROR;

    if ( InsertPos != 0 ) {
	if ( InsertPos <= Cursor - Argument ) {
	    c_delbefore(Argument);	/* delete before */
	    Cursor -= Argument;
	    rc = CC_REFRESH;
	}
    }
    return ( rc );
}   /* v_delprev  */

/*ARGSUSED*/
CCRETVAL
e_delprev(c)
    int c;
{
    if (Cursor > InputBuf) {
	c_delbefore(Argument);	/* delete before dot */
	Cursor -= Argument;
	if (Cursor < InputBuf)
	    Cursor = InputBuf;	/* bounds check */
	return (CC_REFRESH);
    }
    else {
	return (CC_ERROR);
    }
}

/*ARGSUSED*/
CCRETVAL
e_delwordprev(c)
    int c;
{
    register Char *cp, *p, *kp;

    if (Cursor == InputBuf)
	return (CC_ERROR);
    /* else */

    cp = c_prev_word(Cursor, InputBuf, Argument);

    for (p = cp, kp = KillBuf; p < Cursor; p++)	/* save the text */
	*kp++ = *p;
    LastKill = kp;

    c_delbefore(Cursor - cp);	/* delete before dot */
    Cursor = cp;
    if (Cursor < InputBuf)
	Cursor = InputBuf;	/* bounds check */
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_delnext(c)
    int c;
{
    if (Cursor == LastChar) {	/* if I'm at the end */
	if (Cursor == InputBuf && !VImode) {	
	    /* if I'm also at the beginning */
	    so_write(STReof, 4);/* then do a EOF */
	    flush();
	    return (CC_EOF);
	}
	else {
	    return (CC_ERROR);
	}
    }
    else {
	if (VImode)
	    c_delafter_save(Argument);	/* delete after dot */
	else
	    c_delafter(Argument);	/* delete after dot */
	if (Cursor > LastChar)
	    Cursor = LastChar;	/* bounds check */
	return (CC_REFRESH);
    }
}

/*ARGSUSED*/
CCRETVAL
e_list_delnext(c)
    int c;
{
    if (Cursor == LastChar) {	/* if I'm at the end */
	if (Cursor == InputBuf) {	/* if I'm also at the beginning */
	    so_write(STReof, 4);/* then do a EOF */
	    flush();
	    return (CC_EOF);
	}
	else {
	    PastBottom();
	    *LastChar = '\0';	/* just in case */
	    return (CC_LIST_CHOICES);
	}
    }
    else {
	if (VImode)
	    c_delafter_save(Argument);	/* delete after dot */
	else
	    c_delafter(Argument);	/* delete after dot */
	if (Cursor > LastChar)
	    Cursor = LastChar;	/* bounds check */
	return (CC_REFRESH);
    }
}

CCRETVAL
e_list_eof(c)
    int c;
{
    if (Cursor == LastChar && Cursor == InputBuf) {
	so_write(STReof, 4);	/* then do a EOF */
	flush();
	return (CC_EOF);
    }
    else {
	PastBottom();
	*LastChar = '\0';	/* just in case */
	return (CC_LIST_CHOICES);
    }
}

/*ARGSUSED*/
CCRETVAL
e_delwordnext(c)
    int c;
{
    register Char *cp, *p, *kp;

    if (Cursor == LastChar)
	return (CC_ERROR);
    /* else */

    cp = c_next_word(Cursor, LastChar, Argument);

    for (p = Cursor, kp = KillBuf; p < cp; p++)	/* save the text */
	*kp++ = *p;
    LastKill = kp;

    if (VImode)
	c_delafter_save(cp - Cursor);	/* delete after dot */
    else
	c_delafter(cp - Cursor);	/* delete after dot */
    /* Cursor = Cursor; */
    if (Cursor > LastChar)
	Cursor = LastChar;	/* bounds check */
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_toend(c)
    int c;
{
    Cursor = LastChar;
    if (VImode)
	if (ActionFlag == DELETE) {
	    c_delfini();
	    return (CC_REFRESH);
	}
    RefCursor();		/* move the cursor */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tobeg(c)
    int c;
{
    Cursor = InputBuf;

    if (VImode) {
       while (Isspace(*Cursor)) /* We want FIRST non space character */
	Cursor++;
	if (ActionFlag == DELETE) {
	    c_delfini();
	    return(CC_REFRESH);
	}
    }

    RefCursor();		/* move the cursor */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_killend(c)
    int c;
{
    register Char *kp, *cp;

    cp = Cursor;
    kp = KillBuf;
    while (cp < LastChar)
	*kp++ = *cp++;		/* copy it */
    LastKill = kp;
    LastChar = Cursor;		/* zap! -- delete to end */
    return (CC_REFRESH);
}


/*ARGSUSED*/
CCRETVAL
e_killbeg(c)
    int c;
{
    register Char *kp, *cp;

    cp = InputBuf;
    kp = KillBuf;
    while (cp < Cursor)
	*kp++ = *cp++;		/* copy it */
    LastKill = kp;
    c_delbefore(Cursor - InputBuf);
    Cursor = InputBuf;		/* zap! */
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_killall(c)
    int c;
{
    register Char *kp, *cp;

    cp = InputBuf;
    kp = KillBuf;
    while (cp < LastChar)
	*kp++ = *cp++;		/* copy it */
    LastKill = kp;
    LastChar = InputBuf;	/* zap! -- delete all of it */
    Cursor = InputBuf;
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_killregion(c)
    int c;
{
    register Char *kp, *cp;

    if (!Mark)
	return (CC_ERROR);

    if (Mark > Cursor) {
	cp = Cursor;
	kp = KillBuf;
	while (cp < Mark)
	    *kp++ = *cp++;	/* copy it */
	LastKill = kp;
	c_delafter(cp - Cursor);/* delete it - UNUSED BY VI mode */
    }
    else {			/* mark is before cursor */
	cp = Mark;
	kp = KillBuf;
	while (cp < Cursor)
	    *kp++ = *cp++;	/* copy it */
	LastKill = kp;
	c_delbefore(cp - Mark);
	Cursor = Mark;
    }
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_copyregion(c)
    int c;
{
    register Char *kp, *cp;

    if (!Mark)
	return (CC_ERROR);

    if (Mark > Cursor) {
	cp = Cursor;
	kp = KillBuf;
	while (cp < Mark)
	    *kp++ = *cp++;	/* copy it */
	LastKill = kp;
    }
    else {			/* mark is before cursor */
	cp = Mark;
	kp = KillBuf;
	while (cp < Cursor)
	    *kp++ = *cp++;	/* copy it */
	LastKill = kp;
    }
    return (CC_NORM);		/* don't even need to Refresh() */
}

/*ARGSUSED*/
CCRETVAL
e_charswitch(cc)
    int cc;
{
    register Char c;

    if (Cursor < LastChar) {
	if (LastChar <= &InputBuf[1]) {
	    return (CC_ERROR);
	}
	else {
	    Cursor++;
	}
    }
    if (Cursor > &InputBuf[1]) {/* must have at least two chars entered */
	c = Cursor[-2];
	Cursor[-2] = Cursor[-1];
	Cursor[-1] = c;
	return (CC_REFRESH);
    }
    else {
	return (CC_ERROR);
    }
}

/*ARGSUSED*/
CCRETVAL
e_gcharswitch(cc)
    int cc;
{				/* gosmacs style ^T */
    register Char c;

    if (Cursor > &InputBuf[1]) {/* must have at least two chars entered */
	c = Cursor[-2];
	Cursor[-2] = Cursor[-1];
	Cursor[-1] = c;
	return (CC_REFRESH);
    }
    else {
	return (CC_ERROR);
    }
}

/*ARGSUSED*/
CCRETVAL
e_charback(c)
    int c;
{
    if (Cursor > InputBuf) {
	Cursor -= Argument;
	if (Cursor < InputBuf)
	    Cursor = InputBuf;

	if (VImode)
	    if ( ActionFlag == DELETE ) {
		c_delfini();
		return ( CC_REFRESH );
	    }

	RefCursor();
	return (CC_NORM);
    }
    else {
	return (CC_ERROR);
    }
}

/*ARGSUSED*/
CCRETVAL
v_wordback(c)
    int c;
{
    if (Cursor == InputBuf)
	return (CC_ERROR);
    /* else */

    Cursor = c_preword(Cursor, InputBuf, Argument); /* bounds check */

    if ( ActionFlag == DELETE ) {
	c_delfini();
	return(CC_REFRESH);
    }

    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_wordback(c)
    int c;
{
    if (Cursor == InputBuf)
	return (CC_ERROR);
    /* else */

    Cursor = c_prev_word(Cursor, InputBuf, Argument); /* bounds check */

    if (VImode) 
	if ( ActionFlag == DELETE ) {
	    c_delfini();
	    return(CC_REFRESH);
	}

    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_charfwd(c)
    int c;
{
    if (Cursor < LastChar) {
	Cursor += Argument;
	if (Cursor > LastChar)
	    Cursor = LastChar;

	if (VImode)
	    if (ActionFlag == DELETE) {
		c_delfini();
		return(CC_REFRESH);
	    }

	RefCursor();
	return (CC_NORM);
    }
    else {
	return (CC_ERROR);
    }
}

/*ARGSUSED*/
CCRETVAL
e_wordfwd(c)
    int c;
{
    if (Cursor == LastChar)
	return (CC_ERROR);
    /* else */

    Cursor = c_next_word(Cursor, LastChar, Argument);

    if (VImode)
	if ( ActionFlag == DELETE ) {
	    c_delfini();
	    return(CC_REFRESH);
	}

    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_wordbegnext(c)
    int c;
{
    if (Cursor == LastChar)
	return (CC_ERROR);
    /* else */

    Cursor = c_beg_next_word(Cursor, LastChar, Argument);

    if (VImode)
	if (ActionFlag == DELETE) {
	    c_delfini();
	    return (CC_REFRESH);
	}

    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
static CCRETVAL
v_repeat_srch(c)
    int c;
{
#ifdef SDEBUG
    xprintf("dir %d patlen %d patbuf %s\n", 
	    c, patlen, short2str(patbuf));
#endif

    if (c == DN_SEARCH) {
	LastCmd = F_DOWN_SEARCH_HIST;  /* Hack to stop c_hsetpat */
	LastChar = InputBuf;
	return(e_down_search_hist(c));
    }
    else if (c == UP_SEARCH) {
	LastCmd = F_UP_SEARCH_HIST;  /* Hack to stop c_hsetpat */
	LastChar = InputBuf;
	return(e_up_search_hist(c));
    }
    else 
	return(CC_ERROR);
}

#ifdef COMMENT
/* by: Brian Allison <uiucdcs!convex!allison@RUTGERS.EDU> */
static void
c_get_word(begin, end)
    Char  **begin;
    Char  **end;
{
    Char   *cp;

    cp = &Cursor[0];
    while (Argument--) {
	while ((cp <= LastChar) && (isword(*cp)))
	    cp++;
	*end = --cp;
	while ((cp >= InputBuf) && (isword(*cp)))
	    cp--;
	*begin = ++cp;
    }
}
#endif				/* COMMENT */

/*ARGSUSED*/
CCRETVAL
e_uppercase(c)
    int c;
{
    Char   *cp, *end;

    end = c_next_word(Cursor, LastChar, Argument);

    for (cp = Cursor; cp < end; cp++)	/* PWP: was cp=begin */
	if (Islower(*cp))
	    *cp = Toupper(*cp);

    Cursor = end;
    if (Cursor > LastChar)
	Cursor = LastChar;
    return (CC_REFRESH);
}


/*ARGSUSED*/
CCRETVAL
e_capitolcase(c)
    int c;
{
    Char   *cp, *end;

    end = c_next_word(Cursor, LastChar, Argument);

    cp = Cursor;
    for (; cp < end; cp++) {
	if (Isalpha(*cp)) {
	    if (Islower(*cp))
		*cp = Toupper(*cp);
	    cp++;
	    break;
	}
    }
    for (; cp < end; cp++)
	if (Isupper(*cp))
	    *cp = Tolower(*cp);

    Cursor = end;
    if (Cursor > LastChar)
	Cursor = LastChar;
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_lowercase(c)
    int c;
{
    Char   *cp, *end;

    end = c_next_word(Cursor, LastChar, Argument);

    for (cp = Cursor; cp < end; cp++)
	if (Isupper(*cp))
	    *cp = Tolower(*cp);

    Cursor = end;
    if (Cursor > LastChar)
	Cursor = LastChar;
    return (CC_REFRESH);
}


/*ARGSUSED*/
CCRETVAL
e_set_mark(c)
    int c;
{
    Mark = Cursor;
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_exchange_mark(c)
    int c;
{
    register Char *cp;

    cp = Cursor;
    Cursor = Mark;
    Mark = cp;
    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_argfour(c)
    int c;
{				/* multiply current argument by 4 */
    if (Argument > 1000000)
	return CC_ERROR;
    DoingArg = 1;
    Argument *= 4;
    return (CC_ARGHACK);
}

/*ARGSUSED*/
CCRETVAL
e_quote(c)
    int c;
{
    Char    ch;
    int     num;

    QuoteModeOn();
    num = GetNextChar(&ch);
    QuoteModeOff();
    if (num == 1)
	return e_insert(ch);
    else
	return e_send_eof(0);
}

/*ARGSUSED*/
CCRETVAL
e_metanext(c)
    int c;
{
    MetaNext = 1;
    return (CC_ARGHACK);	/* preserve argument */
}

#ifdef notdef
/*ARGSUSED*/
CCRETVAL
e_extendnext(c)
    int c;
{
    CurrentKeyMap = CcAltMap;
    return (CC_ARGHACK);	/* preserve argument */
}

#endif

/*ARGSUSED*/
CCRETVAL
v_insbeg(c)
    int c;
{				/* move to beginning of line and start vi
				 * insert mode */
    Cursor = InputBuf;
    InsertPos = Cursor;

    UndoPtr  = Cursor;
    UndoAction = DELETE;

    RefCursor();		/* move the cursor */
    c_alternativ_key_map(0);
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_replone(c)
    int c;
{				/* vi mode overwrite one character */
    c_alternativ_key_map(0);
    replacemode = 2;
    UndoAction = CHANGE;	/* Set Up for VI undo command */
    UndoPtr = Cursor;
    UndoSize = 0;
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_replmode(c)
    int c;
{				/* vi mode start overwriting */
    c_alternativ_key_map(0);
    replacemode = 1;
    UndoAction = CHANGE;	/* Set Up for VI undo command */
    UndoPtr = Cursor;
    UndoSize = 0;
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_substchar(c)
    int c;
{				/* vi mode substitute for one char */
    c_delafter_save(Argument);
    c_alternativ_key_map(0);
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_substline(c)
    int c;
{				/* vi mode replace whole line */
    (void) e_killall(0);
    c_alternativ_key_map(0);
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_chgtoend(c)
    int c;
{				/* vi mode change to end of line */
    (void) e_killend(0);
    c_alternativ_key_map(0);
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_insert(c)
    int c;
{				/* vi mode start inserting */
    c_alternativ_key_map(0);

    InsertPos = Cursor;
    UndoPtr = Cursor;
    UndoAction = DELETE;

    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_add(c)
    int c;
{				/* vi mode start adding */
    c_alternativ_key_map(0);
    if (Cursor < LastChar)
    {
	Cursor++;
	if (Cursor > LastChar)
	    Cursor = LastChar;
	RefCursor();
    }

    InsertPos = Cursor;
    UndoPtr = Cursor;
    UndoAction = DELETE;

    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_addend(c)
    int c;
{				/* vi mode to add at end of line */
    c_alternativ_key_map(0);
    Cursor = LastChar;

    InsertPos = LastChar;	/* Mark where insertion begins */
    UndoPtr = LastChar;
    UndoAction = DELETE;

    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_change_case(cc)
    int cc;
{
    char    c;

    if (Cursor < LastChar) {
	c = *Cursor;
	if (Isupper(c))
	    *Cursor++ = Tolower(c);
	else if (Islower(c))
	    *Cursor++ = Toupper(c);
	else
	    Cursor++;
	RefPlusOne();		/* fast refresh for one char */
	return (CC_NORM);
    }
    return (CC_ERROR);
}

/*ARGSUSED*/
CCRETVAL
e_expand(c)
    int c;
{
    register Char *p;
    extern bool justpr;

    for (p = InputBuf; Isspace(*p); p++);
    if (p == LastChar)
	return (CC_ERROR);

    justpr++;
    Expand++;
    return (e_newline(0));
}

/*ARGSUSED*/
CCRETVAL
e_startover(c)
    int c;
{				/* erase all of current line, start again */
    ResetInLine();		/* reset the input pointers */
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_redisp(c)
    int c;
{
    ClearLines();
    ClearDisp();
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_cleardisp(c)
    int c;
{
    ClearScreen();		/* clear the whole real screen */
    ClearDisp();		/* reset everything */
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_tty_int(c)
    int c;
{
    /* do no editing */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_insovr(c)
    int c;
{
    replacemode = !replacemode;
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_dsusp(c)
    int c;
{
    /* do no editing */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_flusho(c)
    int c;
{
    /* do no editing */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_quit(c)
    int c;
{
    /* do no editing */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_tsusp(c)
    int c;
{
    /* do no editing */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_tty_stopo(c)
    int c;
{
    /* do no editing */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_expand_history(c)
    int c;
{
    *LastChar = '\0';		/* just in case */
    c_substitute();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_magic_space(c)
    int c;
{
    *LastChar = '\0';		/* just in case */
    c_substitute();
    return (e_insert(' '));
}

/*ARGSUSED*/
CCRETVAL
e_copyprev(c)
    int c;
{
    register Char *cp, *oldc, *dp;

    if (Cursor == InputBuf)
	return (CC_ERROR);
    /* else */

    oldc = Cursor;
    /* does a bounds check */
    cp = c_prev_word(Cursor, InputBuf, Argument);	

    c_insert(oldc - cp);
    for (dp = oldc; cp < oldc && dp < LastChar; cp++)
	*dp++ = *cp;

    Cursor = dp;		/* put cursor at end */

    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
e_tty_starto(c)
    int c;
{
    /* do no editing */
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
e_load_average(c)
    int c;
{
    PastBottom();
#ifdef TIOCSTAT
    if (ioctl(SHIN, TIOCSTAT, 0) < 0) 
	xprintf("Load average unavailable");
#endif
    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_delmeta(c)
int c;
{
    register Char *cp, *kp;
    if (ActionFlag == DELETE) {
	ActionFlag = 0;
	ActionPos = 0;
	
	UndoSize = 0;
	kp = UndoBuf;
	for (cp = InputBuf; cp < LastChar; cp++) {
	    *kp++ = *cp;
	    UndoSize++;
	}
		
	UndoAction = INSERT;
	UndoPtr  = InputBuf;
	LastChar = InputBuf;
	Cursor   = InputBuf;
	return(CC_REFRESH);
    }
#ifdef notdef
    else if (ActionFlag == 0) {
#endif
	ActionPos = Cursor;
	ActionFlag = DELETE;
	return (CC_ARGHACK);  /* Do NOT clear out argument */
#ifdef notdef
    }
    else {
	ActionFlag = 0;
	ActionPos = 0;
	return (CC_ERROR);
    }
#endif
}

/*ARGSUSED*/
CCRETVAL
v_endword(c)
    int c;
{
    if (Cursor == LastChar)
	return (CC_ERROR);
    /* else */

    Cursor = c_endword(Cursor, LastChar, Argument);

    if ( ActionFlag == DELETE )
    {
	Cursor++;
	c_delfini();
	return ( CC_REFRESH );
    }

    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_eword(c)
    int c;
{
    if (Cursor == LastChar)
	return (CC_ERROR);
    /* else */

    Cursor = c_eword(Cursor, LastChar, Argument);

    if ( ActionFlag == DELETE )
    {
	Cursor++;
	c_delfini();
	return ( CC_REFRESH );
    }

    RefCursor();
    return (CC_NORM);
}

/*ARGSUSED*/
CCRETVAL
v_undo(c)
    int c;
{
    register int  loop;
    register Char *kp, *cp;
    Char temp;
    int	 size;

    switch ( UndoAction )
    {
	case DELETE:
    		if ( UndoSize == 0 ) return(CC_NORM);
		cp = UndoPtr;
		kp = UndoBuf;
		for (loop=0; loop < UndoSize; loop++)	/* copy the chars */
		    *kp++ = *cp++;			/* into UndoBuf   */

		for (cp = UndoPtr; cp <= LastChar; cp++)
		    *cp = cp[UndoSize];

		LastChar -= UndoSize;
		Cursor   =  UndoPtr;
		
		UndoAction = INSERT;
	    break;

	case INSERT:
    		if ( UndoSize == 0 ) return(CC_NORM);
		cp = UndoPtr;
		Cursor = UndoPtr;
		kp = UndoBuf;
		c_insert(UndoSize);		/* open the space, */
		for (loop=0; loop < UndoSize; loop++)	/* copy the chars */
		    *cp++ = *kp++;

		UndoAction = DELETE;
	    break;

	case CHANGE:
    		if ( UndoSize == 0 ) return(CC_NORM);
		cp = UndoPtr;
		Cursor = UndoPtr;
		kp = UndoBuf;
		size = (int)(Cursor-LastChar); /*  NOT NSL independant */
		if ( size < UndoSize )
		    size = UndoSize;
		for(loop=0;loop<size;loop++)
		{
			temp = *kp;
			*kp++ = *cp;
			*cp++ = temp;
		}

	    break;

	default:
		return(CC_ERROR);
	    break;

    }

    return (CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_ush_meta(c)
    int c;
{
    searchdir = ActionFlag = UP_SEARCH;
    LastChar = InputBuf;
    Cursor   = InputBuf;
    InsertPos = InputBuf;
    c_insert(1);
    *Cursor++ = c;
    DoingArg = 0;		/* just in case */
    c_alternativ_key_map(0);
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_dsh_meta(c)
    int c;
{
    searchdir = ActionFlag = DN_SEARCH;
    LastChar = InputBuf;
    Cursor   = InputBuf;
    InsertPos = InputBuf;
    c_insert(1);
    *Cursor++ = c;
    DoingArg = 0;		/* just in case */
    c_alternativ_key_map(0);
    return(CC_REFRESH);
}

/*ARGSUSED*/
CCRETVAL
v_rsrch_fwd(c)
    int c;
{
    if ( patlen == 0 ) return(CC_ERROR);
    return (v_repeat_srch(searchdir));
}

/*ARGSUSED*/
CCRETVAL
v_rsrch_rev(c)
    int c;
{
    if ( patlen == 0 ) return(CC_ERROR);
    return (v_repeat_srch(searchdir == DN_SEARCH ? UP_SEARCH : DN_SEARCH));
}

#ifdef notdef
void
MoveCursor(n)			/* move cursor + right - left char */
    int     n;
{
    Cursor = Cursor + n;
    if (Cursor < InputBuf)
	Cursor = InputBuf;
    if (Cursor > LastChar)
	Cursor = LastChar;
    return;
}

Char   *
GetCursor()
{
    return (Cursor);
}

int
PutCursor(p)
    Char   *p;
{
    if (p < InputBuf || p > LastChar)
	return 1;		/* Error */
    Cursor = p;
    return 0;
}
#endif
