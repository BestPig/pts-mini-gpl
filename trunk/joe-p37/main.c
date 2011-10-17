/*
 *	Editor startup and main edit loop
 *	Copyright
 *		(C) 1992 Joseph H. Allen
 *
 * 	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"

#ifdef MOUSE_GPM
#include <gpm.h>
#endif

unsigned char *exmsg = NULL;		/* Message to display when exiting the editor */
int usexmouse=0;
int xmouse=0;
int nonotice;
int help;

Screen *maint;			/* Main edit screen */

/* Make windows follow cursor */

void dofollows(void)
{
	W *w = maint->curwin;

	do {
		if (w->y != -1 && w->watom->follow && w->object)
			w->watom->follow(w->object);
		w = (W *) (w->link.next);
	} while (w != maint->curwin);
}

/* Update screen */

volatile int dostaupd = 1;

void edupd(int flg)
{
	W *w;
	int wid, hei;

	if (dostaupd) {
		staupd = 1;
		dostaupd = 0;
	}
	if (env_columns <= 0 || env_lines <= 0) {
		ttgtsz(&wid, &hei);  /* This is run about once per second. */
		if (nresize(maint->t, wid, hei, 1)) {
			sresize(maint);
#ifdef MOUSE_GPM
			gpm_mx = maint->w;
			gpm_my = maint->h;
#endif
		}
	}
	dofollows();
	ttflsh();
	nscroll(maint->t, BG_COLOR(bg_text));
	help_display(maint);
	w = maint->curwin;
	do {
		if (w->y != -1) {
			if (w->object && w->watom->disp)
				w->watom->disp(w->object, flg);
			msgout(w);
		}
		w = (W *) (w->link.next);
	} while (w != maint->curwin);
	cpos(maint->t, maint->curwin->x + maint->curwin->curx, maint->curwin->y + maint->curwin->cury);
	staupd = 0;
}

static int ahead = 0;
static int ungot = 0;
static int ungotc = 0;
static int ungotc2 = 0;

void nungetc(int c)
{
	if (c != 'C' - '@' && c != 'M' - '@') {
		chmac();
		ungot = 1;
		ungotc = c;
	}
}

/* Push c1 and c2 back to the input buffer so that c1 will be processed first.
 */
void nungetc2(int c1, int c2) {
	chmac();  /* what does this do? */
	ungot = 2;
	ungotc2 = c1;
	ungotc = c2;
}

int edloop(int flg)
{
	int term = 0;
	int ret = 0;

	if (flg) {
		if (maint->curwin->watom->what == TYPETW)
			return 0;
		else
			maint->curwin->notify = &term;
	}
	while (!leave && (!flg || !term)) {
		MACRO *m;
		int c;

		if (exmsg && !flg) {
			vsrm(exmsg);
			exmsg = NULL;
		}
		edupd(1);
		if (!ahead && !have)
			ahead = 1;
		if (ungot) {
			if (ungot == 2) {
				c = ungotc2;
				ungot = 1;
			} else {
				c = ungotc;
				ungot = 0;
			}
		} else
			c = ttgetc();

		if (!ahead && c == 10)
			c = 13;
		m = dokey(maint->curwin->kbd, c);
		if (maint->curwin->main && maint->curwin->main != maint->curwin) {
			int x = maint->curwin->kbd->x;

			maint->curwin->main->kbd->x = x;
			if (x)
				maint->curwin->main->kbd->seq[x - 1] = maint->curwin->kbd->seq[x - 1];
		}
		if (m)
			ret = exemac(m);
	}

	if (term == -1)
		return -1;
	else
		return ret;
}

#ifdef __MSDOS__
extern void setbreak();
extern int breakflg;
#endif

unsigned char **mainenv;

B *startup_log;

unsigned char i_msg[128];

void internal_msg(unsigned char *s)
{
	P *t = pdup(startup_log->eof, USTR "internal_msg");
	binss(t, s);
	prm(t);
}

/* Read system configuration files/dirs JOERC and JOEDATA? */
char read_sys_configs = 1;

char read_user_configs = 1;

int main(int argc, char **real_argv, char **envv)
{
	CAP *cap;
	unsigned char **argv = (unsigned char **)real_argv;
	struct stat sbuf;
	unsigned char *s;
	unsigned char *t;
	long time_rc;
	unsigned char *run;
#ifdef __MSDOS__
	unsigned char *rundir;
#endif
	SCRN *n;
	int opened = 0;
	int omid;
	int backopt;
	int c;

	init_bufs();

	for (c = 1; argv[c] != NULL; ++c) {
		if (0 == zcmp((unsigned char*)argv[c], USTR "-nosys")) {
			read_sys_configs = 0;
		} else if (0 == zcmp((unsigned char*)argv[c], USTR "-nouser")) {
			read_user_configs = 0;
		} else {
			argv[--c] = argv[0];
			argv += c;
			argc -= c;
			break;
		}
	}

	if (read_user_configs) {
		s = (unsigned char *)getenv("HOME");
		if (s) {
			s = vsncpy(NULL, 0, sz(s));
			s = vsncpy(sv(s), sc("/.joe-p37/only.rc"));
			if (!stat((char*)s, &sbuf)) read_sys_configs = 0;
			vsrm(s);
		}
	}

	joe_locale();

	mainenv = (unsigned char **)envv;

#ifdef __MSDOS__
	_fmode = O_BINARY;
	zcpy(stdbuf, argv[0]);
	joesep(stdbuf);
	run = namprt(stdbuf);
	rundir = dirprt(stdbuf);
	for (c = 0; run[c]; ++c)
		if (run[c] == '.') {
			run = vstrunc(run, c);
			break;
		}
#else
	run = namprt(argv[0]);
#endif

	env_lines = 0;
	if ((s = (unsigned char *)getenv("LINES")) != NULL &&
	    sscanf((char *)s, "%d", &env_lines) != 1)
		env_lines = 0;
	env_columns = 0;
	if ((s = (unsigned char *)getenv("COLUMNS")) != NULL &&
	    sscanf((char *)s, "%d", &env_columns) != 1)
		env_columns = 0;
	if ((s = (unsigned char *)getenv("BAUD")) != NULL)
		sscanf((char *)s, "%u", (unsigned *)&Baud);
	if (getenv("DOPADDING"))
		dopadding = 1;
	if (getenv("NOXON"))
		noxon = 1;
	if ((s = (unsigned char *)getenv("JOETERM")) != NULL)
		joeterm = s;

#ifndef __MSDOS__
	if (!(cap = my_getcap(NULL, 9600, NULL, NULL))) {
		fprintf(stderr, (char *)joe_gettext(_("Couldn't load termcap/terminfo entry\n")));
		return 1;
	}
#endif

#ifdef __MSDOS__

	s = vsncpy(NULL, 0, sv(run));
	s = vsncpy(sv(s), sc("rc"));
	c = procrc(cap, s);
	if (c == 0)
		goto donerc;
	if (c == 1) {
		unsigned char buf[8];

		fprintf(stderr, (char *)joe_gettext(_("There were errors in '%s'.  Use it anyway?")), s);
		fflush(stderr);
		if (NULL == fgets(buf, 8, stdin) || yn_checks(yes_key, buf))
			goto donerc;
	}

	vsrm(s);
	s = vsncpy(NULL, 0, sv(rundir));
	s = vsncpy(sv(s), sv(run));
	s = vsncpy(sv(s), sc("rc"));
	c = procrc(cap, s);
	if (c != 0 && c != 1) {
		/* If built-in *fancyjoerc not present, process builtin *joerc,
		 * which is always present.
		 */
		s = vstrunc(s, 0);
		s = vsncpy(sv(s), sc("*joerc"));
		c = procrc(cap, s);
	}
	if (c == 0)
		goto donerc;
	if (c == 1) {
		unsigned char buf[8];

		fprintf(stderr, (char *)joe_gettext(_("There were errors in '%s'.  Use it anyway?")), s);
		fflush(stderr);
		if (NULL == fgets(buf, 8, stdin) || yn_checks(yes_key, buf))
			goto donerc;
	}
#else

	/* Name of system joerc file.  Try to find one with matching language... */
	
	/* Try full language: like joerc.de_DE */
	time_rc = 0;
        if (!read_sys_configs) { t = NULL; goto skip_joerc; }
        t = vsncpy(NULL, 0, sc(JOERC));
	t = vsncpy(sv(t), sv(run));
	t = vsncpy(sv(t), sc("rc."));
	t = vsncpy(sv(t), sz(locale_msgs));
	if (!stat((char *)t,&sbuf))
		time_rc = sbuf.st_mtime;
	else {
		/* Try generic language: like joerc.de */
		if (locale_msgs[0] && locale_msgs[1] && locale_msgs[2]=='_') {
			vsrm(t);
			t = vsncpy(NULL, 0, sc(JOERC));
			t = vsncpy(sv(t), sv(run));
			t = vsncpy(sv(t), sc("rc."));
			t = vsncpy(sv(t), locale_msgs, 2);
			if (!stat((char *)t,&sbuf))
				time_rc = sbuf.st_mtime;
			else
				goto nope;
		} else {
			nope:
			vsrm(t);
			/* Try Joe's bad english */
			t = vsncpy(NULL, 0, sc(JOERC));
			t = vsncpy(sv(t), sv(run));
			t = vsncpy(sv(t), sc("rc"));
			if (!stat((char *)t,&sbuf))
				time_rc = sbuf.st_mtime;
			else {
				/* If built-in *fancyjoerc not present, process builtin *joerc,
				 * which is always present.
				 */
				t = vstrunc(s, 0);
				t = vsncpy(sv(s), sc("*joerc"));
				time_rc = stat((char *)t,&sbuf) ? 0 : sbuf.st_mtime;
			}
		}
	}
	skip_joerc:

	/* User's joerc file */
	s = (unsigned char *)getenv("HOME");
	if (s && !read_sys_configs) {
		if (read_user_configs) {  /* TODO(pts): Don't read /dev/null */
			s = vsncpy(NULL, 0, sz(s));
			s = vsncpy(sv(s), sc("/.joe-p37/rc"));
		} else {
			s = vsncpy(NULL, 0, sc("/dev/null/missing"));
		}
		goto process_user_rc;
	} else if (!read_user_configs) {
		s = vsncpy(NULL, 0, sc("/dev/null/missing"));
		goto process_user_rc;
	} else if (s) {
		unsigned char buf[8];

		s = vsncpy(NULL, 0, sz(s));
		s = vsncpy(sv(s), sc("/."));
		s = vsncpy(sv(s), sv(run));
		s = vsncpy(sv(s), sc("rc"));

		if (!stat((char *)s,&sbuf)) {
			if (sbuf.st_mtime < time_rc) {
				fprintf(stderr,(char *)joe_gettext(_("Warning: %s is newer than your %s.\n")),t,s);
				fprintf(stderr,(char *)joe_gettext(_("You should update or delete %s\n")),s);
				fprintf(stderr,(char *)joe_gettext(_("Hit enter to continue with %s ")),t);
				fflush(stderr);
				(void)!fgets((char *)buf, 8, stdin);
				goto use_sys;
			}
		}

	      process_user_rc:
		c = procrc(cap, s);
		if (c == 0) {
			vsrm(t);
			goto donerc;
		}
		if (c == 1) {
			fprintf(stderr,(char *)joe_gettext(_("There were errors in '%s'.  Use it anyway (y,n)? ")), s);
			fflush(stderr);
			if (NULL == fgets((char *)buf, 8, stdin) || ynchecks(yes_key, buf)) {
				vsrm(t);
				goto donerc;
			}
		}
	}

	use_sys:
	vsrm(s);
	s = t;
	c = s ? procrc(cap, s) : -1;
	if (c == 0)
		goto donerc;
	if (c == 1) {
		unsigned char buf[8];

		fprintf(stderr,(char *)joe_gettext(_("There were errors in '%s'.  Use it anyway (y,n)? ")), s);
		fflush(stderr);
		if (NULL == fgets((char *)buf, 8, stdin) || ynchecks(yes_key, buf))
			goto donerc;
	}

	/* Try built-in *fancyjoerc, e.g. "*joe-p37rc" */
	vsrm(s);
	s = vsncpy(NULL, 0, sc("*"));
	s = vsncpy(sv(s), sv(run));
	s = vsncpy(sv(s), sc("rc"));
	c = procrc(cap, s);
	if (c != 0 && c != 1) {
		/* If built-in *fancyjoerc not present, process builtin "*joerc",
		 * which is always present.
		 */
		s = vstrunc(s, 0);
		s = vsncpy(sv(s), sc("*joerc"));
		c = procrc(cap, s);
	}
	if (c == 0)
		goto donerc;
	if (c == 1) {
		unsigned char buf[8];

		fprintf(stderr,(char *)joe_gettext(_("There were errors in '%s'.  Use it anyway (y,n)? ")), s);
		fflush(stderr);
		if (NULL == fgets((char *)buf, 8, stdin) || ynchecks(yes_key, buf))
			goto donerc;
	}
#endif

	fprintf(stderr,(char *)joe_gettext(_("Couldn't open '%s'\n")), s);
	vsrm(s);
	return 1;

	donerc:
	vsrm(s);

	if (validate_rc()) {
		fprintf(stderr,(char *)joe_gettext(_("rc file has no :main key binding section or no bindings.  Bye.\n")));
		return 1;
	}


	if (!isatty(fileno(stdin)))
		idleout = 0;

	for (c = 1; argv[c]; ++c) {
		if (argv[c][0] == '-') {
			if (argv[c][1])
				switch (glopt(argv[c] + 1, argv[c + 1], NULL, 1)) {
				case 0:
					fprintf(stderr,(char *)joe_gettext(_("Unknown option '%s'\n")), argv[c]);
					break;
				case 1:
					break;
				case 2:
					++c;
					break;
			} else
				idleout = 0;
		}
	}
	if (!dspasis) {  /* Open all files as ASCII by default if `joe --asis' is specified. This is to avoid writing control characters to the UTF-8 terminal. */
		fdefault.charmap = pdefault.charmap = find_charmap(USTR "ascii");
		fdefault.map_name = pdefault.map_name = USTR "ascii";
	}


	/* initialize mouse support */
	if (xmouse && (s=(unsigned char *)getenv("TERM")) && strstr((char *)s,"xterm"))
		usexmouse=1;

	if (!(n = nopen(cap)))
		return 1;
	maint = screate(n);
	vmem = vtmp();

	startup_log = bfind_scratch(USTR "* Startup Log *");
	startup_log->internal = 1;

	load_state();

	/* It would be better if this ran uedit() to load files */

	/* The business with backopt is to load the file first, then apply file
	 * local options afterwords */

	/* orphan is not compatible with exemac()- macros need a window to exist */
	for (c = 1, backopt = 0; argv[c]; ++c)
		if (argv[c][0] == '+' && argv[c][1]>='0' && argv[c][1]<='9') {
			if (!backopt)
				backopt = c;
		} else if (argv[c][0] == '-' && argv[c][1]) {
			if (!backopt)
				backopt = c;
			if (glopt(argv[c] + 1, argv[c + 1], NULL, 0) == 2)
				++c;
		} else {
			B *b = bfind(argv[c]);
			BW *bw = NULL;
			int er = berror;

			/* This is too annoying */
			/* set_current_dir(argv[c],1); */

			setup_history(&filehist);
			append_history(filehist,sz(argv[c]));

			/* wmktw inserts the window before maint->curwin */
			if (!orphan || !opened) {
				bw = wmktw(maint, b);
				if (er)
					msgnwt(bw->parent, joe_gettext(msgs[-er]));
			} else {
				long line;
				b->orphan = 1;
				b->oldcur = pdup(b->bof, USTR "main");
				pline(b->oldcur, get_file_pos(b->name));
				line = b->oldcur->line - (maint->h - 1) / 2;
				if (line < 0)
					line = 0;
				b->oldtop = pdup(b->oldcur, USTR "main");
				pline(b->oldtop, line);
				p_goto_bol(b->oldtop);
			}
			if (bw) {
				long lnum = 0;

				bw->o.readonly = bw->b->rdonly;
				if (backopt) {
					while (backopt != c) {
						if (argv[backopt][0] == '+') {
							sscanf((char *)(argv[backopt] + 1), "%ld", &lnum);
							++backopt;
						} else {
							if (glopt(argv[backopt] + 1, argv[backopt + 1], &bw->o, 0) == 2)
								backopt += 2;
							else
								backopt += 1;
							lazy_opts(bw->b, &bw->o);
						}
					}
				}
				bw->b->o = bw->o;
				bw->b->rdonly = bw->o.readonly;
				/* Put cursor in window, so macros work properly */
				maint->curwin = bw->parent;
				/* Execute macro */
				if (er == -1 && bw->o.mnew)
					exmacro(bw->o.mnew,1);
				if (er == 0 && bw->o.mold)
					exmacro(bw->o.mold,1);
				/* Hmm... window might not exist any more... depends on what macro does... */
				if (lnum > 0)
					pline(bw->cursor, lnum - 1);
				else
					pline(bw->cursor, get_file_pos(bw->b->name));
				p_goto_bol(bw->cursor);
				/* Go back to first window so windows are in same order as command line  */
				if (opened)
					wnext(maint);
				
			}
			opened = 1;
			backopt = 0;
		}

	

	if (opened) {
		wshowall(maint);
		omid = mid;
		mid = 1;
		dofollows();
		mid = omid;
	} else {
		BW *bw = wmktw(maint, bfind(USTR ""));

		if (bw->o.mnew)
			exmacro(bw->o.mnew,1);
	}
	maint->curwin = maint->topwin;

	if (startup_log->eof->byte) {
		BW *bw = wmktw(maint, startup_log);
		startup_log = 0;
		maint->curwin = bw->parent;
		wshowall(maint);
		uparserr(bw);
	}

	if (help) {
		help_on(maint);
	}
	if (!nonotice) {
		joe_snprintf_3(msgbuf,JOE_MSGBUFSIZE,joe_gettext(_("\\i** Joe's Own Editor v%s ** (%s) ** Copyright %s 2008 **\\i")),VERSION,locale_map->name,(locale_map->type ? "©" : "(C)"));

		msgnw(((BASE *)lastw(maint)->object)->parent, msgbuf);
	}

	if (!idleout) {
		if (!isatty(fileno(stdin)) && modify_logic(maint->curwin->object, ((BW *)maint->curwin->object)->b)) {
			/* Start shell going in first window */
			unsigned char **a;
			unsigned char *cmd;

			a = vamk(10);
			cmd = vsncpy(NULL, 0, sc("/bin/sh"));
			a = vaadd(a, cmd);
			cmd = vsncpy(NULL, 0, sc("-c"));
			a = vaadd(a, cmd);
			cmd = vsncpy(NULL, 0, sc("/bin/cat"));
			a = vaadd(a, cmd);
			
			cstart (maint->curwin->object, USTR "/bin/sh", a, NULL, NULL, 0, 1);
		}
	}

	edloop(0);

	save_state();

	/* Delete all buffer so left over locks get eliminated */
	brmall();

	vclose(vmem);
	nclose(n);

	if (exmsg)
		fprintf(stderr, "\n%s\n", exmsg);
	return 0;
}
