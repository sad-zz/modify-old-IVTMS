/**
 * @file stm32f1xx_it.h
 * @brief Cortex-M3 / NVIC IRQ handler declarations
 *
 * این فایل توسط CubeMX به‌صورت خودکار ساخته می‌شود. نسخه‌ی موجود در ریپو
 * جایگزین آن می‌شود (با اجرای setup_after_cubemx.sh یا کپی دستی).
 */

#ifndef STM32F1xx_IT_H
#define STM32F1xx_IT_H

#ifdef __cplusplus
extern "C" {
#endif

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SVC_Handler(void);
void DebugMon_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);

void TIM2_IRQHandler(void);
void TIM4_IRQHandler(void);
void USART3_IRQHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* STM32F1xx_IT_H */
