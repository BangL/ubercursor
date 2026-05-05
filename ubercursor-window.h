#ifndef _ubercursor_window_h_
#define _ubercursor_window_h_

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UBERCURSOR_TYPE_WINDOW ubercursor_window_get_type ()
G_DECLARE_FINAL_TYPE (UberCursorWindow, ubercursor_window, UBERCURSOR, WINDOW, GtkWindow)

UberCursorWindow *ubercursor_window_new (void);

void ubercursor_window_set_image(UberCursorWindow *window, cairo_surface_t *image);
cairo_surface_t *ubercursor_window_get_image(UberCursorWindow *window);

void ubercursor_window_set_mouse_down(UberCursorWindow *window, gboolean mouse_down);
gboolean ubercursor_window_get_mouse_down(UberCursorWindow *window);

G_END_DECLS


#endif
