#include "lcd_display.h"
#include "font_awesome_symbols.h"

#include <esp_log.h>
#include <esp_err.h>
#include <driver/ledc.h>
#include <vector>

#define TAG "LcdDisplay"
#define LCD_LEDC_CH LEDC_CHANNEL_0

#define LCD_LVGL_TICK_PERIOD_MS 2
#define LCD_LVGL_TASK_MAX_DELAY_MS 20
#define LCD_LVGL_TASK_MIN_DELAY_MS 1
#define LCD_LVGL_TASK_STACK_SIZE (4 * 1024)
#define LCD_LVGL_TASK_PRIORITY 10

//向 LVGL 注册外部字体
LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_30_1);
LV_FONT_DECLARE(font_awesome_14_1);

static lv_disp_drv_t disp_drv;
//定义 LVGL 的显示刷新回调函数
static void lcd_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    //从 LVGL 驱动中提取 esp_lcd_panel 的句柄
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    //提取刷新区域的坐标，左上角 (x1, y1) 和右下角 (x2, y2)
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // 将缓冲区中的像素数据复制到 LCD 屏幕上的指定区域
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map); //ESP-IDF 的 API 使用半开区间 [x1, x2)，而 LVGL 使用闭区间 [x1, x2]，所以右边界要加 1。
    lv_disp_flush_ready(&disp_drv);
}

/* Rotate display and touch, when rotated screen in LVGL. Called when driver parameters are updated. */
//屏幕旋转
static void lcd_lvgl_port_update_callback(lv_disp_drv_t *drv)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;

    switch (drv->rotated)
    {
    case LV_DISP_ROT_NONE:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    case LV_DISP_ROT_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISP_ROT_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISP_ROT_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    }
}

//FreeRTOS 任务中驱动 LVGL 图形更新循环
void LcdDisplay::LvglTask() {
    //打印一条信息日志，提示这个任务已经启动
    ESP_LOGI(TAG, "Starting LVGL task");
    //初始化延迟时间为最大延迟，当前是20ms
    uint32_t task_delay_ms = LCD_LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {
        // 尝试加锁，用于防止并发访问 LVGL API（LVGL 不是线程安全的）
        if (Lock())
        {
            task_delay_ms = lv_timer_handler(); //调用 LVGL 的定时器处理函数，检查刷新
            Unlock();
        }
        if (task_delay_ms > LCD_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = LCD_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < LCD_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = LCD_LVGL_TASK_MIN_DELAY_MS;
        }
        //通过 vTaskDelay 延时指定的时间
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

//初始化并配置一个基于 LVGL 的 LCD 显示屏
LcdDisplay::LcdDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           gpio_num_t backlight_pin, bool backlight_output_invert,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy)
    : panel_io_(panel_io), panel_(panel), backlight_pin_(backlight_pin), backlight_output_invert_(backlight_output_invert),
      mirror_x_(mirror_x), mirror_y_(mirror_y), swap_xy_(swap_xy) {
    width_ = width;
    height_ = height;
    offset_x_ = offset_x;
    offset_y_ = offset_y;

    //配置背光控制引脚为输出，并设置初始电平
    InitializeBacklight(backlight_pin);

    // 创建一个全白的像素行缓存（0xFFFF 表示 RGB565 格式下的白色），用于初始化屏幕。
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // 打开 LCD 显示器，确保面板开启显示功能
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    //初始化 LVGL 库
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // 声明 LVGL 的绘图缓冲结构（必须为静态或全局变量）
    static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(width_ * 10 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(width_ * 10 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // 初始化 LVGL 的绘图缓冲结构
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, width_ * 10);

    //初始化 LVGL 的显示驱动描述符
    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = width_;
    disp_drv.ver_res = height_;
    disp_drv.offset_x = offset_x_;
    disp_drv.offset_y = offset_y_;
    disp_drv.flush_cb = lcd_lvgl_flush_cb;
    disp_drv.drv_update_cb = lcd_lvgl_port_update_callback;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_;

    //向 LVGL 注册该显示驱动，正式建立 LVGL 与硬件之间的联系
    lv_disp_drv_register(&disp_drv);

    //日志提示：安装 LVGL 的节拍计时器
    ESP_LOGI(TAG, "Install LVGL tick timer");
    // 创建周期定时器，LVGL 依赖 lv_tick_inc() 每间隔一定时间被调用（如每 2ms）来驱动动画、计时器等内部逻辑
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = [](void* arg) {
            lv_tick_inc(LCD_LVGL_TICK_PERIOD_MS);
        },
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "LVGL Tick Timer",
        .skip_unhandled_events = false
    };
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer_, LCD_LVGL_TICK_PERIOD_MS * 1000));

    //创建递归互斥锁，防止多线程同时访问 LVGL 导致出错
    lvgl_mutex_ = xSemaphoreCreateRecursiveMutex();
    assert(lvgl_mutex_ != nullptr);
    //创建 LVGL 的后台任务，在此任务中运行 LvglTask() 函数，不断轮询刷新 LVGL 界面
    ESP_LOGI(TAG, "Create LVGL task");
    xTaskCreate([](void *arg) {
        static_cast<LcdDisplay*>(arg)->LvglTask();
        vTaskDelete(NULL);
    }, "LVGL", LCD_LVGL_TASK_STACK_SIZE, this, LCD_LVGL_TASK_PRIORITY, NULL);

    //将背光亮度设置为 100%，通常是通过 PWM 输出设置占空比
    SetBacklight(100);

    //初始化图形界面元素
    SetupUI();
}

//释放 LCD 和 LVGL 相关资源，防止内存泄漏或系统异常
LcdDisplay::~LcdDisplay() {
    ESP_ERROR_CHECK(esp_timer_stop(lvgl_tick_timer_));
    ESP_ERROR_CHECK(esp_timer_delete(lvgl_tick_timer_));

    //如果 content_（主显示内容区域）存在，调用 lv_obj_del() 删除它，释放 LVGL 对象
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    //删除状态栏对象，释放其占用的 LVGL 内存
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    //删除侧边栏对象
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    //删除主容器对象
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    //如果 LCD 面板句柄存在，调用 esp_lcd_panel_del() 删除面板驱动，释放底层驱动资源
    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    //删除面板的 IO 传输通道
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    //删除之前创建的互斥锁
    vSemaphoreDelete(lvgl_mutex_);
}

//初始化背光控制（通过 LEDC 模块实现 PWM 调光）
void LcdDisplay::InitializeBacklight(gpio_num_t backlight_pin) {
    //判断是否传入了无效引脚（GPIO_NUM_NC 表示“未连接”）
    if (backlight_pin == GPIO_NUM_NC) {
        return;
    }

    // 准备使用 LEDC 外设实现 PWM（脉宽调制）来调节背光亮度
    const ledc_channel_config_t backlight_channel = {
        .gpio_num = backlight_pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LCD_LEDC_CH,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
        .flags = {
            .output_invert = backlight_output_invert_,
        }
    };
    const ledc_timer_config_t backlight_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT, //设置 PWM 分辨率为 10 位（占空比范围 0~1023）
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false
    };

    ESP_ERROR_CHECK(ledc_timer_config(&backlight_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&backlight_channel));
}

//设置 LCD 背光亮度（通过 PWM 控制）
void LcdDisplay::SetBacklight(uint8_t brightness) {
    if (backlight_pin_ == GPIO_NUM_NC) {
        return;
    }

    //防止传入超过 100 的值，最大亮度限制为 100%
    if (brightness > 100) {
        brightness = 100;
    }

    ESP_LOGI(TAG, "Setting LCD backlight: %d%%", brightness);
    // LEDC 模块使用 10 位分辨率，表示 PWM 占空比最大值是 1023。即 100% 对应 1023，占空比越高亮度越高。
    uint32_t duty_cycle = (1023 * brightness) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH, duty_cycle));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LCD_LEDC_CH));
}

//在多线程环境中对 LVGL 的互斥锁（递归信号量） 进行加锁，从而保护对图形库的访问
bool LcdDisplay::Lock(int timeout_ms) {
    // FreeRTOS 的延时和定时都基于“系统节拍（ticks）”单位，因此需要将毫秒转换为 ticks。
    // 另外，如果 timeout_ms == 0，意味着会无限等待，直到信号量可用
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_mutex_, timeout_ticks) == pdTRUE;
}

void LcdDisplay::Unlock() {
    xSemaphoreGiveRecursive(lvgl_mutex_);
}

//使用 LVGL创建并布局整个 LCD 屏幕的图形界面，包括容器、状态栏、内容区等
void LcdDisplay::SetupUI() {
    DisplayLockGuard lock(this);

    auto screen = lv_disp_get_scr_act(lv_disp_get_default());
    lv_obj_set_style_text_font(screen, &font_puhui_14_1, 0);
    lv_obj_set_style_text_color(screen, lv_color_black(), 0);

    /* Container */
    //创建一个容器对象，作为所有子控件的根容器，父对象是 screen
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);

    /* Status bar */
    //在容器中创建一个高度为 18 像素的状态栏对象，设置圆角为 0（无圆角）
    status_bar_ = lv_obj_create(container_);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 18);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    
    /* Content */
    content_ = lv_obj_create(container_);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_width(content_, LV_HOR_RES);
    lv_obj_set_flex_grow(content_, 1);
    //flex_grow = 1 表示自动占用除状态栏外的剩余空间

    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN); // 垂直布局（从上到下）
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_SPACE_EVENLY); // 子对象居中对齐，等距分布

    //创建一个显示图标的标签，使用 Font Awesome 字体
    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, &font_awesome_30_1, 0);
    lv_label_set_text(emotion_label_, FONT_AWESOME_AI_CHIP);
    // lv_obj_center(emotion_label_);

    //创建一个聊天文本标签，限制宽度为屏幕的 80%
    chat_message_label_ = lv_label_create(content_);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_set_width(chat_message_label_, LV_HOR_RES * 0.8); // 限制宽度为屏幕宽度的 80%
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_WRAP); // 设置为自动换行模式
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_CENTER, 0); // 设置文本居中对齐

    /* Status bar */
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, &font_awesome_14_1, 0);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(notification_label_, "通知");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(status_label_, "正在初始化");
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, &font_awesome_14_1, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, &font_awesome_14_1, 0);
}

//设置聊天消息标签的文本内容
void LcdDisplay::SetChatMessage(const std::string &role, const std::string &content) {
    if (chat_message_label_ == nullptr) {
        return;
    }
    lv_label_set_text(chat_message_label_, content.c_str());
}
