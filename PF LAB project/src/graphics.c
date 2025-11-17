#include "graphics.h"
#include <math.h>

// Color definitions
const Color COLOR_BLACK = {0.0, 0.0, 0.0, 1.0};
const Color COLOR_WHITE = {1.0, 1.0, 1.0, 1.0};
const Color COLOR_RED = {1.0, 0.0, 0.0, 1.0};
const Color COLOR_BLUE = {0.0, 0.5, 1.0, 1.0};
const Color COLOR_GREEN = {0.0, 1.0, 0.0, 1.0};
const Color COLOR_YELLOW = {1.0, 1.0, 0.0, 1.0};
const Color COLOR_GRAY = {0.5, 0.5, 0.5, 1.0};
const Color COLOR_DARK_BLUE = {0.0, 0.2, 0.4, 1.0};
const Color COLOR_LIGHT_GRAY = {0.8, 0.8, 0.8, 1.0};

void graphics_set_color(cairo_t *cr, Color color) {
    cairo_set_source_rgba(cr, color.r, color.g, color.b, color.a);
}

void graphics_draw_rectangle(cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height) {
    cairo_rectangle(cr, x, y, width, height);
    cairo_stroke(cr);
}

void graphics_fill_rectangle(cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height) {
    cairo_rectangle(cr, x, y, width, height);
    cairo_fill(cr);
}

void graphics_draw_circle(cairo_t *cr, gdouble x, gdouble y, gdouble radius) {
    cairo_arc(cr, x, y, radius, 0, 2 * M_PI);
    cairo_stroke(cr);
}

void graphics_fill_circle(cairo_t *cr, gdouble x, gdouble y, gdouble radius) {
    cairo_arc(cr, x, y, radius, 0, 2 * M_PI);
    cairo_fill(cr);
}

void graphics_draw_text(cairo_t *cr, const gchar *text, gdouble x, gdouble y, gdouble size) {
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}

void graphics_draw_text_centered(cairo_t *cr, const gchar *text, gdouble center_x, gdouble y, gdouble size) {
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    
    cairo_text_extents_t extents;
    cairo_text_extents(cr, text, &extents);
    
    gdouble x = center_x - (extents.width / 2.0);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}

void graphics_draw_box(cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height, 
                       Color border_color, Color fill_color, gdouble border_width) {
    // Draw filled box
    graphics_set_color(cr, fill_color);
    graphics_fill_rectangle(cr, x, y, width, height);
    
    // Draw border
    graphics_set_color(cr, border_color);
    cairo_set_line_width(cr, border_width);
    graphics_draw_rectangle(cr, x, y, width, height);
}

void graphics_clear_canvas(cairo_t *cr, Color color) {
    graphics_set_color(cr, color);
    cairo_paint(cr);
}

// Load an image from file
GdkPixbuf* graphics_load_image(const gchar *filename) {
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
    
    if (error != NULL) {
        g_warning("Failed to load image %s: %s — creating fallback pixbuf", filename, error->message);
        g_error_free(error);

        // Create a simple fallback pixbuf (solid color) so game can continue
        gint w = 64;
        gint h = 64;
        GdkPixbuf *fallback = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, w, h);
        if (!fallback) return NULL;
        guchar *pixels = gdk_pixbuf_get_pixels(fallback);
        gint rowstride = gdk_pixbuf_get_rowstride(fallback);
        gint n_channels = gdk_pixbuf_get_n_channels(fallback);

        // Choose color based on filename hint
        guint8 r = 100, g = 100, b = 100, a = 255;
        if (g_strrstr(filename, "car") != NULL) { r = 0; g = 100; b = 255; }
        else if (g_strrstr(filename, "obstacle") != NULL) { r = 220; g = 50; b = 50; }
        else if (g_strrstr(filename, "background") != NULL) { r = 20; g = 40; b = 80; }

        for (gint y = 0; y < h; y++) {
            guchar *p = pixels + y * rowstride;
            for (gint x = 0; x < w; x++) {
                p[0] = r;
                p[1] = g;
                p[2] = b;
                p[3] = a;
                p += n_channels;
            }
        }
        return fallback;
    }
    
    return pixbuf;
}

// Draw a pixbuf (image) to cairo context
void graphics_draw_pixbuf(cairo_t *cr, GdkPixbuf *pixbuf, gdouble x, gdouble y, gdouble width, gdouble height) {
    if (!pixbuf) return;
    
    gint pixbuf_width = gdk_pixbuf_get_width(pixbuf);
    gint pixbuf_height = gdk_pixbuf_get_height(pixbuf);
    
    // Scale pixbuf to desired size
    GdkPixbuf *scaled = gdk_pixbuf_scale_simple(pixbuf, (gint)width, (gint)height, GDK_INTERP_BILINEAR);
    
    // Draw to cairo
    gdk_cairo_set_source_pixbuf(cr, scaled, x, y);
    cairo_paint(cr);
    
    g_object_unref(scaled);
}
