#include <curses.h>

bool curses_alert(const char *text, const char *caption, bool yesno) {
    return false;
#if 0
    init_curses(); // this from curses renderer
    erase(); // this too
    wattrset(*stdscr_p, A_NORMAL | COLOR_PAIR(1));

    mvwaddstr(*stdscr_p, 0, 5, caption);
    mvwaddstr(*stdscr_p, 2, 2, text);
    nodelay(*stdscr_p, false);
    if (yesno) {
        mvwaddstr(*stdscr_p, 5, 0, "Press 'y' or 'n'.");
        refresh();
        while (1) {
            char i = wgetch(*stdscr_p);
            if (i == 'y') {
                ret = true;
                break;
            }
            else if (i == 'n') {
                ret = false;
            break;
            }
        }
    } else {
        mvwaddstr(*stdscr_p, 5, 0, "Press any key to continue.");
        refresh();
        wgetch(*stdscr_p);
    }
    nodelay(*stdscr_p, -1);

    return ret;
#endif
}