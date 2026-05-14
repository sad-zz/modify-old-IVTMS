/**
 * @file main.c  [نسخه SIM800L]
 * @brief STM32F103C8T6 – RATCX1 با SIM800L GPRS به‌جای Air780 4G LTE
 *
 * ─── تفاوت این نسخه با stm32-firmware/Core/Src/main.c ─────────────────
 *  1. include: sim800l_tcp.h  (نه air780_tcp.h)
 *  2. MX_USART3_UART_Init: baud = 9600  (SIM800L auto-baud)
 *  3. USART3_IRQHandler → sim800l_uart_irq()
 *  4. حلقه اصلی: sim800l_task() به‌جای air780_task()
 *  5. init: sim800l_init() به‌جای air780_init()
 *  6. PWRKEY: idle HIGH (SIM800L روشن = LOW pulse، برعکس Air780)
 *
 * ─── Pin Assignments ──────────────────────────────────────────────────
 *  PA0  – ADC1_IN0   Battery voltage (VBAT)
 *  PA1  – ADC1_IN1   Solar panel voltage
 *  PA2  – TIM2_CH3   Input Capture از loop oscillator MUX
 *  PA4  – GPIO Out   W25Q80 /CS
 *  PA5  – SPI1_SCK
 *  PA6  – SPI1_MISO
 *  PA7  – SPI1_MOSI
 *  PA8  – GPIO Out   SIM800L PWRKEY  (idle HIGH → LOW pulse ≥1s to power on)
 *  PA9  – USART1_TX  Debug serial
 *  PA10 – USART1_RX
 *  PA11 – GPIO In    SIM800L STATUS  (HIGH = on)
 *
 *  PB0  – GPIO Out   Loop MUX select A
 *  PB1  – GPIO Out   Loop MUX select B
 *  PB2  – GPIO Out   W25Q80 /WP
 *  PB3  – GPIO Out   W25Q80 /HOLD
 *  PB5-8– GPIO Out   Loop LEDs
 *  PB9  – GPIO Out   Connection LED
 *  PB10 – USART3_TX  → SIM800L RXD
 *  PB11 – USART3_RX  ← SIM800L TXD
 *  PB12 – GPIO Out   SD card /CS
 *  PB13 – SPI2_SCK
 *  PB14 – SPI2_MISO
 *  PB15 – SPI2_MOSI
 *  PC13 – GPIO Out   Heartbeat LED
 *
 * ─── نکته مهم سخت‌افزاری ─────────────────────────────────────────────
 *  SIM800L نیاز به ولتاژ 3.7–4.2V دارد. از 3.3V مستقیم STM32 تغذیه نکنید.
 *  از یک باتری LiPo 3.7V یا رگولاتور مناسب استفاده کنید.
 *  جریان peak تا 2A – از منبع تغذیه ضعیف استفاده نکنید.
 *
 * ─── فایل‌های مشترک با stm32-firmware/ ──────────────────────────────
 *  Core/Inc: classification.h, interval.h, loop_detector.h,
 *             protocol.h, sd_spi.h, storage.h, variables.h, w25q80.h, w5500_tcp.h
 *  Core/Src: classification.c, interval.c, loop_detector.c,
 *             protocol.c, sd_spi.c, storage.c, w25q80.c
 *  این فایل‌ها را کپی کنید یا در CMakeLists/Makefile مسیر مشترک بدهید.
 */

#include "main.h"
#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "variables.h"
#include "loop_detector.h"
#include "classification.h"
#include "interval.h"
#include "protocol.h"
#include "sim800l_tcp.h"   /* <── تنها تفاوت include */
#include "storage.h"

/* ─── HAL handles ────────────────────────────────────────────────────── */
TIM_HandleTypeDef  htim2;
TIM_HandleTypeDef  htim4;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
SPI_HandleTypeDef  hspi1;
SPI_HandleTypeDef  hspi2;
ADC_HandleTypeDef  hadc1;

/* ─── Global variables ───────────────────────────────────────────────── */
volatile uint16_t error_byte         = 0;
volatile uint16_t error_byte_last    = 0;
volatile int16_t  timer_1_sec        = 0;
volatile int32_t  milisec            = 0;
volatile uint32_t how_many_micro     = 0;
volatile int16_t  onloop[4]          = {0};
volatile int16_t  Line_Dir[2]        = {0};
volatile int16_t  timexen[2]         = {0};
volatile int32_t  timex[2]           = {0};
volatile int16_t  current_loop       = 0;
volatile int16_t  docal              = 0;
volatile int16_t  forth              = 0;
volatile int32_t  current_gap[2]     = {0};
volatile int16_t  loop_error_tmp     = 0;
volatile uint32_t capw=0, capx=0, capy=0, capz=0;
volatile uint32_t freq_mean[4]       = {0};
volatile uint32_t caldata[4]         = {0};
volatile uint32_t calsum[4]          = {0};
volatile uint16_t calpointer[4]      = {0};
volatile float    freq_help          = 0;
volatile int16_t  dev[4]             = {0};
volatile uint32_t totalv1a=0,totalv1b=0,totalv1c=0,totalv1d=0,totalv1e=0,totalv1x=0;
volatile uint32_t totalv2a=0,totalv2b=0,totalv2c=0,totalv2d=0,totalv2e=0,totalv2x=0;
volatile int32_t  l1occ=0, l2occ=0;
volatile int32_t  vbat=0, solar=0;
volatile int16_t  line1step=0, line2step=0;
volatile int32_t  T1[2]={0}, T2[2]={0}, T3[2]={0};
volatile int32_t  T2S=0;
volatile uint8_t  cal_class0=0, cal_class1=0;
volatile RTC_Time_t current_time     = {25,1,1,0,0,0};

char datetimesec[22] = "2025.01.01-00:00:00.0";
char interval_data[512];

int loop[4];
int MARGINTOP, MARGINBOT;
int loop_distance, loop_width;
int LIMX_val, LIMA_val, LIMB_val, LIMC_val, LIMD_val, LIMITE_val;
int DSPEED1_val, NSPEED1_val, DSPEED2_val, NSPEED2_val;
int AUTCAL_val, HMM, power_type;

uint16_t cal_interval_cnt;
uint16_t cal_timer[4];
IntervalData_t current_interval;
Vehicle_t lastvehicle;

/* ─── Forward declarations ───────────────────────────────────────────── */
static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_SPI1_Init(void);
static void MX_SPI2_Init(void);
static void MX_ADC1_Init(void);
static void load_config(void);
static void read_adc(void);
static void debug_print(const char *s);

static uint32_t adc_read_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel      = channel;
    sConfig.Rank         = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_55CYCLES_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    return HAL_ADC_GetValue(&hadc1);
}

/* ─── main ───────────────────────────────────────────────────────────── */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_TIM4_Init();
    MX_USART1_UART_Init();
    MX_USART3_UART_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_ADC1_Init();

    load_config();
    reset_interval_data();
    reset_interval();
    loop_detector_init();

    debug_print("RATCX1-STM32-SIM800L started\r\n");

    debug_print("Calibrating loops...\r\n");
    HAL_Delay(500);
    loop_calibrate();
    debug_print("Calibration done\r\n");

    {
        uint8_t be = storage_init();
        if (be == STORAGE_BE_NONE)
            debug_print("Storage: no backend\r\n");
        else {
            if (be & STORAGE_BE_W25Q80) debug_print("Storage: W25Q80 ready\r\n");
            if (be & STORAGE_BE_SDCARD) debug_print("Storage: SD card ready\r\n");
        }
    }

    /* ── SIM800L init (به‌جای air780_init) ───────────────────────────── */
    debug_print("SIM800L init...\r\n");
    if (sim800l_init() != 0) {
        set_error(VMN_ERR);
        debug_print("SIM800L init failed\r\n");
    } else {
        debug_print("SIM800L ready\r\n");
    }

    protocol_init();
    memset(cal_timer, 0, sizeof(cal_timer));

    while (1)
    {
        if (cal_class0 > 0) { cal_class(0); cal_class0 = 0; }
        if (cal_class1 > 0) { cal_class(1); cal_class1 = 0; }

        if (timer_1_sec == 1)
        {
            timer_1_sec = 0;
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

            for (int i = 0; i < 4; i++) {
                cal_timer[i]++;
                if (cal_timer[i] > 30)
                    caldata[i] = (caldata[i] + freq_mean[i]) / 2;
            }

            if (!onloop[0] && !onloop[1]) reset_class(0);
            if (!onloop[2] && !onloop[3]) reset_class(1);

            read_adc();

            if (current_time.second == 0 &&
                (current_time.minute % INTERVAL_PERIOD) == 0)
            {
                cal_interval();
                debug_print("Interval ready\r\n");
            }

            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, onloop[0] ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, onloop[1] ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, onloop[2] ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, onloop[3] ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9,
                protocol_is_connected() ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        protocol_task();
        sim800l_task();   /* <── به‌جای air780_task */
    }
}

/* ─── load_config ────────────────────────────────────────────────────── */
static void load_config(void)
{
    loop[0] = LOOP0_EN; loop[1] = LOOP1_EN;
    loop[2] = LOOP2_EN; loop[3] = LOOP3_EN;
    loop_distance = LOOP_DISTANCE;
    loop_width    = LOOP_WIDTH;
    MARGINTOP     = MARGIN_TOP;
    MARGINBOT     = MARGIN_BOT;
    LIMX_val   = LIMX;  LIMA_val = LIMA;
    LIMB_val   = LIMB;  LIMC_val = LIMC;
    LIMD_val   = LIMD;  LIMITE_val = LIMITE;
    DSPEED1_val = DSPEED1; NSPEED1_val = NSPEED1;
    DSPEED2_val = DSPEED2; NSPEED2_val = NSPEED2;
    AUTCAL_val = AUTCAL;
    power_type = POWER_TYPE;
    HMM        = 10;
    if (loop[0]+loop[1]+loop[2]+loop[3] == 0)
        loop[0] = loop[1] = 1;
}

static void read_adc(void)
{
    vbat  = (int32_t)adc_read_channel(ADC_CHANNEL_0);
    solar = (int32_t)adc_read_channel(ADC_CHANNEL_1);
    if (vbat < VBAT_LOW_ADC) set_error(LBT_ERR);
    else                      reset_error(LBT_ERR);
    if (power_type == POWER_TYPE_SOLAR) {
        if (current_time.hour > 10 && current_time.hour < 14 &&
            solar < VSOL_LOW_ADC)
            set_error(SOL_ERR);
        if (solar > VSOL_HIGH_ADC)
            reset_error(SOL_ERR);
    }
}

static void debug_print(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

/* ─── IRQ handlers ───────────────────────────────────────────────────── */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) loop_detector_isr();
}

void TIM4_IRQHandler(void)   { HAL_TIM_IRQHandler(&htim4); }
void TIM2_IRQHandler(void)   { HAL_TIM_IRQHandler(&htim2); }
void USART3_IRQHandler(void) { sim800l_uart_irq(); }  /* <── تفاوت مهم */

/* ─── Peripheral init ────────────────────────────────────────────────── */
static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState       = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.PLL.PLLState   = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL     = RCC_PLL_MUL9;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                       RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /* PA4=/CS(idle HIGH)، PA8=SIM800L PWRKEY(idle HIGH – برعکس Air780!) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);  /* idle HIGH برای SIM800L */
    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_8;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
    GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                           GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                           GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed= GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin  = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed= GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

static void MX_TIM2_Init(void)
{
    TIM_IC_InitTypeDef sConfigIC = {0};
    __HAL_RCC_TIM2_CLK_ENABLE();
    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = TIM2_PSC;
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period             = 0xFFFFFFFF;
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_IC_Init(&htim2);
    sConfigIC.ICPolarity  = TIM_ICPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter    = 0x0;
    HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_3);
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

static void MX_TIM4_Init(void)
{
    __HAL_RCC_TIM4_CLK_ENABLE();
    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = TIM4_PSC;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period             = TIM4_ARR;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim4);
    HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
}

static void MX_USART1_UART_Init(void)
{
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_9;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin  = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl   = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

static void MX_USART3_UART_Init(void)
{
    /* SIM800L: 9600 baud (auto-baud mode پیش‌فرض) */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_10;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.Pin  = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = SIM800L_UART_BAUD;   /* 9600 */
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl   = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

static void MX_SPI1_Init(void)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    GPIO_InitStruct.Pin  = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);
}

static void MX_SPI2_Init(void)
{
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_13 | GPIO_PIN_15;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    GPIO_InitStruct.Pin  = GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi2);
}

void sd_spi_set_high_speed(void)
{
    __HAL_SPI_DISABLE(&hspi2);
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
    HAL_SPI_Init(&hspi2);
}

static void MX_ADC1_Init(void)
{
    __HAL_RCC_ADC1_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    hadc1.Instance                   = ADC1;
    hadc1.Init.ScanConvMode          = ADC_SCAN_DISABLE;
    hadc1.Init.ContinuousConvMode    = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv      = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign             = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion       = 1;
    HAL_ADC_Init(&hadc1);
    HAL_ADCEx_Calibration_Start(&hadc1);
}

void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(100);
    }
}
