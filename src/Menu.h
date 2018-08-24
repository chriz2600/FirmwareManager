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
    "- Video Mode Settings                   "
    "- Firmware Upgrade                      "
    "- Info                                  "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "          A: Select  B: Exit            "
);

char OSD_OUTPUT_RES_MENU[521] = (
    "Output Resolution                       "
    "                                        "
    "- VGA                                   "
    "- 480p                                  "
    "- 960p                                  "
    "- 1080p                                 "
    "                                        "
    "  '>' marks the default setting         "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "          A: Apply   B: Back            "
);

char OSD_OUTPUT_RES_SAVE_MENU[521] = (
    "Output Resolution                       "
    "                                        "
    "     Save this setting as default?      "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "          A: Ok    B: Cancel            "
);

char OSD_VIDEO_MODE_MENU[521] = (
    "Video Mode Settings                     "
    "                                        "
    "- Force VGA                             "
    "- Cable Detect                          "
    "- Switch Trick VGA                      "
    "                                        "
    "  Save: After saving the Dreamcast      "
    "        has to be power cycled for      "
    "        for the changes to take         "
    "        effect.                         "
    "                                        "
    "                                        "
    "          A: Save  B: Cancel            "
);

char OSD_FIRMWARE_MENU[521] = (
    "Firmware                                "
    "                                        "
    "- Check                                 "
    "- Download                              "
    "- Flash                                 "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "          A: Select  B: Exit            "
);

char OSD_FIRMWARE_CHECK_MENU[521] = (
    "Firmware Check                          "
    "                                        "
    "                                        "
    "                                        "
    "FPGA        XXXXXXXX                    "
    "ESP         XXXXXXXX                    "
    "index.html  XXXXXXXX                    "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "          A: Select  B: Exit            "
);

char OSD_DEBUG_MENU[521] = (
    "Debug                                   "
    "                                        "
    "COMING SOON                             "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                                        "
    "                B: Back                 "
);

typedef std::function<void(uint16_t controller_data, uint8_t menu_activeLine)> ClickHandler;
typedef std::function<uint8_t(uint8_t* menu_text)> PreDisplayHook;

extern FPGATask fpgaTask;

class Menu
{
  public:
    Menu(const char* name, uint8_t* menu, uint8_t first_line, uint8_t last_line, ClickHandler handler, PreDisplayHook pre_hook, WriteCallbackHandlerFunction display_callback) :
        name(name),
        menu_text(menu),
        first_line(first_line),
        last_line(last_line),
        handler(handler),
        pre_hook(pre_hook),
        display_callback(display_callback),
        menu_activeLine(first_line)
    { };

    const char* Name() {
        return name;
    }

    void Display() {
        if (pre_hook != NULL) {
            menu_activeLine = pre_hook(menu_text);
        } else {
            menu_activeLine = first_line;
        }
        fpgaTask.DoWriteToOSD(0, 9, menu_text, [&]() {
            fpgaTask.Write(I2C_OSD_ACTIVE_LINE, menu_activeLine, display_callback);
        });
    }

    void HandleClick(uint16_t controller_data) {
        // pad up down is handled by menu
        if (CHECK_MASK(controller_data, CTRLR_PAD_UP)) {
            menu_activeLine = menu_activeLine <= first_line ? first_line : menu_activeLine - 1;
            fpgaTask.Write(I2C_OSD_ACTIVE_LINE, menu_activeLine);
            return;
        }
        if (CHECK_MASK(controller_data, CTRLR_PAD_DOWN)) {
            menu_activeLine = menu_activeLine >= last_line ? last_line : menu_activeLine + 1;
            fpgaTask.Write(I2C_OSD_ACTIVE_LINE, menu_activeLine);
            return;
        }
        // pass all other pads to handler
        handler(controller_data, menu_activeLine);
    }

    uint8_t* GetMenuText() {
        return menu_text;
    }

private:
    const char* name;
    uint8_t* menu_text;
    uint8_t first_line;
    uint8_t last_line;
    ClickHandler handler;
    PreDisplayHook pre_hook;
    WriteCallbackHandlerFunction display_callback;
    uint8_t menu_activeLine;
};

#endif
