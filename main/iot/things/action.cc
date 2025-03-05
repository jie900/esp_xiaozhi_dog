#include "driver/gpio.h"
#include <esp_log.h>

#include "pet_dog.h"
#include "application.h"
#include "board.h"
#include "iot/thing.h"

#define TAG "Action"


namespace iot {

// 这里仅定义 Speaker 的属性和方法，不包含具体的实现
class Action : public Thing {
private:

    
public:
    Action() : Thing("Action", "当前 AI 机器人的行为（站立，坐下，睡觉,左转，右转，前进，后退)") {
        // 定义设备可以被远程执行的指令
        methods_.AddMethod("Walk", "前进", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateWalk);
        });

        methods_.AddMethod("Walk back", "后退", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateWalkBack);
        });
        methods_.AddMethod("stand", "站立", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateStand);
        });
        methods_.AddMethod("sitdown", "坐下", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateSitdown);
        });
        methods_.AddMethod("sleep", "睡觉", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateSleep);
        });

        methods_.AddMethod("turn left", "左转", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateTurnLeft);
        });

        methods_.AddMethod("turn right", "右转", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateTurnRight);
        });

        methods_.AddMethod("wave", "挥挥手(回复:哥哥你好呀)", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateWave);
        });

        methods_.AddMethod("stop", "停下来", ParameterList(), [this](const ParameterList& parameters) {
            auto& app = Application::GetInstance();
            app.SetActionState(kActionStateStop);
        });
    }
};

} // namespace iot

DECLARE_THING(Action);