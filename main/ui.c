#include "ui.h"
#include "cyd_config.h"
#include "esp_log.h"

static const char *TAG = "ui";

static lv_obj_t *s_touch_label;
static lv_obj_t *s_cursor;
static lv_obj_t *s_main_screen;
static ui_button_callback_t s_green_button_callback = NULL;

static void button_event_cb(lv_event_t *e)
{
    int *btn_id = (int *)lv_event_get_user_data(e);
    
    if (btn_id && *btn_id == 0 && s_green_button_callback) {
        /* Green button pressed */
        s_green_button_callback();
    }
}

static lv_obj_t *create_color_button(lv_obj_t *parent, lv_color_t color, int x_pos, int y_pos, int btn_id, int size)
{
    static int btn_ids[6];
    btn_ids[btn_id] = btn_id;
    
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, size, size);
    lv_obj_set_pos(btn, x_pos, y_pos);
    
    /* Style for squared buttons without shadow */
    lv_obj_set_style_radius(btn, 5, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, color, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    
    /* Pressed state */
    lv_obj_set_style_bg_color(btn, lv_color_darken(color, 50), LV_PART_MAIN | LV_STATE_PRESSED);
    
    lv_obj_add_event_cb(btn, button_event_cb, LV_EVENT_CLICKED, &btn_ids[btn_id]);
    
    return btn;
}

void ui_init(void)
{
    s_main_screen = lv_screen_active();
    lv_obj_set_style_bg_color(s_main_screen, lv_color_make(0, 0, 0), 0);
    lv_obj_set_style_bg_opa(s_main_screen, LV_OPA_COVER, 0);

    /* Calculate button positions - centered grid */
    int btn_size = 85;
    int gap = 10;
    int row_width = 3 * btn_size + 2 * gap;
    int start_x = (LCD_H_RES - row_width) / 2;
    int start_y = 25;
    int row_gap = 20;

    /* Row 1: Green, Red, Yellow */
    create_color_button(s_main_screen, lv_color_make(35, 255, 0), start_x, start_y, 0, btn_size); /* Green */
    create_color_button(s_main_screen, lv_color_make(255, 0, 0), start_x + btn_size + gap, start_y, 1, btn_size); /* Red */
    create_color_button(s_main_screen, lv_color_make(230, 200, 0), start_x + 2 * (btn_size + gap), start_y, 2, btn_size); /* Yellow */

    /* Row 2: Magenta, Grey, Orange */
    create_color_button(s_main_screen, lv_color_make(200, 0, 200), start_x, start_y + btn_size + row_gap, 3, btn_size); /* Magenta */
    create_color_button(s_main_screen, lv_color_make(128, 128, 128), start_x + btn_size + gap, start_y + btn_size + row_gap, 4, btn_size); /* Grey */
    create_color_button(s_main_screen, lv_color_make(255, 165, 0), start_x + 2 * (btn_size + gap), start_y + btn_size + row_gap, 5, btn_size); /* Orange */

    /* Touch label at bottom */
    s_touch_label = lv_label_create(s_main_screen);
    lv_obj_align(s_touch_label, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_label_set_text(s_touch_label, "touch: -");
    lv_obj_set_style_text_color(s_touch_label, lv_color_white(), 0);

    /* Touch cursor */
    s_cursor = lv_obj_create(s_main_screen);
    lv_obj_set_size(s_cursor, 12, 12);
    lv_obj_set_style_radius(s_cursor, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(s_cursor, LV_OPA_COVER, 0);

    /* Title at top */
    lv_obj_t *label = lv_label_create(s_main_screen);
    lv_label_set_text(label, "CYD Menu");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
}

void ui_set_green_button_callback(ui_button_callback_t callback)
{
    s_green_button_callback = callback;
}

lv_obj_t *ui_get_touch_label(void)
{
    return s_touch_label;
}

lv_obj_t *ui_get_cursor(void)
{
    return s_cursor;
}

lv_obj_t *ui_get_main_screen(void)
{
    return s_main_screen;
}
