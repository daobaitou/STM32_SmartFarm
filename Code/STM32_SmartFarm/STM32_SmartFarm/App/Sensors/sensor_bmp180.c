/**
 * @file    sensor_bmp180.c
 * @author  王国维
 * @date    2026-04-20
 * @brief   BMP180大气压强传感器驱动 (软件I2C, 地址0x77)
 * @note    需要先读取校准参数, 再用Bosch公式补偿计算
 */

#include "sensor_bmp180.h"
#include "soft_i2c.h"
#include <string.h>

#define BMP180_ADDR  0x77

#define REG_CAL_START  0xAA  /* 校准参数起始地址, 共22字节 */
#define REG_ID         0xD0  /* 芯片ID寄存器 */
#define REG_CTL        0xF4  /* 控制寄存器 */
#define REG_DATA       0xF6  /* 数据寄存器 */

#define CMD_TEMP       0x2E  /* 温度测量命令 */
#define CMD_PRESS_OSS0 0x34  /* 气压测量, 超采样=0 */
#define CMD_PRESS_OSS1 0x74  /* 气压测量, 超采样=1 */
#define CMD_PRESS_OSS2 0xB4  /* 气压测量, 超采样=2 */
#define CMD_PRESS_OSS3 0xF4  /* 气压测量, 超采样=3 */

/* 校准参数 */
static int16_t  AC1, AC2, AC3;
static uint16_t AC4, AC5, AC6;
static int16_t  B1, B2;
static int16_t  MB, MC, MD;

static int32_t  B5; /* 温度补偿中间值 */

static uint8_t bmp180_read_reg(uint8_t reg, uint8_t *buf, uint8_t len)
{
    return Soft_I2C_WriteThenRead(BMP180_ADDR, &reg, 1, buf, len);
}

static uint8_t bmp180_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return Soft_I2C_Write(BMP180_ADDR, buf, 2);
}

static int16_t read_int16(uint8_t msb, uint8_t lsb)
{
    return (int16_t)((msb << 8) | lsb);
}

HAL_StatusTypeDef BMP180_Init(void)
{
    uint8_t cal[22];
    uint8_t id;

    /* 检查芯片ID, BMP180返回0x55 */
    if (bmp180_read_reg(REG_ID, &id, 1) != HAL_OK)
        return HAL_ERROR;

    if (id != 0x55)
        return HAL_ERROR;

    /* 读取11个校准参数 */
    if (bmp180_read_reg(REG_CAL_START, cal, 22) != HAL_OK)
        return HAL_ERROR;

    AC1 = read_int16(cal[0],  cal[1]);
    AC2 = read_int16(cal[2],  cal[3]);
    AC3 = read_int16(cal[4],  cal[5]);
    AC4 = (uint16_t)((cal[6] << 8)  | cal[7]);
    AC5 = (uint16_t)((cal[8] << 8)  | cal[9]);
    AC6 = (uint16_t)((cal[10] << 8) | cal[11]);
    B1  = read_int16(cal[12], cal[13]);
    B2  = read_int16(cal[14], cal[15]);
    MB  = read_int16(cal[16], cal[17]);
    MC  = read_int16(cal[18], cal[19]);
    MD  = read_int16(cal[20], cal[21]);

    return HAL_OK;
}

static int32_t bmp180_read_uncompensated_temp(void)
{
    uint8_t buf[2];

    if (bmp180_write_reg(REG_CTL, CMD_TEMP) != HAL_OK)
        return 0;

    HAL_Delay(5); /* 温度测量需要4.5ms */

    if (bmp180_read_reg(REG_DATA, buf, 2) != HAL_OK)
        return 0;

    return (int32_t)((buf[0] << 8) | buf[1]);
}

static int32_t bmp180_read_uncompensated_pressure(void)
{
    uint8_t buf[3];

    if (bmp180_write_reg(REG_CTL, CMD_PRESS_OSS0) != HAL_OK)
        return 0;

    HAL_Delay(5); /* OSS=0需要4.5ms */

    if (bmp180_read_reg(REG_DATA, buf, 3) != HAL_OK)
        return 0;

    return (int32_t)(((buf[0] << 16) | (buf[1] << 8) | buf[2]) >> (8 - 0));
}

static float bmp180_compute_temperature(int32_t UT)
{
    int32_t X1, X2;

    X1 = ((UT - (int32_t)AC6) * (int32_t)AC5) >> 15;
    X2 = ((int32_t)MC << 11) / (X1 + MD);
    B5 = X1 + X2;

    return (float)((B5 + 8) >> 4) / 10.0f;
}

static float bmp180_compute_pressure(int32_t UP)
{
    int32_t X1, X2, X3;
    int32_t B3, B6;
    uint32_t B4, B7;
    int32_t P;

    B6 = B5 - 4000;
    X1 = (B2 * ((B6 * B6) >> 12)) >> 11;
    X2 = (AC2 * B6) >> 11;
    X3 = X1 + X2;
    B3 = (((AC1 * 4 + X3) << 0) + 2) / 4;

    X1 = (AC3 * B6) >> 13;
    X2 = (B1 * ((B6 * B6) >> 12)) >> 16;
    X3 = ((X1 + X2) + 2) >> 2;
    B4 = (AC4 * (uint32_t)(X3 + 32768)) >> 15;
    B7 = ((uint32_t)UP - B3) * (50000 >> 0);

    if (B7 < 0x80000000)
        P = (B7 * 2) / B4;
    else
        P = (B7 / B4) * 2;

    X1 = (P >> 8) * (P >> 8);
    X1 = (X1 * 3038) >> 16;
    X2 = (-7357 * P) >> 16;
    P = P + ((X1 + X2 + 3791) >> 4);

    return (float)P / 100.0f; /* Pa -> hPa */
}

HAL_StatusTypeDef BMP180_Read(BMP180_Data_t *data)
{
    memset(data, 0, sizeof(BMP180_Data_t));

    int32_t UT = bmp180_read_uncompensated_temp();
    if (UT == 0)
        return HAL_ERROR;

    int32_t UP = bmp180_read_uncompensated_pressure();
    if (UP == 0)
        return HAL_ERROR;

    data->temperature = bmp180_compute_temperature(UT);
    data->pressure = bmp180_compute_pressure(UP);
    data->valid = 1;

    return HAL_OK;
}
