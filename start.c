#include <stdio.h>
#include <stdlib.h>
#include <curses.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

void start_curses() {
    initscr();              // Start curses mode
    cbreak();               // Disable line buffering
    noecho();               // Don't echo() while we do getch
    keypad(stdscr, TRUE);   // Enable function keys and arrow keys
    curs_set(0);           // Hide the cursor
}

void end_curses() {
    endwin();               // End curses mode
}

void default_start(){
    start_curses();
    box(stdscr, 0, 0); // Draw a box around the window

    int height, width;
    getmaxyx(stdscr, height, width); // Get the size of the window
    mvprintw(height / 2, (width - 20) / 2, "Welcome to the App!"); // Print message in the center
    mvprintw((height) - 2, (width - 16), "from Team Lidoc"); // Print message in the center
    mvprintw(height - 2, 2, "Press any key to continue...");
    refresh(); // Refresh to show changes
}

int show_the_list(){
    clear();
    box(stdscr, 0, 0); // Draw a box around the window

    const char *items[] = {
        "1. Start Server",
        "2. Connect to Server",
        "3. Settings",
        "4. Help",
        "5. Exit"
    };

    int height, width;
    getmaxyx(stdscr, height, width); // Get the size of the window
    for (int i = 0; i < 5; i++) {
        mvprintw((height / 2) - 2 + i, (width - strlen(items[i])) / 2, "%s", items[i]);
    }
    mvprintw(height - 2, 2, "Use arrow keys to navigate and Enter to select.");

    mvprintw((height / 2) - 2,(width - strlen(items[0])) /2 - 4, "->"); // Initial arrow position
    
    static int current_selection = 0;

    while(1){
        int ch = getch();

        // Erase previous arrow
        mvprintw((height / 2) - 2 + current_selection, (width - strlen(items[current_selection])) / 2 - 4, "  ");

        if(ch == KEY_UP){
            current_selection--;
            if(current_selection < 0){
                current_selection = 4; // Wrap around to last item
            }
        } else if(ch == KEY_DOWN){
            current_selection++;
            if(current_selection > 4){
                current_selection = 0; // Wrap around to first item
            }
        } else if(ch == '\n' || ch == KEY_ENTER){
            // Handle selection
            mvprintw(height - 4, 2, "You selected: %s", items[current_selection]);
            break; // Exit loop on selection
        }

        // Draw arrow at new position
        mvprintw((height / 2) - 2 + current_selection, (width - strlen(items[current_selection])) / 2 - 4, "->");
        refresh(); // Refresh to show changes
    }

    return current_selection;
}

int main() {
    default_start();
    
    timeout(100); // Set getch to be non-blocking with 100ms delay

    while(1){
        int ch = getch(); // Wait for user input
        if (ch != ERR) {
            break; // Exit loop on any key press
        }
    }
    show_the_list();

    end_curses();
    return 0;
}