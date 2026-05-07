/**
 * @file stm32f1xx_it.c
 * @brief Cortex-M3 / NVIC IRQ handlers
 *
 * این فایل توسط CubeMX به‌صورت خودکار ساخته می‌شود. نسخه‌ی موجود در ریپو
 * جایگزین آن می‌شود (با اجرای setup_after_cubemx.sh یا کپی دستی).
 *
 * تفاوت با نسخه‌ی پیش‌فرض CubeMX: handler های TIM2/TIM4/USART3 به جای
 * فقط `HAL_TIM_IRQHandler` و `HAL_UART_IRQHandler`، مستقیم به درایور
 * Air780 ما (air780_uart_irq) متصل شده‌اند.
 */

#include "main.h"
#include "stm32f1xx_it.h"
#include "air780_tcp.h"

/* ─── Extern HAL handles (defined in main.c) ──────────────────────── */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;

/* ─── Cortex-M3 system handlers ──────────────────────────────────── */
void NMI_Handler(void)
{
    while (1) { }
}

void HardFault_Handler(void)
{
    while (1) { }
}

void MemManage_Handler(void)
{
    while (1) { }
}

void BusFault_Handler(void)
{
    while (1) { }
}

void UsageFault_Handler(void)
{
    while (1) { }
}

void SVC_Handler(void) { }
void DebugMon_Handler(void) { }
void PendSV_Handler(void) { }

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ─── Peripheral IRQ handlers ────────────────────────────────────── */

/** TIM2 IRQ – Input Capture (CC3) for loop oscillator. Forward to HAL,
 *  HAL calls our HAL_TIM_IC_CaptureCallback in loop_detector.c. */
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim2);
}

/** TIM4 IRQ – 1ms tick. Forward to HAL, HAL calls our
 *  HAL_TIM_PeriodElapsedCallback in main.c. */
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim4);
}

/** USART3 IRQ – Air780 UART RX. Direct to driver (not HAL_UART) so we
 *  can drain the RX register byte-by-byte into our ring buffer with
 *  minimum latency. */
void USART3_IRQHandler(void)
{
    air780_uart_irq();
}
