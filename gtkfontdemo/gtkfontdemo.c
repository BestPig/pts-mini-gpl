#define DUMMY \
  set -ex; \
  gcc -W -Wall -s -O2 `pkg-config --cflags --libs gtk+-2.0` \
  -o gtkfontdemo "$0"; exit 0
/*
 * gtkfonttest.c: Font rendering demo (with colors and markup) for GTK.
 * by pts@fazekas.hu at Tue Nov  2 15:18:09 CET 2010
 */

#include <gtk/gtk.h>

/* http://library.gnome.org/devel/pango/stable/PangoMarkupFormat.html */
char *markup = "<span font_desc=\"Arial 10\">Peter, <b>Peter</b>,</span>";
char *foregroundspec = "#000000";  /* white */
char *backgroundspec = "#ffffff";  /* black */

int main(int argc, char **argv) {
  GtkWidget *window;
  GtkWidget *label;
  GdkColor foreground;
  GdkColor background;

  gtk_init(&argc, &argv);
  if (argv[1] != NULL && argv[1][0] != '\0')
    markup = argv[1];
  if (argv[1] != NULL && argv[2] != NULL && argv[2][0] != '\0')
    foregroundspec = argv[2];
  if (argv[1] != NULL && argv[2] != NULL && argv[3] != NULL &&
      argv[3][0] != '\0')
    backgroundspec = argv[3];
  g_assert(gdk_color_parse(foregroundspec, &foreground));
  g_assert(gdk_color_parse(backgroundspec, &background));

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(window), "GTK font rendering demo");
  gtk_widget_modify_bg(window, GTK_STATE_NORMAL, &background);
  gtk_signal_connect(GTK_OBJECT(window), "destroy",
                     GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
  gtk_container_border_width(GTK_CONTAINER(window), 10);

  label = gtk_label_new(NULL);
  gtk_widget_add_events(window, GDK_BUTTON_PRESS_MASK);
  gtk_signal_connect_object(GTK_OBJECT(window), "button-press-event",
                            GTK_SIGNAL_FUNC(gtk_widget_destroy),
                            GTK_OBJECT(window));
  gtk_label_set_markup(GTK_LABEL(label), markup);
  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &foreground);
  gtk_container_add(GTK_CONTAINER(window), label);
  gtk_widget_show(label);
  
  gtk_widget_show(window);
  gtk_main();
  return 0;
}
