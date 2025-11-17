#ifndef GAME_H
#define GAME_H

#include <gtk/gtk.h>

#define GAME_WIDTH 800
#define GAME_HEIGHT 600
#define FPS 60
#define FRAME_TIME (1000 / FPS)  // milliseconds

typedef enum {
    GAME_STATE_MENU,
    GAME_STATE_CONTROLS,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER
} GameScreenState;

typedef struct {
    gint score;
    gint level;
    gboolean is_running;
    gboolean is_paused;
    GameScreenState screen_state;
} GameState;

typedef struct {
    GtkWidget *window;
    GtkWidget *drawing_area;
    GameState *state;
    guint timer_id;
    gboolean keys_pressed[4];  // 0=Left, 1=Right, 2=Up, 3=Down
    gint menu_selected; // index of selected menu item (0=start, 1=quit)
} Game;

// Game lifecycle functions
Game* game_new(void);
void game_init(Game *game);
void game_start(Game *game);
void game_reset(Game *game);
void game_stop(Game *game);
void game_pause(Game *game);
void game_resume(Game *game);
void game_update(Game *game, gdouble delta_time);
void game_render(Game *game, cairo_t *cr);
void game_cleanup(Game *game);

#endif // GAME_H
