/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "sensor_dht22.h"
#include "sensor_ds18b20.h"
#include "sensor_fc28.h"
#include "display_oled.h"
#include "sensor_bh1750.h"
#include "sensor_bmp180.h"
#include "sensor_yfs201.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  DHT22_Init();
  DS18B20_Init();
  FC28_Init();
  OLED_Init();
  BH1750_Init();
  HAL_StatusTypeDef bmp_ret = BMP180_Init();
  printf("BMP180 Init: %d\r\n", bmp_ret);
  YFS201_Init();
  printf("System Init OK!\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint32_t loop_cnt = 0;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    DHT22_Data_t dht_data;
    DS18B20_Data_t ds_data;
    FC28_Data_t fc_data;
    BMP180_Data_t bmp_data;
    BH1750_Data_t light_data;
    YFS201_Data_t flow_data;
    char buf[20];

    loop_cnt++;
    printf("[%lu] ----\r\n", loop_cnt);

    /* DHT22 空气温湿度 */
    if (DHT22_Read(&dht_data) == HAL_OK && dht_data.valid)
    {
        printf("  DHT22: T=%.1fC H=%.1f%%\r\n", dht_data.temperature, dht_data.humidity);
    }
    else
    {
        printf("  DHT22: FAIL\r\n");
    }

    /* DS18B20 土壤温度 */
    if (DS18B20_Read(&ds_data) == HAL_OK && ds_data.valid)
    {
        printf("  DS18B20: T=%.1fC\r\n", ds_data.temperature);
    }
    else
    {
        printf("  DS18B20: FAIL\r\n");
    }

    /* FC28 土壤湿度 */
    if (FC28_Read(&fc_data) == HAL_OK && fc_data.valid)
    {
        printf("  FC28: ADC=%u Moisture=%u%%\r\n", fc_data.adc_value, fc_data.moisture);
    }
    else
    {
        printf("  FC28: FAIL\r\n");
    }

    /* BMP180 大气压强 */
    if (BMP180_Read(&bmp_data) == HAL_OK && bmp_data.valid)
    {
        printf("  BMP180: T=%.1fC P=%.1fhPa\r\n", bmp_data.temperature, bmp_data.pressure);
    }
    else
    {
        printf("  BMP180: FAIL\r\n");
    }

    /* BH1750 光照 */
    if (BH1750_Read(&light_data) == HAL_OK && light_data.valid)
    {
        printf("  BH1750: L=%.1flux\r\n", light_data.light);
    }
    else
    {
        printf("  BH1750: FAIL\r\n");
    }

    /* YF-S201 水流 */
    if (YFS201_Read(&flow_data) == HAL_OK && flow_data.valid)
    {
        printf("  YFS201: F=%.1fL/min V=%.2fL P=%lu\r\n",
               flow_data.flow_rate, flow_data.total_volume, flow_data.pulse_count);
    }

    /* OLED显示 */
    OLED_Clear();
    OLED_DrawString(0, 0, "SmartFarm", FONT_SMALL);

    if (dht_data.valid)
    {
        snprintf(buf, sizeof(buf), "T:%.1fC H:%.0f%%", dht_data.temperature, dht_data.humidity);
        OLED_DrawString(0, 10, buf, FONT_SMALL);
    }
    if (fc_data.valid)
    {
        snprintf(buf, sizeof(buf), "Soil:%u%%", fc_data.moisture);
        OLED_DrawString(0, 20, buf, FONT_SMALL);
    }
    if (bmp_data.valid)
    {
        snprintf(buf, sizeof(buf), "P:%.0fhPa", bmp_data.pressure);
        OLED_DrawString(64, 20, buf, FONT_SMALL);
    }
    if (light_data.valid)
    {
        snprintf(buf, sizeof(buf), "L:%.0flux", light_data.light);
        OLED_DrawString(0, 30, buf, FONT_SMALL);
    }
    if (flow_data.valid && flow_data.flow_rate > 0.1f)
    {
        snprintf(buf, sizeof(buf), "F:%.1fL/m", flow_data.flow_rate);
        OLED_DrawString(64, 30, buf, FONT_SMALL);
    }
    if (ds_data.valid)
    {
        snprintf(buf, sizeof(buf), "St:%.1fC", ds_data.temperature);
        OLED_DrawString(0, 40, buf, FONT_SMALL);
    }

    OLED_Refresh();
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_Delay(2000);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
