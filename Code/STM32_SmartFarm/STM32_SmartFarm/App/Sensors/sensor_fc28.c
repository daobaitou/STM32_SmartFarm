/**
 * @file    sensor_fc28.c
 * @author  王国维
 * @date    2026-04-19
 * @brief   FC28土壤湿度传感器驱动 - ADC单次转换
 */

#include "sensor_fc28.h"
#include "adc.h"
#include <string.h>

extern ADC_HandleTypeDef hadc1;

HAL_StatusTypeDef FC28_Init(void)
{
    HAL_ADCEx_Calibration_Start(&hadc1);
    return HAL_OK;
}

HAL_StatusTypeDef FC28_Read(FC28_Data_t *data)
{
    memset(data, 0, sizeof(FC28_Data_t));

    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 100) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return HAL_ERROR;
    }

    data->adc_value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    /* ADC值转湿度百分比：干燥(高ADC)~0%, 潮湿(低ADC)~100% */
    /* FC28在空气中~3500, 水中~1200, 根据实际校准 */
    if (data->adc_value > 3500)
        data->moisture = 0;
    else if (data->adc_value < 1200)
        data->moisture = 100;
    else
        data->moisture = (uint8_t)((3500 - data->adc_value) * 100 / (3500 - 1200));

    data->valid = 1;
    return HAL_OK;
}
