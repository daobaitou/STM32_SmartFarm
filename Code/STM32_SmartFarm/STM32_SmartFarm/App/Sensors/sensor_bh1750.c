/**
 * @file    sensor_bh1750.c
 * @author  王国维
 * @date    2026-04-20
 * @brief   BH1750光照传感器驱动 (软件I2C, 地址0x23)
 */

#include "sensor_bh1750.h"
#include "soft_i2c.h"

#define BH1750_ADDR  0x23

static float last_light = 0;

HAL_StatusTypeDef BH1750_Init(void)
{
    return HAL_OK;
}

HAL_StatusTypeDef BH1750_Read(BH1750_Data_t *data)
{
    uint8_t cmd = 0x21;
    uint8_t buf[2];

    data->valid = 0;

    /* 发送测量命令 */
    if (Soft_I2C_Write(BH1750_ADDR, &cmd, 1) != HAL_OK)
        return HAL_ERROR;

    /* 等待测量完成, 高分辨率模式需120ms, 留足余量 */
    HAL_Delay(200);

    /* 读取结果 */
    if (Soft_I2C_Read(BH1750_ADDR, buf, 2) != HAL_OK)
        return HAL_ERROR;

    uint16_t raw = (buf[0] << 8) | buf[1];
    float light = (float)raw / 1.2f;

    /* 数据合理性校验: BH1750量程 0~65535lx */
    if (light > 100000.0f)
        return HAL_ERROR;

    /* 异常跳变过滤: 相邻两次变化超过10倍视为异常 */
    if (last_light > 10.0f && light > last_light * 10.0f)
        return HAL_ERROR;

    last_light = light;
    data->light = light;
    data->valid = 1;

    return HAL_OK;
}
