#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <cairo.h>
#include <string.h>
#include <errno.h>

typedef struct {
    const unsigned char *data;
    size_t size;
    size_t pos;
} mem_buffer_t;

#include "ubercursor-window.h"
#include "cursor_image.h"

#define SECOND 1000000

typedef struct {
    cairo_surface_t *image;
    guint64 timestamp;
    guint64 framerate;
    guint8 hide_buttons;
} State_t;

static cairo_surface_t *load_image(const char *path);
static void show_main_window(State_t *state);
static gboolean tick(GtkWidget *widget, GdkFrameClock *frame_clock, gpointer user_data);
static void run_cursor(int argc, char **argv, pid_t game_pid);

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

static cairo_status_t read_png(void *closure,
                                unsigned char *data,
                                unsigned int length)
{
    mem_buffer_t *b = (mem_buffer_t *)closure;

    if (b->pos + length > b->size)
        length = b->size - b->pos;

    memcpy(data, b->data + b->pos, length);
    b->pos += length;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_surface_t *load_embedded_image()
{
    mem_buffer_t buf = {
        cursors_8_png,
        cursors_8_png_len,
        0
    };

    cairo_surface_t *surface =
        cairo_image_surface_create_from_png_stream(
            read_png,
            &buf
        );

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "PNG decode failed\n");
        return NULL;
    }

    return surface;
}

int main(int argc, char **argv)
{
    int cmd_index = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [options] -- <command>\n\n"
                   "Options:\n"
                   "  -i [file]     Load PNG as cursor image\n"
                   "  -r [number]   Set refresh rate (fps)\n"
                   "  -c [mask]     Set which buttons hide cursor (bitmask)\n"
                   "                  1=left, 2=middle, 4=right (default: 7=all)\n"
                   "  -h            Show this help\n\n"
                   "Example:\n"
                   "  %s -i cursors/cursor-large.png -r 120 -- somegame\n"
                   "  %s -c 1 -- somegame           (hide on left click only)\n"
                   "  %s -c 4 -- somegame           (hide on right click only)\n",
                   argv[0], argv[0], argv[0], argv[0]);
            exit(EXIT_SUCCESS);
        }
        if (strcmp(argv[i], "--") == 0) {
            cmd_index = i + 1;
            break;
        }
    }

    if (cmd_index == -1 || cmd_index >= argc) {
        fprintf(stderr, "Usage: %s [options] -- <command>\n", argv[0]);
        exit(1);
    }

    pid_t game_pid = fork();

    if (game_pid < 0) {
        perror("fork failed");
        exit(1);
    }

    if (game_pid == 0) {
        setpgid(0, 0);
        execvp(argv[cmd_index], &argv[cmd_index]);
        perror("execvp failed");
        exit(1);
    }

    run_cursor(argc, argv, game_pid);
    return 0;
}

static gboolean check_game_dead(gpointer data)
{
    pid_t *pid = data;

    int status;
    pid_t result = waitpid(*pid, &status, WNOHANG);

    if (result == *pid || (result == -1 && errno == ECHILD)) {
        gtk_main_quit();
        free(pid);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

void run_cursor(int argc, char **argv, pid_t game_pid)
{
    int opt;
    cairo_surface_t *image = NULL;

    signal(SIGABRT, cleanup);
    signal(SIGTERM, cleanup);
    signal(SIGINT,  cleanup);

    state = (State_t*)malloc(sizeof(State_t));
    if (!state) {
        fprintf(stderr, "Out of memory\n");
        return;
    }

    state->framerate = SECOND / 60;
    state->timestamp = 0;
    state->image = NULL;
    state->hide_buttons = 7;

    while ((opt = getopt(argc, argv, "c:i:r:h")) != -1) {
        switch (opt) {
        case 'c':
            if (optind < argc) {
                int val = atoi(argv[optind]);
                if (val >= 0 && val <= 7) {
                    state->hide_buttons = (guint8)val;
                } else {
                    fprintf(stderr, "Invalid button mask: %s, using default 7 (all)\n", argv[optind]);
                }
                optind++;
            }
            break;

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

        default:
            fprintf(stderr, "Type -h for help\n");
            exit(EXIT_FAILURE);
        }
    }

    if (image == NULL) {
        image = load_embedded_image();
    }

    if (image) {
        state->image = image;
    } else {
        fprintf(stderr, "Failed to load any cursor image\n");
        free(state);
        return;
    }

    printf("Refreshing cursor at %d fps\n", (int)(SECOND / state->framerate));

    gdk_set_allowed_backends("x11");
    gtk_init(&argc, &argv);
    pid_t *pid_ptr = malloc(sizeof(pid_t));
    *pid_ptr = game_pid;
    g_timeout_add(500, check_game_dead, pid_ptr);
    show_main_window(state);
    gtk_main();

    cleanup(0);
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
    UberCursorWindow *window = ubercursor_window_new();
    ubercursor_window_set_image(window, state->image);
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

        gboolean mouse_down = FALSE;
        if (state->hide_buttons & 1) mouse_down |= !!(mask & Button1Mask);
        if (state->hide_buttons & 2) mouse_down |= !!(mask & Button2Mask);
        if (state->hide_buttons & 4) mouse_down |= !!(mask & Button3Mask);
        ubercursor_window_set_mouse_down(UBERCURSOR_WINDOW(widget), mouse_down);
    }
    else if (show_warning)
    {
        fprintf(stderr, "ubercursor: warning: could not query cursor position (further warnings suppressed)\n");
        show_warning = FALSE;
    }

    return G_SOURCE_CONTINUE;
}