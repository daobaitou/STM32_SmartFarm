/*
 * FreeRTOS V202411.00
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.
 * FreeRTOSConfig.h for STM32F103C8T6 @ 72MHz
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* 时钟与调度基础配置 */
#define configCPU_CLOCK_HZ              (72000000UL)
#define configTICK_RATE_HZ              (1000)
#define configUSE_PREEMPTION            1
#define configUSE_IDLE_HOOK             0
#define configUSE_TICK_HOOK             0

/* 内存配置 */
#define configTOTAL_HEAP_SIZE           ((size_t)(12 * 1024))
#define configMINIMAL_STACK_SIZE        ((uint16_t)128)
#define configMAX_TASK_NAME_LEN         (16)

/* 优先级配置 */
#define configMAX_PRIORITIES            (7)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  ( 5 << 4 )  /* 0x50, priority 5 for STM32F103 4-bit NVIC */

/* 调度特性 */
#define configUSE_TIME_SLICING          1
#define configIDLE_SHOULD_YIELD         1
#define configUSE_16_BIT_TICKS          0
#define configUSE_TRACE_FACILITY        0
#define configUSE_MUTEXES               1
#define configUSE_COUNTING_SEMAPHORES   1
#define configCHECK_FOR_STACK_OVERFLOW  2

/* 软件定时器 */
#define configUSE_TIMERS                1
#define configTIMER_TASK_PRIORITY       (3)
#define configTIMER_QUEUE_LENGTH        (5)
#define configTIMER_TASK_STACK_DEPTH    (128)

/* 协程（不使用） */
#define configUSE_CO_ROUTINES           0
#define configMAX_CO_ROUTINE_PRIORITIES (2)

/* API函数包含开关 */
#define INCLUDE_vTaskPrioritySet        1
#define INCLUDE_uxTaskPriorityGet       1
#define INCLUDE_vTaskDelete             1
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_xTaskGetCurrentTaskHandle 1

/* Cortex-M3中断优先级配置 */
#define configKERNEL_INTERRUPT_PRIORITY         (255)
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY (5)

/* 与HAL兼容：让调用FreeRTOS API的中断优先级>=11 */
/* STM32F103: 4位优先级，0最高，15最低 */
/* 数值转换: NVIC优先级 = (15-STM32优先级) << 4 */

#endif /* FREERTOS_CONFIG_H */
