#ifndef OBSTACLE_H
#define OBSTACLE_H

#include <glib.h>
#include <cairo.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

typedef struct {
    gdouble x;
    gdouble y;
    gdouble width;
    gdouble height;
    gdouble velocity;
    gboolean active;
    GdkPixbuf *sprite;
} Obstacle;

typedef struct {
    GPtrArray *obstacles;
    gdouble spawn_timer;
    gdouble spawn_interval;
    gdouble obstacle_speed;
    /* Multiple sprite templates to allow obstacle variety */
    GPtrArray *sprite_templates; /* array of GdkPixbuf* */
} ObstacleManager;

// Obstacle functions
Obstacle* obstacle_new(gdouble x, gdouble y, gdouble width, gdouble height, gdouble velocity, GdkPixbuf *sprite);
ObstacleManager* obstacle_manager_new(void);
void obstacle_manager_update(ObstacleManager *manager, gdouble delta_time, gint height);
void obstacle_manager_spawn(ObstacleManager *manager, gint width, gint height);
void obstacle_manager_draw(ObstacleManager *manager, cairo_t *cr);
void obstacle_free(Obstacle *obstacle);
void obstacle_manager_free(ObstacleManager *manager);

#endif // OBSTACLE_H
