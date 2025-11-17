#include <gtk/gtk.h>
#include <cairo.h>
#include <string.h>
#include <math.h>
#include "game.h"
#include "player.h"
#include "obstacle.h"
#include "graphics.h"

static Game *game_instance = NULL;
static Player *player = NULL;
static ObstacleManager *obstacle_manager = NULL;

static GdkPixbuf *background_image = NULL;
static GdkPixbuf *car_sprite = NULL;
static GdkPixbuf *obstacle_sprite = NULL;

// Forward declarations for menu drawing functions
static void draw_main_menu(cairo_t *cr);
static void draw_pause_menu(cairo_t *cr);
static void draw_game_over_menu(cairo_t *cr, gint score);
static void draw_controls_screen(cairo_t *cr);

// Input handling with key tracking
static gboolean key_press_handler(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    Game *game = (Game *)user_data;
    
    // Track key state for smooth movement
    switch (event->keyval) {
        case GDK_KEY_Left:
            game->keys_pressed[0] = TRUE;  // Left
            return TRUE;
        case GDK_KEY_Right:
            game->keys_pressed[1] = TRUE;  // Right
            return TRUE;
        case GDK_KEY_Up:
            // Menu navigation when in menu
            game->keys_pressed[2] = TRUE;  // Up
            if (game->state->screen_state == GAME_STATE_MENU) {
                if (game->menu_selected > 0) game->menu_selected--;
                return TRUE;
            }
            return TRUE;
        case GDK_KEY_Down:
            // Menu navigation when in menu
            game->keys_pressed[3] = TRUE;  // Down
            if (game->state->screen_state == GAME_STATE_MENU) {
                if (game->menu_selected < 2) game->menu_selected++;
                return TRUE;
            }
            return TRUE;
        case GDK_KEY_space:
                    if (game->state->screen_state == GAME_STATE_MENU) {
                        // Activate selected menu item
                        if (game->menu_selected == 0) {
                            // Start playing: reset and switch to PLAYING
                            game_reset(game);
                            game->state->screen_state = GAME_STATE_PLAYING;
                            if (game->drawing_area) gtk_widget_grab_focus(game->drawing_area);
                        } else if (game->menu_selected == 1) {
                            // Controls
                            game->state->screen_state = GAME_STATE_CONTROLS;
                            if (game->drawing_area) gtk_widget_grab_focus(game->drawing_area);
                        } else if (game->menu_selected == 2) {
                            // Quit
                            game_stop(game);
                        }
                    } else if (game->state->screen_state == GAME_STATE_CONTROLS) {
                        // Return to main menu from controls
                        game->state->screen_state = GAME_STATE_MENU;
                        if (game->drawing_area) gtk_widget_grab_focus(game->drawing_area);
                    } else if (game->state->screen_state == GAME_STATE_PLAYING) {
                game_pause(game);
            } else if (game->state->screen_state == GAME_STATE_PAUSED) {
                game_resume(game);
            } else if (game->state->screen_state == GAME_STATE_GAME_OVER) {
                // Restart immediately: reset game state and start playing
                game_reset(game);
                game->state->screen_state = GAME_STATE_PLAYING;
            }
            return TRUE;
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            // Treat Enter like Space
            if (game->state->screen_state == GAME_STATE_MENU) {
                if (game->menu_selected == 0) {
                    game_reset(game);
                    game->state->screen_state = GAME_STATE_PLAYING;
                } else if (game->menu_selected == 1) {
                    game->state->screen_state = GAME_STATE_CONTROLS;
                } else if (game->menu_selected == 2) {
                    game_stop(game);
                }
            } else if (game->state->screen_state == GAME_STATE_CONTROLS) {
                // Back to menu
                game->state->screen_state = GAME_STATE_MENU;
            }
            return TRUE;
        case GDK_KEY_Escape:
            if (game->state->screen_state == GAME_STATE_PAUSED) {
                game->state->screen_state = GAME_STATE_MENU;
            } else if (game->state->screen_state == GAME_STATE_PLAYING) {
                game_pause(game);
                if (game->drawing_area) gtk_widget_grab_focus(game->drawing_area);
            } else if (game->state->screen_state == GAME_STATE_CONTROLS) {
                game->state->screen_state = GAME_STATE_MENU;
            } else {
                game_stop(game);
            }
            return TRUE;
    }
    return FALSE;
}

static gboolean key_release_handler(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    Game *game = (Game *)user_data;
    
    switch (event->keyval) {
        case GDK_KEY_Left:
            game->keys_pressed[0] = FALSE;
            return TRUE;
        case GDK_KEY_Right:
            game->keys_pressed[1] = FALSE;
            return TRUE;
        case GDK_KEY_Up:
            game->keys_pressed[2] = FALSE;
            return TRUE;
        case GDK_KEY_Down:
            game->keys_pressed[3] = FALSE;
            return TRUE;
    }
    return FALSE;
}

// Update player movement based on held keys
static void update_player_input(Game *game, gdouble delta_time) {
    if (!player || game->state->screen_state != GAME_STATE_PLAYING) return;
    
    gboolean moving_left = game->keys_pressed[0];
    gboolean moving_right = game->keys_pressed[1];
    gboolean moving_up = game->keys_pressed[2];
    gboolean moving_down = game->keys_pressed[3];
    
    // Apply movement each frame based on held keys
    // Left/Right: turning
    if (moving_left) {
        player_move_left(player, delta_time);
    }
    if (moving_right) {
        player_move_right(player, delta_time);
    }
    
    // Up/Down: acceleration / braking
    if (moving_up) {
        player_move_up(player, delta_time);
    }
    if (moving_down) {
        player_move_down(player, delta_time);
    }
}

// Drawing callback
static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    Game *game = (Game *)user_data;
    
    // Draw background
    if (background_image) {
        graphics_draw_pixbuf(cr, background_image, 0, 0, GAME_WIDTH, GAME_HEIGHT);
    } else {
        graphics_clear_canvas(cr, COLOR_BLACK);
    }
    
    switch (game->state->screen_state) {
        case GAME_STATE_MENU:
            draw_main_menu(cr);
            break;
        case GAME_STATE_PLAYING:
            if (player) player_draw(player, cr);
            if (obstacle_manager) obstacle_manager_draw(obstacle_manager, cr, obstacle_sprite);
            
            // Draw HUD
            graphics_set_color(cr, COLOR_WHITE);
            gchar score_text[50];
            g_snprintf(score_text, sizeof(score_text), "Score: %d | Level: %d", 
                       game->state->score, game->state->level);
            graphics_draw_text(cr, score_text, 10, 20, 16);

            // Debug overlay: show player angle and velocities
            if (player) {
                gdouble angle_deg = player->angle * (180.0 / M_PI);
                gdouble vx = player->velocity_x;
                gdouble vy = player->velocity_y;
                gdouble fwd = vx * cos(player->angle) + vy * sin(player->angle);
                gchar debug_text[128];
                g_snprintf(debug_text, sizeof(debug_text), "Angle: %.2f deg  Vx: %.1f  Vy: %.1f  Fwd: %.1f", angle_deg, vx, vy, fwd);
                graphics_set_color(cr, COLOR_WHITE);
                graphics_draw_text(cr, debug_text, GAME_WIDTH - 420, 20, 14);
            }
            break;
        case GAME_STATE_PAUSED:
            if (player) player_draw(player, cr);
            if (obstacle_manager) obstacle_manager_draw(obstacle_manager, cr, obstacle_sprite);
            
            draw_pause_menu(cr);
            break;
        case GAME_STATE_CONTROLS:
            draw_controls_screen(cr);
            break;
        case GAME_STATE_GAME_OVER:
            draw_game_over_menu(cr, game->state->score);
            break;
    }
    
    return FALSE;
}

// Draw main menu
static void draw_main_menu(cairo_t *cr) {
    // Dark gradient-like background
    graphics_set_color(cr, COLOR_DARK_BLUE);
    graphics_fill_rectangle(cr, 0, 0, GAME_WIDTH, GAME_HEIGHT);
    
    // Title with shadow effect
    graphics_set_color(cr, COLOR_BLACK);
    graphics_draw_text_centered(cr, "CAR GAME", GAME_WIDTH/2 + 2, 100 + 2, 56);
    graphics_set_color(cr, COLOR_YELLOW);
    graphics_draw_text_centered(cr, "CAR GAME", GAME_WIDTH/2, 100, 56);
    
    // Subtitle
    graphics_set_color(cr, COLOR_LIGHT_GRAY);
    graphics_draw_text_centered(cr, "Avoid the Red Obstacles!", GAME_WIDTH/2, 165, 18);
    
    // Menu options (Start, Controls, Quit) with nicer translucent rounded highlight
    const gint start_x = GAME_WIDTH/2;
    const gint start_y = 230;
    const gint option_gap = 70;
    gint selected = 0;
    if (game_instance) selected = game_instance->menu_selected;

    // Helper to draw rounded rect background for selected item
    auto_draw_highlight:
    {
        // draw highlight box if selected
        if (selected >= 0) {
            gdouble box_w = 340;
            gdouble box_h = 56;
            gdouble cx = start_x - box_w/2;
            gdouble cy = start_y - box_h/2 + (selected * option_gap);
            cairo_save(cr);
            cairo_set_source_rgba(cr, 1.0, 0.9, 0.0, 0.18); // subtle yellow
            double radius = 12.0;
            double x = cx, y = cy, w = box_w, h = box_h;
            cairo_new_path(cr);
            cairo_arc(cr, x + w - radius, y + radius, radius, -M_PI/2, 0);
            cairo_arc(cr, x + w - radius, y + h - radius, radius, 0, M_PI/2);
            cairo_arc(cr, x + radius, y + h - radius, radius, M_PI/2, M_PI);
            cairo_arc(cr, x + radius, y + radius, radius, M_PI, 3*M_PI/2);
            cairo_close_path(cr);
            cairo_fill(cr);
            cairo_restore(cr);
        }
    }

    // Draw options text
    // Start
    if (selected == 0) graphics_set_color(cr, COLOR_BLACK);
    else graphics_set_color(cr, COLOR_YELLOW);
    graphics_draw_text_centered(cr, "Start Game", start_x, start_y, 28);

    // Controls
    if (selected == 1) graphics_set_color(cr, COLOR_BLACK);
    else graphics_set_color(cr, COLOR_LIGHT_GRAY);
    graphics_draw_text_centered(cr, "Controls", start_x, start_y + option_gap, 28);

    // Quit
    if (selected == 2) graphics_set_color(cr, COLOR_BLACK);
    else graphics_set_color(cr, COLOR_LIGHT_GRAY);
    graphics_draw_text_centered(cr, "Quit", start_x, start_y + option_gap*2, 28);
    
    /* Instructions removed from main menu to keep UI minimal */
    
    // Footer
    graphics_set_color(cr, COLOR_GRAY);
    graphics_draw_text_centered(cr, "Survive and avoid obstacles to score points!", GAME_WIDTH/2, 550, 12);
}

// Draw pause menu
static void draw_pause_menu(cairo_t *cr) {
    // Semi-transparent overlay
    graphics_set_color(cr, COLOR_BLACK);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.6);
    graphics_fill_rectangle(cr, 0, 0, GAME_WIDTH, GAME_HEIGHT);
    
    // Pause box
    gdouble box_width = 400;
    gdouble box_height = 200;
    gdouble box_x = (GAME_WIDTH - box_width) / 2;
    gdouble box_y = (GAME_HEIGHT - box_height) / 2 - 50;
    
    graphics_set_color(cr, COLOR_DARK_BLUE);
    graphics_fill_rectangle(cr, box_x, box_y, box_width, box_height);
    graphics_set_color(cr, COLOR_YELLOW);
    cairo_set_line_width(cr, 3.0);
    graphics_draw_rectangle(cr, box_x, box_y, box_width, box_height);
    
    // Text
    graphics_set_color(cr, COLOR_YELLOW);
    graphics_draw_text_centered(cr, "PAUSED", GAME_WIDTH/2, box_y + 50, 40);
    
    graphics_set_color(cr, COLOR_WHITE);
    graphics_draw_text_centered(cr, "Press SPACE to Resume", GAME_WIDTH/2, box_y + 110, 18);
    graphics_draw_text_centered(cr, "Press ESC for Menu", GAME_WIDTH/2, box_y + 140, 18);
}

// Draw game over menu
static void draw_game_over_menu(cairo_t *cr, gint score) {
    // Background
    graphics_set_color(cr, COLOR_DARK_BLUE);
    graphics_fill_rectangle(cr, 0, 0, GAME_WIDTH, GAME_HEIGHT);
    
    // Game Over box
    gdouble box_width = 450;
    gdouble box_height = 280;
    gdouble box_x = (GAME_WIDTH - box_width) / 2;
    gdouble box_y = (GAME_HEIGHT - box_height) / 2 - 30;
    
    graphics_set_color(cr, COLOR_RED);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.1);
    graphics_fill_rectangle(cr, box_x, box_y, box_width, box_height);
    
    graphics_set_color(cr, COLOR_RED);
    cairo_set_line_width(cr, 3.0);
    graphics_draw_rectangle(cr, box_x, box_y, box_width, box_height);
    
    // Game Over text with shadow
    graphics_set_color(cr, COLOR_BLACK);
    graphics_draw_text_centered(cr, "GAME OVER", GAME_WIDTH/2 + 2, box_y + 50 + 2, 48);
    graphics_set_color(cr, COLOR_RED);
    graphics_draw_text_centered(cr, "GAME OVER", GAME_WIDTH/2, box_y + 50, 48);
    
    // Score display
    graphics_set_color(cr, COLOR_YELLOW);
    gchar score_text[100];
    g_snprintf(score_text, sizeof(score_text), "Final Score: %d", score);
    graphics_draw_text_centered(cr, score_text, GAME_WIDTH/2, box_y + 110, 28);
    
    // Instructions
    graphics_set_color(cr, COLOR_WHITE);
    graphics_draw_text_centered(cr, "Press SPACE to Play Again", GAME_WIDTH/2, box_y + 160, 18);
    graphics_draw_text_centered(cr, "Press ESC to Menu", GAME_WIDTH/2, box_y + 190, 18);
    
    // Footer
    graphics_set_color(cr, COLOR_GRAY);
    graphics_draw_text_centered(cr, "Try to beat your score next time!", GAME_WIDTH/2, box_y + 240, 12);
}

// Draw controls screen
static void draw_controls_screen(cairo_t *cr) {
    graphics_set_color(cr, COLOR_DARK_BLUE);
    graphics_fill_rectangle(cr, 0, 0, GAME_WIDTH, GAME_HEIGHT);

    graphics_set_color(cr, COLOR_YELLOW);
    graphics_draw_text_centered(cr, "Controls", GAME_WIDTH/2, 80, 40);

    graphics_set_color(cr, COLOR_WHITE);
    graphics_draw_text_centered(cr, "Arrow Keys - Move", GAME_WIDTH/2, 160, 20);
    graphics_draw_text_centered(cr, "Space - Pause/Select", GAME_WIDTH/2, 200, 20);
    graphics_draw_text_centered(cr, "Esc - Back/Quit", GAME_WIDTH/2, 240, 20);

    // Back hint
    graphics_set_color(cr, COLOR_GRAY);
    graphics_draw_text_centered(cr, "Press SPACE or Enter to return", GAME_WIDTH/2, GAME_HEIGHT - 80, 14);
}

// Game loop timer
static gboolean game_loop(gpointer user_data) {
    Game *game = (Game *)user_data;

    // Always process input so menus respond to keys
    update_player_input(game, FRAME_TIME / 1000.0);

    // Only update game logic when actively playing
    if (game->state->screen_state == GAME_STATE_PLAYING) {
        gdouble dt = FRAME_TIME / 1000.0;
        game_update(game, dt);
        game->state->is_running = TRUE; // ensure loop keeps running while playing
    }

    // If we're on menu / paused / game over, do not update game logic but keep drawing
    if (game && game->drawing_area && GTK_IS_WIDGET(game->drawing_area)) {
        gtk_widget_queue_draw(game->drawing_area);
    }

    return TRUE;
}

// Window close handler
static gboolean on_window_destroy(GtkWidget *widget, gpointer user_data) {
    game_instance = NULL;
    gtk_main_quit();
    return FALSE;
}

Game* game_new(void) {
    Game *game = g_malloc(sizeof(Game));
    game->state = g_malloc(sizeof(GameState));
    game->state->score = 0;
    game->state->level = 1;
    game->state->is_running = FALSE;
    game->state->is_paused = FALSE;
    game->state->screen_state = GAME_STATE_MENU;
    game->timer_id = 0;
    memset(game->keys_pressed, 0, sizeof(game->keys_pressed));
    game->menu_selected = 0;
    
    game_instance = game;
    return game;
}

void game_init(Game *game) {
    // Create main window
    game->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(game->window), "Car Game");
    gtk_window_set_default_size(GTK_WINDOW(game->window), GAME_WIDTH, GAME_HEIGHT);
    gtk_window_set_position(GTK_WINDOW(game->window), GTK_WIN_POS_CENTER);
    gtk_widget_set_app_paintable(game->window, TRUE);
    
    // Load image assets (paths relative to build/ when running from build)
    background_image = graphics_load_image("../assets/background.png");
    car_sprite = graphics_load_image("../assets/car.png");
    obstacle_sprite = graphics_load_image("../assets/obstacle.png");
    
    g_signal_connect(game->window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    // Create drawing area
    game->drawing_area = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(game->window), game->drawing_area);
    g_signal_connect(game->drawing_area, "draw", G_CALLBACK(draw_callback), game);
    
    // Ensure drawing area receives key events and has focus
    gtk_widget_set_can_focus(game->drawing_area, TRUE);
    gtk_widget_grab_focus(game->drawing_area);
    g_signal_connect(game->drawing_area, "key-press-event", G_CALLBACK(key_press_handler), game);
    g_signal_connect(game->drawing_area, "key-release-event", G_CALLBACK(key_release_handler), game);
    
    // Initialize game objects only when game starts
    player = NULL;
    obstacle_manager = NULL;
}

void game_start(Game *game) {
    // Start the main loop and show the window. Game objects (player/obstacles)
    // are created when the player actually starts the game via the menu.
    game->state->is_running = TRUE;
    game->state->screen_state = GAME_STATE_MENU;
    if (!game->timer_id) {
        game->timer_id = g_timeout_add(FRAME_TIME, game_loop, game);
    }
    gtk_widget_show_all(game->window);
}

void game_reset(Game *game) {
    // Reset score and level
    game->state->score = 0;
    game->state->level = 1;
    
    // Reset player
    if (player) {
        player_free(player);
    }
    player = player_new(GAME_WIDTH / 2 - 25, GAME_HEIGHT - 100, car_sprite);
    
    // Reset obstacles
    if (obstacle_manager) {
        obstacle_manager_free(obstacle_manager);
    }
    obstacle_manager = obstacle_manager_new();
    
    // Clear key states
    memset(game->keys_pressed, 0, sizeof(game->keys_pressed));
}

void game_stop(Game *game) {
    game->state->is_running = FALSE;
    if (game->timer_id) {
        g_source_remove(game->timer_id);
        game->timer_id = 0;
    }
    gtk_main_quit();
}

void game_pause(Game *game) {
    if (game->state->screen_state == GAME_STATE_PLAYING) {
        game->state->screen_state = GAME_STATE_PAUSED;
    }
}

void game_resume(Game *game) {
    if (game->state->screen_state == GAME_STATE_PAUSED) {
        game->state->screen_state = GAME_STATE_PLAYING;
    }
}

// Helper function to check rectangle collision
static gboolean check_collision(gdouble x1, gdouble y1, gdouble w1, gdouble h1,
                                gdouble x2, gdouble y2, gdouble w2, gdouble h2) {
    return !(x1 + w1 < x2 || x2 + w2 < x1 || y1 + h1 < y2 || y2 + h2 < y1);
}

void game_update(Game *game, gdouble delta_time) {
    if (!player || !obstacle_manager) return;
    
    // Update player
    player_update(player, delta_time, GAME_WIDTH, GAME_HEIGHT);
    
    // Update obstacles
    obstacle_manager_update(obstacle_manager, delta_time, GAME_HEIGHT);
    
    // Spawn new obstacles
    obstacle_manager_spawn(obstacle_manager, GAME_WIDTH, GAME_HEIGHT, obstacle_sprite);
    
    // Collision detection
    for (guint i = 0; i < obstacle_manager->obstacles->len; i++) {
        Obstacle *obs = g_ptr_array_index(obstacle_manager->obstacles, i);
        
        if (check_collision(player->x, player->y, player->width, player->height,
                           obs->x, obs->y, obs->width, obs->height)) {
            // Collision detected -> switch to GAME_OVER screen but keep loop running so it renders
            game->state->screen_state = GAME_STATE_GAME_OVER;
            // Optionally stop further gameplay updates by returning early
            return;
        }
    }
    
    // Increase score for survival (every obstacle passed)
    game->state->score += 1;
    
    // Level up every 1000 points
    if (game->state->score % 1000 == 0 && game->state->score > 0) {
        game->state->level++;
        // Increase obstacle speed
        obstacle_manager->obstacle_speed += 50.0;
        obstacle_manager->spawn_interval *= 0.9;  // Spawn more frequently
    }
}

void game_render(Game *game, cairo_t *cr) {
    // This is called from draw_callback
}

void game_cleanup(Game *game) {
    if (player) {
        player_free(player);
        player = NULL;
    }
    if (obstacle_manager) {
        obstacle_manager_free(obstacle_manager);
        obstacle_manager = NULL;
    }
    if (game->state) {
        g_free(game->state);
    }
    if (game->timer_id) {
        g_source_remove(game->timer_id);
    }
    g_free(game);
}
