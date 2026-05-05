#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "swcursor-window.h"

#define SECOND 1000000

typedef struct {
    cairo_surface_t *image;
    guint64 timestamp;
    guint64 framerate;
} State_t;

static cairo_surface_t *load_image(const char *path);
static void show_main_window(State_t *state);
static gboolean tick(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer user_data);

static State_t *state = NULL;

static void cleanup(int sig)
{
    (void)sig;
    if (state) {
        if (state->image)
            cairo_surface_destroy(state->image);
        free(state);
        state = NULL;
    }
    exit(1);
}

int main(int argc, char **argv)
{
    int opt;
    cairo_surface_t *image = NULL;

    signal(SIGABRT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGINT,  cleanup);

    state = (State_t*)malloc(sizeof(State_t));
    if (!state) {
        fprintf(stderr, "Out of memory\n");
        return 1;
    }

    state->framerate = SECOND / 60;
    state->timestamp = 0;
    state->image = NULL;

    while ((opt = getopt(argc, argv, "irh")) != -1) {
        switch (opt) {
        case 'i':
            if (optind < argc) {
                image = load_image(argv[optind]);
                optind++;
            }
            break;

        case 'r':
            if (optind < argc) {
                int rate = atoi(argv[optind]);
                if (rate > 0 && rate <= 1000) {
                    state->framerate = SECOND / rate;
                } else {
                    fprintf(stderr, "Invalid rate: %s, using default 60 fps\n", argv[optind]);
                }
                optind++;
            }
            break;

        case 'h':
            printf("Usage: %s [-i file.png] [-r fps]\n\n"
                   "Options:\n"
                   "  -i [file]     Load PNG as cursor image\n"
                   "  -r [number]   Set refresh rate (fps)\n"
                   "  -h            Show this help\n\n"
                   "Example:\n"
                   "  %s -i cursors/cursor-large.png -r 120\n", 
                   argv[0], argv[0]);
            exit(EXIT_SUCCESS);

        default:
            fprintf(stderr, "Type -h for help\n");
            exit(EXIT_FAILURE);
        }
    }

    if (image == NULL) {
        image = load_image("cursors/8.png");
    }

    if (image) {
        state->image = image;
    } else {
        fprintf(stderr, "Failed to load any cursor image\n");
        free(state);
        return 1;
    }

    printf("Refreshing cursor at %d fps\n", (int)(SECOND / state->framerate));

    gdk_set_allowed_backends("x11");
    gtk_init(&argc, &argv);
    show_main_window(state);
    gtk_main();

    cleanup(0);
    return 0;
}

static cairo_surface_t*
load_image(const char *path)
{
	cairo_surface_t *image = cairo_image_surface_create_from_png(path);
	if (cairo_surface_status(image) != CAIRO_STATUS_SUCCESS) {
		const char *msg = cairo_status_to_string(cairo_surface_status(image));
		fprintf(stderr, "Error loading '%s': %s\n", path, msg);
    return NULL;
	}

	return image;
}

static void show_main_window(State_t *state)
{
    SWCursorWindow *window = swcursor_window_new();
    swcursor_window_set_image(window, state->image);
    gtk_widget_add_tick_callback(GTK_WIDGET(window), tick, state, NULL);
    gtk_widget_show_all(GTK_WIDGET(window));
}

static gboolean
tick(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer user_data)
{
    static gboolean show_warning = TRUE;
    State_t *state = (State_t*)user_data;
    guint64 timestamp;

    timestamp = gdk_frame_clock_get_frame_time(frame_clock);

    if ((timestamp - state->timestamp) < state->framerate)
        return G_SOURCE_CONTINUE;

    state->timestamp = timestamp;

    if (!gtk_widget_get_realized(widget))
        return G_SOURCE_CONTINUE;

    GdkWindow *gdk_window = gtk_widget_get_window(widget);
    if (gdk_window == NULL)
        return G_SOURCE_CONTINUE;

    int w_width = 0, w_height = 0;
    gtk_window_get_size(GTK_WINDOW(widget), &w_width, &w_height);

    Display *xdisplay = gdk_x11_display_get_xdisplay(gtk_widget_get_display(widget));
    Window xroot_window = XDefaultRootWindow(xdisplay);
    gint scale_factor = gdk_window_get_scale_factor(gdk_window);

    Window ret_root, ret_child;
    int root_x, root_y, win_x, win_y;
    unsigned int mask;

    if (XQueryPointer(xdisplay, xroot_window,
                      &ret_root, &ret_child,
                      &root_x, &root_y,
                      &win_x, &win_y, &mask))
    {
        int move_x = (root_x / scale_factor) - (w_width / 2);
        int move_y = (root_y / scale_factor) - (w_height / 2);

        gtk_window_move(GTK_WINDOW(widget), move_x, move_y);

        gboolean mouse_down = (mask & Button1Mask) ||
                              (mask & Button2Mask) ||
                              (mask & Button3Mask);

        swcursor_window_set_mouse_down(SWCURSOR_WINDOW(widget), mouse_down);
    }
    else if (show_warning)
    {
        fprintf(stderr, "swcursor: warning: could not query cursor position (further warnings suppressed)\n");
        show_warning = FALSE;
    }

    return G_SOURCE_CONTINUE;
}