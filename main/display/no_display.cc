//空实现的显示接口，可能用于没有连接显示屏时的占位符、避免 LVGL 调用崩溃、测试模式或服务器模式
#include "no_display.h"

NoDisplay::NoDisplay() {}

NoDisplay::~NoDisplay() {}

bool NoDisplay::Lock(int timeout_ms) {
    return true;
}

void NoDisplay::Unlock() {}
