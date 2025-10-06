#ifndef MENU_H
#define MENU_H

#include <kos.h>

extern int state;

typedef enum {
    MENU_NEW_GAME,
    MENU_LOAD_GAME,
    MENU_SAVE_GAME,
    MENU_EXIT,
    MENU_COUNT
} MenuOption;

typedef enum
{
    STATE_VIDEO,
    STATE_MENU,
    STATE_GAME
} AppState;

typedef struct {
    MenuOption selected;
    int option_count;
    const char *option_text[MENU_COUNT];
} Menu;



// menu.c (arriba)
void start_new_game(void);
void load_game(void);
void save_game(void);

void menu_init(Menu *menu);

void menu_draw(Menu *menu);

void menu_update(Menu *menu, cont_state_t *st, int *menu_active);

#endif