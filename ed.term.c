/* $Header$ */
/*
 * ed.term.c: Low level terminal interface
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
#include "sh.h"

RCSID("$Id$")

#include "ed.h"
#include "ed.term.h"

int didsetty = 0;
ttyperm_t ttylist = {   
    {
#if defined(POSIX) || defined(TERMIO)
	{ "iflag:", ICRNL, (INLCR|IGNCR) },
	{ "oflag:", (OPOST|ONLCR), ONLRET },
	{ "cflag:", 0, 0 },
	{ "lflag:", (ISIG|ICANON|ECHO|ECHOE|ECHOCTL|IEXTEN),
		    (NOFLSH|ECHONL|EXTPROC|FLUSHO) },
#else /* GSTTY */
	{ "nrmal:", (ECHO|CRMOD|ANYP), (CBREAK|RAW|LCASE|VTDELAY|ALLDELAY) },
	{ "local:", (LCRTBS|LCRTERA|LCRTKIL), (LPRTERA|LFLUSHO) },
#endif /* TERMIO */
	{ "chars:", 	0, 0 },
    },
    {
#ifdef TERMIO
	{ "iflag:", (INLCR|ICRNL), IGNCR },
	{ "oflag:", (OPOST|ONLCR), ONLRET },
	{ "cflag:", 0, 0 },
	{ "lflag:", ISIG,
		    (NOFLSH|ICANON|ECHO|ECHOK|ECHONL|EXTPROC|IEXTEN|FLUSHO) },
#else /* GSTTY */
	{ "nrmal:", (CBREAK|CRMOD|ANYP), (RAW|ECHO|LCASE|VTDELAY|ALLDELAY) },
	{ "local:", (LCRTBS|LCRTERA|LCRTKIL), (LPRTERA|LFLUSHO) },
#endif /* TERMIO */
	{ "chars:", (C_SH(C_MIN)|C_SH(C_TIME)|C_SH(C_SWTCH)|C_SH(C_WERASE)|
		     C_SH(C_REPRINT)|C_SH(C_SUSP)|C_SH(C_DSUSP)|C_SH(C_EOF)|
		     C_SH(C_EOL)|C_SH(C_DISCARD)|C_SH(C_PGOFF)|C_SH(C_PAGE)|
		     C_SH(C_STATUS)|C_SH(C_LNEXT)), 0 }
    },
    {
#ifdef TERMIO
	{ "iflag:", 0, IXON | IXOFF },
	{ "oflag:", 0, 0 },
	{ "cflag:", 0, 0 },
	{ "lflag:", 0, ISIG | IEXTEN | PARENB },
#else /* GSTTY */
	{ "nrmal:", RAW, CBREAK },
	{ "local:", 0, 0 },
#endif /* TERMIO */
	{ "chars:", 0, 0 },
    }
};

static struct tcshmodes {
    char *m_name;
    int   m_value;
    int   m_type;
} modelist[] = {
#ifdef TERMIO

# ifdef	IGNBRK
    { "ignbrk",	IGNBRK,	M_INPUT },
# endif /* IGNBRK */
# ifdef	BRKINT
    { "brkint",	BRKINT,	M_INPUT },
# endif /* BRKINT */
# ifdef	IGNPAR
    { "ignpar",	IGNPAR,	M_INPUT },
# endif /* IGNPAR */
# ifdef	PARMRK
    { "parmrk",	PARMRK,	M_INPUT },
# endif /* PARMRK */
# ifdef	INPCK
    { "inpck",	INPCK,	M_INPUT },
# endif /* INPCK */
# ifdef	ISTRIP
    { "istrip",	ISTRIP,	M_INPUT },
# endif /* ISTRIP */
# ifdef	INLCR
    { "inlcr",	INLCR,	M_INPUT },
# endif /* INLCR */
# ifdef	IGNCR
    { "igncr",	IGNCR,	M_INPUT },
# endif /* IGNCR */
# ifdef	ICRNL
    { "icrnl",	ICRNL,	M_INPUT },
# endif /* ICRNL */
# ifdef	IUCLC
    { "iuclc",	IUCLC,	M_INPUT },
# endif /* IUCLC */
# ifdef	IXON
    { "ixon",	IXON,	M_INPUT },
# endif /* IXON */
# ifdef	IXANY
    { "ixany",	IXANY,	M_INPUT },
# endif /* IXANY */
# ifdef	IXOFF
    { "ixoff",	IXOFF,	M_INPUT },
# endif /* IXOFF */
# ifdef  IMAXBEL
    { "imaxbel",IMAXBEL,M_INPUT },
# endif /* IMAXBEL */

# ifdef	OPOST
    { "opost",	OPOST,	M_OUTPUT },
# endif /* OPOST */
# ifdef	OLCUC
    { "olcuc",	OLCUC,	M_OUTPUT },
# endif /* OLCUC */
# ifdef	ONLCR
    { "onlcr",	ONLCR,	M_OUTPUT },
# endif /* ONLCR */
# ifdef	OCRNL
    { "ocrnl",	OCRNL,	M_OUTPUT },
# endif /* OCRNL */
# ifdef	ONOCR
    { "onocr",	ONOCR,	M_OUTPUT },
# endif /* ONOCR */
# ifdef ONOEOT
    { "onoeot",	ONOEOT,	M_OUTPUT },
# endif /* ONOEOT */
# ifdef	ONLRET
    { "onlret",	ONLRET,	M_OUTPUT },
# endif /* ONLRET */
# ifdef	OFILL
    { "ofill",	OFILL,	M_OUTPUT },
# endif /* OFILL */
# ifdef	OFDEL
    { "ofdel",	OFDEL,	M_OUTPUT },
# endif /* OFDEL */
# ifdef	NLDLY
    { "nldly",	NLDLY,	M_OUTPUT },
# endif /* NLDLY */
# ifdef	CRDLY
    { "crdly",	CRDLY,	M_OUTPUT },
# endif /* CRDLY */
# ifdef	TABDLY
    { "tabdly",	TABDLY,	M_OUTPUT },
# endif /* TABDLY */
# ifdef	XTABS
    { "xtabs",	XTABS,	M_OUTPUT },
# endif /* XTABS */
# ifdef	BSDLY
    { "bsdly",	BSDLY,	M_OUTPUT },
# endif /* BSDLY */
# ifdef	VTDLY
    { "vtdly",	VTDLY,	M_OUTPUT },
# endif /* VTDLY */
# ifdef	FFDLY
    { "ffdly",	FFDLY,	M_OUTPUT },
# endif /* FFDLY */
# ifdef	PAGEOUT
    { "pageout",PAGEOUT,M_OUTPUT },
# endif /* PAGEOUT */
# ifdef	WRAP
    { "wrap",	WRAP,	M_OUTPUT },
# endif /* WRAP */

# ifdef	CIGNORE
    { "cignore",CIGNORE,M_CONTROL },
# endif /* CBAUD */
# ifdef	CBAUD
    { "cbaud",	CBAUD,	M_CONTROL },
# endif /* CBAUD */
# ifdef	CSTOPB
    { "cstopb",	CSTOPB,	M_CONTROL },
# endif /* CSTOPB */
# ifdef	CREAD
    { "cread",	CREAD,	M_CONTROL },
# endif /* CREAD */
# ifdef	PARENB
    { "parenb",	PARENB,	M_CONTROL },
# endif /* PARENB */
# ifdef	PARODD
    { "parodd",	PARODD,	M_CONTROL },
# endif /* PARODD */
# ifdef	HUPCL
    { "hupcl",	HUPCL,	M_CONTROL },
# endif /* HUPCL */
# ifdef	CLOCAL
    { "clocal",	CLOCAL,	M_CONTROL },
# endif /* CLOCAL */
# ifdef	LOBLK
    { "loblk",	LOBLK,	M_CONTROL },
# endif /* LOBLK */
# ifdef	CIBAUD
    { "cibaud",	CIBAUD,	M_CONTROL },
# endif /* CIBAUD */
# ifdef CRTSCTS
#  ifdef CCTS_OFLOW
    { "ccts_oflow",CCTS_OFLOW,M_CONTROL },
#  else
    { "crtscts",CRTSCTS,M_CONTROL },
#  endif /* CCTS_OFLOW */
# endif /* CRTSCTS */
# ifdef CRTS_IFLOW
    { "crts_iflow",CRTS_IFLOW,M_CONTROL },
# endif /* CRTS_IFLOW */
# ifdef MDMBUF
    { "mdmbuf",	MDMBUF,	M_CONTROL },
# endif /* MDMBUF */

# ifdef	ISIG
    { "isig",	ISIG,	M_LINED },
# endif /* ISIG */
# ifdef	ICANON
    { "icanon",	ICANON,	M_LINED },
# endif /* ICANON */
# ifdef	XCASE
    { "xcase",	XCASE,	M_LINED },
# endif /* XCASE */
# ifdef	ECHO
    { "echo",	ECHO,	M_LINED },
# endif /* ECHO */
# ifdef	ECHOE
    { "echoe",	ECHOE,	M_LINED },
# endif /* ECHOE */
# ifdef	ECHOK
    { "echok",	ECHOK,	M_LINED },
# endif /* ECHOK */
# ifdef	ECHONL
    { "echonl",	ECHONL,	M_LINED },
# endif /* ECHONL */
# ifdef	NOFLSH
    { "noflsh",	NOFLSH,	M_LINED },
# endif /* NOFLSH */
# ifdef	TOSTOP
    { "tostop",	TOSTOP,	M_LINED },
# endif /* TOSTOP */
# ifdef	ECHOCTL
    { "echoctl",ECHOCTL,M_LINED },
# endif /* ECHOCTL */
# ifdef	ECHOPRT
    { "echoprt",ECHOPRT,M_LINED },
# endif /* ECHOPRT */
# ifdef	ECHOKE
    { "echoke",	ECHOKE,	M_LINED },
# endif /* ECHOKE */
# ifdef	DEFECHO
    { "defecho",DEFECHO,M_LINED },
# endif /* DEFECHO */
# ifdef	FLUSHO
    { "flusho",	FLUSHO,	M_LINED },
# endif /* FLUSHO */
# ifdef	PENDIN
    { "pendin",	PENDIN,	M_LINED },
# endif /* PENDIN */
# ifdef	IEXTEN
    { "iexten",	IEXTEN,	M_LINED },
# endif /* IEXTEN */
# ifdef	NOKERNINFO
    { "nokerninfo",NOKERNINFO,M_LINED },
# endif /* NOKERNINFO */
# ifdef	ALTWERASE
    { "altwerase",ALTWERASE,M_LINED },
# endif /* ALTWERASE */

#else /* GSTTY */

# ifdef	TANDEM
    { "tandem",	TANDEM,	M_CONTROL },
# endif /* TANDEM */
# ifdef	CBREAK
    { "cbreak",	CBREAK,	M_CONTROL },
# endif /* CBREAK */
# ifdef	LCASE
    { "lcase",	LCASE,	M_CONTROL },
# endif /* LCASE */
# ifdef	ECHO
    { "echo",	ECHO,	M_CONTROL },
# endif /* ECHO */	
# ifdef	CRMOD
    { "crmod",	CRMOD,	M_CONTROL },
# endif /* CRMOD */
# ifdef	RAW
    { "raw",	RAW,	M_CONTROL },
# endif /* RAW */
# ifdef	ODDP
    { "oddp",	ODDP,	M_CONTROL },
# endif /* ODDP */
# ifdef	EVENP
    { "evenp",	EVENP,	M_CONTROL },
# endif /* EVENP */
# ifdef	ANYP
    { "anyp",	ANYP,	M_CONTROL },
# endif /* ANYP */
# ifdef	NLDELAY
    { "nldelay",NLDELAY,M_CONTROL },
# endif /* NLDELAY */
# ifdef	TBDELAY
    { "tbdelay",TBDELAY,M_CONTROL },
# endif /* TBDELAY */
# ifdef	XTABS
    { "xtabs",	XTABS,	M_CONTROL },
# endif /* XTABS */
# ifdef	CRDELAY
    { "crdelay",CRDELAY,M_CONTROL },
# endif /* CRDELAY */
# ifdef	VTDELAY
    { "vtdelay",VTDELAY,M_CONTROL },
# endif /* VTDELAY */
# ifdef	BSDELAY
    { "bsdelay",BSDELAY,M_CONTROL },
# endif /* BSDELAY */
# ifdef	CRTBS
    { "crtbs",	CRTBS,	M_CONTROL },
# endif /* CRTBS */
# ifdef	PRTERA
    { "prtera",	PRTERA,	M_CONTROL },
# endif /* PRTERA */
# ifdef	CRTERA
    { "crtera",	CRTERA,	M_CONTROL },
# endif /* CRTERA */
# ifdef	TILDE
    { "tilde",	TILDE,	M_CONTROL },
# endif /* TILDE */
# ifdef	MDMBUF
    { "mdmbuf",	MDMBUF,	M_CONTROL },
# endif /* MDMBUF */
# ifdef	LITOUT
    { "litout",	LITOUT,	M_CONTROL },
# endif /* LITOUT */
# ifdef	TOSTOP
    { "tostop",	TOSTOP,	M_CONTROL },
# endif /* TOSTOP */
# ifdef	FLUSHO
    { "flusho",	FLUSHO,	M_CONTROL },
# endif /* FLUSHO */
# ifdef	NOHANG
    { "nohang",	NOHANG,	M_CONTROL },
# endif /* NOHANG */
# ifdef	L001000
    { "l001000",L001000,M_CONTROL },
# endif /* L001000 */
# ifdef	CRTKIL
    { "crtkil",	CRTKIL,	M_CONTROL },
# endif /* CRTKIL */
# ifdef	PASS8
    { "pass8",	PASS8,	M_CONTROL },
# endif /* PASS8 */
# ifdef	CTLECH
    { "ctlech",	CTLECH,	M_CONTROL },
# endif /* CTLECH */
# ifdef	PENDIN
    { "pendin",	PENDIN,	M_CONTROL },
# endif /* PENDIN */
# ifdef	DECCTQ
    { "decctq",	DECCTQ,	M_CONTROL },
# endif /* DECCTQ */
# ifdef	NOFLSH
    { "noflsh",	NOFLSH,	M_CONTROL },
# endif /* NOFLSH */

# ifdef	LCRTBS
    { "lcrtbs",	LCRTBS,	M_LOCAL },
# endif /* LCRTBS */
# ifdef	LPRTERA
    { "lprtera",LPRTERA,M_LOCAL },
# endif /* LPRTERA */
# ifdef	LCRTERA
    { "lcrtera",LCRTERA,M_LOCAL },
# endif /* LCRTERA */
# ifdef	LTILDE
    { "ltilde",	LTILDE,	M_LOCAL },
# endif /* LTILDE */
# ifdef	LMDMBUF
    { "lmdmbuf",LMDMBUF,M_LOCAL },
# endif /* LMDMBUF */
# ifdef	LLITOUT
    { "llitout",LLITOUT,M_LOCAL },
# endif /* LLITOUT */
# ifdef	LTOSTOP
    { "ltostop",LTOSTOP,M_LOCAL },
# endif /* LTOSTOP */
# ifdef	LFLUSHO
    { "lflusho",LFLUSHO,M_LOCAL },
# endif /* LFLUSHO */
# ifdef	LNOHANG
    { "lnohang",LNOHANG,M_LOCAL },
# endif /* LNOHANG */
# ifdef	LCRTKIL
    { "lcrtkil",LCRTKIL,M_LOCAL },
# endif /* LCRTKIL */
# ifdef	LPASS8
    { "lpass8",	LPASS8,	M_LOCAL },
# endif /* LPASS8 */	
# ifdef	LCTLECH
    { "lctlech",LCTLECH,M_LOCAL },
# endif /* LCTLECH */
# ifdef	LPENDIN
    { "lpendin",LPENDIN,M_LOCAL },
# endif /* LPENDIN */
# ifdef	LDECCTQ
    { "ldecctq",LDECCTQ,M_LOCAL },
# endif /* LDECCTQ */
# ifdef	LNOFLSH
    { "lnoflsh",LNOFLSH,M_LOCAL },
# endif /* LNOFLSH */

#endif /* TERMIO */
# if defined(VINTR) || defined(TIOCGETC)
    { "intr",		C_SH(C_INTR), 	M_CHAR },
# endif /* VINTR */
# if defined(VQUIT) || defined(TIOCGETC)
    { "quit",		C_SH(C_QUIT), 	M_CHAR },
# endif /* VQUIT */
# if defined(VERASE) || defined(TIOCGETP)
    { "erase",		C_SH(C_ERASE), 	M_CHAR },
# endif /* VERASE */
# if defined(VKILL) || defined(TIOCGETP)
    { "kill",		C_SH(C_KILL), 	M_CHAR },
# endif /* VKILL */
# if defined(VEOF) || defined(TIOCGETC)
    { "eof",		C_SH(C_EOF), 	M_CHAR },
# endif /* VEOF */
# if defined(VEOL)
    { "eol",		C_SH(C_EOL), 	M_CHAR },
# endif /* VEOL */
# if defined(VEOL2)
    { "eol2",		C_SH(C_EOL2), 	M_CHAR },
# endif  /* VEOL2 */
# if defined(VSWTCH)
    { "swtch",		C_SH(C_SWTCH), 	M_CHAR },
# endif /* VSWTCH */
# if defined(VERASE2)
    { "erase2",		C_SH(C_ERASE2),	M_CHAR },
# endif /* VERASE2 */
# if defined(VSTART) || defined(TIOCGETC)
    { "start",		C_SH(C_START), 	M_CHAR },
# endif /* VSTART */
# if defined(VSTOP) || defined(TIOCGETC)
    { "stop",		C_SH(C_STOP), 	M_CHAR },
# endif /* VSTOP */
# if defined(VWERASE) || defined(TIOCGLTC)
    { "werase",		C_SH(C_WERASE),	M_CHAR },
# endif /* VWERASE */
# if defined(VSUSP) || defined(TIOCGLTC)
    { "susp",		C_SH(C_SUSP), 	M_CHAR },
# endif /* VSUSP */
# if defined(VDSUSP) || defined(TIOCGLTC)
    { "dsusp",		C_SH(C_DSUSP), 	M_CHAR },
# endif /* VDSUSP */
# if defined(VREPRINT) || defined(TIOCGLTC)
    { "reprint",	C_SH(C_REPRINT),M_CHAR },
# endif /* WREPRINT */
# if defined(VDISCARD) || defined(TIOCGLTC)
    { "discard",	C_SH(C_DISCARD),M_CHAR },
# endif /* VDISCARD */
# if defined(VLNEXT) || defined(TIOCGLTC)
    { "lnext",		C_SH(C_LNEXT), 	M_CHAR },
# endif /* VLNEXT */
# if defined(VSTATUS) || defined(TIOCGPAGE)
    { "status",		C_SH(C_STATUS),	M_CHAR },
# endif /* VSTATUS */
# if defined(VPAGE) || defined(TIOCGPAGE)
    { "page",		C_SH(C_PAGE), 	M_CHAR },
# endif /* VPAGE */
# if defined(VPGOFF) || defined(TIOCGPAGE)
    { "pgoff",		C_SH(C_PGOFF), 	M_CHAR },
# endif /* VPGOFF */
# if defined(VBRK) || defined(TIOCGETC)
    { "min",		C_SH(C_BRK), 	M_CHAR },
# endif /* VBRK */
# if defined(VMIN)
    { "min",		C_SH(C_MIN), 	M_CHAR },
# endif /* VMIN */
# if defined(VTIME)
    { "time",		C_SH(C_TIME), 	M_CHAR },
# endif /* VTIME */
    { NULL, 0, -1 },
};

void
dosetty(v, t)
    Char **v;
    struct command *t;
{
    struct tcshmodes *m;
    char x, *d;
    int aflag = 0;
    Char *s;
    int z = EXIO;
    char cmdname[BUFSIZ];

    setname(strcpy(cmdname, short2str(*v++)));

    while (v && *v && v[0][0] == '-' && v[0][2] == '\0') 
	switch (v[0][1]) {
	case 'a':
	    aflag++;
	    v++;
	    break;
	case 'd':
	    v++;
	    z = EDIO;
	    break;
	case 'x':
	    v++;
	    z = EXIO;
	    break;
	case 'q':
	    v++;
	    z = QUIO;
	    break;
	default:
	    stderror(ERR_NAME | ERR_SYSTEM, short2str(v[0]), 
		     "Unknown switch");
	    break;
	}

    didsetty = 1;
    if (!v || !*v) {
	int i = -1;
	int len = 0, st = 0, cu;
	for (m = modelist; m->m_name; m++) {
	    if (m->m_type != i) {
		xprintf("%s%s", i != -1 ? "\n" : "", 
			ttylist[z][m->m_type].t_name);
		i = m->m_type;
		st = len = strlen(ttylist[z][m->m_type].t_name);
	    }

	    x = (ttylist[z][i].t_setmask & m->m_value) ? '+' : '\0';
	    x = (ttylist[z][i].t_clrmask & m->m_value) ? '-' : x;

	    if (x != '\0' || aflag) {
		cu = strlen(m->m_name) + (x != '\0') + 1;
		if (len + cu >= T_Cols) {
		    xprintf("\n%*s", st, "");
		    len = st + cu;
		}
		else 
		    len += cu;
		if (x != '\0')
		    xprintf("%c%s ", x, m->m_name);
		else
		    xprintf("%s ", m->m_name);
	    }
	}
	xprintf("\n");
	return;
    }
    while (v && (s = *v++)) {
	switch (*s) {
	case '+':
	case '-':
	    x = *s++;
	    break;
	default:
	    x = '\0';
	    break;
	}
	d = short2str(s);
	for (m = modelist; m->m_name; m++)
	    if (strcmp(m->m_name, d) == 0)
		break;
	if (!m->m_name) 
	    stderror(ERR_NAME | ERR_SYSTEM, d, "Invalid argument");

	switch (x) {
	case '+':
	    ttylist[z][m->m_type].t_setmask |= m->m_value;
	    ttylist[z][m->m_type].t_clrmask &= ~m->m_value;
	    break;
	case '-':
	    ttylist[z][m->m_type].t_setmask &= ~m->m_value;
	    ttylist[z][m->m_type].t_clrmask |= m->m_value;
	    break;
	default:
	    ttylist[z][m->m_type].t_setmask &= ~m->m_value;
	    ttylist[z][m->m_type].t_clrmask &= ~m->m_value;
	    break;
	}
    }
} /* end dosetty */


int
tty_getty(fd, td)
   int fd;
   ttydata_t *td;
{
#ifdef POSIX
    if (tcgetattr(fd, &td->d_t) == -1)
	return -1;
#else /* TERMIO || GSTTY */
# ifdef TERMIO
    if (ioctl(fd, TCGETA,    (ioctl_t) &td->d_t) == -1)
	return -1;
# else /* GSTTY */
#  ifdef TIOCGETP
    if (ioctl(fd, TIOCGETP,  (ioctl_t) &td->d_t) == -1)
	return -1;
#  endif /* TIOCGETP */
#  ifdef TIOCGETC
    if (ioctl(fd, TIOCGETC,  (ioctl_t) &td->d_tc) == -1)
	return -1;
#  endif /* TIOCGETC */
#  ifdef TIOCGPAGE
    if (ioctl(fd, TIOCGPAGE, (ioctl_t) &td->d_pc) == -1)
	return -1;
#  endif /* TIOCGPAGE */
#  ifdef TIOCLGET
    if (ioctl(fd, TIOCLGET,  (ioctl_t) &td->d_lb) == -1)
	return -1;
#  endif /* TIOCLGET */
# endif /* TERMIO */
#endif /* POSIX */

#ifdef TIOCGLTC
    if (ioctl(fd, TIOCGLTC,  (ioctl_t) &td->d_ltc) == -1)
	return -1;
#endif /* TIOCGLTC */

    return 0;
}

int
tty_setty(fd, td)
   int fd;
   ttydata_t *td;
{
#ifdef POSIX
    if (tcsetattr(fd, TCSADRAIN, &td->d_t) == -1) 
	return -1;
#else
# ifdef TERMIO
    if (ioctl(fd, TCSETA,    (ioctl_t) &td->d_t) == -1)
	return -1;
# else
#  ifdef TIOCSETN
    if (ioctl(fd, TIOCSETN,  (ioctl_t) &td->d_t) == -1)
	return -1;
#  endif /* TIOCSETN */
#  ifdef TIOCGETC
    if (ioctl(fd, TIOCSETC,  (ioctl_t) &td->d_tc) == -1)
	return -1;
#  endif /* TIOCGETC */
#  ifdef TIOCGPAGE
    if (ioctl(fd, TIOCSPAGE, (ioctl_t) &td->d_pc) == -1)
	return -1;
#  endif /* TIOCGPAGE */
#  ifdef TIOCLGET
    if (ioctl(fd, TIOCLSET,  (ioctl_t) &td->d_lb) == -1)
	return -1;
#  endif /* TIOCLGET */
# endif /* TERMIO */
#endif /* POSIX */

#ifdef TIOCGLTC
    if (ioctl(fd, TIOCSLTC,  (ioctl_t) &td->d_ltc) == -1)
	return -1;
#endif /* TIOCGLTC */

    return 0;
}

void
tty_getchar(td, s)
    ttydata_t *td;
    unsigned char *s;
{   
#ifdef TIOCGLTC
    {
	struct ltchars *n = &td->d_ltc;

	s[C_SUSP]	= n->t_suspc;
	s[C_DSUSP]	= n->t_dsuspc;
	s[C_REPRINT]	= n->t_rprntc;
	s[C_DISCARD]	= n->t_flushc;
	s[C_WERASE]	= n->t_werasc;
	s[C_LNEXT]	= n->t_lnextc;
    }
#endif /* TIOCGLTC */

#if defined(POSIX) || defined(TERMIO)
    {
# ifdef POSIX
	struct termios *n = &td->d_t;
# else
	struct termio *n = &td->d_t;
# endif /* POSIX */

# ifdef VINTR
	s[C_INTR]	= n->c_cc[VINTR];
# endif /* VINTR */
# ifdef VQUIT
	s[C_QUIT]	= n->c_cc[VQUIT];
# endif /* VQUIT */
# ifdef VERASE
	s[C_ERASE]	= n->c_cc[VERASE];
# endif /* VERASE */
# ifdef VKILL
	s[C_KILL]	= n->c_cc[VKILL];
# endif /* VKILL */
# ifdef VEOF
	s[C_EOF]	= n->c_cc[VEOF];
# endif /* VEOF */
# ifdef VEOL
	s[C_EOL]	= n->c_cc[VEOL];
# endif /* VEOL */
# ifdef VEOL2
	s[C_EOL2]	= n->c_cc[VEOL2];
# endif  /* VEOL2 */
# ifdef VSWTCH
	s[C_SWTCH]	= n->c_cc[VSWTCH];
# endif /* VSWTCH */
# ifdef VERASE2
	s[C_ERASE2]	= n->c_cc[VERASE2];
# endif /* VERASE2 */
# ifdef VSTART
	s[C_START]	= n->c_cc[VSTART];
# endif /* VSTART */
# ifdef VSTOP
	s[C_STOP]	= n->c_cc[VSTOP];
# endif /* VSTOP */
# ifdef VWERASE
	s[C_WERASE]	= n->c_cc[VWERASE];
# endif /* VWERASE */
# ifdef VSUSP
	s[C_SUSP]	= n->c_cc[VSUSP];
# endif /* VSUSP */
# ifdef VDSUSP
	s[C_DSUSP]	= n->c_cc[VDSUSP];
# endif /* VDSUSP */
# ifdef VREPRINT
	s[C_REPRINT]	= n->c_cc[VREPRINT];
# endif /* WREPRINT */
# ifdef VDISCARD
	s[C_DISCARD]	= n->c_cc[VDISCARD];
# endif /* VDISCARD */
# ifdef VLNEXT
	s[C_LNEXT]	= n->c_cc[VLNEXT];
# endif /* VLNEXT */
# ifdef VSTATUS
	s[C_STATUS]	= n->c_cc[VSTATUS];
# endif /* VSTATUS */
# ifdef VPAGE
	s[C_PAGE]	= n->c_cc[VPAGE];
# endif /* VPAGE */
# ifdef VPGOFF
	s[C_PGOFF]	= n->c_cc[VPGOFF];
# endif /* VPGOFF */
# ifdef VMIN
	s[C_MIN]	= n->c_cc[VMIN];
# endif /* VMIN */
# ifdef VTIME
	s[C_TIME]	= n->c_cc[VTIME];
# endif /* VTIME */
    }

#else /* SGTTY */

# ifdef TIOCGPAGE
    {
	struct ttypagestat *n = &td->d_pc;

	s[C_STATUS]	= n->tps_statc;
	s[C_PAGE]	= n->tps_pagec;
	s[C_PGOFF]	= n->tps_pgoffc;
    }
# endif /* TIOCGPAGE */

# ifdef TIOCGETC
    {
	struct tchars *n = &td->d_tc;

	s[C_INTR]	= n->t_intrc;
	s[C_QUIT]	= n->t_quitc;
	s[C_START]	= n->t_startc;
	s[C_STOP]	= n->t_stopc;
	s[C_EOF]	= n->t_eofc;
	s[C_BRK]	= n->t_brkc;
    }
# endif /* TIOCGETC */

# ifdef TIOCGETP
    {
	struct sgttyb *n = &td->d_t;

	s[C_ERASE]	= n->sg_erase;
	s[C_KILL]	= n->sg_kill;
    }
# endif /* TIOCGETP */
#endif /* !POSIX || TERMIO */

} /* tty_getchar */


void
tty_setchar(td, s)
    ttydata_t *td;
    unsigned char *s;
{   
#ifdef TIOCGLTC
    {
	struct ltchars *n = &td->d_ltc; 

	n->t_suspc 		= s[C_SUSP];
	n->t_dsuspc		= s[C_DSUSP];
	n->t_rprntc		= s[C_REPRINT];
	n->t_flushc		= s[C_DISCARD];
	n->t_werasc		= s[C_WERASE];
	n->t_lnextc		= s[C_LNEXT];
    }
#endif /* TIOCGLTC */

#if defined(POSIX) || defined(TERMIO)
    {
# ifdef POSIX
	struct termios *n = &td->d_t;
# else
	struct termio *n = &td->d_t;
# endif /* POSIX */

# ifdef VINTR
	n->c_cc[VINTR]		= s[C_INTR];
# endif /* VINTR */
# ifdef VQUIT
	n->c_cc[VQUIT]		= s[C_QUIT];
# endif /* VQUIT */
# ifdef VERASE
	n->c_cc[VERASE]		= s[C_ERASE];
# endif /* VERASE */
# ifdef VKILL
	n->c_cc[VKILL]		= s[C_KILL];
# endif /* VKILL */
# ifdef VEOF
	n->c_cc[VEOF]		= s[C_EOF];
# endif /* VEOF */
# ifdef VEOL
	n->c_cc[VEOL]		= s[C_EOL];
# endif /* VEOL */
# ifdef VEOL2
	n->c_cc[VEOL2]		= s[C_EOL2];
# endif  /* VEOL2 */
# ifdef VSWTCH
	n->c_cc[VSWTCH]		= s[C_SWTCH];
# endif /* VSWTCH */
# ifdef VERASE2
	n->c_cc[VERASE2]	= s[C_ERASE2];
# endif /* VERASE2 */
# ifdef VSTART
	n->c_cc[VSTART]		= s[C_START];
# endif /* VSTART */
# ifdef VSTOP
	n->c_cc[VSTOP]		= s[C_STOP];
# endif /* VSTOP */
# ifdef VWERASE
	n->c_cc[VWERASE]	= s[C_WERASE];
# endif /* VWERASE */
# ifdef VSUSP
	n->c_cc[VSUSP]		= s[C_SUSP];
# endif /* VSUSP */
# ifdef VDSUSP
	n->c_cc[VDSUSP]		= s[C_DSUSP];
# endif /* VDSUSP */
# ifdef VREPRINT
	n->c_cc[VREPRINT]	= s[C_REPRINT];
# endif /* WREPRINT */
# ifdef VDISCARD
	n->c_cc[VDISCARD]	= s[C_DISCARD];
# endif /* VDISCARD */
# ifdef VLNEXT
	n->c_cc[VLNEXT]		= s[C_LNEXT];
# endif /* VLNEXT */
# ifdef VSTATUS
	n->c_cc[VSTATUS]	= s[C_STATUS];
# endif /* VSTATUS */
# ifdef VPAGE
	n->c_cc[VPAGE]		= s[C_PAGE];
# endif /* VPAGE */
# ifdef VPGOFF
	n->c_cc[VPGOFF]		= s[C_PGOFF];
# endif /* VPGOFF */
# ifdef VMIN
	n->c_cc[VMIN]		= s[C_MIN];
# endif /* VMIN */
# ifdef VTIME
	n->c_cc[VTIME]		= s[C_TIME];
# endif /* VTIME */
    }

#else /* GSTTY */

# ifdef TIOCGPAGE
    {
	struct ttypagestat *n = &td->d_pc;

	n->tps_length		= 0;
	n->tps_lpos		= 0;
	n->tps_statc		= s[C_STATUS];
	n->tps_pagec		= s[C_PAGE];
	n->tps_pgoffc		= s[C_PGOFF];
	n->tps_flag		= 0;
    }
# endif /* TIOCGPAGE */

# ifdef TIOCGETC
    {
	struct tchars *n = &td->d_tc;
	n->t_intrc		= s[C_INTR];
	n->t_quitc		= s[C_QUIT];
	n->t_startc		= s[C_START];
	n->t_stopc		= s[C_STOP];
	n->t_eofc		= s[C_EOF];
	n->t_brkc		= s[C_BRK];
    }
# endif /* TIOCGETC */

# ifdef TIOCGETP
    {
	struct sgttyb *n = &td->d_t;

	n->sg_erase		= s[C_ERASE];
	n->sg_kill		= s[C_KILL];
    }
# endif /* TIOCGETP */
#endif /* !POSIX || TERMIO */

} /* tty_setchar */

speed_t
tty_getspeed(td)
    ttydata_t *td;
{
    speed_t spd;

#ifdef POSIX
    if ((spd = cfgetispeed(&td->d_t)) == 0)
	spd = cfgetospeed(&td->d_t);
#else /* ! POSIX */
# ifdef TERMIO
#  ifdef CBAUD
    spd = td->d_t.c_cflag & CBAUD;
#  else 
    spd = 0;
#  endif 
# else /* SGTTY */
    spd = td->d_t.sg_ispeed;
# endif /* TERMIO */
#endif /* POSIX */

    return spd;
} /* end tty_getspeed */

int
tty_gettabs(td)
    ttydata_t *td;
{
#if defined(POSIX) || defined(TERMIO)
    return ((td->d_t.c_oflag & TAB3) == TAB3) ? 0 : 1;
#else /* SGTTY */
    return (td->d_t.sg_flags & XTABS) == XTABS ? 0 : 1;
#endif /* POSIX || TERMIO */
} /* end tty_gettabs */

int
tty_geteightbit(td)
    ttydata_t *td;
{
#if defined(POSIX) || defined(TERMIO)
    return (td->d_t.c_cflag & CSIZE) == CS8;
#else /* SGTTY */
    return td->d_lb & (LPASS8 | LLITOUT);
#endif /* POSIX || TERMIO */
} /* end tty_geteightbit */


#ifdef _IBMR2
void
tty_setdisc(fd, dis)
    int fd;
    int dis;
{
    static bool edit_discipline = 0;
    static union txname tx_disc;
    extern char strPOSIX[];

    switch (dis) {
    case EXIO:
	if (edit_discipline) {
	    if (ioctl(fd, TXSETLD, (ioctl_t) & tx_disc) == -1)
		return;
	    edit_discipline = 0;
	}
	return;

    case EDIO:
	tx_disc.tx_which = 0;
	if (ioctl(fd, TXGETLD, (ioctl_t) & tx_disc) == -1)
	    return;
	if (strcmp(tx_disc.tx_name, strPOSIX) != 0) {
	    edit_discipline = 1;
	    if (ioctl(fd, TXSETLD, (ioctl_t) strPOSIX) == -1)
	    return;
	}
	return;

    default:
	return;
    }
} /* end tty_setdisc */
#endif /* _IBMR2 */

#ifdef DEBUG_TTY
static void
tty_printchar(s)
    unsigned char *s;
{
    struct tcshmodes *m;
    int i;

    for (i = 0; i < C_NCC; i++) {
	for (m = modelist; m->m_name; m++) 
	    if (m->m_type == M_CHAR && C_SH(i) == m->m_value)
		break;
	if (m->m_name)
	    xprintf("%s ^%c ", m->m_name, s[i] + 'A' - 1);
	if (i % 5 == 0)
	    xprintf("\n");
    }
    xprintf("\n"); 
}
#endif /* DEBUG_TTY */
