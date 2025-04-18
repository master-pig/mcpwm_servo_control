/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm_prelude.h"

static const char *TAG = "example";

// Please consult the datasheet of your servo before changing the following parameters
#define SERVO_MIN_PULSEWIDTH_US 500  // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US 2500  // Maximum pulse width in microsecond
#define SERVO_MIN_DEGREE        0   // Minimum angle
#define SERVO_MAX_DEGREE        300    // Maximum angle

#define SERVO_PULSE_GPIO_0             20        // GPIO connects to the PWM signal line
#define SERVO_PULSE_GPIO_1             9        // GPIO connects to the PWM signal line
#define SERVO_PULSE_GPIO_2             10        // GPIO connects to the PWM signal line
#define SERVO_PULSE_GPIO_3             11        // GPIO connects to the PWM signal line
#define SERVO_PULSE_GPIO_4             12        // GPIO connects to the PWM signal line

#define SERVO_TIMEBASE_RESOLUTION_HZ 1000000  // 1MHz, 1us per tick
#define SERVO_TIMEBASE_PERIOD        20000    // 20000 ticks, 20ms

static inline uint32_t example_angle_to_compare(int angle)
{
    return (angle - SERVO_MIN_DEGREE) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_DEGREE - SERVO_MIN_DEGREE) + SERVO_MIN_PULSEWIDTH_US;
}

void app_main(void)
{
    ESP_LOGI(TAG, "Create timer and operator");
    mcpwm_timer_handle_t timer = NULL;
    mcpwm_timer_config_t timer_config = {
        .group_id = 0,
        .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
        .resolution_hz = SERVO_TIMEBASE_RESOLUTION_HZ,
        .period_ticks = SERVO_TIMEBASE_PERIOD,
        .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
    };
    ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer));

    mcpwm_oper_handle_t oper = NULL;
    mcpwm_operator_config_t operator_config = {
        .group_id = 0, // operator must be in the same group to the timer
    };
    ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper));

    ESP_LOGI(TAG, "Connect timer and operator");
    ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper, timer));

    ESP_LOGI(TAG, "Create comparator and generator from the operator");
    mcpwm_cmpr_handle_t comparator = NULL;
    mcpwm_comparator_config_t comparator_config = {
        .flags.update_cmp_on_tez = true,
    };
    ESP_ERROR_CHECK(mcpwm_new_comparator(oper, &comparator_config, &comparator));

    mcpwm_gen_handle_t generator = NULL;
    mcpwm_generator_config_t generator_config = {
        .gen_gpio_num = SERVO_PULSE_GPIO_0,
    };
    ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

    // set the initial compare value, so that the servo will spin to the center position
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(0)));

    ESP_LOGI(TAG, "Set generator action on timer and compare event");
    // go high on counter empty
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
    // go low on compare threshold
    ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

    ESP_LOGI(TAG, "Enable and start timer");
    ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
    ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer, MCPWM_TIMER_START_NO_STOP));

    int thumb_num = 0;
    bool isstart = 1;
    int flag = 0;
    int basic_step = 10;

    int angle = 0;
    int step = basic_step;
    int range = 80;

    
    while (1) {
               
        ESP_LOGI(TAG, "Angle of rotation: %d", angle);
        printf("thumb_num=%d, angle=%d,generator_config.gen_gpio_num=%d\n", thumb_num, angle,generator_config.gen_gpio_num);
        ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator, example_angle_to_compare(angle)));
        //Add delay, since it takes time for servo to rotate, usually 200ms/60degree rotation under 5V power supply
        vTaskDelay(pdMS_TO_TICKS(500));
        
        if (isstart){
            vTaskDelay(pdMS_TO_TICKS(1500));
            isstart = 0;
        }

        if ((angle + step) > range || (angle + step) < 0) {
            step *= -1;
            flag += 1;
        }
        angle += step;

        if (flag==2){
            flag = 0;
            thumb_num += 1;
            
            if (thumb_num > 4){
                thumb_num = 0;
            }
            
            if (thumb_num==0){
                range = 80;
            }
            else{
                range = 240;
            }
            
            if (thumb_num==1||thumb_num==3||thumb_num==4){
                angle = range;
                step = -1*basic_step;
            }
            else{
                angle = 0;
                step = basic_step;
            }
            
            ESP_ERROR_CHECK(mcpwm_timer_disable(timer));
            ESP_ERROR_CHECK(mcpwm_generator_disable(generator));
            ESP_ERROR_CHECK(mcpwm_del_generator());
            
            generator = NULL;
            switch (thumb_num) {
                case 0:
                    generator_config.gen_gpio_num = SERVO_PULSE_GPIO_0;
                    break;
                case 1:
                    generator_config.gen_gpio_num = SERVO_PULSE_GPIO_1;
                    break;
                case 2:
                    generator_config.gen_gpio_num = SERVO_PULSE_GPIO_2;
                    break;
                case 3:
                    generator_config.gen_gpio_num = SERVO_PULSE_GPIO_3;
                    break;
                case 4:
                    generator_config.gen_gpio_num = SERVO_PULSE_GPIO_4;
                    break;
                default:
                    // 处理非法 thumb_num，例如赋予一个默认值或报错
                    generator_config.gen_gpio_num = SERVO_PULSE_GPIO_0;
                    break;
            ESP_ERROR_CHECK(mcpwm_new_generator(oper, &generator_config, &generator));

            ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator,
                                                              MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
            // go low on compare threshold
            ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator,
                                                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator, MCPWM_GEN_ACTION_LOW)));

            ESP_ERROR_CHECK(mcpwm_timer_enable(timer));
            }
        }

    }
}
