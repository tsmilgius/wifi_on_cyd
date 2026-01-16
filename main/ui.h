#pragma once

#include "lvgl.h"

typedef void (*ui_button_callback_t)(void);

void ui_init(void);
void ui_set_green_button_callback(ui_button_callback_t callback);
lv_obj_t *ui_get_touch_label(void);
lv_obj_t *ui_get_cursor(void);
lv_obj_t *ui_get_main_screen(void);
