// Copyright 2019 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.#include <stdlib.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "ir_tools.h"
#include "ir_timings.h"
#include "driver/rmt.h"



static const char *TAG = "nec_builder";
#define NEC_CHECK(a, str, goto_tag, ret_value, ...)                               \
    do                                                                            \
    {                                                                             \
        if (!(a))                                                                 \
        {                                                                         \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            ret = ret_value;                                                      \
            goto goto_tag;                                                        \
        }                                                                         \
    } while (0)



typedef struct {
    ir_builder_t parent;
    uint32_t buffer_size;
    uint32_t cursor;    // 记录当前读取到的items的下标
    uint32_t flags;
    uint32_t leading_code_high_ticks;
    uint32_t leading_code_low_ticks;
    uint32_t repeat_code_high_ticks;
    uint32_t repeat_code_low_ticks;
    uint32_t payload_logic0_high_ticks;
    uint32_t payload_logic0_low_ticks;
    uint32_t payload_logic1_high_ticks;
    uint32_t payload_logic1_low_ticks;
    uint32_t ending_code_high_ticks;
    uint32_t ending_code_low_ticks;
    uint32_t divide_code_high_ticks;    // 添加分割码
    uint32_t divide_code_low_ticks;
    bool inverse;
    rmt_item32_t buffer[0];
} nec_builder_t;

static esp_err_t nec_builder_make_head(ir_builder_t *builder)
{
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);     // 获取结构体成员所在的结构体的地址
    // nec_builder->cursor = 0;
    nec_builder->buffer[nec_builder->cursor].level0 = !nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration0 = nec_builder->leading_code_high_ticks;
    nec_builder->buffer[nec_builder->cursor].level1 = nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration1 = nec_builder->leading_code_low_ticks;
    nec_builder->cursor += 1;
    return ESP_OK;
}

static esp_err_t nec_builder_make_logic0(ir_builder_t *builder)
{
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);
    nec_builder->buffer[nec_builder->cursor].level0 = !nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration0 = nec_builder->payload_logic0_high_ticks;
    nec_builder->buffer[nec_builder->cursor].level1 = nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration1 = nec_builder->payload_logic0_low_ticks;
    nec_builder->cursor += 1;
    return ESP_OK;
}

static esp_err_t nec_builder_make_logic1(ir_builder_t *builder)
{
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);
    nec_builder->buffer[nec_builder->cursor].level0 = !nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration0 = nec_builder->payload_logic1_high_ticks;
    nec_builder->buffer[nec_builder->cursor].level1 = nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration1 = nec_builder->payload_logic1_low_ticks;
    nec_builder->cursor += 1;
    return ESP_OK;
}

static esp_err_t nec_builder_make_end(ir_builder_t *builder)
{
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);
    nec_builder->buffer[nec_builder->cursor].level0 = !nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration0 = nec_builder->ending_code_high_ticks;
    nec_builder->buffer[nec_builder->cursor].level1 = nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration1 = nec_builder->ending_code_low_ticks;
    nec_builder->cursor += 1;
    nec_builder->buffer[nec_builder->cursor].val = 0;   // 全部放0表示结束
    nec_builder->cursor += 1;
    return ESP_OK;
}

static esp_err_t nec_build_frame(ir_builder_t *builder, const AC_R05D_PAYLOAD_CODE send_code)
{
    esp_err_t ret = ESP_OK;
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);
   
    nec_builder->cursor = 0;
    builder->make_head(builder);
    // 从最高位开始发送(填入到32*(128-1)的buffer中 上面的head占用了1个嘛)
    // A
    for (int i = 0; i < 8; i++) {
        if (send_code.A & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // A'
    for (int i = 0; i < 8; i++) {
        if (send_code.A_ & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // B
    for (int i = 0; i < 8; i++) {
        if (send_code.B & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // B'
    for (int i = 0; i < 8; i++) {
        if (send_code.B_ & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // C
    for (int i = 0; i < 8; i++) {
        if (send_code.C & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // C'
    for (int i = 0; i < 8; i++) {
        if (send_code.C_ & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // 添加分隔码   S
    builder->build_divide_frame(builder);
    // 再发一次
    builder->make_head(builder);
    // A
    for (int i = 0; i < 8; i++) {
        if (send_code.A & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // A'
    for (int i = 0; i < 8; i++) {
        if (send_code.A_ & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // B
    for (int i = 0; i < 8; i++) {
        if (send_code.B & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // B'
    for (int i = 0; i < 8; i++) {
        if (send_code.B_ & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // C
    for (int i = 0; i < 8; i++) {
        if (send_code.C & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    // C'
    for (int i = 0; i < 8; i++) {
        if (send_code.C_ & ((1 << 7) >> i)) {
            builder->make_logic1(builder);
        } else {
            builder->make_logic0(builder);
        }
    }
    
    builder->make_end(builder);
    
    return ret;
}

static esp_err_t nec_build_repeat_frame(ir_builder_t *builder)
{
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);
    nec_builder->cursor = 0;
    nec_builder->buffer[nec_builder->cursor].level0 = !nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration0 = nec_builder->repeat_code_high_ticks;
    nec_builder->buffer[nec_builder->cursor].level1 = nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration1 = nec_builder->repeat_code_low_ticks;
    nec_builder->cursor += 1;
    nec_builder_make_end(builder);
    return ESP_OK;
}
// 添加分割码
static esp_err_t nec_build_divide_frame(ir_builder_t *builder)
{
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);
    nec_builder->buffer[nec_builder->cursor].level0 = !nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration0 = nec_builder->divide_code_high_ticks;
    nec_builder->buffer[nec_builder->cursor].level1 = nec_builder->inverse;
    nec_builder->buffer[nec_builder->cursor].duration1 = nec_builder->divide_code_low_ticks;
    nec_builder->cursor += 1;
    return ESP_OK;
}

static esp_err_t nec_builder_get_result(ir_builder_t *builder, void *result, uint32_t *length)
{
    esp_err_t ret = ESP_OK;
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);
    NEC_CHECK(result && length, "result and length can't be null", err, ESP_ERR_INVALID_ARG);
    *(rmt_item32_t **)result = nec_builder->buffer;     // 传进来的实际上是一个void的指针的指针，那肯定要先变成rmt_item32_t的指针的指针后，再改变*result的值(指向的地址)
    *length = nec_builder->cursor;
    // printf("DEBUG:%d\n", *length);
    return ESP_OK;
err:
    return ret;
}

static esp_err_t nec_builder_del(ir_builder_t *builder)
{
    nec_builder_t *nec_builder = __containerof(builder, nec_builder_t, parent);
    free(nec_builder);
    return ESP_OK;
}

ir_builder_t *ir_builder_rmt_new_nec(const ir_builder_config_t *config)
{
    ir_builder_t *ret = NULL;
    NEC_CHECK(config, "nec configuration can't be null", err, NULL);
    NEC_CHECK(config->buffer_size, "buffer size can't be zero", err, NULL);

    uint32_t builder_size = sizeof(nec_builder_t) + config->buffer_size * sizeof(rmt_item32_t);
    nec_builder_t *nec_builder = calloc(1, builder_size);
    NEC_CHECK(nec_builder, "request memory for nec_builder failed", err, NULL);

    nec_builder->buffer_size = config->buffer_size;
    nec_builder->flags = config->flags;
    if (config->flags & IR_TOOLS_FLAGS_INVERSE) {
        nec_builder->inverse = true;
    }
    uint32_t counter_clk_hz = 0;
    NEC_CHECK(rmt_get_counter_clock((rmt_channel_t)config->dev_hdl, &counter_clk_hz) == ESP_OK,
              "get rmt counter clock failed", err, NULL);
    float ratio = (float)counter_clk_hz / 1e6;  // 一个ticks的时间us 默认时钟80M，80分频那么就是1M也就是1us
    nec_builder->leading_code_high_ticks = (uint32_t)(ratio * R05D_LEADING_CODE_HIGH_US);   // 改变引导码长度
    nec_builder->leading_code_low_ticks = (uint32_t)(ratio * R05D_LEADING_CODE_LOW_US);
    nec_builder->repeat_code_high_ticks = (uint32_t)(ratio * R05D_REPEAT_CODE_HIGH_US);
    nec_builder->repeat_code_low_ticks = (uint32_t)(ratio * R05D_REPEAT_CODE_LOW_US);
    nec_builder->payload_logic0_high_ticks = (uint32_t)(ratio * R05D_PAYLOAD_ZERO_HIGH_US);
    nec_builder->payload_logic0_low_ticks = (uint32_t)(ratio * R05D_PAYLOAD_ZERO_LOW_US);
    nec_builder->payload_logic1_high_ticks = (uint32_t)(ratio * R05D_PAYLOAD_ONE_HIGH_US);
    nec_builder->payload_logic1_low_ticks = (uint32_t)(ratio * R05D_PAYLOAD_ONE_LOW_US);
    nec_builder->ending_code_high_ticks = (uint32_t)(ratio * R05D_ENDING_CODE_HIGH_US); 
    nec_builder->ending_code_low_ticks = (uint32_t)(ratio * R05D_ENDING_CODE_LOW_US);
    nec_builder->divide_code_high_ticks = (uint32_t)(ratio * R05D_DIVIDE_CODE_HIGH_US); // 添加分隔码
    nec_builder->divide_code_low_ticks = (uint32_t)(ratio * R05D_DIVIDE_CODE_LOW_US);
    nec_builder->parent.make_head = nec_builder_make_head;
    nec_builder->parent.make_logic0 = nec_builder_make_logic0;
    nec_builder->parent.make_logic1 = nec_builder_make_logic1;
    nec_builder->parent.make_end = nec_builder_make_end;
    nec_builder->parent.build_frame = nec_build_frame;
    nec_builder->parent.build_repeat_frame = nec_build_repeat_frame;
    nec_builder->parent.build_divide_frame = nec_build_divide_frame;    // 添加分割码
    nec_builder->parent.get_result = nec_builder_get_result;
    nec_builder->parent.del = nec_builder_del;
    nec_builder->parent.repeat_period_ms = 4000;
    return &nec_builder->parent;
err:
    return ret;
}
