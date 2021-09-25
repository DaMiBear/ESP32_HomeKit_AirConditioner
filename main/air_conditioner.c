/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */



#include <stdio.h>

#include "driver/ledc.h"
#include "esp_log.h"
#include "air_conditioner.h"
#include <hap_apple_servs.h>
#include <hap_apple_chars.h>
#include "driver/rmt.h"
#include "ir_tools.h"


static const char *TAG = "Air Conditioner";
static rmt_channel_t ac_tx_channel = RMT_CHANNEL_0;

static rmt_item32_t *items = NULL;
static uint32_t length = 0;
static ir_builder_t *ir_builder = NULL;

const uint8_t kCoolixTempMap[kCoolixTempRange] = {
    0b0000,  // 17C
    0b0001,  // 18c
    0b0011,  // 19C
    0b0010,  // 20C
    0b0110,  // 21C
    0b0111,  // 22C
    0b0101,  // 23C
    0b0100,  // 24C
    0b1100,  // 25C
    0b1101,  // 26C
    0b1001,  // 27C
    0b1000,  // 28C
    0b1010,  // 29C
    0b1011   // 30C
};


hap_serv_t *hap_air_conditioner_create(void)
{
    hap_serv_t *hs = hap_serv_heater_cooler_create(0, 26.0, 0, 0);      // 实际上创建的是加热器制冷器
    hap_serv_add_char(hs, hap_char_heating_threshold_temperature_create(24.0));     // 可以理解为制热模式的初始温度，以及自动模式的初始下限
    hap_serv_add_char(hs, hap_char_cooling_threshold_temperature_create(26.0));     // 制冷模式的初始温度，以及自动模式的初始上限
    return hs;
}

/**
 * @brief 初始化rmt模块
 * 
 */
void air_conditioner_init(void)
{
    /* 红外初始化 */
    rmt_config_t rmt_tx_config = RMT_DEFAULT_CONFIG_TX(RMT_TX_GPIO, ac_tx_channel);
    rmt_tx_config.tx_config.carrier_en = true;
    rmt_tx_config.flags = RMT_CHANNEL_FLAGS_AWARE_DFS;
    rmt_tx_config.clk_div = 1;
    rmt_config(&rmt_tx_config);
    rmt_driver_install(ac_tx_channel, 0, 0);
    ir_builder_config_t ir_builder_config = IR_BUILDER_DEFAULT_CONFIG((ir_dev_t)ac_tx_channel);
    ir_builder_config.buffer_size = 128;
    ir_builder_config.flags |= IR_TOOLS_FLAGS_PROTO_EXT; // Using extended IR protocols (both NEC and RC5 have extended version)
    ir_builder = ir_builder_rmt_new_nec(&ir_builder_config);
}

/**
 * @brief 根据风速、温度、模式等(todo)通过红外二极管发送r05d指令
 * 
 * @param ac_info 空调设置信息
 * 
 * @return void
 */
void ac_send_r05d_code(AC_INFO ac_info)
{ 
    ESP_LOGI(TAG, "Send - Fan Speed: %d, Temp: %d, Mode: %d, On: %s", ac_info.fan_speed, ac_info.temp, ac_info.mode, ac_info.on ? "on" : "off");
    AC_R05D_PAYLOAD_CODE send_code = { 
        .A = 0,
        .B = 0,
        .C = 0
    };
    /* 默认识别码 */
    send_code.A = 0xB2;
    send_code.A_ = 0x4D;
    if (ac_info.on == true)     // 开机前提下
    {
        /* 风速代码 B7 B6 B5 */
        switch (ac_info.fan_speed)
        {
        case AUTO_FAN_SPEED:
            send_code.B = (kCoolixFanAuto << 5) | 0b11111;
            break;
        case MIN_FAN_SPEED:
            send_code.B = (kCoolixFanMin << 5) | 0b11111;
            break;
        case MEDIUM_FAN_SPEED:
            send_code.B = (kCoolixFanMed << 5) | 0b11111;
            break;
        case MAX_FAN_SPEED:
            send_code.B = (kCoolixFanMax << 5) | 0b11111;
            break;
        case FIXED_FAN_SPEED:
            send_code.B = (kCoolixFanAuto0 << 5) | 0b11111;
            break;
        default:
            send_code.B = (kCoolixFanAuto << 5) | 0b11111;
            break;
        }

        /* 温度代码 */    
        if (ac_info.temp > kCoolixTempMax)  // 先限制范围
            ac_info.temp = kCoolixTempMax;
        else if (ac_info.temp < kCoolixTempMin)
            ac_info.temp = kCoolixTempMin;

        send_code.C = kCoolixTempMap[ac_info.temp%kCoolixTempMin] << 4;

        /* 模式代码 */
        switch (ac_info.mode)
        {
        case AUTO_MODE:
            send_code.C |= kCoolixAuto << 2;
            send_code.B = (kCoolixFanAuto0 << 5) | 0b11111;     // 自动风模式下，风速必须为固定风
            break;
        case COOL_MODE:
            send_code.C |= kCoolixCool << 2;
            break;
        case HEAT_MODE:
            send_code.C |= kCoolixHeat << 2;
            break;
        default:
            send_code.C |= kCoolixAuto << 2;
            break;
        }
    }
    else
    {   // 关机代码
        send_code.B = 0x7B;
        send_code.C = 0xE0;
    }

    send_code.B_ = ~send_code.B;
    send_code.C_ = ~send_code.C;

    ESP_ERROR_CHECK(ir_builder->build_frame(ir_builder, send_code));
    ESP_ERROR_CHECK(ir_builder->get_result(ir_builder, &items, &length));   // 改变items这个指针指向的地址，相当指向rmt_item32_t结构体数组首地址
    //To send data according to the waveform items.
    rmt_write_items(ac_tx_channel, items, length, true);
    ESP_LOGI(TAG, "Send R05D Code : %#X %#X %#X %#X %#X %#X", send_code.A, send_code.A_, send_code.B, send_code.B_, send_code.C, send_code.C_);
}


/**
 * @brief deinitialize the air conditioner 
 * 
 */
void air_conditioner_deinit(void)
{
    ir_builder->del(ir_builder);
    rmt_driver_uninstall(ac_tx_channel);
}



