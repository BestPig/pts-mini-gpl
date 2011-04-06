/***************************************************************************
        xbindkeys : a program to bind keys to commands under X11.
                           -------------------
    begin                : Sat Oct 13 14:11:34 CEST 2001
    copyright            : (C) 2001 by Philippe Brochard
    email                : hocwp@free.fr
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gdk/gdk.h>
#include <gdk/gdkdisplay.h>
#include <gdk/gdkx.h>
#include <glib/gmem.h>
#include "xbindkeys.h"
#include "keys.h"
#include "options.h"
#include "get_key.h"
#include "grab_key.h"

#ifdef GUILE_FLAG
#include <libguile.h>
#endif



void end_it_all (Display * d);

static Display *start (char *);
#if 0
static void event_loop (Display *);
#endif
static int *null_X_error (Display *, XErrorEvent *);
static void reload_rc_file (void);
static void catch_HUP_signal (int sig);
static void catch_CHLD_signal (int sig);
static void start_as_daemon (void);


Display *current_display;  // The current display

#ifndef GUILE_FLAG
extern char rc_file[512];
#else
extern char rc_guile_file[512];
#endif

/* In milliseconds.
 * TODO(pts): Configure this to be disabled.
 */
#define SLEEP_TIME 100

#ifndef GUILE_FLAG
int
main (int argc, char **argv)
#else
int argc_t; char** argv_t;
void
inner_main (int argc, char **argv)
#endif
{
  Display *d;

#ifdef GUILE_FLAG
  argc = argc_t;
  argv = argv_t;
#endif

  gdk_init(&argc, &argv);
  get_options (argc, argv);

  if (!rc_file_exist ())
    exit (-1);

  if (have_to_start_as_daemon && !have_to_show_binding && !have_to_get_binding)
    start_as_daemon ();

  if (!display_name)
    display_name = XDisplayName (NULL);

  show_options ();

  d = start (display_name);
  current_display = d;

  get_offending_modifiers (d);

  if (have_to_get_binding)
    {
      get_key_binding (d, argv, argc);
      end_it_all (d);
      exit (0);
    }

#ifdef GUILE_FLAG
  if (get_rc_guile_file () != 0)
    {
#endif
      if (get_rc_file () != 0)
	{
	  end_it_all (d);
	  exit (-1);
	}
#ifdef GUILE_FLAG
    }
#endif


  if (have_to_show_binding)
    {
      show_key_binding (d);
      end_it_all (d);
      exit (0);
    }

  grab_keys (d);

  /* This: for restarting reading the RC file if get a HUP signal */
  signal (SIGHUP, catch_HUP_signal);
  /* and for reaping dead children */
  signal (SIGCHLD, catch_CHLD_signal);

  if (verbose)
    printf ("starting loop...\n");
  for (;;) {
    g_main_context_iteration(NULL, TRUE);
  }

#ifndef GUILE_FLAG
  return (0);			/* not reached... */
#endif
}

#ifdef GUILE_FLAG
int
main (int argc, char** argv)
{
  //guile shouldn't steal our arguments! we already parse them!
  //so we put them in temporary variables.
  argv_t = argv;
  argc_t = argc;
  //printf("Starting in guile mode...\n"); //debugery!
  scm_boot_guile(0,(char**)NULL,(void *)inner_main,NULL);
  return 0; /* not reached ...*/
}
#endif

static GdkDisplay* dpy = NULL;

static void adjust_display (XAnyEvent * xany);

/** Get the root GdkWindow corresponding to an X11 root window, or NULL */
static GdkWindow* get_root_window(Window xwin) {
  GdkScreen *scr;
  GdkWindow *win;
  int screen, screen_count;
  screen_count = gdk_display_get_n_screens(dpy);
  for (screen = 0; screen < screen_count; screen++) {
    scr = gdk_display_get_screen(dpy, screen);
    win = gdk_screen_get_root_window(scr);
    if (GDK_WINDOW_XID(win) == xwin)
      return win;
  }
  return NULL;
}

guint32 last_event_time;

static GdkFilterReturn event_filter(GdkXEvent *xevent, GdkEvent *event, gpointer data) {
  Display *d = (Display*)data;
  int i;
  GdkFilterReturn ret = GDK_FILTER_CONTINUE;
  (void)event;
  XEvent *e = (XEvent*)xevent;
  if (get_root_window(e->xany.window) == NULL) {
    /* Don't do anything if not called from a root window. */
  } else if (e->type == KeyPress) {
    last_event_time = e->xkey.time;
    if (verbose)
      {
        printf ("Key press !\n");
        printf ("e->xkey.keycode=%d\n", e->xkey.keycode);
        printf ("e->xkey.state=%d\n", e->xkey.state);
      }

    e->xkey.state &= ~(numlock_mask | capslock_mask | scrolllock_mask);

    for (i = 0; i < nb_keys; i++)
      {
        if (keys[i].type == SYM && keys[i].event_type == PRESS)
          {
            if (e->xkey.keycode == XKeysymToKeycode (d, keys[i].key.sym)
                && e->xkey.state == keys[i].modifier)
              {
                ret = GDK_FILTER_REMOVE;
                print_key (d, &keys[i]);
                adjust_display(&e->xany);
                start_command_key (&keys[i]);
              }
          }
        else
        if (keys[i].type == CODE && keys[i].event_type == PRESS)
          {
            if (e->xkey.keycode == keys[i].key.code
                && e->xkey.state == keys[i].modifier)
              {
                ret = GDK_FILTER_REMOVE;
                print_key (d, &keys[i]);
                adjust_display(&e->xany);
                start_command_key (&keys[i]);
              }
          }
      }
  } else if (e->type == KeyRelease) {
    last_event_time = e->xkey.time;
    if (verbose)
      {
        printf ("Key release !\n");
        printf ("e->xkey.keycode=%d\n", e->xkey.keycode);
        printf ("e->xkey.state=%d\n", e->xkey.state);
      }

    e->xkey.state &= ~(numlock_mask | capslock_mask | scrolllock_mask);

    for (i = 0; i < nb_keys; i++)
      {
        if (keys[i].type == SYM && keys[i].event_type == RELEASE)
          {
            if (e->xkey.keycode == XKeysymToKeycode (d, keys[i].key.sym)
                && e->xkey.state == keys[i].modifier)
              {
                ret = GDK_FILTER_REMOVE;
                print_key (d, &keys[i]);
                adjust_display(&e->xany);
                start_command_key (&keys[i]);
              }
          }
        else
        if (keys[i].type == CODE && keys[i].event_type == RELEASE)
          {
            if (e->xkey.keycode == keys[i].key.code
                && e->xkey.state == keys[i].modifier)
              {
                ret = GDK_FILTER_REMOVE;
                print_key (d, &keys[i]);
                adjust_display(&e->xany);
                start_command_key (&keys[i]);
              }
          }
      }
  } else if (e->type == ButtonPress) {
    last_event_time = e->xbutton.time;
    if (verbose)
      {
        printf ("Button press !\n");
        printf ("e->xbutton.button=%d\n", e->xbutton.button);
        printf ("e->xbutton.state=%d\n", e->xbutton.state);
      }

    e->xbutton.state &= ~(numlock_mask | capslock_mask | scrolllock_mask
                         | Button1Mask | Button2Mask | Button3Mask
                         | Button4Mask | Button5Mask);

    for (i = 0; i < nb_keys; i++)
      {
        if (keys[i].type == BUTTON && keys[i].event_type == PRESS)
          {
            if (e->xbutton.button == keys[i].key.button
                && e->xbutton.state == keys[i].modifier)
              {
                ret = GDK_FILTER_REMOVE;
                print_key (d, &keys[i]);
                adjust_display(&e->xany);
                start_command_key (&keys[i]);
              }
          }
      }
  } else if (e->type == ButtonRelease) {
    last_event_time = e->xbutton.time;
    if (verbose)
      {
        printf ("Button release !\n");
        printf ("e->xbutton.button=%d\n", e->xbutton.button);
        printf ("e->xbutton.state=%d\n", e->xbutton.state);
      }

    e->xbutton.state &= ~(numlock_mask | capslock_mask | scrolllock_mask
                         | Button1Mask | Button2Mask | Button3Mask
                         | Button4Mask | Button5Mask);

    for (i = 0; i < nb_keys; i++)
      {
        if (keys[i].type == BUTTON && keys[i].event_type == RELEASE)
          {
            if (e->xbutton.button == keys[i].key.button
                && e->xbutton.state == keys[i].modifier)
              {
                ret = GDK_FILTER_REMOVE;
                print_key (d, &keys[i]);
                adjust_display(&e->xany);
                start_command_key (&keys[i]);
              }
          }
      }
#if 0  /* Does not happen in GNOME. */
  } else {
    g_assert(0);
#endif
  }
  return ret;
}

#ifndef GUILE_FLAG
static time_t rc_file_changed;
#else
static time_t rc_guile_file_changed;
#endif

gboolean reload_rc_file_timeout(gpointer data) {
  (void)data;
  struct stat rc_file_info;
#ifndef GUILE_FLAG
  // if the rc file has been modified, reload it
  stat (rc_file, &rc_file_info);
  if (rc_file_info.st_mtime != rc_file_changed)
    {
      reload_rc_file ();
      if (verbose)
        {
          printf ("The rc file has been modified, reload it\n");
        }
      rc_file_changed = rc_file_info.st_mtime;
    }
#else
  // if the rc guile file has been modified, reload it
  stat (rc_guile_file, &rc_file_info);
  if (rc_file_info.st_mtime != rc_guile_file_changed)
    {
      reload_rc_file ();
      if (verbose)
        {
          printf ("The rc guile file has been modified, reload it\n");
        }
      rc_guile_file_changed = rc_file_info.st_mtime;
    }
#endif
  return TRUE;  /* Don't remove this event handler yet. */
}

static Display *
start (char *display)
{
  Display *d;
  GdkScreen *scr;
  GdkWindow *win;
  int screen, screen_count;
  struct stat rc_file_info;

  dpy = gdk_display_open(display);
  if (dpy == NULL) {
    fprintf(stderr, "error: cannot open display\n");
    exit(2);
  }
  d = GDK_DISPLAY_XDISPLAY(dpy);
  g_assert(d);
  XAllowEvents (d, AsyncBoth, CurrentTime);
  screen_count = gdk_display_get_n_screens(dpy);
  for (screen = 0; screen < screen_count; screen++) {
    scr = gdk_display_get_screen(dpy, screen);
    win = gdk_screen_get_root_window(scr);
    /* TODO(pts): Maybe add GDK_MOTION_NOTIFY | GDK_BUTTON_PRESS | GDK_BUTTON_RELEASE? */
    gdk_window_set_events(win, GDK_KEY_PRESS | GDK_KEY_RELEASE | GDK_MOTION_NOTIFY | GDK_BUTTON_PRESS | GDK_BUTTON_RELEASE);
    gdk_window_add_filter(win, event_filter, d);
  }
#ifndef GUILE_FLAG
  stat(rc_file, &rc_file_info);
  rc_file_changed = rc_file_info.st_mtime;
#else
  stat (rc_guile_file, &rc_file_info);
  rc_guile_file_changed = rc_file_info.st_mtime;
#endif
  g_timeout_add(SLEEP_TIME, reload_rc_file_timeout, NULL);

  gdk_flush();
  g_main_context_iteration(NULL, FALSE);
  
  XSetErrorHandler ((XErrorHandler) null_X_error);

  return d;
}



void
end_it_all (Display * d)
{
  ungrab_all_keys (d);

  close_keys ();
  XCloseDisplay (d);
}



static void
adjust_display (XAnyEvent * xany)
{
  size_t envstr_size = strlen(DisplayString(xany->display)) + 8 + 1;
  char* envstr = g_malloc (envstr_size);
  XWindowAttributes attr;
  char* ptr;
  char buf[16];

  snprintf (envstr, envstr_size, "DISPLAY=%s", DisplayString(xany->display));

  XGetWindowAttributes (xany->display, xany->window,  &attr);

  if (verbose)
    printf ("got screen %d for window %x\n", XScreenNumberOfScreen(attr.screen), (unsigned int)xany->window );

  ptr = strchr (strchr (envstr, ':'), '.');
  if (ptr)
    *ptr = '\0';

  snprintf (buf, sizeof(buf), ".%i", XScreenNumberOfScreen(attr.screen));
  strncat (envstr, buf, 16);

  putenv (envstr);
}

static int *
null_X_error (Display * d, XErrorEvent * e)
{
  static int already = 0;

  (void)d; (void)e;

  /* The warning is displayed only once */
  if (already != 0)
    return (NULL);
  already = 1;

  printf ("\n*** Warning ***\n");
  printf ("Please verify that there is not another program running\n");
  printf ("which captures one of the keys captured by xbindkeys.\n");
  printf ("It seems that there is a conflict, and xbindkeys can't\n");
  printf ("grab all the keys defined in its configuration file.\n");

/*   end_it_all (d); */

/*   exit (-1); */

  return (NULL);
}



static void
reload_rc_file (void)
{
  int min, max;
  int screen;

  XDisplayKeycodes (current_display, &min, &max);

  if (verbose)
    printf ("Reload RC file\n");

  for (screen = 0; screen < ScreenCount (current_display); screen++)
    {
      XUngrabKey (current_display, AnyKey, AnyModifier, RootWindow (current_display, screen));
    }
  close_keys ();

#ifdef GUILE_FLAG
  if (get_rc_guile_file () != 0)
    {
#endif
      if (get_rc_file () != 0)
	{
	  end_it_all (current_display);
	  exit (-1);
	}
#ifdef GUILE_FLAG
    }
#endif

  grab_keys (current_display);
}


static void
catch_HUP_signal (int sig)
{
  (void)sig;
  reload_rc_file ();
}


static void
catch_CHLD_signal (int sig)
{
  pid_t child;

  (void)sig;

  /*   If more than one child exits at approximately the same time, the signals */
  /*   may get merged. Handle this case correctly. */
  while ((child = waitpid(-1, NULL, WNOHANG)) > 0)
    {
      if (verbose)
	printf ("Catch CHLD signal -> pid %i terminated\n", child);
    }
}





static void
start_as_daemon (void)
{
  pid_t pid;
  int i;

  pid = fork ();
  if (pid < 0)
    {
      perror ("Could not fork");
    }
  if (pid > 0)
    {
      exit (EXIT_SUCCESS);
    }

  setsid ();

  pid = fork ();
  if (pid < 0)
    {
      perror ("Could not fork");
    }
  if (pid > 0)
    {
      exit (EXIT_SUCCESS);
    }

  i = getdtablesize();
  if (i > 1024)
    i = 1024;
  for (; i >= 0; i--)
    {
      close (i);
    }

  i = open ("/dev/null", O_RDWR);
  if (i != 0) abort();
  if (dup(i) != 1) abort();
  if (dup(i) != 2) abort();

/*   umask (022); */
/*   chdir ("/tmp"); */
}
