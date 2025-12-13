/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "waveshare_rgb_lcd_port.h"

// VSYNC event callback function
IRAM_ATTR static bool rgb_lcd_on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    return lvgl_port_notify_rgb_vsync();
}

#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
/**
 * @brief I2C master initialization
 */
static esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    // Configure I2C parameters
    i2c_param_config(i2c_master_port, &i2c_conf);

    // Install I2C driver
    return i2c_driver_install(i2c_master_port, i2c_conf.mode, 0, 0, 0);
}

// GPIO initialization
void gpio_init(void)
{
    // Zero-initialize the config structure
    gpio_config_t io_conf = {};
    // Disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    // Bit mask of the pins, use GPIO4 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    // Set as input mode
    io_conf.mode = GPIO_MODE_OUTPUT;

    gpio_config(&io_conf);
}

// Reset the touch screen
void waveshare_esp32_s3_touch_reset()
{
    uint8_t write_buf = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    // Reset the touch screen. It is recommended to reset the touch screen before using it.
    write_buf = 0x2C;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    esp_rom_delay_us(100 * 1000);
    gpio_set_level(GPIO_INPUT_IO_4, 0);
    esp_rom_delay_us(100 * 1000);
    write_buf = 0x2E;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    esp_rom_delay_us(200 * 1000);
}

#endif

// Initialize RGB LCD
esp_err_t waveshare_esp32_s3_rgb_lcd_init()
{
    ESP_LOGI(TAG, "Install RGB LCD panel driver"); // Log the start of the RGB LCD panel driver installation
    esp_lcd_panel_handle_t panel_handle = NULL; // Declare a handle for the LCD panel
    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT, // Set the clock source for the panel
        .timings =  {
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ, // Pixel clock frequency
            .h_res = EXAMPLE_LCD_H_RES, // Horizontal resolution
            .v_res = EXAMPLE_LCD_V_RES, // Vertical resolution
            .hsync_pulse_width = 4, // Horizontal sync pulse width
            .hsync_back_porch = 8, // Horizontal back porch
            .hsync_front_porch = 8, // Horizontal front porch
            .vsync_pulse_width = 4, // Vertical sync pulse width
            .vsync_back_porch = 8, // Vertical back porch
            .vsync_front_porch = 8, // Vertical front porch
            .flags = {
                .pclk_active_neg = 1, // Active low pixel clock
            },
        },
        .data_width = EXAMPLE_RGB_DATA_WIDTH, // Data width for RGB
        .bits_per_pixel = EXAMPLE_RGB_BIT_PER_PIXEL, // Bits per pixel
        .num_fbs = LVGL_PORT_LCD_RGB_BUFFER_NUMS, // Number of frame buffers
        .bounce_buffer_size_px = EXAMPLE_RGB_BOUNCE_BUFFER_SIZE, // Bounce buffer size in pixels
        .sram_trans_align = 4, // SRAM transaction alignment
        .psram_trans_align = 64, // PSRAM transaction alignment
        .hsync_gpio_num = EXAMPLE_LCD_IO_RGB_HSYNC, // GPIO number for horizontal sync
        .vsync_gpio_num = EXAMPLE_LCD_IO_RGB_VSYNC, // GPIO number for vertical sync
        .de_gpio_num = EXAMPLE_LCD_IO_RGB_DE, // GPIO number for data enable
        .pclk_gpio_num = EXAMPLE_LCD_IO_RGB_PCLK, // GPIO number for pixel clock
        .disp_gpio_num = EXAMPLE_LCD_IO_RGB_DISP, // GPIO number for display
        .data_gpio_nums = {
            EXAMPLE_LCD_IO_RGB_DATA0,
            EXAMPLE_LCD_IO_RGB_DATA1,
            EXAMPLE_LCD_IO_RGB_DATA2,
            EXAMPLE_LCD_IO_RGB_DATA3,
            EXAMPLE_LCD_IO_RGB_DATA4,
            EXAMPLE_LCD_IO_RGB_DATA5,
            EXAMPLE_LCD_IO_RGB_DATA6,
            EXAMPLE_LCD_IO_RGB_DATA7,
            EXAMPLE_LCD_IO_RGB_DATA8,
            EXAMPLE_LCD_IO_RGB_DATA9,
            EXAMPLE_LCD_IO_RGB_DATA10,
            EXAMPLE_LCD_IO_RGB_DATA11,
            EXAMPLE_LCD_IO_RGB_DATA12,
            EXAMPLE_LCD_IO_RGB_DATA13,
            EXAMPLE_LCD_IO_RGB_DATA14,
            EXAMPLE_LCD_IO_RGB_DATA15,
        },
        .flags = {
            .fb_in_psram = 1, // Use PSRAM for framebuffer
        },
    };

    // Create a new RGB panel with the specified configuration
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    ESP_LOGI(TAG, "Initialize RGB LCD panel"); // Log the initialization of the RGB LCD panel
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle)); // Initialize the LCD panel

    esp_lcd_touch_handle_t tp_handle = NULL; // Declare a handle for the touch panel
#if CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911
    ESP_LOGI(TAG, "Initialize I2C bus"); // Log the initialization of the I2C bus
    i2c_master_init(); // Initialize the I2C master
    ESP_LOGI(TAG, "Initialize GPIO"); // Log GPIO initialization
    gpio_init(); // Initialize GPIO pins
    ESP_LOGI(TAG, "Initialize Touch LCD"); // Log touch LCD initialization
    waveshare_esp32_s3_touch_reset(); // Reset the touch panel

    esp_lcd_panel_io_handle_t tp_io_handle = NULL; // Declare a handle for touch panel I/O
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG(); // Configure I2C for GT911 touch controller

    ESP_LOGI(TAG, "Initialize I2C panel IO"); // Log I2C panel I/O initialization
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)I2C_MASTER_NUM, &tp_io_config, &tp_io_handle)); // Create new I2C panel I/O

    ESP_LOGI(TAG, "Initialize touch controller GT911"); // Log touch controller initialization
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_H_RES, // Set maximum X coordinate
        .y_max = EXAMPLE_LCD_V_RES, // Set maximum Y coordinate
        .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST, // GPIO number for reset
        .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT, // GPIO number for interrupt
        .levels = {
            .reset = 0, // Reset level
            .interrupt = 0, // Interrupt level
        },
        .flags = {
            .swap_xy = 0, // No swap of X and Y
            .mirror_x = 0, // No mirroring of X
            .mirror_y = 0, // No mirroring of Y
        },
    };
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &tp_handle)); // Create new I2C GT911 touch controller
#endif // CONFIG_EXAMPLE_LCD_TOUCH_CONTROLLER_GT911

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle)); // Initialize LVGL with the panel and touch handles

    // Register callbacks for RGB panel events
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
#if EXAMPLE_RGB_BOUNCE_BUFFER_SIZE > 0
        .on_bounce_frame_finish = rgb_lcd_on_vsync_event, // Callback for bounce frame finish
#else
        .on_vsync = rgb_lcd_on_vsync_event, // Callback for vertical sync
#endif
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL)); // Register event callbacks

    return ESP_OK; // Return success 
}

/******************************* Turn on the screen backlight **************************************/
esp_err_t wavesahre_rgb_lcd_bl_on()
{
    //Configure CH422G to output mode 
    uint8_t write_buf = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    //Pull the backlight pin high to light the screen backlight 
    write_buf = 0x1E;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    return ESP_OK;
}

/******************************* Turn off the screen backlight **************************************/
esp_err_t wavesahre_rgb_lcd_bl_off()
{
    //Configure CH422G to output mode 
    uint8_t write_buf = 0x01;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x24, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);

    //Turn off the screen backlight by pulling the backlight pin low 
    write_buf = 0x1A;
    i2c_master_write_to_device(I2C_MASTER_NUM, 0x38, &write_buf, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    return ESP_OK;
}

void base_background(void)
{
    lv_obj_t *scr = lv_scr_act();
    if (!scr) return;

    lv_obj_clean(scr);

    /* --- Set screen background to white --- */
    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* --- Remove screen padding so grid aligns to true pixel (0,0) --- */
    lv_obj_set_style_pad_all(scr, 0, LV_PART_MAIN);

    /* Screen resolution */
    lv_coord_t scr_w = lv_disp_get_hor_res(NULL);
    lv_coord_t scr_h = lv_disp_get_ver_res(NULL);

    /* Grid parameters */
    const lv_coord_t spacing = 160;   // horizontal spacing
    const lv_coord_t line_w  = 1;     // line thickness
    const lv_coord_t top_gap = 10;    // top gap
    const lv_coord_t bot_gap = 10;    // bottom gap
    const lv_coord_t line_h  = scr_h - top_gap - bot_gap;

    lv_color_t grey = lv_color_hex(0xCCCCCC);

    /* Container to hold grid lines */
    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, scr_w, scr_h);
    lv_obj_set_pos(grid, 0, 0);

    /* Remove padding + visuals from container */
    lv_obj_set_style_pad_all(grid, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(grid, 0, LV_PART_MAIN);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_CLICKABLE);

    /* Draw vertical lines */
    for (lv_coord_t x = 0; x < scr_w; x += spacing) {

        lv_obj_t *line = lv_obj_create(grid);

        lv_obj_set_size(line, line_w, line_h);
        lv_obj_set_pos(line, x, top_gap);

        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);

        /* Style the line */
        lv_obj_set_style_bg_color(line, grey, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(line, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(line, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(line, 0, LV_PART_MAIN);
    }
}


void date_month(lv_coord_t x, lv_coord_t y, const char *text)
{
    lv_obj_t *scr = lv_scr_act();
    if (!scr) return;

    lv_obj_t *label = lv_label_create(scr);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);

    /* Place label with its center at (x,y) */
    lv_obj_align(label, LV_ALIGN_CENTER, x - (lv_disp_get_hor_res(NULL) / 2),
                                   y - (lv_disp_get_ver_res(NULL) / 2));
}

void waveshare_rect_box(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const char text[])
{
    lv_obj_t *scr = lv_scr_act();

    /* Make screen background solid white */
    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* Blue color (adjust hex if you want) */
    lv_color_t blue = lv_color_hex(0x0A6AFF);

    /* Create card object */
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);

    /* Visual style: blue rounded background, no border */
    lv_obj_set_style_bg_color(card, blue, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);      /* rounded corners */
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN); /* no border */
    lv_obj_set_style_outline_width(card, 0, LV_PART_MAIN);

    /* Make card non-interactive (visual only) */
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    /* Label centered in the card */
    lv_obj_t *label = lv_label_create(card);
    lv_label_set_text(label, text ? text : "");   /* use passed text (defensive null check) */

    /* Allow multi-line text if it doesn't fit */
    lv_obj_set_width(label, w - 12); /* leave some padding */
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    

}

void waveshare_rect_event_box(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const char text1[], const char text2[], const char text3[])
{
    lv_obj_t *scr = lv_scr_act();

    /* Make screen background solid white */
    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    /* Blue color (adjust hex if you want) */
    lv_color_t blue = lv_color_hex(0x0A6AFF);

    /* Create card object */
    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);

    /* Visual style: blue rounded background, no border */
    lv_obj_set_style_bg_color(card, blue, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);      /* rounded corners */
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN); /* no border */
    lv_obj_set_style_outline_width(card, 0, LV_PART_MAIN);

    /* Make card non-interactive (visual only) */
    lv_obj_clear_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // --- Start of Text Label Modifications ---

    /* Label 1: Top line (Event Name) */
    lv_obj_t *label1 = lv_label_create(card);
    lv_label_set_text(label1, text1 ? text1 : "");
    lv_obj_set_width(label1, w - 12);
    lv_label_set_long_mode(label1, LV_LABEL_LONG_WRAP);
    lv_obj_align(label1, LV_ALIGN_TOP_MID, 0, -10); // Aligned to top, 4px padding
    lv_obj_set_style_text_color(label1, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label1, &lv_font_montserrat_12, LV_PART_MAIN); // Optional: Make header slightly larger/bolder

    /* Label 2: Center line (Main Time) */
    lv_obj_t *label2 = lv_label_create(card);
    lv_label_set_text(label2, text2 ? text2 : "");
    lv_obj_set_width(label2, w - 12);
    lv_label_set_long_mode(label2, LV_LABEL_LONG_WRAP);
    lv_obj_align(label2, LV_ALIGN_CENTER, 0, 7); // Aligned to the exact center
    lv_obj_set_style_text_color(label2, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label2, &lv_font_montserrat_12, LV_PART_MAIN);

    /* Label 3: Bottom line (Location/Detail) */
    lv_obj_t *label3 = lv_label_create(card);
    lv_label_set_text(label3, text3 ? text3 : "");
    lv_obj_set_width(label3, w - 12);
    lv_label_set_long_mode(label3, LV_LABEL_LONG_WRAP);
    lv_obj_align(label3, LV_ALIGN_BOTTOM_MID, 0, 3); // Aligned to bottom, 4px padding
    lv_obj_set_style_text_color(label3, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label3, &lv_font_montserrat_12, LV_PART_MAIN); // Optional: Make detail text slightly smaller

    // Note: The text size/font adjustments (montserrat_16/12) are suggestions to improve visual hierarchy; 
    // they can be removed if you prefer a uniform font.
}