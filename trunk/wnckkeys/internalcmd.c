/* by pts@fazekas.hu at Mon Apr  4 15:33:34 CEST 2011
 *
 * slow to run it again: perl -e 'for(1..1000){system "./a.out", "x"}' > /dev/null  13.54s user 15.08s system 55% cpu 51.453 total
 */
#define WNCK_I_KNOW_THIS_IS_UNSTABLE 1
#include <X11/Xlib.h>
#include <gdk/gdkdisplay.h>
#include <libwnck/libwnck.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int verbose;
#define is_internalcmd_verbose verbose
/* static const int is_internalcmd_verbose = 1; */

typedef enum Direction {
  DIRECTION_UP = 1,
  DIRECTION_DOWN = 2,
  DIRECTION_LEFT = 3,
  DIRECTION_RIGHT = 4,
} Direction;

typedef struct WindowInfo {
  int x, y, w, h;  /* (top, left, width, height) of wwin */
  WnckWindow *wwin;
  struct WindowInfo *wi0;  /* The active window. */
} WindowInfo;

/* Return the square of the distances of the middle points of two WindowInfo
 * objects.
 */
static gint64 windowinfo_distance(const WindowInfo *wi0, const WindowInfo *wi1) {
  gint64 a = ((gint64)wi0->x * 2 + wi0->w - (gint64)wi1->x * 2 - wi1->w);
  gint64 b = ((gint64)wi0->y * 2 + wi0->h - (gint64)wi1->y * 2 - wi1->h);
  return a * a + b * b;
}

/* Return -1 if the middle point of WindowInfo a is closer to a->wi0 than
 * WindowInfo b is to b->wi0.
 */
static int cmp_windowinfo(const void *a, const void *b) {
  const WindowInfo *wia = (WindowInfo*)a;
  const WindowInfo *wib = (WindowInfo*)b;
  gint64 d = windowinfo_distance(wia->wi0, wia) -
             windowinfo_distance(wib->wi0, wib);
  return d < 0 ? -1 : d == 0 ? 0 : 1;
}

/* If we draw a vector from wi0 to wi1, is it approximately in the specified
 * Direction (i.e. not exactly opposite)?
 */
static gboolean is_in_direction(const WindowInfo *wi0,
                                const WindowInfo *wi1,
                                Direction direction) {
  if (direction == DIRECTION_UP) {
    return (gint64)wi0->y * 2 + wi0->h > (gint64)wi1->y * 2 + wi1->h;
  } else if (direction == DIRECTION_DOWN) {
    return (gint64)wi0->y * 2 + wi0->h < (gint64)wi1->y * 2 + wi1->h;
  } else if (direction == DIRECTION_LEFT) {
    return (gint64)wi0->x * 2 + wi0->w > (gint64)wi1->x * 2 + wi1->w;
  } else if (direction == DIRECTION_RIGHT) {
    return (gint64)wi0->x * 2 + wi0->w < (gint64)wi1->x * 2 + wi1->w;
  }
  g_assert(0);
  return FALSE;  /* Not reached. */
}

extern guint32 last_event_time;

/* Called from run_command in xbindkeys for interal commands (i.e. those
 * starting with `|'; called with `|' and spaces and tabs stripped).
 */
void run_internal_command(const char* command) {
  WnckScreen *wscr;
  WnckWorkspace *wwork;
  GList *windows;
  WindowInfo wi0;
  WindowInfo *wis = NULL;
  gsize wi_size = 0;
  gsize wi_capacity = 0;
  gsize i;
  Direction direction;

  if (0 == strcmp(command, "up")) {
    direction = DIRECTION_UP;
  } else if (0 == strcmp(command, "down")) {
    direction = DIRECTION_DOWN;
  } else if (0 == strcmp(command, "left")) {
    direction = DIRECTION_LEFT;
  } else if (0 == strcmp(command, "right")) {
    direction = DIRECTION_RIGHT;
  } else {
    g_warning("unknown internal command: |%s", command);
    return;
  }

  wscr = wnck_screen_get_default();
  g_assert(wscr);

  /* Talk to the X server to populate the screen.
   * TODO(pts): Do this more efficiently.
   */
  wnck_screen_force_update(wscr);

  wi0.wwin = wnck_screen_get_active_window(wscr);
  if (wi0.wwin == NULL) {
    g_warning("cannot get active WnckWindow");
    return;
  }
  if (is_internalcmd_verbose) {
    printf("\n%d.%s\n", wnck_window_is_active(wi0.wwin),
                        wnck_window_get_name(wi0.wwin));
  }
  wi0.wi0 = &wi0;
  wnck_window_get_geometry(wi0.wwin, &wi0.x, &wi0.y, &wi0.w, &wi0.h);
  wwork = wnck_window_get_workspace(wi0.wwin);
  
  for (windows = wnck_screen_get_windows(wscr);
       windows != NULL;
       windows = windows->next) {
    WnckWindow *wwin1 = (WnckWindow*)windows->data;
    WnckWindowState wst1 = wnck_window_get_state(wwin1);
    WindowInfo *wip;
    if (wwin1 == wi0.wwin ||
        (wst1 & WNCK_WINDOW_STATE_MINIMIZED) ||
        (wst1 & WNCK_WINDOW_STATE_SHADED) ||
        (wst1 & WNCK_WINDOW_STATE_SKIP_PAGER) ||
        (wst1 & WNCK_WINDOW_STATE_SKIP_TASKLIST) ||
        (wst1 & WNCK_WINDOW_STATE_HIDDEN) ||
        !wnck_window_is_on_workspace(wwin1, wwork))
      continue;

    if (wi_size == wi_capacity) {
      wi_capacity <<= 1;
      if (wi_capacity < 16)
        wi_capacity = 16;
      wis = g_realloc(wis, wi_capacity * sizeof *wis);
    }
    wip = wis + wi_size++;
    wip->wi0 = &wi0;
    wip->wwin = wwin1;
    wnck_window_get_geometry(wwin1, &wip->x, &wip->y, &wip->w, &wip->h);
    if (is_internalcmd_verbose) {
      printf("%d %s %dx%d+%d+%d\n",
              wnck_window_is_active(wwin1),
              wnck_window_get_name(wwin1),
              wip->w, wip->h, wip->x, wip->y);
    }
  }
  if (is_internalcmd_verbose) {
    printf("---\n");
    fflush(stdout);
  }
  if (wi_size == 0)  /* No other window to switch to. */
    return;


  /* Now we have the active window in wi, and the other windows in the same
   * screen and workspace in wis[:wi_size].
   */

  /* TODO(pts): Implement a better window picker, e.g. ignore completely
   * covered windows (at least those covered by wi0).
   */
  qsort(wis, wi_size, sizeof *wis, cmp_windowinfo);
  /* Now the window closest to wi0 is wis[0]. */

  for (i = 0; i < wi_size; ++i) {
    if (is_in_direction(&wi0, wis + i, direction)) {
      /* TODO(pts): Maybe raise as well? wnck cannot do this */
      /* TODO(pts): Add better timestamp, from the millisecs in the XEvent */
      wnck_window_activate(wis[i].wwin, last_event_time);
      break;
    }
  }

  g_free(wis);
}
