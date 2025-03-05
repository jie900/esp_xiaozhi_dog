#include "iot/thing.h"
#include "board.h"
#include "audio_codec.h"

#include <driver/gpio.h>
#include <esp_log.h>
#include "driver/rmt_tx.h"
#include "led_strip.h"

#define TAG "Lamp"

#define LED_STRIP_GPIO_NUM1         GPIO_NUM_38
#define LED_STRIP_GPIO_NUM2         GPIO_NUM_8
#define LED_NUM                     4

namespace iot {

// 这里仅定义 Lamp 的属性和方法，不包含具体的实现
class Lamp : public Thing {
private:
    bool power_ = false;
    led_strip_handle_t strip_1 = NULL;
    led_strip_handle_t strip_2 = NULL;

    void InitializeGpio() {
        led_strip_config_t strip_config1 = {
            .strip_gpio_num = LED_STRIP_GPIO_NUM1,
            .max_leds = LED_NUM,
            .led_pixel_format = LED_PIXEL_FORMAT_GRB,
            .led_model = LED_MODEL_WS2812
        };

        led_strip_config_t strip_config2 = {
            .strip_gpio_num = LED_STRIP_GPIO_NUM2,
            .max_leds = LED_NUM,
            .led_pixel_format = LED_PIXEL_FORMAT_GRB,
            .led_model = LED_MODEL_WS2812
        };

        led_strip_rmt_config_t rmt_config = {
            .resolution_hz = 10 * 1000 * 1000,
        };

        ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config1, &rmt_config, &strip_1));
        ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config2, &rmt_config, &strip_2));

        led_strip_clear(strip_1);
        led_strip_clear(strip_2);
    }

public:
    Lamp() : Thing("Lamp", "一个测试用的灯"){
        InitializeGpio();
        // 定义设备可以被远程执行的指令
        methods_.AddMethod("TurnOn", "打开灯", ParameterList(), [this](const ParameterList& parameters) {
            led_strip_set_pixel(strip_1,0,255,255,255);
            led_strip_set_pixel(strip_1,1,255,255,255);
            led_strip_set_pixel(strip_1,2,255,255,255);
            led_strip_set_pixel(strip_1,3,255,255,255);

            led_strip_set_pixel(strip_2,0,255,255,255);
            led_strip_set_pixel(strip_2,1,255,255,255);
            led_strip_set_pixel(strip_2,2,255,255,255);
            led_strip_set_pixel(strip_2,3,255,255,255);
            
            
            led_strip_refresh(strip_1);
            led_strip_refresh(strip_2);
        });

        methods_.AddMethod("TurnOff", "关闭灯", ParameterList(), [this](const ParameterList& parameters) {
            led_strip_set_pixel(strip_1,0,0,0,0);
            led_strip_set_pixel(strip_1,1,0,0,0);
            led_strip_set_pixel(strip_1,2,0,0,0);
            led_strip_set_pixel(strip_1,3,0,0,0);

            led_strip_set_pixel(strip_2,0,0,0,0);
            led_strip_set_pixel(strip_2,1,0,0,0);
            led_strip_set_pixel(strip_2,2,0,0,0);
            led_strip_set_pixel(strip_2,3,0,0,0);
            
            led_strip_refresh(strip_1);
            led_strip_refresh(strip_2);
        });

        methods_.AddMethod("flashlight", "闪光灯", ParameterList(), [this](const ParameterList& parameters) 
        {
            int count = 5;
            while (count--)
            {
                led_strip_set_pixel(strip_1,0,255,255,255);
                led_strip_set_pixel(strip_1,1,255,255,255);
                led_strip_set_pixel(strip_1,2,255,255,255);
                led_strip_set_pixel(strip_1,3,255,255,255);

                led_strip_set_pixel(strip_2,0,255,255,255);
                led_strip_set_pixel(strip_2,1,255,255,255);
                led_strip_set_pixel(strip_2,2,255,255,255);
                led_strip_set_pixel(strip_2,3,255,255,255);
                
                led_strip_refresh(strip_1);
                led_strip_refresh(strip_2);

                vTaskDelay(100 / portTICK_PERIOD_MS);

                led_strip_set_pixel(strip_1,0,0,0,0);
                led_strip_set_pixel(strip_1,1,0,0,0);
                led_strip_set_pixel(strip_1,2,0,0,0);
                led_strip_set_pixel(strip_1,3,0,0,0);

                led_strip_set_pixel(strip_2,0,0,0,0);
                led_strip_set_pixel(strip_2,1,0,0,0);
                led_strip_set_pixel(strip_2,2,0,0,0);
                led_strip_set_pixel(strip_2,3,0,0,0);
                
                led_strip_refresh(strip_1);
                led_strip_refresh(strip_2);

                vTaskDelay(100 / portTICK_PERIOD_MS);
            }
        });

        methods_.AddMethod("breathe", "呼吸灯", ParameterList(), [this](const ParameterList& parameters) 
        {
            int count = 5;
            while (count--)
            {
                for(int i = 0;i < 256 ;i++)
                {
                    led_strip_set_pixel(strip_1,0,i,i,i);
                    led_strip_set_pixel(strip_1,1,i,i,i);
                    led_strip_set_pixel(strip_1,2,i,i,i);
                    led_strip_set_pixel(strip_1,3,i,i,i);

                    led_strip_set_pixel(strip_2,0,i,i,i);
                    led_strip_set_pixel(strip_2,1,i,i,i);
                    led_strip_set_pixel(strip_2,2,i,i,i);
                    led_strip_set_pixel(strip_2,3,i,i,i);
                    
                    led_strip_refresh(strip_1);
                    led_strip_refresh(strip_2);
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }

                for(int i = 255;i > 0 ;i--)
                {
                    led_strip_set_pixel(strip_1,0,i,i,i);
                    led_strip_set_pixel(strip_1,1,i,i,i);
                    led_strip_set_pixel(strip_1,2,i,i,i);
                    led_strip_set_pixel(strip_1,3,i,i,i);

                    led_strip_set_pixel(strip_2,0,i,i,i);
                    led_strip_set_pixel(strip_2,1,i,i,i);
                    led_strip_set_pixel(strip_2,2,i,i,i);
                    led_strip_set_pixel(strip_2,3,i,i,i);
                    
                    led_strip_refresh(strip_1);
                    led_strip_refresh(strip_2);
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
            }
        });
    }
};

} // namespace iot


DECLARE_THING(Lamp);