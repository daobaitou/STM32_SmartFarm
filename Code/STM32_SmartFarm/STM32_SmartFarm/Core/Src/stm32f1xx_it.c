/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "stm32f1xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sensor_yfs201.h"
#include "usart2_driver.h"
#include "tim.h"
#include "stm32f1xx_hal_tim.h"
/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
/* FreeRTOS port handlers (declared in port.c) */
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

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
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern TIM_HandleTypeDef htim4;
extern UART_HandleTypeDef huart2;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M3 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  /* 直接操作USART1寄存器输出诊断信息，不依赖printf/栈 */
  const char *msg;

#define HF_UART_TX(c) do { \
    while(!(USART1->SR & USART_SR_TXE)); \
    USART1->DR = (c); \
} while(0)

#define HF_PRINT(s) do { \
    msg = (s); \
    while(*msg) { HF_UART_TX(*msg++); } \
} while(0)

#define HF_PRINT_HEX(v, d) do { \
    for(int _i=(d)-4; _i>=0; _i-=4) { \
        uint8_t _n = ((v)>>_i) & 0xF; \
        HF_UART_TX(_n<10 ? '0'+_n : 'A'+_n-10); \
    } \
} while(0)

  HF_PRINT("\r\n\r\n!!! HARDFAULT !!!\r\n");

  /* 读取故障寄存器 */
  uint32_t cfsr = SCB->CFSR;
  uint32_t hfsr = SCB->HFSR;
  uint32_t mmfar = SCB->MMFAR;
  uint32_t bfar  = SCB->BFAR;

  HF_PRINT("HFSR="); HF_PRINT_HEX(hfsr, 32); HF_PRINT("\r\n");
  HF_PRINT("CFSR="); HF_PRINT_HEX(cfsr, 32); HF_PRINT("\r\n");

  /* 解码HFSR */
  if (hfsr & (1<<30)) HF_PRINT("  FORCED (configurable fault escalated)\r\n");
  if (hfsr & (1<<1))  HF_PRINT("  VECTBL (bad vector table read)\r\n");

  /* 解码CFSR各部分 */
  uint8_t ufsr = (cfsr >> 16) & 0xFFFF;
  uint8_t bfsr = (cfsr >> 8) & 0xFF;
  uint8_t mmfsr = cfsr & 0xFF;

  if (ufsr) {
    HF_PRINT("UFSR="); HF_PRINT_HEX(ufsr, 16); HF_PRINT("\r\n");
    if (ufsr & (1<<2)) HF_PRINT("  INVPC (invalid PC load)\r\n");
    if (ufsr & (1<<1)) HF_PRINT("  INVSTATE (invalid state)\r\n");
    if (ufsr & (1<<0)) HF_PRINT("  UNDEFINSTR (undefined instruction)\r\n");
    if (ufsr & (1<<3)) HF_PRINT("  NOCP (no coprocessor)\r\n");
    if (ufsr & (1<<8)) HF_PRINT("  UNALIGNED (unaligned access)\r\n");
    if (ufsr & (1<<9)) HF_PRINT("  DIVBYZERO\r\n");
  }

  if (bfsr) {
    HF_PRINT("BFSR="); HF_PRINT_HEX(bfsr, 8); HF_PRINT("\r\n");
    if (bfsr & (1<<7)) { HF_PRINT("  BFARVALID addr="); HF_PRINT_HEX(bfar, 32); HF_PRINT("\r\n"); }
    if (bfsr & (1<<4)) HF_PRINT("  STKERR (stacking error)\r\n");
    if (bfsr & (1<<3)) HF_PRINT("  UNSTKERR (unstacking error)\r\n");
    if (bfsr & (1<<2)) HF_PRINT("  IMPRECISERR\r\n");
    if (bfsr & (1<<1)) HF_PRINT("  PRECISERR\r\n");
    if (bfsr & (1<<0)) HF_PRINT("  IBUSERR\r\n");
  }

  if (mmfsr) {
    HF_PRINT("MMFSR="); HF_PRINT_HEX(mmfsr, 8); HF_PRINT("\r\n");
    if (mmfsr & (1<<7)) { HF_PRINT("  MMARVALID addr="); HF_PRINT_HEX(mmfar, 32); HF_PRINT("\r\n"); }
    if (mmfsr & (1<<4)) HF_PRINT("  MSTKERR (stacking error)\r\n");
    if (mmfsr & (1<<3)) HF_PRINT("  MUNSTKERR (unstacking error)\r\n");
    if (mmfsr & (1<<1)) HF_PRINT("  DACCVIOL (data access violation)\r\n");
    if (mmfsr & (1<<0)) HF_PRINT("  IACCVIOL (instruction access violation)\r\n");
  }

  /* 从栈中提取PC和LR */
  uint32_t *stack_ptr;
  __ASM volatile ("MRS %0, MSP" : "=r" (stack_ptr));
  uint32_t stacked_pc = stack_ptr[6];
  uint32_t stacked_lr = stack_ptr[5];

  HF_PRINT("PC="); HF_PRINT_HEX(stacked_pc, 32); HF_PRINT("\r\n");
  HF_PRINT("LR="); HF_PRINT_HEX(stacked_lr, 32); HF_PRINT("\r\n");

  HF_PRINT("--- END HARDFAULT ---\r\n");

  __disable_irq();
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Prefetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  vPortSVCHandler();
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  xPortPendSVHandler();
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  /* FreeRTOS接管SysTick，HAL tick改由TIM4提供 */
  xPortSysTickHandler();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F1xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f1xx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */
/**
  * @brief EXTI Line0 interrupt handler (PB0 - YF-S201 water flow sensor)
  */
void EXTI0_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_FLAG(GPIO_PIN_0))
    {
        __HAL_GPIO_EXTI_CLEAR_FLAG(GPIO_PIN_0);
        YFS201_PulseHandler();
    }
}

/**
  * @brief TIM4 global interrupt handler (HAL time base)
  */
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim4);
}

/**
  * @brief USART2 interrupt handler (ESP8266 RX)
  */
void USART2_IRQHandler(void)
{
    USART2_IRQ_Callback();
}
/* USER CODE END 1 */
