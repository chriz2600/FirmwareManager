#ifndef MENU_H
#define MENU_H

#include <inttypes.h>
#include <functional>
#include "global.h"
#include "FPGATask.h"

char OSD_MAIN_MENU[521] = (
    "MainMenu                                "
    "                                        "
    "- Output Resolution                     "
    "- Debug                                 "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "          A: Select  B: Back            "
);

char OSD_OUTPUT_RES_MENU[521] = (
    "Output Resolution                       "
    "                                        "
    "- VGA                                   "
    "- 480p                                  "
    "- 960p                                  "
    "- 1080p                                 "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "          A: Apply   B: Back            "
);

typedef std::function<void(uint16_t controller_data)> ClickHandler;

extern FPGATask fpgaTask;
extern uint8_t menu_activeLine;

class Menu
{
  public:
    Menu(uint8_t* menu, uint8_t first_line, uint8_t last_line, ClickHandler handler) :
        menu_text(menu),
        first_line(first_line),
        last_line(last_line),
        handler(handler)
    { };

    void Display() {
        fpgaTask.DoWriteToOSD(0, 9, menu_text, [&]() {
            menu_activeLine = first_line;
            fpgaTask.Write(I2C_OSD_ACTIVE_LINE, menu_activeLine);
        });
    }

    void HandleClick(uint16_t controller_data) {
        // pad up down is handled by menu
        if (CHECK_BIT(controller_data, CTRLR_PAD_UP)) {
            menu_activeLine = menu_activeLine <= first_line ? first_line : menu_activeLine - 1;
            fpgaTask.Write(I2C_OSD_ACTIVE_LINE, menu_activeLine);
            return;
        }
        if (CHECK_BIT(controller_data, CTRLR_PAD_DOWN)) {
            menu_activeLine = menu_activeLine >= last_line ? last_line : menu_activeLine + 1;
            fpgaTask.Write(I2C_OSD_ACTIVE_LINE, menu_activeLine);
            return;
        }
        // pass all other pads to handler
        handler(controller_data);
    }

private:
    uint8_t* menu_text;
    uint8_t first_line;
    uint8_t last_line;
    ClickHandler handler;
};

#endif

/*








--------------------
MainMenu

- Output resolution
- Debug

A: Apply
B: Exit
--------------------

click_handler(current_line) {

}

Menu

*/