#ifndef _AIR_CONDITIONER_H_
#define _AIR_CONDITIONER_H_


#include "hap.h"

#define RMT_TX_GPIO     GPIO_NUM_18
/* 空调设定模式 */
typedef enum {
    AUTO_MODE = 0,
    COOL_MODE,
    HEAT_MODE
}AC_MODE;

/* 空调设定的风速 */
typedef enum {
    AUTO_FAN_SPEED = 0,
    MIN_FAN_SPEED,
    MEDIUM_FAN_SPEED,
    MAX_FAN_SPEED,
    FIXED_FAN_SPEED,
    OFF_FAN_SPEED
}AC_FAN_SPEED;

/* R05D码格式 */
typedef struct {
    uint8_t A;
    uint8_t A_;     // A的按位取反
    uint8_t B;
    uint8_t B_;
    uint8_t C;
    uint8_t C_;
}AC_R05D_PAYLOAD_CODE;

/* 空调指令信息 */
typedef struct {
    bool on;        // 记录空调开关状态
    AC_MODE mode;   // 记录当前空调设定的模式
    uint8_t temp;   // 记录当前空调设定的温度
    AC_FAN_SPEED fan_speed;   // 记录当前空调设定的风速
}AC_INFO;
/* 下面部分参考 https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Coolix.h */
/* 风速代码 */
#define kCoolixFanMin  (0b100)
#define kCoolixFanMed  (0b010)
#define kCoolixFanMax  (0b001)
#define kCoolixFanAuto (0b101)
#define kCoolixFanAuto0 (0b000)     // 固定风
/* 模式代码 只选用了四种模式 */
#define kCoolixCool (0b00)
#define kCoolixDry  (0b01)
#define kCoolixAuto (0b10)
#define kCoolixHeat (0b11)

/* 温度代码 */
#define kCoolixTempMin  (17)  // Celsius
#define kCoolixTempMax  (30)  // Celsius
#define kCoolixTempRange (kCoolixTempMax - kCoolixTempMin + 1)


/**
 * @brief initialize the air conditioner 
 *
 * @param none
 *
 * @return none
 */
void air_conditioner_init(void);



/**
 * @brief 创建空调服务
 * 
 * @return hap_serv_t* HAP Service handle
 */
hap_serv_t *hap_air_conditioner_create(void);

/**
 * @brief 根据风速、温度、模式等(todo)通过红外二极管发送r05d指令
 * 
 * @param ac_info 空调设置信息
 * 
 * @return void
 */
void ac_send_r05d_code(AC_INFO ac_info);

/**
 * @brief 清除申请的内存空间以及失能红外配置
 * 
 */
void air_conditioner_deinit(void);



#endif /* _AIR_CONDITIONER_H_ */
