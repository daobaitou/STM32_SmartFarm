/**
 * @file    tim.c
 * @brief   TIM4 as HAL timebase (replaces SysTick for FreeRTOS)
 * @note    HAL_InitTick() override so HAL never touches SysTick
 */

#include "tim.h"
#include "stm32f1xx_hal_tim.h"

TIM_HandleTypeDef htim4;

/**
 * @brief  Override weak HAL_InitTick() — use TIM4 instead of SysTick
 *         so SysTick is free for FreeRTOS.
 */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    __HAL_RCC_TIM4_CLK_ENABLE();

    htim4.Instance           = TIM4;
    htim4.Init.Prescaler     = 72 - 1;     /* 72 MHz / 72 = 1 MHz */
    htim4.Init.CounterMode   = TIM_COUNTERMODE_UP;
    htim4.Init.Period        = 1000 - 1;   /* 1 MHz / 1000 = 1 kHz = 1 ms */
    htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
        return HAL_ERROR;

    HAL_NVIC_SetPriority(TIM4_IRQn, TickPriority, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);

    if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK)
        return HAL_ERROR;

    return HAL_OK;
}

/**
 * @brief  Override weak HAL_SuspendTick()
 */
void HAL_SuspendTick(void)
{
    __HAL_TIM_DISABLE_IT(&htim4, TIM_IT_UPDATE);
}

/**
 * @brief  Override weak HAL_ResumeTick()
 */
void HAL_ResumeTick(void)
{
    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
}

/* TIM4 interrupt callback → HAL tick */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
        HAL_IncTick();
}

/* MSP init / deinit for TIM4 */
void HAL_TIM_Base_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_ENABLE();
    }
}

void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        __HAL_RCC_TIM4_CLK_DISABLE();
        HAL_NVIC_DisableIRQ(TIM4_IRQn);
    }
}
