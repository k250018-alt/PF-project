#include "player.h"
#include "graphics.h"
#include <cairo.h>
#include <math.h>

// Movement constants
#define TURN_SPEED 7.0           // radians per second
#define ACCELERATION 500.0       // units per second squared
#define BRAKE_FORCE 300.0        // units per second squared
#define FRICTION 3.0             // exponential damping per second
#define MAX_SPEED 800.0          // maximum velocity magnitude

Player* player_new(gdouble start_x, gdouble start_y, GdkPixbuf *sprite) {
    Player *player = g_malloc(sizeof(Player));
    player->x = start_x;
    player->y = start_y;
    player->width = 50;
    player->height = 60;
    player->velocity_x = 0.0;
    player->velocity_y = 0.0;
    player->speed = 0.0;
    player->max_speed = MAX_SPEED;
    player->angle = -M_PI / 2.0;  // Start facing up
    player->angular_velocity = 0.0;
    player->lateral_damping = 0.0;
    player->sprite = sprite ? g_object_ref(sprite) : NULL;
    return player;
}

void player_update(Player *player, gdouble delta_time, gint width, gint height) {
    // Normalize angle to [-PI, PI]
    while (player->angle > M_PI) player->angle -= 2.0 * M_PI;
    while (player->angle < -M_PI) player->angle += 2.0 * M_PI;

    // Apply friction damping
    gdouble friction_factor = exp(-FRICTION * delta_time);
    player->velocity_x *= friction_factor;
    player->velocity_y *= friction_factor;

    // Clamp speed to max
    gdouble speed = sqrt(player->velocity_x * player->velocity_x + player->velocity_y * player->velocity_y);
    if (speed > player->max_speed) {
        gdouble scale = player->max_speed / speed;
        player->velocity_x *= scale;
        player->velocity_y *= scale;
    }

    // Update position
    player->x += player->velocity_x * delta_time;
    player->y += player->velocity_y * delta_time;

    // Screen boundaries
    if (player->x < 0) player->x = 0;
    if (player->x + player->width > width) player->x = width - player->width;
    if (player->y < 0) player->y = 0;
    if (player->y + player->height > height) player->y = height - player->height;
}

void player_move_left(Player *player, gdouble delta_time) {
    player->angle -= TURN_SPEED * delta_time;
}

void player_move_right(Player *player, gdouble delta_time) {
    player->angle += TURN_SPEED * delta_time;
}

void player_move_up(Player *player, gdouble delta_time) {
    gdouble vx = cos(player->angle) * ACCELERATION * delta_time;
    gdouble vy = sin(player->angle) * ACCELERATION * delta_time;
    player->velocity_x += vx;
    player->velocity_y += vy;
}

void player_move_down(Player *player, gdouble delta_time) {
    gdouble vx = cos(player->angle) * (-BRAKE_FORCE) * delta_time;
    gdouble vy = sin(player->angle) * (-BRAKE_FORCE) * delta_time;
    player->velocity_x += vx;
    player->velocity_y += vy;
}

void player_stop_x(Player *player) {
    // No-op; friction handled in update
}

void player_stop_y(Player *player) {
    // No-op; friction handled in update
}

void player_draw(Player *player, cairo_t *cr) {
    if (!player) return;

    gdouble cx = player->x + player->width / 2.0;
    gdouble cy = player->y + player->height / 2.0;

    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, player->angle);
    cairo_translate(cr, -player->width / 2.0, -player->height / 2.0);

    if (player->sprite) {
        GdkPixbuf *scaled = gdk_pixbuf_scale_simple(player->sprite, 
                                                     (gint)player->width, 
                                                     (gint)player->height, 
                                                     GDK_INTERP_BILINEAR);
        if (scaled) {
            gdk_cairo_set_source_pixbuf(cr, scaled, 0, 0);
            cairo_paint(cr);
            g_object_unref(scaled);
        }
    } else {
        // Draw red car from scratch (realistic top-down view)
        // Main body (red)
        cairo_set_source_rgb(cr, 0.85, 0.05, 0.05);  // Dark red
        
        // Car body outline (rounded rectangle shape)
        cairo_move_to(cr, 10, 5);
        cairo_line_to(cr, 40, 5);
        cairo_arc(cr, 40, 10, 5, -M_PI/2, 0);
        cairo_line_to(cr, 45, 50);
        cairo_arc(cr, 40, 55, 5, 0, M_PI/2);
        cairo_line_to(cr, 10, 60);
        cairo_arc(cr, 10, 55, 5, M_PI/2, M_PI);
        cairo_line_to(cr, 5, 10);
        cairo_arc(cr, 10, 5, 5, M_PI, 3*M_PI/2);
        cairo_close_path(cr);
        cairo_fill(cr);
        
        // Windshield (front window - light blue-gray)
        cairo_set_source_rgb(cr, 0.6, 0.7, 0.85);
        cairo_move_to(cr, 12, 8);
        cairo_line_to(cr, 38, 8);
        cairo_line_to(cr, 36, 20);
        cairo_line_to(cr, 14, 20);
        cairo_close_path(cr);
        cairo_fill(cr);
        
        // Rear window (darker blue-gray)
        cairo_set_source_rgb(cr, 0.5, 0.6, 0.75);
        cairo_rectangle(cr, 11, 40, 28, 12);
        cairo_fill(cr);
        
        // Left headlight (yellow)
        cairo_set_source_rgb(cr, 1.0, 0.9, 0.2);
        cairo_arc(cr, 15, 6, 2.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Right headlight (yellow)
        cairo_arc(cr, 35, 6, 2.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Left wheel (dark gray/black)
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_arc(cr, 15, 18, 6, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Left wheel rim (lighter gray)
        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
        cairo_arc(cr, 15, 18, 3.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Right wheel (dark gray/black)
        cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
        cairo_arc(cr, 35, 18, 6, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Right wheel rim (lighter gray)
        cairo_set_source_rgb(cr, 0.4, 0.4, 0.4);
        cairo_arc(cr, 35, 18, 3.5, 0, 2 * M_PI);
        cairo_fill(cr);
        
        // Rear lights (red)
        cairo_set_source_rgb(cr, 0.9, 0.1, 0.1);
        cairo_arc(cr, 15, 58, 2, 0, 2 * M_PI);
        cairo_fill(cr);
        cairo_arc(cr, 35, 58, 2, 0, 2 * M_PI);
        cairo_fill(cr);
    }

    // Front indicator (yellow triangle at top of car)
    cairo_save(cr);
    cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);  // Yellow
    cairo_move_to(cr, player->width / 2.0 - 5, 0);
    cairo_line_to(cr, player->width / 2.0 + 5, 0);
    cairo_line_to(cr, player->width / 2.0, -8);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_restore(cr);

    cairo_restore(cr);
}

void player_free(Player *player) {
    if (!player) return;
    if (player->sprite) {
        g_object_unref(player->sprite);
    }
    g_free(player);
}
