/* 
 *	Text editing windows
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

int staen = 0;
int staupd = 0;
int keepup = 0;
int bg_stalin;

/* Move text window */

static void movetw(BW *bw, int x, int y)
{
	TW *tw = (TW *) bw->object;

	if (y || !staen) {
		if (!tw->staon) {	/* Scroll down and shrink */
			nscrldn(bw->parent->t->t, y, bw->parent->nh + y, 1);
		}
		bwmove(bw, x + (bw->o.linums ? LINCOLS : 0), y + 1);
		tw->staon = 1;
	} else {
		if (tw->staon) {	/* Scroll up and grow */
			nscrlup(bw->parent->t->t, y, bw->parent->nh + y, 1);
		}
		bwmove(bw, x + (bw->o.linums ? LINCOLS : 0), y);
		tw->staon = 0;
	}
}

/* Resize text window */

static void resizetw(BW *bw, int wi, int he)
{
	if (bw->parent->ny || !staen)
		bwresz(bw, wi - (bw->o.linums ? LINCOLS : 0), he - 1);
	else
		bwresz(bw, wi - (bw->o.linums ? LINCOLS : 0), he);
}

/* Get current context */

/* Find first line (going backwards) which has 0 indentation level
 * and is not a comment, blank, or block structuring line.  This is
 * likely to be the line with the function name.
 *
 * There are actually two possibilities:
 *
 * We want the first line- 
 *
 * int
 * foo(int x,int y) {
 *
 *   }
 *
 * We want the last line-
 *
 * program foo(input,output);
 * var a, b, c : real;
 * begin
 *
 */

unsigned char *get_context(BW *bw)
{
	P *p = pdup(bw->cursor, USTR "get_context");
	static unsigned char buf1[stdsiz];
	int i, j, spc;


	buf1[0] = 0;
	/* Find first line with 0 indentation which is not a comment line */
	do {
		p_goto_bol(p);
		if (!pisindent(p) && !pisblank(p)) {
			/* next: */
			brzs(p,stdbuf,stdsiz/8); /* To avoid buffer overruns with my_iconv */
			/* Ignore comment and block structuring lines */
			if (!(stdbuf[0]=='{' ||
			    (stdbuf[0]=='/' && stdbuf[1]=='*') ||
			    stdbuf[0]=='\f' ||
			    (stdbuf[0]=='/' && stdbuf[1]=='/') ||
			    stdbuf[0]=='#' ||
			    (stdbuf[0]=='b' && stdbuf[1]=='e' && stdbuf[2]=='g' && stdbuf[3]=='i' && stdbuf[4]=='n') ||
			    (stdbuf[0]=='B' && stdbuf[1]=='E' && stdbuf[2]=='G' && stdbuf[3]=='I' && stdbuf[4]=='N') ||
			    (stdbuf[0]=='-' && stdbuf[1]=='-') ||
			    stdbuf[0]==';')) {
			    	/* zcpy(buf1,stdbuf); */
 				/* replace tabs to spaces and remove adjoining spaces */
 				for (i=0,j=0,spc=0; stdbuf[i]; i++) {
 					if (stdbuf[i]=='\t' || stdbuf[i]==' ') {
 						if (spc) continue;
 						spc = 1;
 					}
 					else spc = 0;
 					if (stdbuf[i]=='\t')
 						buf1[j++] = ' ';
					else if (stdbuf[i]=='\\') {
						buf1[j++] = '\\';
						buf1[j++] = '\\';
					} else
						buf1[j++] = stdbuf[i];
 				}
 				buf1[j]= '\0';
				/* Uncomment to get the last line instead of the first line (see above)
			    	if (pprevl(p)) {
			    		p_goto_bol(p);
			    		if (!pisindent(p) && !pisblank(p))
			    			goto next;
			    	}
			    	*/
			    	break;
			}
			
		}
	} while (!buf1[0] && pprevl(p));

	prm(p);

	return buf1;
}

unsigned char *duplicate_backslashes(unsigned char *s, int len)
{
	unsigned char *m;
	int x, count;
	for (x = count = 0; x != len; ++x)
		if (s[x] == '\\')
			++count;
	m = vsmk(len + count);
	for (x = 0; x != len; ++x) {
		m = vsadd(m, s[x]);
		if (s[x] == '\\')
			m = vsadd(m, '\\');
	}
	return m;
}

/* static */unsigned char *stagen(unsigned char *stalin, BW *bw, unsigned char *s, int fill)
{
	unsigned char buf[80];
	int x;
	W *w = bw->parent;
	time_t n=time(NULL);
	struct tm *cas;
	cas=localtime(&n);

	stalin = vstrunc(stalin, 0);
	while (*s) {
		if (*s == '%' && s[1]) {
			switch (*++s) {
			case 'x': /* Context (but only if autoindent is enabled) */
				{
					if ( bw->o.autoindent) {
						unsigned char *s = get_context(bw);
						/* We need to translate between file's character set to
						   locale */
						my_iconv(stdbuf,locale_map,s,bw->o.charmap);
						stalin = vsncpy(sv(stalin), sz(stdbuf));
					}
				}
				break;

			case 'y':
				{
					if (bw->o.syntax) {
						joe_snprintf_1(buf, sizeof(buf), "(%s)", bw->o.syntax->name);
						stalin = vsncpy(sv(stalin), sz(buf));
					}
				}
				break;
			case 't':
				{
					time_t n = time(NULL);
					int l;
					unsigned char *d = (unsigned char *)ctime(&n);

					l = (d[11] - '0') * 10 + d[12] - '0';
					if (l > 12)
						l -= 12;
					joe_snprintf_1(buf, sizeof(buf), "%2.2d", l);
					if (buf[0] == '0')
						buf[0] = fill;
					stalin = vsncpy(sv(stalin), buf, 2);
					stalin = vsncpy(sv(stalin), d + 13, 3);
				}
				break;
			case 'd':
				{
					if (s[1]) switch (*++s) {
						case 'd' : joe_snprintf_1(buf, sizeof(buf), "%02d",cas->tm_mday); break;
						case 'm' : joe_snprintf_1(buf, sizeof(buf), "%02d",cas->tm_mon + 1); break;
						case 'y' : joe_snprintf_1(buf, sizeof(buf), "%02d",cas->tm_year % 100); break;
						case 'Y' : joe_snprintf_1(buf, sizeof(buf), "%04d",cas->tm_year + 1900); break;
						case 'w' : joe_snprintf_1(buf, sizeof(buf), "%d",cas->tm_wday); break;
						case 'D' : joe_snprintf_1(buf, sizeof(buf), "%03d",cas->tm_yday); break;
						default : buf[0]='d'; buf[1]=*s; buf[2]=0;
					} else {
						buf[0]='d'; buf[1]=0;
					}
					stalin=vsncpy(sv(stalin),sz(buf));
				}
				break;

			case 'E':
				{
					unsigned char *ch;
					int l;
					buf[0]=0;
					for(l=0;s[l+1] && s[l+1] != '%'; l++) buf[l]=s[l+1];
					if (s[l+1]=='%' && buf[0]) {
						buf[l]=0;
						s+=l+1;
						ch=(unsigned char *)getenv((char *)buf);
						if (ch) stalin=vsncpy(sv(stalin),sz(ch));
					} 
				}
				break;

			case 'Z':
				{
					unsigned char *ch;
					int l;
					buf[0]=0;
					for(l=0;s[l+1] && s[l+1] != '%'; l++) buf[l]=s[l+1];
					if (s[l+1]=='%' && buf[0]) {
						buf[l]=0;
						s+=l+1;
						ch=get_status(bw, buf);
						if (ch) stalin=vsncpy(sv(stalin),sz(ch));
					} 
				}
				break;

			case 'u':
				{
					time_t n = time(NULL);
					unsigned char *d = (unsigned char *)ctime(&n);

					stalin = vsncpy(sv(stalin), d + 11, 5);
				}
				break;
			case 'T':
				if (bw->o.overtype)
					stalin = vsadd(stalin, 'O');
				else
					stalin = vsadd(stalin, 'I');
				break;
			case 'W':
				if (bw->o.wordwrap)
					stalin = vsadd(stalin, 'W');
				else
					stalin = vsadd(stalin, fill);
				break;
			case 'I':
				if (bw->o.autoindent)
					stalin = vsadd(stalin, 'A');
				else
					stalin = vsadd(stalin, fill);
				break;
			case 'X':
				if (square)
					stalin = vsadd(stalin, 'X');
				else
					stalin = vsadd(stalin, fill);
				break;
			case 'n':
				{
				if (bw->b->name) {
					unsigned char *tmp = simplify_prefix(bw->b->name);
					unsigned char *tmp1 = duplicate_backslashes(sv(tmp));
					vsrm(tmp);
					stalin = vsncpy(sv(stalin), sv(tmp1));
					vsrm(tmp1);
				} else {
					stalin = vsncpy(sv(stalin), sz(joe_gettext(_("Unnamed"))));
				}
				}
				break;
			case 'm':
				if (bw->b->changed)
					stalin = vsncpy(sv(stalin), sz(joe_gettext(_("(Modified)"))));
				break;
			case 'R':
				if (bw->b->rdonly)
					stalin = vsncpy(sv(stalin), sz(joe_gettext(_("(Read only)"))));
				break;
			case '*':
				if (bw->b->changed)
					stalin = vsadd(stalin, '*');
				else
					stalin = vsadd(stalin, fill);
				break;
			case 'r':
				joe_snprintf_1(buf, sizeof(buf), "%-4ld", bw->cursor->line + 1);
				for (x = 0; buf[x]; ++x)
					if (buf[x] == ' ')
						buf[x] = fill;
				stalin = vsncpy(sv(stalin), sz(buf));
				break;
			case 'o':
				joe_snprintf_1(buf, sizeof(buf), "%-4lld", bw->cursor->byte);
				for (x = 0; buf[x]; ++x)
					if (buf[x] == ' ')
						buf[x] = fill;
				stalin = vsncpy(sv(stalin), sz(buf));
				break;
			case 'O':
				joe_snprintf_1(buf, sizeof(buf), "%-4llX", bw->cursor->byte);
				for (x = 0; buf[x]; ++x)
					if (buf[x] == ' ')
						buf[x] = fill;
				stalin = vsncpy(sv(stalin), sz(buf));
				break;
			case 'a':
				if (!piseof(bw->cursor))
					joe_snprintf_1(buf, sizeof(buf), "%3d", brch(bw->cursor));
				else
					joe_snprintf_0(buf, sizeof(buf), "   ");
				for (x = 0; buf[x]; ++x)
					if (buf[x] == ' ')
						buf[x] = fill;
				stalin = vsncpy(sv(stalin), sz(buf));
				break;
			case 'A':
				if (!piseof(bw->cursor))
					joe_snprintf_1(buf, sizeof(buf), "%2.2X", brch(bw->cursor));
				else
					joe_snprintf_0(buf, sizeof(buf), "  ");
				for (x = 0; buf[x]; ++x)
					if (buf[x] == ' ')
						buf[x] = fill;
				stalin = vsncpy(sv(stalin), sz(buf));
				break;
			case 'c':
				joe_snprintf_1(buf, sizeof(buf), "%-3ld", piscol(bw->cursor) + 1);
				for (x = 0; buf[x]; ++x)
					if (buf[x] == ' ')
						buf[x] = fill;
				stalin = vsncpy(sv(stalin), sz(buf));
				break;
			case 'p':
				if (bw->b->eof->byte >= 1024*1024)
					joe_snprintf_1(buf, sizeof(buf), "%3lld", (bw->cursor->byte >> 10) * 100 / (bw->b->eof->byte >> 10));
				else if (bw->b->eof->byte)
					joe_snprintf_1(buf, sizeof(buf), "%3lld", bw->cursor->byte * 100 / bw->b->eof->byte);
				else
					joe_snprintf_0(buf, sizeof(buf), "100");
				for (x = 0; buf[x]; ++x)
					if (buf[x] == ' ')
						buf[x] = fill;
				stalin = vsncpy(sv(stalin), sz(buf));
				break;
			case 'l':
				joe_snprintf_1(buf, sizeof(buf), "%-4ld", bw->b->eof->line + 1);
				for (x = 0; buf[x]; ++x)
					if (buf[x] == ' ')
						buf[x] = fill;
				stalin = vsncpy(sv(stalin), sz(buf));
				break;
			case 'k':
				{
					int i;
					unsigned char *cpos = buf;

					buf[0] = 0;
					if (w->kbd->x && w->kbd->seq[0])
						for (i = 0; i != w->kbd->x; ++i) {
							int c = w->kbd->seq[i] & 127;

							if (c < 32) {
								cpos[0] = '^';
								cpos[1] = c + '@';
								cpos += 2;
							} else if (c == 127) {
								cpos[0] = '^';
								cpos[1] = '?';
								cpos += 2;
							} else {
								cpos[0] = c;
								cpos += 1;
							}
						}
					*cpos++ = fill;
					while (cpos - buf < 4)
						*cpos++ = fill;
					stalin = vsncpy(sv(stalin), buf, cpos - buf);
				}
				break;
			case 'S':
				if (bw->b->pid)
					stalin = vsncpy(sv(stalin), sz(joe_gettext(_("*SHELL*"))));
				break;
			case 'M':
				if (recmac) {
					joe_snprintf_1(buf, sizeof(buf), joe_gettext(_("(Macro %d recording...)")), recmac->n);
					stalin = vsncpy(sv(stalin), sz(buf));
				}
				break;
			default:
				stalin = vsadd(stalin, *s);
			}
		} else
			stalin = vsadd(stalin, *s);
		++s;
	}
	return stalin;
}

static void disptw(BW *bw, int flg)
{
	W *w = bw->parent;
	TW *tw = (TW *) bw->object;

	if (bw->o.linums != bw->linums) {
		bw->linums = bw->o.linums;
		resizetw(bw, w->w, w->h);
		movetw(bw, w->x, w->y);
		bwfllw(bw);
	}

	if (bw->o.hex) {
		w->cury = (bw->cursor->byte-bw->top->byte)/16 + bw->y - w->y;
		w->curx = (bw->cursor->byte-bw->top->byte)%16 + 60 - bw->offset;
	} else {
		w->cury = bw->cursor->line - bw->top->line + bw->y - w->y;
		w->curx = bw->cursor->xcol - bw->offset + (bw->o.linums ? LINCOLS : 0);
	}

	if ((staupd || keepup || bw->cursor->line != tw->prevline || bw->b->changed != tw->changed || bw->b != tw->prev_b) && (w->y || !staen)) {
		int fill;

		tw->prevline = bw->cursor->line;
		tw->changed = bw->b->changed;
		tw->prev_b = bw->b;
		if (bw->o.rmsg[0])
			fill = bw->o.rmsg[0];
		else
			fill = ' ';
		tw->stalin = stagen(tw->stalin, bw, bw->o.lmsg, fill);
		tw->staright = stagen(tw->staright, bw, bw->o.rmsg, fill);
		if (fmtlen(tw->staright) < w->w) {
			int x = fmtpos(tw->stalin, w->w - fmtlen(tw->staright));

			if (x > sLEN(tw->stalin))
				tw->stalin = vsfill(sv(tw->stalin), fill, x - sLEN(tw->stalin));
			tw->stalin = vsncpy(tw->stalin, fmtpos(tw->stalin, w->w - fmtlen(tw->staright)), sv(tw->staright));
		}
		tw->stalin = vstrunc(tw->stalin, fmtpos(tw->stalin, w->w));
		genfmt(w->t->t, w->x, w->y, 0, tw->stalin, bg_stalin, 0);
		w->t->t->updtab[w->y] = 0;
	}

	if (flg) {
		if (bw->o.hex)
			bwgenh(bw);
		else
			bwgen(bw, bw->o.linums);
	}
}

static void iztw(TW *tw, int y)
{
	tw->stalin = NULL;
	tw->staright = NULL;
	tw->changed = -1;
	tw->prevline = -1;
	tw->staon = (!staen || y);
	tw->prev_b = 0;
}

/* Split current window */
int usplitw(BW *bw)
{
	W *w = bw->parent;
	int newh = getgrouph(w);
	W *new;
	TW *newtw;
	BW *newbw;

	dostaupd = 1;
	if (newh / 2 < FITHEIGHT)
		return -1;
	new = wcreate(w->t, w->watom, findbotw(w), NULL, w, newh / 2 + (newh & 1), NULL, NULL);
	if (!new)
		return -1;
	wfit(new->t);
	new->object = (void *) (newbw = bwmk_takeref(new, bw->b, 0));
	++bw->b->count;
	newbw->offset = bw->offset;
	newbw->object = (void *) (newtw = (TW *) joe_malloc(sizeof(TW)));
	iztw(newtw, new->y);
	bwposcpy(newbw, bw);
	new->t->curwin = new;
	return 0;
}

/* Duplicate current window, move the original to the end of the window list,
 * and hide it. Show the duplicate in place of the original, with the same
 * height.
 */
int uduptw(BW *bw)
{
	W *w = bw->parent;
	int newh = getgrouph(w);
	W *new;
	TW *newtw;
	BW *newbw;

	dostaupd = 1;
	new = wcreate(w->t, w->watom, findbotw(w), NULL, NULL, newh, NULL, NULL);
	if (!new)
		return -1;
	if (demotegroup(w))
		new->t->topwin = new;
	new->object = (void *) (newbw = bwmk_takeref(new, bw->b, 0));
	++bw->b->count;
	newbw->offset = bw->offset;
	newbw->object = (void *) (newtw = (TW *) joe_malloc(sizeof(TW)));
	iztw(newtw, new->y);
	bwposcpy(newbw, bw);
	new->t->curwin = new;
	if (w->y != -1)
		fatal(USTR "uduptw.a");
	wfit(w->t);
	if (w->y != -1)
		fatal(USTR "uduptw.b");
	return 0;
}

static void instw(BW *bw, B *b, long int l, long int n, int flg)
{
	if (b == bw->b)
		bwins(bw, l, n, flg);
}

static void deltw(BW *bw, B *b, long int l, long int n, int flg)
{
	if (b == bw->b)
		bwdel(bw, l, n, flg);
}

WATOM watomtw = {
	USTR "main",
	disptw,
	bwfllw,
	NULL,
	rtntw,
	utypebw,
	resizetw,
	movetw,
	instw,
	deltw,
	TYPETW
};

/* Find a window containing b, which is not w.
 *
 * If there is no such window, return NULL.
 */
static W* find_another_window(B *b, W *w2) {
	W *w = w2->link.prev;
	if (w == w2 || b->orphan)
		return NULL;
	while (1) {
		if ((w->watom->what&TYPETW) &&
		    ((BW*)w->object)->b == b) return w;
		w = w->link.prev;
		if (w == w2) return NULL;
	}
}

int abortit(BW *bw)
{
	W *w;
	TW *tw;
	if (bw->parent->watom != &watomtw)
		return wabort(bw->parent);
	if (bw->b->pid && bw->b->count==1)
		return ukillpid(bw);
	w = bw->parent;
	tw = (TW*)bw->object;

	if (w == w->t->curwin->main &&
	    count_onscreen_main_windows(w->t) == 1) {  /* Almost always true. */
		B *bother = bprev_get(bw->b, w->t);
		if (bother != NULL) {
			void *object = bw->object;  /* TW */
			W *wold = w;
			bwrm(bw);
			w->object = (void *) (bw = bwmk_takeref(w, b_incref(bother), 0));
			wredraw(w);
			bw->object = object;  /* TODO(pts): Move this above wredraw(w)? */
			w = find_another_window(bother, w);
			if (w == NULL)
				return 0;
			bwposcpy(bw, (BW*)w->object);
			/* Continue deleting that other window. */
			bw = (BW*)w->object;
			tw = (TW*)bw->object;
			w->orgwin = NULL;  /* Just to make sure wabort(w) won't change w->t->curwin. */
			wold->msgb = NULL;  /* Cancel genexmsg() in naborttw(). */
		}
	}

	/* If only one main window on the screen (maybe invisible): */
	if (countmain(w->t) == 1) {
		B *b;
		if ((b = borphan()) != NULL) {
			/* Replace it with an orphaned buffer if there are any */
			void *object = bw->object;
			/* FIXME: Shouldn't we wabort() and wcreate here to kill
			   any prompt windows? */

			bwrm(bw);
			w->object = (void *) (bw = bwmk_takeref(w, b, 0));
			wredraw(bw->parent);
			bw->object = object;
			return 0;
		}
	}
	bwrm(bw);
	vsrm(tw->stalin);
	joe_free(tw);
	w->object = NULL;
	wabort(w);	/* Eliminate this window and it's children */
	return 0;
}

/* User routine for aborting a text window */

static int naborttw(BW *bw, int k, void *object, int *notify)
{
	if (notify)
		*notify = 1;
	if (k != YES_CODE && !yncheck(yes_key, k))
		return -1;

	genexmsg(bw, 0, NULL);
	return abortit(bw);
}

static int naborttw1(BW *bw, int k, void *object, int *notify)
{
	if (notify)
		*notify = 1;
	if (k != YES_CODE && !yncheck(yes_key, k))
		return -1;

	if (!exmsg) genexmsg(bw, 0, NULL);
	return abortit(bw);
}

/* k is last character types which lead to uabort.  If k is -1, it means uabort
   was called internally, and not by the user: which means uabort will not send
   Ctrl-C (?) to process */
int uabort(BW *bw, int k)
{
	if (bw->parent->watom != &watomtw)
		return wabort(bw->parent);
	if (bw->b->pid && bw->b->count==1)
		return ukillpid(bw);
	if (bw->b->scratch && bw->b == errbuf && bw->b->name[0] == '*') {
		W *w0 = bw->parent;
		W *w = w0->link.prev;
		while (w != w0 && w->y == -1) w = w->link.prev;
		if (w != w0) {  /* There is another visible window (w). */
			if (bw->b->count == 1)
				orphit(bw);
			w0->orgwin = w;  /* Instruct wabort() called by abortit() to make w0's height add to w's height, and w the next current window (w->t->curwin = w). */
			return abortit(bw);
		}
	}
	if (bw->b->changed && bw->b->count == 1 && !bw->b->scratch)
		if (mkqw(bw->parent, sz(joe_gettext(_("Lose changes to this file (y,n,^C)? "))), naborttw, NULL, NULL, NULL))
			return 0;
		else
			return -1;
	else
		return naborttw(bw, YES_CODE, NULL, NULL);
}

int ucancel(BW *bw, int k)
{
	if (bw->parent->watom != &watomtw) {
		wabort(bw->parent);
		return 0;
	} else
		return uabort(bw,k);
}

/* Same as above, but only calls genexmsg if nobody else has */

int uabort1(BW *bw, int k)
{
	if (bw->parent->watom != &watomtw)
		return wabort(bw->parent);
	if (bw->b->pid && bw->b->count==1)
		return ukillpid(bw);
	if (bw->b->changed && bw->b->count == 1 && !bw->b->scratch)
		if (mkqw(bw->parent, sz(joe_gettext(_("Lose changes to this file (y,n,^C)? "))), naborttw1, NULL, NULL, NULL))
			return 0;
		else
			return -1;
	else
		return naborttw1(bw, YES_CODE, NULL, NULL);
}

/* Abort buffer without prompting: just fail if this is last window on buffer */

int uabortbuf(BW *bw)
{
	W *w = bw->parent;
	B *b;

	if (bw->b->pid && bw->b->count==1)
		return ukillpid(bw);

	if (okrepl(bw))
		return -1;

	if ((b = borphan()) != NULL) {  /* Replace bw in w with an orphaned window. */
		void *object = bw->object;

		bwrm(bw);
		w->object = (void *) (bw = bwmk_takeref(w, b, 0));
		wredraw(bw->parent);
		bw->object = object;
		return 0;
	}

	return naborttw(bw, YES_CODE, NULL, NULL);  /* Remove w. */
}

/* Kill current window (orphans buffer) */

int utw0(BASE *b)
{
	BW *bw = b->parent->main->object;

	if (countmain(b->parent->t) == 1)
		return -1;
	if (bw->b->count == 1)
		orphit(bw);
	return uabort(bw, -1);
}

/* Kill all other windows (orphans buffers) */

int utw1(BASE *b)
{
	W *starting = b->parent;
	W *mainw = starting->main;
	Screen *t = mainw->t;
	int yn;

	do {
		yn = 0;
	      loop:
		do {
			wnext(t);
		} while (t->curwin->main == mainw && t->curwin != starting);
		if (t->curwin->main != mainw) {
			BW *bw = t->curwin->main->object;
			utw0((BASE *)bw);
			yn = 1;
			goto loop;
		}
	} while (yn);
	return 0;
}

void setline(B *b, long int line)
{
	W *w = maint->curwin;

	do {
		if (w->watom->what == TYPETW) {
			BW *bw = w->object;

			if (bw->b == b) {
				long oline = bw->top->line;

				/* pline(bw->top, line); */
				pline(bw->cursor, line);
				if (w->y >= 0 && bw->top->line > oline && bw->top->line - oline < bw->h)
					nscrlup(w->t->t, bw->y, bw->y + bw->h, (int) (bw->top->line - oline));
				else if (w->y >= 0 && bw->top->line < oline && oline - bw->top->line < bw->h)
					nscrldn(w->t->t, bw->y, bw->y + bw->h, (int) (oline - bw->top->line));
				msetI(bw->t->t->updtab + bw->y, 1, bw->h);
			}
		}
	} while ((w = w->link.next) != maint->curwin);
	/* In case error buffer was orphaned */
	if (errbuf == b && b->oldcur) {
		pline(b->oldcur, line);
	}
}

/* Create a text window.  It becomes the last window on the screen */

BW *wmktw_takeref(Screen *t, B *b)
{
	W *w;
	BW *bw;
	TW *tw;

	w = wcreate(t, &watomtw, NULL, NULL, NULL, t->h, NULL, NULL);
	wfit(w->t);
	w->object = (void *) (bw = bwmk_takeref(w, b, 0));
	bw->object = (void *) (tw = (TW *) joe_malloc(sizeof(TW)));
	iztw(tw, w->y);
	return bw;
}
