/*
 *	*rc file parserr
 *	Copyright
 *		(C) 1992 Joseph H. Allen; 
 *
 *	This file is part of JOE (Joe's Own Editor)
 */
#include "types.h"
#include "preproc.h"

extern char read_sys_configs;
extern char read_user_configs;

/* Commands which just type in variable values */

/* Insert the current buffer's charset to the current buffer */
int ucharset(BW *bw)
{
	unsigned char *s;
	W *w=bw->parent->main;
	s=((BW *)w->object)->o.charmap->name;
	if (!s || !*s)
		return -1;
	while (*s)
		if (utypebw(bw,*s++))
			return -1;
	return 0;
}

/* Insert the locale charset to the current buffer */
int ulcharset(BW *bw)
{
	unsigned char *s;
	s=locale_map->name;
	if (!s || !*s)
		return -1;
	while (*s)
		if (utypebw(bw,*s++))  /* TODO(pts): Why not insert at once? */
			return -1;
	return 0;
}

int ulanguage(BW *bw)
{
	unsigned char *s;
	W *w=bw->parent->main;
	s=((BW *)w->object)->o.language;
	if (!s || !*s)
		return -1;
	while (*s)
		if (utypebw(bw,*s++))
			return -1;
	return 0;
}

#define OPT_BUF_SIZE 300

static struct context {
	struct context *next;
	unsigned char *name;
	KMAP *kmap;
} *contexts = NULL;		/* List of named contexts */

/* Find a context of a given name- if not found, one with an empty kmap
 * is created.
 */

KMAP *kmap_getcontext(unsigned char *name)
{
	struct context *c;

	for (c = contexts; c; c = c->next)
		if (!zcmp(c->name, name))
			return c->kmap;
	c = (struct context *) joe_malloc(sizeof(struct context));

	c->next = contexts;
	c->name = zdup(name);
	contexts = c;
	return c->kmap = mkkmap();
}

/* JM - ngetcontext(name) - like getcontext, but return NULL if it
 * doesn't exist, instead of creating a new one.
 */

KMAP *ngetcontext(unsigned char *name)
{
	struct context *c;
	for(c=contexts;c;c=c->next)
		if(!zcmp(c->name,name))
			return c->kmap;
	return 0;
}

/* Validate joerc file */

int validate_rc()
{
	KMAP *k = ngetcontext(USTR "main");
	int x;
	/* Make sure main exists */
	if (!k)
		return -1;
	/* Make sure there is at least one key binding */
	for (x = 0; x != KEYS; ++x)
		if (k->keys[x].value.bind)
			return 0;
	return -1;
}

unsigned char **get_keymap_list()
{
	unsigned char **lst = 0;
	struct context *c;
	for (c=contexts; c; c=c->next)
		lst = vaadd(lst, vsncpy(NULL,0,sz(c->name)));

	return lst;
}

OPTIONS *options = NULL;

/* Set to use ~/.joe_state file */
int joe_state;

/* Default options for prompt windows */

OPTIONS pdefault = {
	NULL,		/* *next */
	NULL,		/* *name_regex */
	NULL,		/* *contents_regex */
	0,		/* overtype */
	0,		/* lmargin */
	76,		/* rmargin */
	0,		/* autoindent */
	0,		/* wordwrap */
	0,		/* nobackup */
	8,		/* tab */
	' ',		/* indent char */
	1,		/* indent step */
	NULL,		/* *context */
	NULL,		/* *lmsg */
	NULL,		/* *rmsg */
	0,		/* line numbers */
	0,		/* read only */
	0,		/* french spacing */
	0,		/* flowed text */
	0,		/* spaces */
#ifdef __MSDOS__
	1,		/* crlf */
#else
	0,		/* crlf */
#endif
	0,		/* Highlight */
	NULL,		/* Syntax name */
	NULL,		/* Syntax */
	NULL,		/* Name of character set */
	NULL,		/* Character set */
	NULL,		/* Language */
	0,		/* Smart home key */
	0,		/* Goto indent first */
	0,		/* Smart backspace key */
	0,		/* Purify indentation */
	0,		/* Picture mode */
	0,		/* single_quoted */
	0,		/* no_double_quoted */
	0,		/* c_comment */
	0,		/* cpp_comment */
	0,		/* pound_comment */
	0,		/* vhdl_comment */
	0,		/* semi_comment */
	0,		/* tex_comment */
	0,		/* hex */
	NULL,		/* text_delimiters */
	NULL,		/* Characters which can indent paragraphs */
	NULL,		/* macro to execute for new files */
	NULL,		/* macro to execute for existing files */
	NULL,		/* macro to execute before saving new files */
	NULL,		/* macro to execute before saving existing files */
	NULL		/* macro to execute on first change */
};

/* Default options for file windows */

OPTIONS fdefault = {
	NULL,		/* *next */
	NULL,		/* *name_regex */
	NULL,		/* *contents_regex */
	0,		/* overtype */
	0,		/* lmargin */
	76,		/* rmargin */
	0,		/* autoindent */
	0,		/* wordwrap */
	0,		/* nobackup */
	8,		/* tab */
	' ',		/* indent char */
	1,		/* indent step */
	USTR "main",		/* *context */
	USTR "\\i%n %m %M",	/* *lmsg */
	USTR " %S Ctrl-K H for help",	/* *rmsg */
	0,		/* line numbers */
	0,		/* read only */
	0,		/* french spacing */
	0,		/* flowed text */
	0,		/* spaces */
#ifdef __MSDOS__
	1,		/* crlf */
#else
	0,		/* crlf */
#endif
	0,		/* Highlight */
	NULL,		/* Syntax name */
	NULL,		/* Syntax */
	NULL,		/* Name of character set */
	NULL,		/* Character set */
	NULL,		/* Language */
	0,		/* Smart home key */
	0,		/* Goto indent first */
	0,		/* Smart backspace key */
	0,		/* Purity indentation */
	0,		/* Picture mode */
	0,		/* single_quoted */
	0,		/* no_double_quoted */
	0,		/* c_comment */
	0,		/* cpp_comment */
	0,		/* pound_comment */
	0,		/* vhdl_comment */
	0,		/* semi_comment */
	0,		/* tex_comment */
	0,		/* hex */
	NULL,		/* text_delimiters */
	USTR ">;!#%/*-",	/* Characters which can indent paragraphs */
	NULL, NULL, NULL, NULL, NULL	/* macros (see above) */
};

/* Update options */

void lazy_opts(B *b, OPTIONS *o)
{
	o->syntax = load_syntax(o->syntax_name);
	if (!o->map_name) {
		/* Guess encoding if it's not explicitly given */
		unsigned char buf[1024];
		int len = sizeof(buf) - 1;
		if (b->eof->byte < len)
			len = b->eof->byte;
		brmem(b->bof, buf, len);
		buf[len] = '\0';
		o->charmap = guess_map(buf, len);
		o->map_name = zdup(o->charmap->name);
	} else {
		o->charmap = find_charmap(o->map_name);
	}
	if (!o->charmap)
		o->charmap = locale_map;
	if (!o->language)
		o->language = zdup(locale_msgs);
	/* Hex not allowed with UTF-8 */
	if (o->hex && o->charmap->type) {
		o->charmap = find_charmap(USTR "c");
		o->hex = 2;
	}
}

/* Set local options depending on file name and contents */

void setopt(B *b, unsigned char *parsed_name)
{
	OPTIONS *o;
	int x;
	unsigned char *pieces[26];
	for (x = 0; x!=26; ++x)
		pieces[x] = NULL;

	for (o = options; o; o = o->next)
		if (rmatch(o->name_regex, parsed_name)) {
			if(o->contents_regex) {
				P *p = pdup(b->bof, USTR "setopt");
				if (pmatch(pieces,o->contents_regex,zlen(o->contents_regex),p,0,0)) {
					prm(p);
					b->o = *o;
					lazy_opts(b, &b->o);
					goto done;
				} else {
					prm(p);
				}
			} else {
				b->o = *o;
				lazy_opts(b, &b->o);
				goto done;
			}
		}

	b->o = fdefault;
	lazy_opts(b, &b->o);

	done:
	for (x = 0; x!=26; ++x)
		vsrm(pieces[x]);
}

/* Table of options and how to set them */

/* local means it's in an OPTION structure, global means it's in a global
 * variable */

struct glopts {
	unsigned char *name;		/* Option name */
	int type;		/*      0 for global option flag
				   1 for global option numeric
				   2 for global option string
				   4 for local option flag
				   5 for local option numeric
				   6 for local option string
				   7 for local option numeric+1, with range checking
				 */
	void *set;		/* Address of global option */
	unsigned char *addr;		/* Local options structure member address */
	unsigned char *yes;		/* Message if option was turned on, or prompt string */
	unsigned char *no;		/* Message if option was turned off */
	unsigned char *menu;		/* Menu string */
	int ofst;		/* Local options structure member offset */
	int low;		/* Low limit for numeric options */
	int high;		/* High limit for numeric options */
} glopts[] = {
	{USTR "overwrite",4, NULL, (unsigned char *) &fdefault.overtype, USTR _("Overtype mode"), USTR _("Insert mode"), USTR _("T Overtype "), 0, 0, 0 },
	{USTR "hex",4, NULL, (unsigned char *) &fdefault.hex, USTR _("Hex edit mode"), USTR _("Text edit mode"), USTR _("  Hex edit mode "), 0, 0, 0 },
	{USTR "autoindent",	4, NULL, (unsigned char *) &fdefault.autoindent, USTR _("Autoindent enabled"), USTR _("Autoindent disabled"), USTR _("I Autoindent "), 0, 0, 0 },
	{USTR "wordwrap",	4, NULL, (unsigned char *) &fdefault.wordwrap, USTR _("Wordwrap enabled"), USTR _("Wordwrap disabled"), USTR _("W Word wrap "), 0, 0, 0 },
	{USTR "tab",	5, NULL, (unsigned char *) &fdefault.tab, USTR _("Tab width (%d): "), 0, USTR _("D Tab width "), 0, 1, 64 },
	{USTR "lmargin",	7, NULL, (unsigned char *) &fdefault.lmargin, USTR _("Left margin (%d): "), 0, USTR _("L Left margin "), 0, 0, 63 },
	{USTR "rmargin",	7, NULL, (unsigned char *) &fdefault.rmargin, USTR _("Right margin (%d): "), 0, USTR _("R Right margin "), 0, 7, 255 },
	{USTR "restore",	0, &restore_file_pos, NULL, USTR _("Restore cursor position when files loaded"), USTR _("Don't restore cursor when files loaded"), USTR _("  Restore cursor "), 0, 0, 0 },
	{USTR "square",	0, &square, NULL, USTR _("Rectangle mode"), USTR _("Text-stream mode"), USTR _("X Rectangle mode "), 0, 0, 0 },
	{USTR "icase",	0, &icase, NULL, USTR _("Search ignores case by default"), USTR _("Case sensitive search by default"), USTR _("  Case insensitivity "), 0, 0, 0 },
	{USTR "wrap",	0, &wrap, NULL, USTR _("Search wraps"), USTR _("Search doesn't wrap"), USTR _("  Search wraps "), 0, 0, 0 },
	{USTR "menu_explorer",	0, &menu_explorer, NULL, USTR _("Menu explorer mode"), USTR _("Simple completion mode"), USTR _("  Menu explorer "), 0, 0, 0 },
	{USTR "menu_above",	0, &menu_above, NULL, USTR _("Menu above prompt"), USTR _("Menu below prompt"), USTR _("  Menu position "), 0, 0, 0 },
	{USTR "search_prompting",	0, &pico, NULL, USTR _("Search prompting on"), USTR _("Search prompting off"), USTR _("  Search prompting "), 0, 0, 0 },
	{USTR "menu_jump",	0, &menu_jump, NULL, USTR _("Jump into menu is on"), USTR _("Jump into menu is off"), USTR _("  Jump into menu "), 0, 0, 0 },
	{USTR "autoswap",	0, &autoswap, NULL, USTR _("Autoswap ^KB and ^KK"), USTR _("Autoswap off "), USTR _("  Autoswap mode "), 0, 0, 0 },
	{USTR "indentc",	5, NULL, (unsigned char *) &fdefault.indentc, USTR _("Indent char %d (SPACE=32, TAB=9, ^C to abort): "), 0, USTR _("  Indent char "), 0, 0, 255 },
	{USTR "istep",	5, NULL, (unsigned char *) &fdefault.istep, USTR _("Indent step %d (^C to abort): "), 0, USTR _("  Indent step "), 0, 1, 64 },
	{USTR "french",	4, NULL, (unsigned char *) &fdefault.french, USTR _("One space after periods for paragraph reformat"), USTR _("Two spaces after periods for paragraph reformat"), USTR _("  French spacing "), 0, 0, 0 },
	{USTR "flowed",	4, NULL, (unsigned char *) &fdefault.flowed, USTR _("One space after paragraph line"), USTR _("No spaces after paragraph lines"), USTR _("  Flowed text "), 0, 0, 0 },
	{USTR "highlight",	4, NULL, (unsigned char *) &fdefault.highlight, USTR _("Highlighting enabled"), USTR _("Highlighting disabled"), USTR _("H Highlighting "), 0, 0, 0 },
	{USTR "spaces",	4, NULL, (unsigned char *) &fdefault.spaces, USTR _("Inserting spaces when tab key is hit"), USTR _("Inserting tabs when tab key is hit"), USTR _("  No tabs "), 0, 0, 0 },
	{USTR "mid",	0, &mid, NULL, USTR _("Cursor will be recentered on scrolls"), USTR _("Cursor will not be recentered on scroll"), USTR _("C Center on scroll "), 0, 0, 0 },
	{USTR "guess_crlf",0, &guesscrlf, NULL, USTR _("Automatically detect MS-DOS files"), USTR _("Do not automatically detect MS-DOS files"), USTR _("  Auto detect CR-LF "), 0, 0, 0 },
	{USTR "guess_indent",0, &guessindent, NULL, USTR _("Automatically detect indentation"), USTR _("Do not automatically detect indentation"), USTR _("  Guess indent "), 0, 0, 0 },
	{USTR "guess_non_utf8",0, &guess_non_utf8, NULL, USTR _("Automatically detect file charset in non-UTF-8 locale"), USTR _("Do not automatically detect file charset in non-UTF-8 locale"), USTR _("  Guess in non-UTF-8 "), 0, 0, 0 },
	{USTR "guess_utf8",0, &guess_utf8, NULL, USTR _("Automatically detect file charset in UTF-8 locale"), USTR _("Do not automatically detect file charset in UTF-8 locale"), USTR _("  Guess in UTF-8 "), 0, 0, 0 },
	{USTR "transpose",0, &transpose, NULL, USTR _("Menu is transposed"), USTR _("Menus are not transposed"), USTR _("  Transpose menus "), 0, 0, 0 },
	{USTR "crlf",	4, NULL, (unsigned char *) &fdefault.crlf, USTR _("CR-LF is line terminator"), USTR _("LF is line terminator"), USTR _("Z CR-LF (MS-DOS) "), 0, 0, 0 },
	{USTR "linums",	4, NULL, (unsigned char *) &fdefault.linums, USTR _("Line numbers enabled"), USTR _("Line numbers disabled"), USTR _("N Line numbers "), 0, 0, 0 },
	{USTR "marking",	0, &marking, NULL, USTR _("Anchored block marking on"), USTR _("Anchored block marking off"), USTR _("  Marking "), 0, 0, 0 },
	{USTR "asis",	0, &dspasis, NULL, USTR _("Characters above 127 shown as-is"), USTR _("Characters above 127 shown in inverse"), USTR _("  Meta chars as-is "), 0, 0, 0 },
	{USTR "force",	0, &force, NULL, USTR _("Last line forced to have NL when file saved"), USTR _("Last line not forced to have NL"), USTR _("  Force last NL "), 0, 0, 0 },
	{USTR "joe_state",0, &joe_state, NULL, USTR _("~/.joe_state file will be updated"), USTR _("~/.joe_state file will not be updated"), USTR _("  Joe_state file "), 0, 0, 0 },
	{USTR "nobackup",	4, NULL, (unsigned char *) &fdefault.nobackup, USTR _("Nobackup enabled"), USTR _("Nobackup disabled"), USTR _("  No backup "), 0, 0, 0 },
	{USTR "nobackups",	0, &nobackups, NULL, USTR _("Backup files will not be made"), USTR _("Backup files will be made"), USTR _("  Disable backups "), 0, 0, 0 },
	{USTR "nolocks",	0, &nolocks, NULL, USTR _("Files will not be locked"), USTR _("Files will be locked"), USTR _("  Disable locks "), 0, 0, 0 },
	{USTR "nomodcheck",	0, &nomodcheck, NULL, USTR _("No file modification time check"), USTR _("File modification time checking enabled"), USTR _("  Disable mtime check "), 0, 0, 0 },
	{USTR "nocurdir",	0, &nocurdir, NULL, USTR _("No current dir"), USTR _("Current dir enabled"), USTR _("  Disable current dir "), 0, 0, 0 },
	{USTR "break_hardlinks",	0, &break_links, NULL, USTR _("Hardlinks will be broken"), USTR _("Hardlinks not broken"), USTR _("  Break hard links "), 0, 0, 0 },
	{USTR "break_links",	0, &break_symlinks, NULL, USTR _("Links will be broken"), USTR _("Links not broken"), USTR _("  Break links "), 0, 0, 0 },
	{USTR "lightoff",	0, &lightoff, NULL, USTR _("Highlighting turned off after block operations"), USTR _("Highlighting not turned off after block operations"), USTR _("  Auto unmark "), 0, 0, 0 },
	{USTR "exask",	0, &exask, NULL, USTR _("Prompt for filename in save & exit command"), USTR _("Don't prompt for filename in save & exit command"), USTR _("  Exit ask "), 0, 0, 0 },
	{USTR "beep",	0, &joe_beep, NULL, USTR _("Warning bell enabled"), USTR _("Warning bell disabled"), USTR _("B Beeps "), 0, 0, 0 },
	{USTR "nosta",	0, &staen, NULL, USTR _("Top-most status line disabled"), USTR _("Top-most status line enabled"), USTR _("  Disable status line "), 0, 0, 0 },
	{USTR "keepup",	0, &keepup, NULL, USTR _("Status line updated constantly"), USTR _("Status line updated once/sec"), USTR _("  Fast status line "), 0, 0, 0 },
	{USTR "pg",		1, &pgamnt, NULL, USTR _("Lines to keep for PgUp/PgDn or -1 for 1/2 window (%d): "), 0, USTR _("  No. PgUp/PgDn lines "), 0, -1, 64 },
	{USTR "undo_keep",		1, &undo_keep, NULL, USTR _("No. undo records to keep, or (0 for infinite): "), 0, USTR _("  No. undo records "), 0, -1, 64 },
	{USTR "csmode",	0, &csmode, NULL, USTR _("Start search after a search repeats previous search"), USTR _("Start search always starts a new search"), USTR _("  Continued search "), 0, 0, 0 },
	{USTR "rdonly",	4, NULL, (unsigned char *) &fdefault.readonly, USTR _("Read only"), USTR _("Full editing"), USTR _("O Read only "), 0, 0, 0 },
	{USTR "smarthome",	4, NULL, (unsigned char *) &fdefault.smarthome, USTR _("Smart home key enabled"), USTR _("Smart home key disabled"), USTR _("  Smart home key "), 0, 0, 0 },
	{USTR "indentfirst",	4, NULL, (unsigned char *) &fdefault.indentfirst, USTR _("Smart home goes to indentation first"), USTR _("Smart home goes home first"), USTR _("  To indent first "), 0, 0, 0 },
	{USTR "smartbacks",	4, NULL, (unsigned char *) &fdefault.smartbacks, USTR _("Smart backspace key enabled"), USTR _("Smart backspace key disabled"), USTR _("  Smart backspace "), 0, 0, 0 },
	{USTR "purify",	4, NULL, (unsigned char *) &fdefault.purify, USTR _("Indentation clean up enabled"), USTR _("Indentation clean up disabled"), USTR _("  Clean up indents "), 0, 0, 0 },
	{USTR "picture",	4, NULL, (unsigned char *) &fdefault.picture, USTR _("Picture drawing mode enabled"), USTR _("Picture drawing mode disabled"), USTR _("P Picture mode "), 0, 0, 0 },
	{USTR "backpath",	2, &backpath, NULL, USTR _("Backup files stored in (%s): "), 0, USTR _("  Path to backup files "), 0, 0, 0 },
	{USTR "syntax",	9, NULL, NULL, USTR _("Select syntax (^C to abort): "), 0, USTR _("Y Syntax"), 0, 0, 0 },
	{USTR "encoding",13, NULL, NULL, USTR _("Select file character set (now %s, ^C to abort): "), 0, USTR _("E Encoding "), 0, 0, 0 },
	{USTR "single_quoted",	4, NULL, (unsigned char *) &fdefault.single_quoted, USTR _("Single quoting enabled"), USTR _("Single quoting disabled"), USTR _("  ^G ignores '... ' "), 0, 0, 0 },
	{USTR "no_double_quoted",4, NULL, (unsigned char *) &fdefault.no_double_quoted, USTR _("Double quoting disabled"), USTR _("Double quoting enabled"), USTR _("  ^G ignores \"... \" "), 0, 0, 0 },
	{USTR "c_comment",	4, NULL, (unsigned char *) &fdefault.c_comment, USTR _("/* comments enabled"), USTR _("/* comments disabled"), USTR _("  ^G ignores /*...*/ "), 0, 0, 0 },
	{USTR "cpp_comment",	4, NULL, (unsigned char *) &fdefault.cpp_comment, USTR _("// comments enabled"), USTR _("// comments disabled"), USTR _("  ^G ignores //... "), 0, 0, 0 },
	{USTR "pound_comment",	4, NULL, (unsigned char *) &fdefault.pound_comment, USTR _("# comments enabled"), USTR _("# comments disabled"), USTR _("  ^G ignores #... "), 0, 0, 0 },
	{USTR "vhdl_comment",	4, NULL, (unsigned char *) &fdefault.vhdl_comment, USTR _("-- comments enabled"), USTR _("-- comments disabled"), USTR _("  ^G ignores --... "), 0, 0, 0 },
	{USTR "semi_comment",	4, NULL, (unsigned char *) &fdefault.semi_comment, USTR _("; comments enabled"), USTR _("; comments disabled"), USTR _("  ^G ignores ;... "), 0, 0, 0 },
	{USTR "tex_comment",	4, NULL, (unsigned char *) &fdefault.tex_comment, USTR _("% comments enabled"), USTR _("% comments disabled"), USTR _("  ^G ignores %... "), 0, 0, 0 },
	{USTR "text_delimiters",	6, NULL, (unsigned char *) &fdefault.text_delimiters, USTR _("Text delimiters (%s): "), 0, USTR _("  Text delimiters "), 0, 0, 0 },
	{USTR "language",	6, NULL, (unsigned char *) &fdefault.language, USTR _("Language (%s): "), 0, USTR _("V Language "), 0, 0, 0 },
	{USTR "cpara",		6, NULL, (unsigned char *) &fdefault.cpara, USTR _("Characters which can indent paragraphs (%s): "), 0, USTR _("  Paragraph indent chars "), 0, 0, 0 },
	{USTR "floatmouse",	0, &floatmouse, 0, USTR _("Clicking can move the cursor past end of line"), USTR _("Clicking past end of line moves cursor to the end"), USTR _("  Click past end "), 0, 0, 0 },
	{USTR "rtbutton",	0, &rtbutton, 0, USTR _("Mouse action is done with the right button"), USTR _("Mouse action is done with the left button"), USTR _("  Right button "), 0, 0, 0 },
	{USTR "nonotice",	0, &nonotice, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "help_is_utf8",	0, &help_is_utf8, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "noxon",	0, &noxon, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "orphan",	0, &orphan, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "help",	0, &help, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "dopadding",	0, &dopadding, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "lines",	1, &env_lines, NULL, 0, 0, 0, 0, 2, 1024 },
	{USTR "baud",	1, &Baud, NULL, 0, 0, 0, 0, 50, 32767 },
	{USTR "columns",	1, &env_columns, NULL, 0, 0, 0, 0, 2, 1024 },
	{USTR "skiptop",	1, &skiptop, NULL, 0, 0, 0, 0, 0, 64 },
	{USTR "notite",	0, &notite, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "mouse",	0, &xmouse, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "usetabs",	0, &usetabs, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "assume_color", 0, &assume_color, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "assume_256color", 0, &assume_256color, NULL, 0, 0, 0, 0, 0, 0 },
	{USTR "joexterm", 0, &joexterm, NULL, 0, 0, 0, 0, 0, 0 },
	{ NULL,		0, NULL, NULL, NULL, NULL, NULL, 0, 0, 0 },
};

/* Initialize .ofsts above.  Is this really necessary? */

int isiz = 0;
HASH *opt_tab;

static void izopts(void)
{
	int x;

	opt_tab = htmk(128);

	for (x = 0; glopts[x].name; ++x) {
		htadd(opt_tab, glopts[x].name, glopts + x);
		switch (glopts[x].type) {
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
			glopts[x].ofst = glopts[x].addr - (unsigned char *) &fdefault;
		}
	}
	isiz = 1;
}

/* Set a global or local option:
 * 's' is option name
 * 'arg' is a possible argument string (taken only if option has an arg)
 * 'options' points to options structure to modify (can be NULL).
 * 'set'==0: set only in 'options' if it's given.
 * 'set'!=0: set global variable option.
 * return value: no. of fields taken (1 or 2), or 0 if option not found.
 *
 * So this function is used both to set options, and to parse over options
 * without setting them.
 *
 * These combinations are used:
 *
 * glopt(name,arg,NULL,1): set global variable option
 * glopt(name,arg,NULL,0): parse over option
 * glopt(name,arg,options,0): set file local option
 * glopt(name,arg,&fdefault,1): set default file options
 * glopt(name,arg,options,1): set file local option
 */

int glopt(unsigned char *s, unsigned char *arg, OPTIONS *options, int set)
{
	int val;
	int ret = 0;
	int st = 1;	/* 1 to set option, 0 to clear it */
	struct glopts *opt;

	/* Initialize offsets */
	if (!isiz)
		izopts();

	/* Clear instead of set? */
	if (s[0] == '-') {
		st = 0;
		++s;
	}

	opt = htfind(opt_tab, s);

	if (opt) {
		switch (opt->type) {
		case 0: /* Global variable flag option */
			if (set)
				*(int *)opt->set = st;
			break;
		case 1: /* Global variable integer option */
			if (set && arg) {
				sscanf((char *)arg, "%d", &val);
				if (val >= opt->low && val <= opt->high)
					*(int *)opt->set = val;
			}
			break;
		case 2: /* Global variable string option */
			if (set) {
				if (arg)
					*(unsigned char **) opt->set = zdup(arg);
				else
					*(unsigned char **) opt->set = 0;
			}
			break;
		case 4: /* Local option flag */
			if (options)
				*(int *) ((unsigned char *) options + opt->ofst) = st;
			break;
		case 5: /* Local option integer */
			if (arg) {
				if (options) {
					sscanf((char *)arg, "%d", &val);
					if (val >= opt->low && val <= opt->high)
						*(int *) ((unsigned char *)
							  options + opt->ofst) = val;
				} 
			}
			break;
		case 6: /* Local string option */
			if (options) {
				if (arg) {
					*(unsigned char **) ((unsigned char *)
							  options + opt->ofst) = zdup(arg);
				} else {
					*(unsigned char **) ((unsigned char *)
							  options + opt->ofst) = 0;
				}
			}
			break;
		case 7: /* Local option numeric + 1, with range checking */
			if (arg) {
				int zz = 0;

				sscanf((char *)arg, "%d", &zz);
				if (zz >= opt->low && zz <= opt->high) {
					--zz;
					if (options)
						*(int *) ((unsigned char *)
							  options + opt->ofst) = zz;
				}
			}
			break;

		case 9: /* Set syntax */
			if (arg && options)
				options->syntax_name = zdup(arg);
			/* this was causing all syntax files to be loaded...
			if (arg && options)
				options->syntax = load_syntax(arg); */
			break;

		case 13: /* Set byte mode encoding */
			if (arg && options)
				options->map_name = zdup(arg);
			break;
		}
		/* This is a stupid hack... */
		if ((opt->type & 3) == 0 || !arg)
			return 1;
		else
			return 2;
	} else {
		/* Why no case 6, string option? */
		/* Keymap, mold, mnew, etc. are not strings */
		/* These options do not show up in ^T */
		if (!zcmp(s, USTR "lmsg")) {
			if (arg) {
				if (options)
					options->lmsg = zdup(arg);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "rmsg")) {
			if (arg) {
				if (options)
					options->rmsg = zdup(arg);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "keymap")) {
			if (arg) {
				int y;

				for (y = 0; !joe_isspace(locale_map,arg[y]); ++y) ;
				if (!arg[y])
					arg[y] = 0;
				if (options && y)
					options->context = zdup(arg);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "mnew")) {
			if (arg) {
				int sta;

				if (options)
					options->mnew = mparse(NULL, arg, &sta);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "mfirst")) {
			if (arg) {
				int sta;

				if (options)
					options->mfirst = mparse(NULL, arg, &sta);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "mold")) {
			if (arg) {
				int sta;

				if (options)
					options->mold = mparse(NULL, arg, &sta);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "msnew")) {
			if (arg) {
				int sta;

				if (options)
					options->msnew = mparse(NULL, arg, &sta);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "msold")) {
			if (arg) {
				int sta;

				if (options)
					options->msold = mparse(NULL, arg, &sta);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "text_color")) {
			if (arg) {
				bg_text = meta_color(arg);
				bg_help = bg_text;
				bg_prompt = bg_text;
				bg_menu = bg_text;
				bg_msg = bg_text;
				bg_stalin = bg_text;
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "help_color")) {
			if (arg) {
				bg_help = meta_color(arg);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "status_color")) {
			if (arg) {
				bg_stalin = meta_color(arg);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "menu_color")) {
			if (arg) {
				bg_menu = meta_color(arg);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "prompt_color")) {
			if (arg) {
				bg_prompt = meta_color(arg);
				ret = 2;
			} else
				ret = 1;
		} else if (!zcmp(s, USTR "msg_color")) {
			if (arg) {
				bg_msg = meta_color(arg);
				ret = 2;
			} else
				ret = 1;
		}
	}

	return ret;
}

/* Option setting user interface (^T command) */

static int doabrt1(BW *bw, int *xx)
{
	joe_free(xx);
	return -1;
}

static int doopt1(BW *bw, unsigned char *s, int *xx, int *notify)
{
	int ret = 0;
	int x = *xx;
	int v;

	joe_free(xx);
	switch (glopts[x].type) {
	case 1:
		v = calc(bw, s);
		if (merr) {
			msgnw(bw->parent, merr);
			ret = -1;
		} else if (v >= glopts[x].low && v <= glopts[x].high)
			*(int *)glopts[x].set = v;
		else {
			msgnw(bw->parent, joe_gettext(_("Value out of range")));
			ret = -1;
		}
		break;
	case 2:
		if (s[0])
			*(unsigned char **) glopts[x].set = zdup(s);
		break;
	case 6:
		*(unsigned char **)((unsigned char *)&bw->o+glopts[x].ofst) = zdup(s);
		break;
	case 5:
		v = calc(bw, s);
		if (merr) {
			msgnw(bw->parent, merr);
			ret = -1;
		} else if (v >= glopts[x].low && v <= glopts[x].high)
			*(int *) ((unsigned char *) &bw->o + glopts[x].ofst) = v;
		else {
			msgnw(bw->parent, joe_gettext(_("Value out of range")));
			ret = -1;
		}
		break;
	case 7:
		v = calc(bw, s) - 1.0;
		if (merr) {
			msgnw(bw->parent, merr);
			ret = -1;
		} else if (v >= glopts[x].low && v <= glopts[x].high)
			*(int *) ((unsigned char *) &bw->o + glopts[x].ofst) = v;
		else {
			msgnw(bw->parent, joe_gettext(_("Value out of range")));
			ret = -1;
		}
		break;
	}
	vsrm(s);
	bw->b->o = bw->o;
	wfit(bw->parent->t);
	updall();
	if (notify)
		*notify = 1;
	return ret;
}

static int dosyntax(BW *bw, unsigned char *s, int *xx, int *notify)
{
	int ret = 0;
	struct high_syntax *syn;

	syn = load_syntax(s);

	if (syn)
		bw->o.syntax = syn;
	else
		msgnw(bw->parent, joe_gettext(_("Syntax definition file not found")));

	vsrm(s);
	bw->b->o = bw->o;
	updall();
	if (notify)
		*notify = 1;
	return ret;
}

unsigned char **syntaxes = NULL; /* Array of available syntaxes */

static int syntaxcmplt(BW *bw)
{
	if (!syntaxes) {
		unsigned char *oldpwd = pwd();
		unsigned char **t;
		unsigned char *p;
		int x, y;

		if (!read_sys_configs || chpwd(USTR (JOEDATA "syntax")))
			goto skip_joerc;
		t = rexpnd(USTR "*.jsf");
		if (!t) {
			chpwd(oldpwd);
			return -1;
		}
		if (!aLEN(t)) {
			varm(t);
			chpwd(oldpwd);
			return -1;
		}

		for (x = 0; x != aLEN(t); ++x) {
			unsigned char *r = vsncpy(NULL,0,t[x],(unsigned char *)strrchr((char *)(t[x]),'.')-t[x]);
			syntaxes = vaadd(syntaxes,r);
		}
		varm(t);
		skip_joerc:

		p = read_user_configs ? (unsigned char *)getenv("HOME") : NULL;
		if (p) {
			unsigned char buf[1024];
			joe_snprintf_1(buf,sizeof(buf),"%s/.joe-p37/syntax",p);
			if (!chpwd(buf) && (t = rexpnd(USTR "*.jsf"))) {
				for (x = 0; x != aLEN(t); ++x)
					*strrchr((char *)t[x],'.') = 0;
				for (x = 0; x != aLEN(t); ++x) {
					for (y = 0; y != aLEN(syntaxes); ++y)
						if (!zcmp(t[x],syntaxes[y]))
							break;
					if (y == aLEN(syntaxes)) {
						unsigned char *r = vsncpy(NULL,0,sv(t[x]));
						syntaxes = vaadd(syntaxes,r);
					}
				}
				varm(t);
			}
		}

		vasort(av(syntaxes));
		chpwd(oldpwd);
	}
	return simple_cmplt(bw,syntaxes);
}

/* Return true iff there is a hex window with this bw */
int check_for_hex(BW *bw)
{
	W *w;
	if (bw->o.hex)
		return 1;
	for (w = bw->parent->link.next; w != bw->parent; w = w->link.next)
		if ((w->watom == &watomtw || w->watom == &watompw) && ((BW *)w->object)->b == bw->b &&
		    ((BW *)w->object)->o.hex)
		    	return 1;
	return 0;
}

static int doencoding(BW *bw, unsigned char *s, int *xx, int *notify)
{
	int ret = 0;
	struct charmap *map;


	map = find_charmap(s);

	if (map && map->type && check_for_hex(bw)) {
		msgnw(bw->parent, joe_gettext(_("UTF-8 encoding not allowed with hexadecimal windows")));
		if (notify)
			*notify = 1;
		return -1;
	}

	if (map) {
		bw->o.charmap = map;
		bw->o.map_name = map->name;
		if (!xx) {
			joe_snprintf_1(msgbuf, JOE_MSGBUFSIZE, joe_gettext(_("%s encoding assumed for this file")), map->name);
			msgnw(bw->parent, msgbuf);
		}
	} else
		msgnw(bw->parent, joe_gettext(_("Character set not found")));

	vsrm(s);
	bw->b->o = bw->o;
	updall();
	if (notify)
		*notify = 1;
	return ret;
}

unsigned char **encodings = NULL; /* Array of available encodinges */

static int encodingcmplt(BW *bw)
{
	if (!encodings) {
		encodings = get_encodings();
		vasort(av(encodings));
	}
	return simple_cmplt(bw,encodings);
}

/* Menus of macros */

struct rc_menu_entry {
	MACRO *m;
	unsigned char *name;
};

struct rc_menu {
	struct rc_menu *next;	/* Next one in list */
	unsigned char *name;	/* Name of this menu */
	int last_position;	/* Last cursor position */
	int size;		/* Number of entries */
	struct rc_menu_entry **entries;
} *menus;

struct menu_instance {
	struct rc_menu *menu;
	unsigned char **s;
};

int find_option(unsigned char *s)
{
	int y;
	for (y = 0; glopts[y].name; ++y)
		if (!zcmp(glopts[y].name, s))
			return y;
	return -1;
}

struct rc_menu *find_menu(unsigned char *s)
{
	struct rc_menu *m;
	for (m = menus; m; m = m->next)
		if (!zcmp(m->name, s))
			break;
	return m;
}

struct rc_menu *create_menu(unsigned char *name)
{
	struct rc_menu *menu = find_menu(name);
	if (menu)
		return menu;
	menu = (struct rc_menu *)joe_malloc(sizeof(struct rc_menu));
	menu->name = zdup(name);
	menu->next = menus;
	menus = menu;
	menu->last_position = 0;
	menu->size = 0;
	menu->entries = 0;
	return menu;
}

void add_menu_entry(struct rc_menu *menu, unsigned char *entry_name, MACRO *m)
{
	struct rc_menu_entry *e = (struct rc_menu_entry *)joe_malloc(sizeof(struct rc_menu_entry));
	e->m = m;
	e->name = zdup(entry_name);
	++menu->size;
	if (!menu->entries) {
		menu->entries = (struct rc_menu_entry **)joe_malloc(menu->size * sizeof(struct rc_menu_entry *));
	} else {
		menu->entries = (struct rc_menu_entry **)joe_realloc(menu->entries, menu->size * sizeof(struct rc_menu_entry *));
	}
	menu->entries[menu->size - 1] = e;
}

static int olddoopt(BW *bw, int y, int flg, int *notify)
{
	int *xx, *optp, oldval;
	unsigned char buf[OPT_BUF_SIZE];

	if (y >= 0) {
		switch (glopts[y].type) {
		case 0:
			if (!flg)
				*(int *)glopts[y].set = !*(int *)glopts[y].set;
			else if (flg == 1)
				*(int *)glopts[y].set = 1;
			else
				*(int *)glopts[y].set = 0;
			msgnw(bw->parent, *(int *)glopts[y].set ? joe_gettext(glopts[y].yes) : joe_gettext(glopts[y].no));
			break;
		case 4:
			optp = (int *) ((unsigned char *) &bw->o + glopts[y].ofst);
			oldval = *optp;
			*optp = !flg ? !*optp : (flg == 1);
			msgnw(bw->parent, joe_gettext(*optp ? glopts[y].yes : glopts[y].no));
			if (glopts[y].ofst == (unsigned char *) &fdefault.readonly - (unsigned char *) &fdefault)
				bw->b->rdonly = bw->o.readonly;
			/* Kill UTF-8 mode if we switch to hex display */
			if (glopts[y].ofst == (unsigned char *) &fdefault.hex - (unsigned char *) &fdefault) {
				if (bw->o.hex && bw->b->o.charmap->type) {
					/* Quietly cancel UTF-8 mode if we switch to hex display */
			        	bw ->o.hex = 2;
					doencoding(bw, vsncpy(NULL, 0, sc("C")), 1+(int*)0, NULL);
				} else if (oldval == 2 && !bw->o.hex && !bw->b->o.charmap->type) {
					/* Quietly restore UTF-8 mode after leaving hex display */
					doencoding(bw, vsncpy(NULL, 0, sc("UTF-8")), 1+(int*)0, NULL);
				}
			}
			break;
		case 6:
			xx = (int *) joe_malloc(sizeof(int));
			*xx = y;
			if(*(unsigned char **)((unsigned char *)&bw->o+glopts[y].ofst))
				joe_snprintf_1(buf, OPT_BUF_SIZE, glopts[y].yes,*(unsigned char **)((unsigned char *)&bw->o+glopts[y].ofst));
			else
				joe_snprintf_1(buf, OPT_BUF_SIZE, glopts[y].yes,"");
			if(wmkpw(bw->parent, buf, NULL, doopt1, NULL, doabrt1, utypebw, xx, notify, locale_map, 0))
				return 0;
			else
				return -1;
			/* break; warns on some systems */
		case 1:
			joe_snprintf_1(buf, OPT_BUF_SIZE, joe_gettext(glopts[y].yes), *(int *)glopts[y].set);
			xx = (int *) joe_malloc(sizeof(int));

			*xx = y;
			if (wmkpw(bw->parent, buf, NULL, doopt1, NULL, doabrt1, utypebw, xx, notify, locale_map, 0))
				return 0;
			else
				return -1;
		case 2:
			if (*(unsigned char **) glopts[y].set)
				joe_snprintf_1(buf, OPT_BUF_SIZE, joe_gettext(glopts[y].yes), *(unsigned char **) glopts[y].set);
			else
				joe_snprintf_1(buf, OPT_BUF_SIZE, joe_gettext(glopts[y].yes), "");
			xx = (int *) joe_malloc(sizeof(int));

			*xx = y;
			if (wmkpw(bw->parent, buf, NULL, doopt1, NULL, doabrt1, utypebw, xx, notify, locale_map, 0))
				return 0;
			else
				return -1;
		case 5:
			joe_snprintf_1(buf, OPT_BUF_SIZE, joe_gettext(glopts[y].yes), *(int *) ((unsigned char *) &bw->o + glopts[y].ofst));
			goto in;
		case 7:
			joe_snprintf_1(buf, OPT_BUF_SIZE, joe_gettext(glopts[y].yes), *(int *) ((unsigned char *) &bw->o + glopts[y].ofst) + 1);
		      in:xx = (int *) joe_malloc(sizeof(int));

			*xx = y;
			if (wmkpw(bw->parent, buf, NULL, doopt1, NULL, doabrt1, utypebw, xx, notify, locale_map, 0))
				return 0;
			else
				return -1;

		case 9:
			joe_snprintf_1(buf, OPT_BUF_SIZE, joe_gettext(glopts[y].yes), "");
			if (wmkpw(bw->parent, buf, NULL, dosyntax, NULL, NULL, syntaxcmplt, NULL, notify, locale_map, 0))
				return 0;
			else
				return -1;

		case 13:
			if (zchr(joe_gettext(glopts[y].yes), '%') != NULL) {
				joe_snprintf_1(buf, OPT_BUF_SIZE, joe_gettext(glopts[y].yes), get_status(bw, glopts[y].name));
			} else {
				joe_snprintf_1(buf, OPT_BUF_SIZE, "%s", joe_gettext(glopts[y].yes));
			}
			/* TODO(pts): Auto-insert the current encoding as default to the prompt response. */
			if (wmkpw(bw->parent, buf, NULL, doencoding, NULL, NULL, encodingcmplt, NULL, notify, locale_map, 0))
				return 0;
			else
				return -1;
		}
	}
	if (notify)
		*notify = 1;
	bw->b->o = bw->o;
	wfit(bw->parent->t);
	updall();
	return 0;
}

static int doabrt(MENU *m, int x, struct menu_instance *mi)
{
	mi->menu->last_position = x;
	for (x = 0; mi->s[x]; ++x)
		vsrm(mi->s[x]);
		/* joe_free(mi->s[x]); */
	joe_free(mi->s);
	joe_free(mi);
	return -1;
}

int menu_flg; /* Key used to select menu entry */

static int execmenu(MENU *m, int x, struct menu_instance *mi, int flg)
{
	struct rc_menu *menu = mi->menu;
	int *notify = m->parent->notify;
	if (notify)
		*notify = 1;
	wabort(m->parent);
	menu_flg = flg;
	return exmacro(menu->entries[x]->m, 1);
}

int display_menu(BW *bw, struct rc_menu *menu, int *notify)
{
	struct menu_instance *m = (struct menu_instance *)joe_malloc(sizeof(struct menu_instance));
	unsigned char **s = (unsigned char **)joe_malloc(sizeof(unsigned char *) * (menu->size + 1));
	int x;
	for (x = 0; x != menu->size; ++x) {
		/* s[x] = zdup(menu->entries[x]->name); */
		s[x] = stagen(NULL, bw, menu->entries[x]->name, ' ');


#if 0
		int y;
		if ((y = find_option(menu->entries[x])) >= 0) {
			s[x] = (unsigned char *) joe_malloc(OPT_BUF_SIZE);		/* FIXME: why 40 ??? */
			switch (glopts[y].type) {
				case 0:
					joe_snprintf_2((s[x]), OPT_BUF_SIZE, "%s%s", joe_gettext(glopts[y].menu), *(int *)glopts[y].set ? "ON" : "OFF");
					break;
				case 1:
					joe_snprintf_2((s[x]), OPT_BUF_SIZE, "%s%d", joe_gettext(glopts[y].menu), *(int *)glopts[y].set);
					break;
				case 2:
				case 9:
				case 13:
				case 6:
					zcpy(s[x], joe_gettext(glopts[y].menu));
					break;
				case 4:
					joe_snprintf_2((s[x]), OPT_BUF_SIZE, "%s%s", joe_gettext(glopts[y].menu), *(int *) ((unsigned char *) &bw->o + glopts[y].ofst) ? "ON" : "OFF");
					break;
				case 5:
					joe_snprintf_2((s[x]), OPT_BUF_SIZE, "%s%d", joe_gettext(glopts[y].menu), *(int *) ((unsigned char *) &bw->o + glopts[y].ofst));
					break;
				case 7:
					joe_snprintf_2((s[x]), OPT_BUF_SIZE, "%s%d", joe_gettext(glopts[y].menu), *(int *) ((unsigned char *) &bw->o + glopts[y].ofst) + 1);
					break;
			}
		} else {
			s[x] = zdup(menu->entries[x]);
		}
#endif
	}
	s[x] = 0;
	m->menu = menu;
	m->s = s;
	if (mkmenu(bw->parent, bw->parent, m->s, execmenu, doabrt, NULL, menu->last_position, m, notify))
		return 0;
	else
		return -1;
}

unsigned char *get_status(BW *bw, unsigned char *s)
{
	static unsigned char buf[OPT_BUF_SIZE];
	int y = find_option(s);
	if (y == -1)
		return USTR "???";
	else {
		switch (glopts[y].type) {
			case 0: {
				return *(int *)glopts[y].set ? USTR "ON" : USTR "OFF";
			} case 1: {
				joe_snprintf_1(buf, OPT_BUF_SIZE, "%d", *(int *)glopts[y].set);
				return buf;
			} case 4: {
				return *(int *) ((unsigned char *) &bw->o + glopts[y].ofst) ? USTR "ON" : USTR "OFF";
			} case 5: {
				joe_snprintf_1(buf, OPT_BUF_SIZE, "%d", *(int *) ((unsigned char *) &bw->o + glopts[y].ofst));
				return buf;
			} case 7: {
				joe_snprintf_1(buf, OPT_BUF_SIZE, "%d", *(int *) ((unsigned char *) &bw->o + glopts[y].ofst) + 1);
				return buf;
			} default: {
				if (0 == zcmp(s, USTR "encoding")) {
					return bw->o.map_name;
				}
				return USTR "";
			}
		}
	}
}

/* ^T command */

unsigned char **getmenus(void)
{
	unsigned char **s = vaensure(NULL, 20);
	struct rc_menu *m;

	for (m = menus; m; m = m->next)
		s = vaadd(s, vsncpy(NULL, 0, sz(m->name)));
	vasort(s, aLen(s));
	return s;
}

unsigned char **smenus = NULL;	/* Array of command names */

static int menucmplt(BW *bw)
{
	if (!smenus)
		smenus = getmenus();
	return simple_cmplt(bw,smenus);
}

static int domenu(BW *bw, unsigned char *s, void *object, int *notify)
{
	struct rc_menu *menu = find_menu(s);
	vsrm(s);
	if (!menu) {
		msgnw(bw->parent, joe_gettext(_("No such menu")));
		if (notify)
			*notify = 1;
		return -1;
	} else {
		bw->b->o.readonly = bw->o.readonly = bw->b->rdonly;
		return display_menu(bw, menu, notify);
	}
}

B *menuhist = NULL;

int umenu(BW *bw)
{
	if (wmkpw(bw->parent, joe_gettext(_("Menu: ")), &menuhist, domenu, USTR "menu", NULL, menucmplt, NULL, NULL, locale_map, 0)) {
		return 0;
	} else {
		return -1;
	}
}

/* Simplified mode command */

unsigned char **getoptions(void)
{
	unsigned char **s = vaensure(NULL, 20);
	int x;

	for (x = 0; glopts[x].name; ++x)
		s = vaadd(s, vsncpy(NULL, 0, sz(glopts[x].name)));
	vasort(s, aLen(s));
	return s;
}

/* Command line */

unsigned char **sopts = NULL;	/* Array of command names */

static int optcmplt(BW *bw)
{
	if (!sopts)
		sopts = getoptions();
	return simple_cmplt(bw,sopts);
}

static int doopt(BW *bw, unsigned char *s, void *object, int *notify)
{
	int y = find_option(s);
	vsrm(s);
	if (y == -1) {
		msgnw(bw->parent, joe_gettext(_("No such option")));
		if (notify)
			*notify = 1;
		return -1;
	} else {
		int flg = menu_flg;
		menu_flg = 0;
		return olddoopt(bw, y, flg, notify);
	}
}

B *opthist = NULL;

int umode(BW *bw)
{
	if (wmkpw(bw->parent, joe_gettext(USTR _("Option: ")), &opthist, doopt, USTR "opt", NULL, optcmplt, NULL, NULL, locale_map, 0)) {
		return 0;
	} else {
		return -1;
	}
}

/* Process rc file
 * Returns 0 if the rc file was succefully processed
 *        -1 if the rc file couldn't be opened
 *         1 if there was a syntax error in the file
 */

int procrc(CAP *cap, unsigned char *name)
{
	OPTIONS *o = &fdefault;	/* Current options */
	KMAP *context = NULL;	/* Current context */
	struct rc_menu *current_menu = NULL;
	unsigned char buf[1024];	/* Input buffer */
	unsigned char tmp[1024];	/* Temporary input buffer */
	unsigned buflen;
	JFILE *fd;		/* rc file */
	int line = 0;		/* Line number */
	int err = 0;		/* Set to 1 if there was a syntax error */

	strncpy((char *)buf, (char *)name, sizeof(buf) - 1);
	buf[sizeof(buf)-1] = '\0';
#ifdef __MSDOS__
	fd = jfopen(buf, "rt");
#else
	fd = jfopen(buf, "r");
#endif

	if (!fd)
		return -1;	/* Return if we couldn't open the rc file */

	fprintf(stderr,(char *)joe_gettext(_("Processing '%s'...")), name);
	fflush(stderr);

	while (jfgets(buf, sizeof(buf), fd)) {
		line++;

		switch (buf[0]) {
		case '\t':
		case '\f':
			buf[0]=' ';
		}
		buflen=zlen(buf);
		if (buf[0]!='\0' && buf[buflen-1]!='\n') { /* line too long */
			if (buf[0]!='Q') { /* not a comment */
				fprintf(stderr,(char *)joe_gettext(_("\n%s %d: Config line too long, truncated")), name, line);
				err=1;
			}
		        buf[buflen-1]='\n';
			/* vvv read rest of line (to make `line' counting intact) */
			while (jfgets(tmp, sizeof(tmp), fd) &&
			  (tmp[0]!='\0' && tmp[zlen(tmp)-1]!='\n')) {}
		}
		if (preprocess_rc_line(buf, sizeof buf, tmp, sizeof tmp, name, line)) err=1;
                
		switch (buf[0]) {
		case '\0':
		case '\n':
		case ' ':
			break;	/* Skip comment lines */

		case '=':	/* Define a global color */
			{ /* # introduces comment */
			parse_color_def(&global_colors,buf+1,name,line);
			}
			break;

		case '*':	/* Select file types for file-type dependant options */
			{ /* Space and tab introduce comments- which means we can't have them in the regex */
				int x;

				o = (OPTIONS *) joe_malloc(sizeof(OPTIONS));
				*o = fdefault;
				for (x = 0; buf[x] && buf[x] != '\n' && buf[x] != ' ' && buf[x] != '\t'; ++x) ;
				buf[x] = 0;
				o->next = options;
				options = o;
				o->name_regex = zdup(buf);
			}
			break;
		case '+':	/* Set file contents match regex */
			{ /* No comments allowed- entire line used. */
				int x;

				for (x = 0; buf[x] && buf[x] != '\n' && buf[x] != '\r'; ++x) ;
				buf[x] = 0;
				if (o)
					o->contents_regex = zdup(buf+1);
			}
			break;
		case '-':	/* Set an option */
			{ /* parse option and arg.  arg goes to end of line.  This is bad. */
				unsigned char *opt = buf + 1;
				int x;
				unsigned char *arg = NULL;

				for (x = 0; buf[x] && buf[x] != '\n' && buf[x] != ' ' && buf[x] != '\t'; ++x) ;
				if (buf[x] && buf[x] != '\n') {
					buf[x] = 0;
					for (arg = buf + ++x; buf[x] && buf[x] != '\n'; ++x) ;
				}
				buf[x] = 0;
				if (!glopt(opt, arg, o, 2)) {
					err = 1;
					fprintf(stderr,(char *)joe_gettext(_("\n%s %d: Unknown option %s")), name, line, opt);
				}
			}
			break;
		case '{':	/* Process help text.  No comment allowed after {name */
			{	/* everything after } is ignored. */
				line = help_init(fd,buf,line);
			}
			break;
		case ':':	/* Select context */
			{
				int x, c;

				for (x = 1; !joe_isspace_eof(locale_map,buf[x]); ++x) ;
				c = buf[x];
				buf[x] = 0;
				if (x != 1)
					if (!zcmp(buf + 1, USTR "def")) {
						int y;

						for (buf[x] = c; joe_isblank(locale_map,buf[x]); ++x) ;
						for (y = x; !joe_isspace_eof(locale_map,buf[y]); ++y) ;
						c = buf[y];
						buf[y] = 0;
						if (y != x) {
							int sta;
							MACRO *m;

							if (joe_isblank(locale_map,c)
							    && (m = mparse(NULL, buf + y + 1, &sta)))
								addcmd(buf + x, m);
							else {
								err = 1;
								fprintf(stderr, (char *)joe_gettext(_("\n%s %d: macro missing from :def")), name, line);
							}
						} else {
							err = 1;
							fprintf(stderr, (char *)joe_gettext(_("\n%s %d: command name missing from :def")), name, line);
						}
					} else if (!zcmp(buf + 1, USTR "inherit")) {
						if (context) {
							for (buf[x] = c; joe_isblank(locale_map,buf[x]); ++x) ;
							for (c = x; !joe_isspace_eof(locale_map,buf[c]); ++c) ;
							buf[c] = 0;
							if (c != x)
								kcpy(context, kmap_getcontext(buf + x));
							else {
								err = 1;
								fprintf(stderr, (char *)joe_gettext(_("\n%s %d: context name missing from :inherit")), name, line);
							}
						} else {
							err = 1;
							fprintf(stderr, (char *)joe_gettext(_("\n%s %d: No context selected for :inherit")), name, line);
						}
					} else if (!zcmp(buf + 1, USTR "include")) {
						for (buf[x] = c; joe_isblank(locale_map,buf[x]); ++x) ;
						for (c = x; !joe_isspace_eof(locale_map,buf[c]); ++c) ;
						buf[c] = 0;
						if (c != x) {
							unsigned char bf[1024];
							unsigned char *p = (unsigned char *)getenv("HOME");
							int rtn = -1;
							bf[0] = 0;
							if (p && buf[x] != '/') {
								joe_snprintf_2(bf,sizeof(bf),"%s/.joe-p37/%s",p,buf + x);
								rtn = procrc(cap, bf);
							}
							if (rtn == -1 && read_sys_configs && buf[x] != '/') {
								joe_snprintf_2(bf,sizeof(bf),"%s%s",JOERC,buf + x);
								rtn = procrc(cap, bf);
							}
							if (rtn == -1 && buf[x] == '/') {
								joe_snprintf_1(bf,sizeof(bf),"%s",buf + x);
								rtn = procrc(cap, bf);
							}
							switch (rtn) {
							case 1:
								err = 1;
								break;
							case -1:
								fprintf(stderr, (char *)joe_gettext(_("\n%s %d: Couldn't open %s")), name, line, bf);
								err = 1;
								break;
							}
							context = 0;
							o = &fdefault;
						} else {
							err = 1;
							fprintf(stderr, (char *)joe_gettext(_("\n%s %d: :include missing file name")), name, line);
						}
					} else if (!zcmp(buf + 1, USTR "delete")) {
						if (context) {
							int y;

							for (buf[x] = c; joe_isblank(locale_map,buf[x]); ++x) ;
							for (y = x; buf[y] != 0 && buf[y] != '\t' && buf[y] != '\n' && (buf[y] != ' ' || buf[y + 1]
															!= ' '); ++y) ;
							buf[y] = 0;
							kdel(context, buf + x);
						} else {
							err = 1;
							fprintf(stderr, (char *)joe_gettext(_("\n%s %d: No context selected for :delete")), name, line);
						}
					} else if (!zcmp(buf + 1, USTR "defmap")) {
						for (buf[x] = c; joe_isblank(locale_map,buf[x]); ++x) ;
						for (c = x; !joe_isspace_eof(locale_map,buf[c]); ++c) ;
						buf[c] = 0;
						if (c != x) {
							context = kmap_getcontext(buf + x);
							current_menu = 0;
						} else {
							err = 1;
							fprintf(stderr, (char *)joe_gettext(_("\n%s %d: :defmap missing name")), name, line);
						}
					} else if (!zcmp(buf + 1, USTR "defmenu")) {
						for (buf[x] = c; joe_isblank(locale_map,buf[x]); ++x) ;
						for (c = x; !joe_isspace_eof(locale_map,buf[c]); ++c) ;
						buf[c] = 0;
						current_menu = create_menu(buf + x);
						context = 0;
					} else {
						context = kmap_getcontext(buf + 1);
						current_menu = 0;
						/* err = 1;
						fprintf(stderr, (char *)joe_gettext(_("\n%s %d: unknown :command")), name, line);*/
					}
				else {
					err = 1;
					fprintf(stderr,(char *)joe_gettext(_("\n%s %d: Invalid context name")), name, line);
				}
			}
			break;
		default:	/* Get key-sequence to macro binding */
			{
				int x, y;
				MACRO *m;

				if (!context && !current_menu) {
					err = 1;
					fprintf(stderr,(char *)joe_gettext(_("\n%s %d: No context selected for macro to key-sequence binding")), name, line);
					break;
				}

				m = 0;
			      macroloop:
				m = mparse(m, buf, &x);
				if (x == -1) {
					err = 1;
					fprintf(stderr,(char *)joe_gettext(_("\n%s %d: Unknown command in macro")), name, line);
					break;
				} else if (x == -2) {
					jfgets(buf, 1024, fd);
					++line;
					goto macroloop;
				}
				if (!m)
					break;

				/* Skip to end of key sequence */
				for (y = x; buf[y] != 0 && buf[y] != '\t' && buf[y] != '\n' && (buf[y] != ' ' || buf[y + 1] != ' '); ++y) ;
				buf[y] = 0;

				if (current_menu) {
					/* Add menu entry */
					add_menu_entry(current_menu, buf + x, m);
				} else {
					/* Add binding to context */
					if (kadd(cap, context, buf + x, m) == -1) {
						fprintf(stderr,(char *)joe_gettext(_("\n%s %d: Bad key sequence '%s'")), name, line, buf + x);
						err = 1;
					}
				}
			}
			break;
		}
	}
	jfclose(fd);		/* Close rc file */

	/* Print proper ending string */
	if (err)
		fprintf(stderr, (char *)joe_gettext(_("\ndone\n")));
	else
		fprintf(stderr, (char *)joe_gettext(_("done\n")));

	return err;		/* 0 for success, 1 for syntax error */
}

/* Save a history buffer */

void save_hist(FILE *f,B *b)
{
	unsigned char buf[512];
	int len;
	if (b) {
		P *p = pdup(b->bof, USTR "save_hist");
		P *q = pdup(b->bof, USTR "save_hist");
		if (b->eof->line>10)
			pline(p,b->eof->line-10);
		pset(q,p);
		while (!piseof(p)) {
			pnextl(q);
			if (q->byte-p->byte<512) {
				len = q->byte - p->byte;
				brmem(p,buf,len);
			} else {
				brmem(p,buf,512);
				len = 512;
			}
			fprintf(f,"\t");
			emit_string(f,buf,len);
			fprintf(f,"\n");
			pset(p,q);
		}
		prm(p);
		prm(q);
	}
	fprintf(f,"done\n");
}

/* Load a history buffer */

void load_hist(FILE *f,B **bp)
{
	B *b;
	unsigned char buf[1024];
	unsigned char bf[1024];
	P *q;

	b = *bp;
	if (!b)
		*bp = b = bmk(NULL);

	q = pdup(b->eof, USTR "load_hist");

	while(fgets((char *)buf,1023,f) && zcmp(buf,USTR "done\n")) {
		unsigned char *p = buf;
		int len;
		parse_ws(&p,'#');
		len = parse_string(&p,bf,sizeof(bf));
		if (len>0) {
			binsm(q,bf,len);
			pset(q,b->eof);
		}
	}

	prm(q);
}

/* Save state */

#define STATE_ID (unsigned char *)"# JOE state file v1.0\n"

void save_state()
{
	unsigned char *home = (unsigned char *)getenv("HOME");
	int old_mask;
	FILE *f;
	if (!joe_state)
		return;
	if (!home)
		return;
	joe_snprintf_1(stdbuf,stdsiz,"%s/.joe_state",home);
	old_mask = umask(0066);
	f = fopen((char *)stdbuf,"w");
	umask(old_mask);
	if(!f)
		return;

	/* Write ID */
	fprintf(f,"%s",(char *)STATE_ID);

	/* Write state information */
	fprintf(f,"search\n"); save_srch(f);
	fprintf(f,"macros\n"); save_macros(f);
	fprintf(f,"files\n"); save_hist(f,filehist);
	fprintf(f,"find\n"); save_hist(f,findhist);
	fprintf(f,"replace\n"); save_hist(f,replhist);
	fprintf(f,"run\n"); save_hist(f,runhist);
	fprintf(f,"build\n"); save_hist(f,buildhist);
	fprintf(f,"grep\n"); save_hist(f,grephist);
	fprintf(f,"cmd\n"); save_hist(f,cmdhist);
	fprintf(f,"math\n"); save_hist(f,mathhist);
	fprintf(f,"yank\n"); save_yank(f);
	fprintf(f,"file_pos\n"); save_file_pos(f);
	fclose(f);
}

/* Load state */

void load_state()
{
	unsigned char *home = (unsigned char *)getenv("HOME");
	unsigned char buf[1024];
	FILE *f;
	if (!joe_state)
		return;
	if (!home)
		return;
	joe_snprintf_1(stdbuf,stdsiz,"%s/.joe_state",home);
	f = fopen((char *)stdbuf,"r");
	if(!f)
		return;

	/* Only read state information if the version is correct */
	if (fgets((char *)buf,1024,f) && !zcmp(buf,STATE_ID)) {

		/* Read state information */
		while(fgets((char *)buf,1023,f)) {
			if(!zcmp(buf,USTR "search\n"))
				load_srch(f);
			else if(!zcmp(buf,USTR "macros\n"))
				load_macros(f);
			else if(!zcmp(buf,USTR "files\n"))
				load_hist(f,&filehist);
			else if(!zcmp(buf,USTR "find\n"))
				load_hist(f,&findhist);
			else if(!zcmp(buf,USTR "replace\n"))
				load_hist(f,&replhist);
			else if(!zcmp(buf,USTR "run\n"))
				load_hist(f,&runhist);
			else if(!zcmp(buf,USTR "build\n"))
				load_hist(f,&buildhist);
			else if(!zcmp(buf,USTR "grep\n"))
				load_hist(f,&grephist);
			else if(!zcmp(buf,USTR "cmd\n"))
				load_hist(f,&cmdhist);
			else if(!zcmp(buf,USTR "math\n"))
				load_hist(f,&mathhist);
			else if(!zcmp(buf,USTR "yank\n"))
				load_yank(f);
			else if (!zcmp(buf,USTR "file_pos\n"))
				load_file_pos(f);
			else { /* Unknown... skip until next done */
				while(fgets((char *)buf,1023,f) && zcmp(buf,USTR "done\n"));
			}
		}
	}

	fclose(f);
}
