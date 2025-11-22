#include <gtk/gtk.h>
#include <cairo.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
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
/* Additional obstacle variants */
static GdkPixbuf *obs_bags1 = NULL;
static GdkPixbuf *obs_barrel1 = NULL;
static GdkPixbuf *obs_barrel2 = NULL;
static GdkPixbuf *obs_barrels = NULL;

// Try multiple candidate paths when loading assets so the game finds images
// regardless of current working directory (build vs project root).
static GdkPixbuf* find_asset(const gchar *name) {
    const gchar *candidates[] = {"./assets/%s", "assets/%s", "../assets/%s", "%s"};
    for (guint i = 0; i < G_N_ELEMENTS(candidates); i++) {
        gchar *path = g_strdup_printf(candidates[i], name);
        if (g_file_test(path, G_FILE_TEST_EXISTS)) {
            GdkPixbuf *pb = graphics_load_image(path);
            g_free(path);
            return pb;
        }
        g_free(path);
    }
    // Fallback: attempt to load the raw name (may trigger fallback pixbuf inside graphics_load_image)
    return graphics_load_image(name);
}

/* Background scrolling state */
static gdouble bg_scroll = 0.0;
/* Speedup factor applied to major movement/score rates (20-30% increase) */
#define SPEEDUP_FACTOR 1.25
/* Background scroll: base then scaled by SPEEDUP_FACTOR */
static const gdouble BG_SCROLL_SPEED = 120.0 * SPEEDUP_FACTOR; /* pixels per second */

/* Score rate base (points per second) scaled by SPEEDUP_FACTOR */
#define SCORE_RATE_BASE (60.0 * SPEEDUP_FACTOR)

/* Accumulator for fractional score increments */
static gdouble score_accum = 0.0;

/* ============================================================================
   EXPONENTIAL DIFFICULTY SYSTEM
   
   The difficulty increases exponentially with score. This creates a smooth
   progression from easy to extreme as the player survives longer.
   ============================================================================ */

/* Base constants for exponential scaling */
#define BASE_SPEED 250.0           /* Base obstacle speed (px/s) before speedup */
#define BASE_SPAWN_INTERVAL 1.5    /* Base spawn interval (seconds) */
#define DIFFICULTY_K_SPEED 2000.0  /* Exponent divisor for speed scaling */
#define DIFFICULTY_K_SPAWN 1500.0  /* Exponent divisor for spawn scaling */
#define MAX_SPEED_MULT 3.0         /* Cap speed at 3x base */
#define MIN_SPAWN_INTERVAL 0.3     /* Minimum spawn interval to prevent impossibility */

/* Difficulty stages: score thresholds for stage transitions */
#define STAGE_1_EASY_MAX 500
#define STAGE_2_MEDIUM_MAX 1500
#define STAGE_3_HARD_MAX 3000
#define STAGE_4_VERYHARD_MAX 5000
/* Stage 5 (Extreme) is everything above 5000 */

/* Calculate current difficulty multipliers based on score using exponential formulas */
static void update_difficulty(GameState *state) {
    if (!state) return;
    
    gdouble score_norm = (gdouble)state->score;
    
    /* Exponential speed multiplier: base_speed * (1 + score/DIFFICULTY_K_SPEED)^1.5
       This makes speed increase noticeably but controllably. */
    gdouble speed_factor = 1.0 + (score_norm / DIFFICULTY_K_SPEED);
    state->current_speed_multiplier = pow(speed_factor, 1.5);
    if (state->current_speed_multiplier > MAX_SPEED_MULT) {
        state->current_speed_multiplier = MAX_SPEED_MULT;
    }
    
    /* Exponential spawn rate: base_interval / (1 + score/DIFFICULTY_K_SPAWN)^1.2
       Smaller interval = more frequent spawns. */
    gdouble spawn_factor = 1.0 + (score_norm / DIFFICULTY_K_SPAWN);
    state->current_spawn_multiplier = 1.0 / pow(spawn_factor, 1.2);
    if (state->current_spawn_multiplier < (MIN_SPAWN_INTERVAL / BASE_SPAWN_INTERVAL)) {
        state->current_spawn_multiplier = MIN_SPAWN_INTERVAL / BASE_SPAWN_INTERVAL;
    }
    
    /* Score multiplier: increases rewards as difficulty rises
       multiplier = 1.0 + (score / 3000.0)^0.8, capped at reasonable value */
    gdouble mult_factor = 1.0 + pow(score_norm / 3000.0, 0.8);
    if (mult_factor > 4.0) mult_factor = 4.0;
    state->score_multiplier = mult_factor;
    
    /* Determine difficulty stage based on score thresholds */
    if (score_norm < STAGE_1_EASY_MAX) {
        state->difficulty_stage = 1;
    } else if (score_norm < STAGE_2_MEDIUM_MAX) {
        state->difficulty_stage = 2;
    } else if (score_norm < STAGE_3_HARD_MAX) {
        state->difficulty_stage = 3;
    } else if (score_norm < STAGE_4_VERYHARD_MAX) {
        state->difficulty_stage = 4;
    } else {
        state->difficulty_stage = 5;
    }
}

/* Get a description of the current difficulty stage */
static const gchar* get_stage_name(gint stage) {
    switch (stage) {
        case 1: return "EASY";
        case 2: return "MEDIUM";
        case 3: return "HARD";
        case 4: return "VERY HARD";
        case 5: return "EXTREME";
        default: return "?";
    }
}
static gint load_highscore(void) {
    FILE *f = fopen("highscore.txt", "r");
    if (!f) return 0;
    gint val = 0;
    if (fscanf(f, "%d", &val) != 1) {
        val = 0;
    }
    fclose(f);
    return val;
}

static void save_highscore(gint score) {
    FILE *f = fopen("highscore.txt", "w");
    if (!f) return; // silently ignore write failures
    fprintf(f, "%d\n", score);
    fclose(f);
}

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
        case GDK_KEY_m:
        case GDK_KEY_M:
            /* Toggle movement mode: Arcade vs Physics (hybrid mode) */
            if (game && game->state) {
                game->state->arcade_mode = !game->state->arcade_mode;
                g_debug("Movement mode toggled: %s", game->state->arcade_mode ? "Arcade" : "Physics");
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
        case GDK_KEY_p:
        case GDK_KEY_P:
            if (game->state->screen_state == GAME_STATE_PLAYING) {
                game_pause(game);
            } else if (game->state->screen_state == GAME_STATE_PAUSED) {
                game_resume(game);
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
    if (game->state->arcade_mode) {
        /* Arcade movement: direct X/Y movement independent of rotation.
         * Use a constant speed and normalize diagonal movement so
         * diagonal speed equals single-axis speed.
         */
        const gdouble ARCADE_SPEED = 400.0; /* units per second */
        gdouble dir_x = 0.0;
        gdouble dir_y = 0.0;
        if (moving_left) dir_x -= 1.0;
        if (moving_right) dir_x += 1.0;
        if (moving_up) dir_y -= 1.0;    /* screen Y grows downward, so up is -1 */
        if (moving_down) dir_y += 1.0;

        if (dir_x == 0.0 && dir_y == 0.0) {
            /* No movement keys: stop immediately for tight arcade feel */
            player->velocity_x = 0.0;
            player->velocity_y = 0.0;
        } else {
            /* Normalize diagonal movement so magnitude == ARCADE_SPEED */
            gdouble len = sqrt(dir_x * dir_x + dir_y * dir_y);
            if (len > 0.0) {
                dir_x /= len;
                dir_y /= len;
            }
            player->velocity_x = dir_x * ARCADE_SPEED;
            player->velocity_y = dir_y * ARCADE_SPEED;
        }
    } else {
        /* Physics movement (existing behavior): turning + forward/backward acceleration */
        if (moving_left) {
            player_move_left(player, delta_time);
        }
        if (moving_right) {
            player_move_right(player, delta_time);
        }

        /* Up/Down: acceleration / braking */
        if (moving_up) {
            player_move_up(player, delta_time);
        }
        if (moving_down) {
            player_move_down(player, delta_time);
        }
    }
}

// Drawing callback
static gboolean draw_callback(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    Game *game = (Game *)user_data;
    
    // Draw scrolling background (if available). We draw two copies offset by GAME_HEIGHT
    if (background_image) {
        /* bg_scroll increases while playing; compute wrapped Y */
        gdouble y = fmod(bg_scroll, (gdouble)GAME_HEIGHT);
        if (y < 0) y += GAME_HEIGHT;

        /* Draw the tile above and the one at y so they loop seamlessly */
        gdouble y1 = y - GAME_HEIGHT;
        gdouble y2 = y;
        graphics_draw_pixbuf(cr, background_image, 0, (gint)y1, GAME_WIDTH, GAME_HEIGHT);
        graphics_draw_pixbuf(cr, background_image, 0, (gint)y2, GAME_WIDTH, GAME_HEIGHT);
    } else {
        graphics_clear_canvas(cr, COLOR_BLACK);
    }
    
    switch (game->state->screen_state) {
        case GAME_STATE_MENU:
            draw_main_menu(cr);
            break;
        case GAME_STATE_PLAYING:
            if (player) player_draw(player, cr);
            if (obstacle_manager) obstacle_manager_draw(obstacle_manager, cr);
            
            // Draw HUD (with shadow for readability)
            gchar score_text[120];
            g_snprintf(score_text, sizeof(score_text), "Score: %d (x%.2f)  High: %d | Level: %d", 
                       game->state->score, game->state->score_multiplier, game->state->highscore, game->state->level);
            graphics_draw_text_with_shadow(cr, score_text, 14, 24, 18);
            
            /* Display difficulty stage with color coding */
            gchar stage_text[64];
            g_snprintf(stage_text, sizeof(stage_text), "Difficulty: %s", get_stage_name(game->state->difficulty_stage));
            graphics_set_color(cr, COLOR_WHITE);
            graphics_draw_text(cr, stage_text, GAME_WIDTH - 280, 24, 14);

            /* Display movement mode (Arcade / Physics) */
            gchar mode_text[64];
            g_snprintf(mode_text, sizeof(mode_text), "Mode: %s", game->state->arcade_mode ? "Arcade" : "Physics");
            graphics_set_color(cr, COLOR_WHITE);
            graphics_draw_text(cr, mode_text, GAME_WIDTH - 140, 24, 14);

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
            if (obstacle_manager) obstacle_manager_draw(obstacle_manager, cr);
            
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

    // Draw buttons: Resume, Restart, Main Menu, Quit
    gdouble btn_w = 140;
    gdouble btn_h = 32;
    gdouble resume_x = box_x + 30;
    gdouble resume_y = box_y + 60;
    gdouble restart_x = box_x + 220;
    gdouble restart_y = resume_y;
    gdouble menu_x = box_x + 30;
    gdouble menu_y = box_y + 110;
    gdouble quit_x = box_x + 220;
    gdouble quit_y = menu_y;

    graphics_set_color(cr, COLOR_DARK_BLUE);
    graphics_fill_rectangle(cr, resume_x, resume_y, btn_w, btn_h);
    graphics_fill_rectangle(cr, restart_x, restart_y, btn_w, btn_h);
    graphics_fill_rectangle(cr, menu_x, menu_y, btn_w, btn_h);
    graphics_fill_rectangle(cr, quit_x, quit_y, btn_w, btn_h);

    graphics_set_color(cr, COLOR_YELLOW);
    graphics_draw_text_centered(cr, "Resume", resume_x + btn_w/2, resume_y + 20, 14);
    graphics_draw_text_centered(cr, "Restart", restart_x + btn_w/2, restart_y + 20, 14);
    graphics_draw_text_centered(cr, "Main Menu", menu_x + btn_w/2, menu_y + 20, 14);
    graphics_draw_text_centered(cr, "Quit", quit_x + btn_w/2, quit_y + 20, 14);
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

    // High score display
    gchar hs_text[100];
    g_snprintf(hs_text, sizeof(hs_text), "High Score: %d", game_instance && game_instance->state ? game_instance->state->highscore : 0);
    graphics_draw_text_centered(cr, hs_text, GAME_WIDTH/2, box_y + 145, 20);

    // If this run produced a new high score, show a celebration line
    if (game_instance && game_instance->state && score == game_instance->state->highscore) {
        graphics_set_color(cr, COLOR_YELLOW);
        graphics_draw_text_centered(cr, "NEW HIGH SCORE!", GAME_WIDTH/2, box_y + 175, 18);
    }
    
    // Instructions
    graphics_set_color(cr, COLOR_WHITE);
    graphics_draw_text_centered(cr, "Press SPACE to Play Again", GAME_WIDTH/2, box_y + 160, 18);
    graphics_draw_text_centered(cr, "Press ESC to Menu", GAME_WIDTH/2, box_y + 190, 18);
    
    // Footer
    graphics_set_color(cr, COLOR_GRAY);
    graphics_draw_text_centered(cr, "Try to beat your score next time!", GAME_WIDTH/2, box_y + 240, 12);

    // Draw buttons: Play Again and Main Menu
    gdouble btn_w = 140;
    gdouble btn_h = 36;
    gdouble play_x = box_x + 60;
    gdouble play_y = box_y + 200;
    gdouble menu_x = box_x + 250;
    gdouble menu_y = play_y;

    // Play Again button
    graphics_set_color(cr, COLOR_DARK_BLUE);
    graphics_fill_rectangle(cr, play_x, play_y, btn_w, btn_h);
    graphics_set_color(cr, COLOR_YELLOW);
    graphics_draw_text_centered(cr, "Play Again", play_x + btn_w/2, play_y + 22, 16);

    // Main Menu button
    graphics_set_color(cr, COLOR_DARK_BLUE);
    graphics_fill_rectangle(cr, menu_x, menu_y, btn_w, btn_h);
    graphics_set_color(cr, COLOR_WHITE);
    graphics_draw_text_centered(cr, "Main Menu", menu_x + btn_w/2, menu_y + 22, 16);
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

// Mouse click handler for menu interactions
static gboolean button_press_handler(GtkWidget *widget, GdkEventButton *event, gpointer user_data) {
    Game *game = (Game *)user_data;
    gdouble mx = event->x;
    gdouble my = event->y;

    if (!game || !game->state) return FALSE;

    if (game->state->screen_state == GAME_STATE_MENU) {
        const gint start_x = GAME_WIDTH/2;
        const gint start_y = 230;
        const gint option_gap = 70;
        gdouble box_w = 340;
        gdouble box_h = 56;
        for (int i = 0; i < 3; i++) {
            gdouble cx = start_x - box_w/2;
            gdouble cy = start_y - box_h/2 + (i * option_gap);
            if (mx >= cx && mx <= cx + box_w && my >= cy && my <= cy + box_h) {
                game->menu_selected = i;
                if (i == 0) {
                    game_reset(game);
                    game->state->screen_state = GAME_STATE_PLAYING;
                    if (game->drawing_area) gtk_widget_grab_focus(game->drawing_area);
                } else if (i == 1) {
                    game->state->screen_state = GAME_STATE_CONTROLS;
                } else if (i == 2) {
                    game_stop(game);
                }
                if (game->drawing_area) gtk_widget_queue_draw(game->drawing_area);
                return TRUE;
            }
        }
    } else if (game->state->screen_state == GAME_STATE_GAME_OVER) {
        // Buttons drawn at box_y + 200 and +240 (Play Again, Main Menu)
        gdouble box_width = 450;
        gdouble box_height = 280;
        gdouble box_x = (GAME_WIDTH - box_width) / 2;
        gdouble box_y = (GAME_HEIGHT - box_height) / 2 - 30;
        // Play Again button
        gdouble play_x = box_x + 60;
        gdouble play_y = box_y + 200;
        gdouble btn_w = 140;
        gdouble btn_h = 36;
        if (mx >= play_x && mx <= play_x + btn_w && my >= play_y && my <= play_y + btn_h) {
            game_reset(game);
            game->state->screen_state = GAME_STATE_PLAYING;
            if (game->drawing_area) gtk_widget_grab_focus(game->drawing_area);
            return TRUE;
        }
        // Main Menu button
        gdouble menu_x = box_x + 250;
        gdouble menu_y = play_y;
        if (mx >= menu_x && mx <= menu_x + btn_w && my >= menu_y && my <= menu_y + btn_h) {
            game->state->screen_state = GAME_STATE_MENU;
            if (game->drawing_area) gtk_widget_grab_focus(game->drawing_area);
            return TRUE;
        }
    } else if (game->state->screen_state == GAME_STATE_PAUSED) {
        // Pause menu buttons
        gdouble box_width = 400;
        gdouble box_height = 200;
        gdouble box_x = (GAME_WIDTH - box_width) / 2;
        gdouble box_y = (GAME_HEIGHT - box_height) / 2 - 50;
        gdouble btn_w = 140;
        gdouble btn_h = 32;
        // Resume
        gdouble resume_x = box_x + 30;
        gdouble resume_y = box_y + 60;
        if (mx >= resume_x && mx <= resume_x + btn_w && my >= resume_y && my <= resume_y + btn_h) {
            game_resume(game);
            return TRUE;
        }
        // Restart
        gdouble restart_x = box_x + 220;
        gdouble restart_y = resume_y;
        if (mx >= restart_x && mx <= restart_x + btn_w && my >= restart_y && my <= restart_y + btn_h) {
            game_reset(game);
            game->state->screen_state = GAME_STATE_PLAYING;
            return TRUE;
        }
        // Main Menu
        gdouble menu_x = box_x + 30;
        gdouble menu_y = box_y + 110;
        if (mx >= menu_x && mx <= menu_x + btn_w && my >= menu_y && my <= menu_y + btn_h) {
            game->state->screen_state = GAME_STATE_MENU;
            return TRUE;
        }
        // Quit
        gdouble quit_x = box_x + 220;
        gdouble quit_y = menu_y;
        if (mx >= quit_x && mx <= quit_x + btn_w && my >= quit_y && my <= quit_y + btn_h) {
            game_stop(game);
            return TRUE;
        }
    }

    return FALSE;
}

// Mouse motion handler to update hover (menu highlight)
static gboolean motion_notify_handler(GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
    Game *game = (Game *)user_data;
    if (!game || !game->state) return FALSE;
    gdouble mx = event->x;
    gdouble my = event->y;

    if (game->state->screen_state == GAME_STATE_MENU) {
        const gint start_x = GAME_WIDTH/2;
        const gint start_y = 230;
        const gint option_gap = 70;
        gdouble box_w = 340;
        gdouble box_h = 56;
        for (int i = 0; i < 3; i++) {
            gdouble cx = start_x - box_w/2;
            gdouble cy = start_y - box_h/2 + (i * option_gap);
            if (mx >= cx && mx <= cx + box_w && my >= cy && my <= cy + box_h) {
                if (game->menu_selected != i) {
                    game->menu_selected = i;
                    if (game->drawing_area) gtk_widget_queue_draw(game->drawing_area);
                }
                return TRUE;
            }
        }
        return FALSE;
    }
    return FALSE;
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
        /* Advance background scroll while playing */
        bg_scroll += BG_SCROLL_SPEED * dt;
        if (bg_scroll >= GAME_HEIGHT) bg_scroll = fmod(bg_scroll, GAME_HEIGHT);
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
    game->state->highscore = 0;
    game->state->is_running = FALSE;
    game->state->is_paused = FALSE;
    game->state->screen_state = GAME_STATE_MENU;
    /* Initialize difficulty system */
    game->state->current_speed_multiplier = 1.0;
    game->state->current_spawn_multiplier = 1.0;
    game->state->score_multiplier = 1.0;
    game->state->difficulty_stage = 1;
    game->state->last_stage_shown = 0;
    game->state->arcade_mode = FALSE; /* default to physics movement */
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
    
    // Load image assets using flexible loader (tries several candidate paths)
    // Prefer the new background image name; fall back to old if missing
    background_image = find_asset("background-1.png");
    if (!background_image) background_image = find_asset("background.png");
    // Prefer rotated car image if present
    car_sprite = find_asset("car_rotated.png");
    if (!car_sprite) car_sprite = find_asset("car.png");
    obstacle_sprite = find_asset("obstacle.png");
    // Load obstacle variants (optional)
    obs_bags1 = find_asset("obj_bags1.png");
    obs_barrel1 = find_asset("obj_barrel1.png");
    obs_barrel2 = find_asset("obj_barrel2.png");
    obs_barrels = find_asset("obj_barrels.png");

    /* Load persisted high score (if any) */
    if (game->state) {
        game->state->highscore = load_highscore();
    }
    
    g_signal_connect(game->window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    // Create drawing area
    game->drawing_area = gtk_drawing_area_new();
    gtk_container_add(GTK_CONTAINER(game->window), game->drawing_area);
    g_signal_connect(game->drawing_area, "draw", G_CALLBACK(draw_callback), game);
    /* Enable mouse events for menus */
    gtk_widget_add_events(game->drawing_area, GDK_BUTTON_PRESS_MASK | GDK_POINTER_MOTION_MASK);
    g_signal_connect(game->drawing_area, "button-press-event", G_CALLBACK(button_press_handler), game);
    g_signal_connect(game->drawing_area, "motion-notify-event", G_CALLBACK(motion_notify_handler), game);
    
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
    /* Reset difficulty system */
    game->state->difficulty_stage = 1;
    game->state->last_stage_shown = 0;
    update_difficulty(game->state);
    
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
    /* Apply initial exponential difficulty scaling to obstacles */
    obstacle_manager->obstacle_speed = (BASE_SPEED * SPEEDUP_FACTOR) * game->state->current_speed_multiplier;
    obstacle_manager->spawn_interval = (BASE_SPAWN_INTERVAL / SPEEDUP_FACTOR) * game->state->current_spawn_multiplier;
    /* Add loaded obstacle variant sprites to manager (if any) */
    if (obs_bags1) g_ptr_array_add(obstacle_manager->sprite_templates, g_object_ref(obs_bags1));
    if (obs_barrel1) g_ptr_array_add(obstacle_manager->sprite_templates, g_object_ref(obs_barrel1));
    if (obs_barrel2) g_ptr_array_add(obstacle_manager->sprite_templates, g_object_ref(obs_barrel2));
    if (obs_barrels) g_ptr_array_add(obstacle_manager->sprite_templates, g_object_ref(obs_barrels));
    
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
    /* Shrink hitboxes slightly to make collisions feel fair and avoid early triggers.
       We inset each box by INSET_RATIO of its size. */
    const gdouble INSET_RATIO = 0.12; /* 12% inset */
    gdouble ix1 = w1 * INSET_RATIO;
    gdouble iy1 = h1 * INSET_RATIO;
    gdouble ix2 = w2 * INSET_RATIO;
    gdouble iy2 = h2 * INSET_RATIO;

    gdouble nx1 = x1 + ix1 * 0.5;
    gdouble ny1 = y1 + iy1 * 0.5;
    gdouble nw1 = w1 - ix1;
    gdouble nh1 = h1 - iy1;

    gdouble nx2 = x2 + ix2 * 0.5;
    gdouble ny2 = y2 + iy2 * 0.5;
    gdouble nw2 = w2 - ix2;
    gdouble nh2 = h2 - iy2;

    if (nw1 <= 0 || nh1 <= 0 || nw2 <= 0 || nh2 <= 0) {
        /* Fallback to original sizes if inset would eliminate boxes */
        return !(x1 + w1 < x2 || x2 + w2 < x1 || y1 + h1 < y2 || y2 + h2 < y1);
    }

    return !(nx1 + nw1 < nx2 || nx2 + nw2 < nx1 || ny1 + nh1 < ny2 || ny2 + nh2 < ny1);
}

void game_update(Game *game, gdouble delta_time) {
    if (!player || !obstacle_manager) return;
    
    // Update player
    player_update(player, delta_time, GAME_WIDTH, GAME_HEIGHT);
    
    // Update obstacles
    obstacle_manager_update(obstacle_manager, delta_time, GAME_HEIGHT);
    
    // Spawn new obstacles
    obstacle_manager_spawn(obstacle_manager, GAME_WIDTH, GAME_HEIGHT);
    
    // Collision detection
    for (guint i = 0; i < obstacle_manager->obstacles->len; i++) {
        Obstacle *obs = g_ptr_array_index(obstacle_manager->obstacles, i);
        
        if (check_collision(player->x, player->y, player->width, player->height,
                           obs->x, obs->y, obs->width, obs->height)) {
            // Collision detected -> check high score, persist if needed, then switch to GAME_OVER
            if (game->state) {
                if (game->state->score > game->state->highscore) {
                    game->state->highscore = game->state->score;
                    save_highscore(game->state->highscore);
                }
            }
            game->state->screen_state = GAME_STATE_GAME_OVER;
            // Optionally stop further gameplay updates by returning early
            return;
        }
    }
    
    /* EXPONENTIAL DIFFICULTY SYSTEM: Score accumulation with multiplier */
    score_accum += (SCORE_RATE_BASE * game->state->score_multiplier) * delta_time;
    while (score_accum >= 1.0) {
        game->state->score += 1;
        score_accum -= 1.0;
        
        /* Update difficulty exponentially and apply to obstacles each frame */
        update_difficulty(game->state);
        obstacle_manager->obstacle_speed = (BASE_SPEED * SPEEDUP_FACTOR) * game->state->current_speed_multiplier;
        obstacle_manager->spawn_interval = (BASE_SPAWN_INTERVAL / SPEEDUP_FACTOR) * game->state->current_spawn_multiplier;
    }

    /* Announce difficulty stage transitions */
    if (game->state->difficulty_stage != game->state->last_stage_shown) {
        g_debug("DIFFICULTY STAGE %d: %s!", game->state->difficulty_stage, get_stage_name(game->state->difficulty_stage));
        game->state->last_stage_shown = game->state->difficulty_stage;
    }
    
    // Legacy level system (kept for compatibility; exponential difficulty now primary)
    if (game->state->score % 1000 == 0 && game->state->score > 0) {
        game->state->level++;
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
