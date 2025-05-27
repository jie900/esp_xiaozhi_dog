#include <esp_log.h>
#include <esp_err.h>
#include <string>
#include <cstdlib>
#include "display.h"
#include "board.h"
#include "application.h"
#include "font_awesome_symbols.h"
#include "audio_codec.h"
#define TAG "Display"

Display::Display() {
    // Notification timer 通知定时器
    esp_timer_create_args_t notification_timer_args = {
        .callback = [](void *arg) {
            Display *display = static_cast<Display*>(arg);
            DisplayLockGuard lock(display);
            lv_obj_add_flag(display->notification_label_, LV_OBJ_FLAG_HIDDEN); //隐藏
            lv_obj_clear_flag(display->status_label_, LV_OBJ_FLAG_HIDDEN); //显示
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Notification Timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&notification_timer_args, &notification_timer_));

    // Update display timer 更新定时器
    esp_timer_create_args_t update_display_timer_args = {
        .callback = [](void *arg) {
            Display *display = static_cast<Display*>(arg);
            display->Update();
        },
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "Update Display Timer",
        .skip_unhandled_events = false,
    };
    ESP_ERROR_CHECK(esp_timer_create(&update_display_timer_args, &update_timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(update_timer_, 1000000)); //每秒执行一次 Update() 函数，用于更新 UI 状态（如电池图标、网络状态、静音图标）
}

Display::~Display() {
    esp_timer_stop(notification_timer_); //停止用于通知显示的定时器
    esp_timer_stop(update_timer_);
    esp_timer_delete(notification_timer_); //删除通知用的定时器，释放相关系统资源。
    esp_timer_delete(update_timer_);

    if (network_label_ != nullptr) { //判断UI是否初始化过
        lv_obj_del(network_label_);
        lv_obj_del(notification_label_);
        lv_obj_del(status_label_);
        lv_obj_del(mute_label_);
        lv_obj_del(battery_label_);
    } //在对象销毁时停止并删除定时器，删除 UI 元素（标签对象）以释放内存
}

void Display::SetStatus(const std::string &status) {
    if (status_label_ == nullptr) {
        return; //如果 status_label_ 是空指针（nullptr），就直接返回，不执行后面的代码
    }
    DisplayLockGuard lock(this); //保证线程安全（一般用于同步 LVGL 线程操作）
    lv_label_set_text(status_label_, status.c_str()); //设置状态栏的文本
    lv_obj_clear_flag(status_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN); //显示状态标签，隐藏通知标签。
}

void Display::ShowNotification(const std::string &notification, int duration_ms) { //显示一条通知消息，并在 duration_ms 毫秒后自动隐藏。
    if (notification_label_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(this); //进入临界区，防止多线程/中断期间对 UI 控件的并发访问。
    lv_label_set_text(notification_label_, notification.c_str()); //设置 notification_label_ 上显示的文字为 notification 的内容。
    lv_obj_clear_flag(notification_label_, LV_OBJ_FLAG_HIDDEN); //显示通知标签
    lv_obj_add_flag(status_label_, LV_OBJ_FLAG_HIDDEN); //隐藏状态标签

    esp_timer_stop(notification_timer_); //重启计时器
    ESP_ERROR_CHECK(esp_timer_start_once(notification_timer_, duration_ms * 1000)); //esp_timer_start_once() 的单位是微秒，所以乘以 1000。在 duration_ms 毫秒后隐藏通知。
}

void Display::Update() {
    if (mute_label_ == nullptr) {
        return;
    }

    auto& board = Board::GetInstance();
    auto codec = board.GetAudioCodec(); //获取音频编解码器（Audio Codec）的对象，用于查询音量（静音）状态。

    DisplayLockGuard lock(this);
    // 如果静音状态改变，则更新图标
    if (codec->output_volume() == 0 && !muted_) {
        muted_ = true;
        lv_label_set_text(mute_label_, FONT_AWESOME_VOLUME_MUTE);
    } else if (codec->output_volume() > 0 && muted_) {
        muted_ = false;
        lv_label_set_text(mute_label_, "");
    }
    // 更新电池图标
    int battery_level;
    bool charging;
    const char* icon = nullptr;
    if (board.GetBatteryLevel(battery_level, charging)) {
        if (charging) {
            icon = FONT_AWESOME_BATTERY_CHARGING;
        } else {
            const char* levels[] = {
                FONT_AWESOME_BATTERY_EMPTY, // 0-19%
                FONT_AWESOME_BATTERY_1,    // 20-39%
                FONT_AWESOME_BATTERY_2,    // 40-59%
                FONT_AWESOME_BATTERY_3,    // 60-79%
                FONT_AWESOME_BATTERY_FULL, // 80-99%
                FONT_AWESOME_BATTERY_FULL, // 100%
            };
            icon = levels[battery_level / 20];
        }
        if (battery_icon_ != icon) {
            battery_icon_ = icon;
            lv_label_set_text(battery_label_, battery_icon_);
        }
    }
    // 仅在聊天状态为空闲时，读取网络状态（避免升级时占用 UART 资源）
    auto device_state = Application::GetInstance().GetDeviceState();
    if (device_state == kDeviceStateIdle || device_state == kDeviceStateStarting) {
        icon = board.GetNetworkStateIcon();
        if (network_icon_ != icon) {
            network_icon_ = icon;
            lv_label_set_text(network_label_, network_icon_);
        }
    }
}
void Display::SetEmotion(const std::string &emotion) {
    if (emotion_label_ == nullptr) {
        return;
    }
    struct Emotion {
        const char* icon;
        const char* text;
    };
    static const std::vector<Emotion> emotions = {
        {FONT_AWESOME_EMOJI_NEUTRAL, "neutral"},
        {FONT_AWESOME_EMOJI_HAPPY, "happy"},
        {FONT_AWESOME_EMOJI_LAUGHING, "laughing"},
        {FONT_AWESOME_EMOJI_FUNNY, "funny"},
        {FONT_AWESOME_EMOJI_SAD, "sad"},
        {FONT_AWESOME_EMOJI_ANGRY, "angry"},
        {FONT_AWESOME_EMOJI_CRYING, "crying"},
        {FONT_AWESOME_EMOJI_LOVING, "loving"},
        {FONT_AWESOME_EMOJI_EMBARRASSED, "embarrassed"},
        {FONT_AWESOME_EMOJI_SURPRISED, "surprised"},
        {FONT_AWESOME_EMOJI_SHOCKED, "shocked"},
        {FONT_AWESOME_EMOJI_THINKING, "thinking"},
        {FONT_AWESOME_EMOJI_WINKING, "winking"},
        {FONT_AWESOME_EMOJI_COOL, "cool"},
        {FONT_AWESOME_EMOJI_RELAXED, "relaxed"},
        {FONT_AWESOME_EMOJI_DELICIOUS, "delicious"},
        {FONT_AWESOME_EMOJI_KISSY, "kissy"},
        {FONT_AWESOME_EMOJI_CONFIDENT, "confident"},
        {FONT_AWESOME_EMOJI_SLEEPY, "sleepy"},
        {FONT_AWESOME_EMOJI_SILLY, "silly"},
        {FONT_AWESOME_EMOJI_CONFUSED, "confused"}
    };
    DisplayLockGuard lock(this);
    
    // 查找匹配的表情
    auto it = std::find_if(emotions.begin(), emotions.end(),
        [&emotion](const Emotion& e) { return e.text == emotion; });
    
    // 如果找到匹配的表情就显示对应图标，否则显示默认的neutral表情
    if (it != emotions.end()) {
        lv_label_set_text(emotion_label_, it->icon);
    } else {
        lv_label_set_text(emotion_label_, FONT_AWESOME_EMOJI_NEUTRAL);
    }
}
void Display::SetIcon(const char* icon) {
    if (emotion_label_ == nullptr) {
        return;
    }
    DisplayLockGuard lock(this);
    lv_label_set_text(emotion_label_, icon);
}
void Display::SetChatMessage(const std::string &role, const std::string &content) {
}
