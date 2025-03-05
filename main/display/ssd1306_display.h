#ifndef SSD1306_DISPLAY_H
#define SSD1306_DISPLAY_H

#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

class Ssd1306Display : public Display {
private:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    TaskHandle_t blink_task_handle;
    TaskHandle_t idle_task_handle;

    bool mirror_x_ = false;
    bool mirror_y_ = false;

    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;

    lv_obj_t *square1 = NULL;   //眼睛
    lv_obj_t *square2 = NULL;

    int cur_square1_x = 0;
    int cur_square1_y = 0;
    int cur_square2_x = 0;
    int cur_square2_y = 0;

    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    void SetupUI_128x64();
    void SetupUI_128x32();

    void to_any_position(int tar1x,int tar1y,int tarx2,int tary2);
    int bresenham_line(int x1, int y1, int x2, int y2,int ret[]);

    void close_eyes();
    void open_eyes();
    void blink_eyes();

    void blink_task();
    void eye_move_emtion_task();
    

public:
    Ssd1306Display(void* i2c_master_handle, int width, int height, bool mirror_x = false, bool mirror_y = false);
    ~Ssd1306Display();

    virtual void start_emtion() override;
    virtual void stop_emtion()  override;
    virtual void idle_emtion()  override;
};

#endif // SSD1306_DISPLAY_H
