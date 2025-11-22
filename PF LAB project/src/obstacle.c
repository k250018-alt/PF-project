#include "obstacle.h"
#include "graphics.h"
#include "game.h"
#include <stdlib.h>
#include <time.h>

Obstacle* obstacle_new(gdouble x, gdouble y, gdouble width, gdouble height, gdouble velocity, GdkPixbuf *sprite) {
    Obstacle *obstacle = g_malloc(sizeof(Obstacle));
    obstacle->x = x;
    obstacle->y = y;
    obstacle->width = width;
    obstacle->height = height;
    obstacle->velocity = velocity;
    obstacle->active = TRUE;
    obstacle->sprite = sprite ? g_object_ref(sprite) : NULL;
    return obstacle;
}

ObstacleManager* obstacle_manager_new(void) {
    ObstacleManager *manager = g_malloc(sizeof(ObstacleManager));
    manager->obstacles = g_ptr_array_new();
    manager->spawn_timer = 0;
    manager->spawn_interval = 1.5;  // Spawn every 1.5 seconds
    manager->obstacle_speed = 250.0;
    manager->sprite_templates = g_ptr_array_new();
    /* seed RNG once */
    srand((unsigned)time(NULL));
    return manager;
}

void obstacle_manager_update(ObstacleManager *manager, gdouble delta_time, gint height) {
    for (guint i = 0; i < manager->obstacles->len; i++) {
        Obstacle *obstacle = g_ptr_array_index(manager->obstacles, i);
        
        // Update position
        obstacle->y += obstacle->velocity * delta_time;
        
        // Mark as inactive if off-screen
        if (obstacle->y > height) {
            obstacle->active = FALSE;
        }
    }
    
    // Remove inactive obstacles
    for (guint i = manager->obstacles->len; i > 0; i--) {
        Obstacle *obstacle = g_ptr_array_index(manager->obstacles, i - 1);
        if (!obstacle->active) {
            obstacle_free(obstacle);
            g_ptr_array_remove_index(manager->obstacles, i - 1);
        }
    }
}

void obstacle_manager_spawn(ObstacleManager *manager, gint width, gint height) {
    manager->spawn_timer -= 0.016;  // ~60 FPS

    if (manager->spawn_timer <= 0) {
        /* Choose obstacle type: 0=small fast, 1=medium, 2=large slow */
        int type = rand() % 3;
        gdouble w, h, vel;
        /* Base sizes, then scale up by ~35% to increase obstacle visibility */
        if (type == 0) {
            w = 30; h = 30; vel = manager->obstacle_speed * 1.4;
        } else if (type == 1) {
            w = 40; h = 40; vel = manager->obstacle_speed;
        } else {
            w = 70; h = 50; vel = manager->obstacle_speed * 0.75;
        }
        const gdouble SIZE_SCALE = 1.35; /* 35% larger */
        w *= SIZE_SCALE;
        h *= SIZE_SCALE;

        /* Random x position constrained by obstacle width */
        gint max_x = (width - (gint)w);
        if (max_x < 0) max_x = 0;
        gdouble x = (max_x > 0) ? (rand() % max_x) : 0;

        /* Pick a random sprite template if available */
        GdkPixbuf *chosen = NULL;
        if (manager->sprite_templates && manager->sprite_templates->len > 0) {
            guint idx = rand() % manager->sprite_templates->len;
            chosen = g_ptr_array_index(manager->sprite_templates, idx);
        }

        Obstacle *obstacle = obstacle_new(x, -h - 10, w, h, vel, chosen);
        g_ptr_array_add(manager->obstacles, obstacle);

        manager->spawn_timer = manager->spawn_interval;
    }
}

void obstacle_manager_draw(ObstacleManager *manager, cairo_t *cr) {
    graphics_set_color(cr, COLOR_RED);
    
    for (guint i = 0; i < manager->obstacles->len; i++) {
        Obstacle *obstacle = g_ptr_array_index(manager->obstacles, i);
        if (obstacle->sprite) {
            graphics_draw_pixbuf(cr, obstacle->sprite, obstacle->x, obstacle->y, obstacle->width, obstacle->height);
        } else {
            graphics_fill_rectangle(cr, obstacle->x, obstacle->y, obstacle->width, obstacle->height);
            graphics_set_color(cr, COLOR_YELLOW);
            graphics_draw_rectangle(cr, obstacle->x, obstacle->y, obstacle->width, obstacle->height);
            graphics_set_color(cr, COLOR_RED);
        }
    }
}

void obstacle_free(Obstacle *obstacle) {
    if (obstacle->sprite) {
        g_object_unref(obstacle->sprite);
    }
    g_free(obstacle);
}

void obstacle_manager_free(ObstacleManager *manager) {
    for (guint i = 0; i < manager->obstacles->len; i++) {
        Obstacle *obstacle = g_ptr_array_index(manager->obstacles, i);
        obstacle_free(obstacle);
    }
    g_ptr_array_free(manager->obstacles, TRUE);
    if (manager->sprite_templates) {
        for (guint i = 0; i < manager->sprite_templates->len; i++) {
            GdkPixbuf *pb = g_ptr_array_index(manager->sprite_templates, i);
            if (pb) g_object_unref(pb);
        }
        g_ptr_array_free(manager->sprite_templates, TRUE);
    }
    g_free(manager);
}
