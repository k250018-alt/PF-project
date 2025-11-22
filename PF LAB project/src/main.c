#include <gtk/gtk.h>
#include "game.h"

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    // Create and initialize game
    Game *game = game_new();
    game_init(game);
    game_start(game);
    
    // Run GTK main loop
    gtk_main();
    
    // Cleanup
    game_cleanup(game);
    
    return 0;
}
