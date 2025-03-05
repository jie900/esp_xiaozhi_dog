#ifndef PET_DOG_H
#define PET_DOG_H

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "string"
#include "functional"
#include "time.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "board.h"
#include "display.h"

//站立角度
#define STAND_ANGLE_LF             90
#define STAND_ANGLE_RF             103
#define STAND_ANGLE_LB             87
#define STAND_ANGLE_RB             90

//睡眠角度
#define SLEEP_ANGLE_LF             180
#define SLEEP_ANGLE_RF             180                    
#define SLEEP_ANGLE_LB             0
#define SLEEP_ANGLE_RB             0

//坐下角度
#define SITDOWN_ANGLE_LF             90
#define SITDOWN_ANGLE_RF             100
#define SITDOWN_ANGLE_LB             25
#define SITDOWN_ANGLE_RB             25

//速度
#define SPEED                   40
#define SPEED_MODE              200

#define START_TASK_EVENT        (1 << 0)
#define STOP_TASK_EVENT         (1 << 1)

#define LEDC_TIMER              LEDC_TIMER_1
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY          (50)
#define LEDC_MIN_DUTY           (8191 * 0.025)      //205
#define LEDC_MAX_DUTY           (8191 * 0.13)       //1065
#define per_angle               (((LEDC_MAX_DUTY) - (LEDC_MIN_DUTY)) / 180)

#define CHANNEL_0               LEDC_CHANNEL_0
#define CHANNEL_1               LEDC_CHANNEL_1
#define CHANNEL_2               LEDC_CHANNEL_2
#define CHANNEL_3               LEDC_CHANNEL_3

#define LEDC_OUTPUT_IO_1        GPIO_NUM_14
#define LEDC_OUTPUT_IO_2        GPIO_NUM_17
#define LEDC_OUTPUT_IO_3        GPIO_NUM_13
#define LEDC_OUTPUT_IO_4        GPIO_NUM_18

typedef enum
{
    LEG1,
    LEG2,
    LEG3,
    LEG4
}leg_index;

typedef struct
{
    uint8_t angle;
    leg_index index;
    int speed;
}target_angle_config_t;

class PetDog
{
public:
    PetDog();
    ~PetDog();
    void InitializeDog(gpio_num_t LEDC_OUTPUT_IO_1, gpio_num_t LEDC_OUTPUT_IO_2, gpio_num_t LEDC_OUTPUT_IO_3, gpio_num_t LEDC_OUTPUT_IO_4);
    void ActionTask();
    void ActionIdleTask();
    void OnActionTask(std::function<void()> callback);

    void petsleep();
    void stand();
    void sitdown();
    void stretch();
    void stretch2();
    void scratching();

    void walkBack();
    void walkfront();
    void turnLeft();
    void turnRight();
    void petwave();

    void stop();
    void idle_activate(int rand_actin);

    void Action(int action);

private:
    EventGroupHandle_t action_task_event_;

    target_angle_config_t lf_;
    target_angle_config_t rf_;
    target_angle_config_t lb_;
    target_angle_config_t rb_;

    int left_front_angle = SLEEP_ANGLE_LF - 5;
    int right_front_angle = SLEEP_ANGLE_RF - 5;
    int left_back_angle = SLEEP_ANGLE_LB + 5;
    int right_back_angle = SLEEP_ANGLE_RB + 5;

    ledc_timer_config_t ledc_timer_;
    std::function<void()> action_task_;

    void to_any_angle_task(uint8_t lf_angle,uint8_t rf_angle,uint8_t lb_angle,uint8_t rb_angle);
    void to_tar_angle(target_angle_config_t *tar);
    void set_angle(uint8_t lf_angle,uint8_t rf_angle,uint8_t lb_angle,uint8_t rb_angle);
    
    void set_left_front_angle(int angle);
    void set_right_front_angle(int angle);
    void set_left_back_angle(int angle);
    void set_right_back_angle(int angle);
};

#endif // PET_DOG_H