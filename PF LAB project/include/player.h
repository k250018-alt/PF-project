#ifndef PLAYER_H
#define PLAYER_H

#include <glib.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct {
    gdouble x;
    gdouble y;
    gdouble width;
    gdouble height;
    gdouble velocity_x;
    gdouble velocity_y;
    gdouble speed;
    gdouble max_speed;
    gdouble angle; // facing direction in radians
    gdouble angular_velocity;
    // drift helper: lower lateral_damping => more slide
    gdouble lateral_damping;
    GdkPixbuf *sprite;
} Player;

// Player functions
Player* player_new(gdouble start_x, gdouble start_y, GdkPixbuf *sprite);
void player_update(Player *player, gdouble delta_time, gint width, gint height);
void player_move_left(Player *player, gdouble delta_time);
void player_move_right(Player *player, gdouble delta_time);
void player_move_up(Player *player, gdouble delta_time);
void player_move_down(Player *player, gdouble delta_time);
void player_stop_x(Player *player);
void player_stop_y(Player *player);
void player_draw(Player *player, cairo_t *cr);
void player_free(Player *player);

#endif // PLAYER_H
