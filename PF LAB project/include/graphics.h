#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <cairo.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

// Color definitions
typedef struct {
    gdouble r;
    gdouble g;
    gdouble b;
    gdouble a;
} Color;

// Predefined colors
extern const Color COLOR_BLACK;
extern const Color COLOR_WHITE;
extern const Color COLOR_RED;
extern const Color COLOR_BLUE;
extern const Color COLOR_GREEN;
extern const Color COLOR_YELLOW;
extern const Color COLOR_GRAY;
extern const Color COLOR_DARK_BLUE;
extern const Color COLOR_LIGHT_GRAY;

// Drawing functions
void graphics_set_color(cairo_t *cr, Color color);
void graphics_draw_rectangle(cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height);
void graphics_fill_rectangle(cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height);
void graphics_draw_circle(cairo_t *cr, gdouble x, gdouble y, gdouble radius);
void graphics_fill_circle(cairo_t *cr, gdouble x, gdouble y, gdouble radius);
void graphics_draw_text(cairo_t *cr, const gchar *text, gdouble x, gdouble y, gdouble size);
void graphics_draw_text_centered(cairo_t *cr, const gchar *text, gdouble center_x, gdouble y, gdouble size);
void graphics_clear_canvas(cairo_t *cr, Color color);
void graphics_draw_box(cairo_t *cr, gdouble x, gdouble y, gdouble width, gdouble height, Color border_color, Color fill_color, gdouble border_width);
GdkPixbuf* graphics_load_image(const gchar *filename);
void graphics_draw_pixbuf(cairo_t *cr, GdkPixbuf *pixbuf, gdouble x, gdouble y, gdouble width, gdouble height);

/* Draw text with a subtle shadow for readability */
void graphics_draw_text_with_shadow(cairo_t *cr, const gchar *text, gdouble x, gdouble y, gdouble size);

#endif // GRAPHICS_H
