/**
 * @file main.c
 * @brief STM32F103C8T6 port of RATCX1 traffic counter firmware
 *
 * Original device: dsPIC30F4011 (mikroC compiler, MPLAB IDE)
 * Target device:   STM32F103C8T6 (Blue Pill) + Air780 4G LTE + W25Q80 Flash
 * Toolchain:       STM32CubeIDE / arm-none-eabi-gcc + STM32 HAL
 *
 * ─── Pin Assignments ──────────────────────────────────────────────────
 *
 *  PA0  – ADC1_IN0   Battery voltage (VBAT)
 *  PA1  – ADC1_IN1   Solar panel voltage
 *  PA2  – TIM2_CH3   Input Capture from loop oscillator MUX output
 *  PA4  – GPIO Out   W25Q80 /CS  (SPI1 chip-select, active-low)
 *  PA5  – SPI1_SCK   W25Q80 CLK
 *  PA6  – SPI1_MISO  W25Q80 DO
 *  PA7  – SPI1_MOSI  W25Q80 DI
 *  PA8  – GPIO Out   Air780 PWRKEY (pulse HIGH ≥600ms to power on)
 *  PA9  – USART1_TX  Debug / config serial port (115200 baud)
 *  PA10 – USART1_RX
 *  PA11 – GPIO In    Air780 STATUS (HIGH = module powered on)
 *
 *  PB0  – GPIO Out   Loop MUX select A (LSB)
 *  PB1  – GPIO Out   Loop MUX select B (MSB)
 *  PB2  – GPIO Out   W25Q80 /WP  (normally HIGH = write-protect disabled)
 *  PB3  – GPIO Out   W25Q80 /HOLD (normally HIGH)
 *  PB5  – GPIO Out   onloop[0] LED indicator
 *  PB6  – GPIO Out   onloop[1] LED indicator
 *  PB7  – GPIO Out   onloop[2] LED indicator
 *  PB8  – GPIO Out   onloop[3] LED indicator
 *  PB9  – GPIO Out   connection_state LED
 *  PB10 – USART3_TX  → Air780 RXD
 *  PB11 – USART3_RX  ← Air780 TXD
 *  PB12 – GPIO Out   SD card /CS  (SPI2 chip-select, active-low)
 *  PB13 – SPI2_SCK   SD card SCK
 *  PB14 – SPI2_MISO  SD card DO  (DAT0)
 *  PB15 – SPI2_MOSI  SD card DI  (CMD)
 *
 *  PC13 – GPIO Out   System heartbeat LED (Blue Pill built-in, active-low)
 *
 * ─── External hardware ────────────────────────────────────────────────
 *  Loop oscillator: 4 inductive loops → 4:1 analogue mux (e.g. CD4052)
 *    Mux A = PB0, Mux B = PB1
 *    Mux output → PA2 (TIM2_CH3 Input Capture)
 *
 *  W25Q80 8Mbit SPI NOR Flash on SPI1 (PA4–PA7)
 *    Stores up to 128 intervals (10.7 hours) for offline recovery.
 *  SD card socket on SPI2 (PB12 /CS, PB13/14/15 SCK/MISO/MOSI)
 *    Optional secondary storage (raw blocks or FAT32 via FatFS).
 *  هر کدام از این دو که بوت شوند در runtime detect می‌شوند.
 *
 *  Air780 4G LTE module on USART3 (PB10/PB11) at 115200 baud
 *    PWRKEY = PA8, STATUS = PA11.
 *
 * ─── Clock configuration ──────────────────────────────────────────────
 *  HSE = 8 MHz crystal → PLL × 9 = 72 MHz SYSCLK
 *  AHB  = 72 MHz
 *  APB1 = 36 MHz (max for APB1) – USART3, TIM4, SPI2 here
 *  APB2 = 72 MHz – SPI1, USART1, ADC1, TIM2 here
 *
 * ─── Build notes ──────────────────────────────────────────────────────
 *  1. Create a new STM32F103C8T6 project in STM32CubeIDE
 *  2. Enable: TIM2 (CH3 IC), TIM4 (base), USART1, USART3, SPI1, SPI2,
 *     ADC1 (CH0+CH1)
 *  3. Copy Core/Src/*.c and Core/Inc/*.h to your project
 *  4. SD card optional – with FatFS فعال، Drivers/FatFS/README.md را ببینید
 *  5. Insert SIM card into Air780 module, set APN in config.h
 *  6. Build and flash with ST-Link
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
#include "air780_tcp.h"
#include "storage.h"

/* ─── HAL peripheral handles ─────────────────────────────────────────── */
TIM_HandleTypeDef  htim2;   /* Input Capture @1MHz                        */
TIM_HandleTypeDef  htim4;   /* 1ms tick                                   */
UART_HandleTypeDef huart1;  /* Debug serial port (PA9/PA10)               */
UART_HandleTypeDef huart3;  /* Air780 4G LTE (PB10/PB11)                  */
SPI_HandleTypeDef  hspi1;   /* W25Q80 SPI NOR Flash (PA4-PA7)             */
SPI_HandleTypeDef  hspi2;   /* SD card (PB13-PB15 + PB12 /CS)             */
ADC_HandleTypeDef  hadc1;   /* Battery + solar ADC                        */

/* ─── Global variables (defined here, declared extern in variables.h) ── */
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
volatile RTC_Time_t current_time     = {25,1,1,0,0,0}; /* default: 2025-01-01 */

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

/* ─── Private function prototypes ────────────────────────────────────── */
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

/* ─── ADC channel read helper ────────────────────────────────────────── */
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

/* ─────────────────────────────────────────────────────────────────────── */
/*                                main()                                   */
/* ─────────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* ── HAL + Clock init ─────────────────────────────────────────────── */
    HAL_Init();
    SystemClock_Config();

    /* ── Peripheral init ──────────────────────────────────────────────── */
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_TIM4_Init();
    MX_USART1_UART_Init();
    MX_USART3_UART_Init();
    MX_SPI1_Init();
    MX_SPI2_Init();
    MX_ADC1_Init();

    /* ── Load configuration from config.h defaults ────────────────────── */
    load_config();

    /* ── Reset interval data ──────────────────────────────────────────── */
    reset_interval_data();
    reset_interval();

    /* ── Start loop detector (TIM2 IC + TIM4 tick) ────────────────────── */
    loop_detector_init();

    debug_print("RATCX1-STM32 started\r\n");

    /* ── Calibrate loops (all loops must be free of vehicles) ─────────── */
    debug_print("Calibrating loops...\r\n");
    HAL_Delay(500);
    loop_calibrate();
    debug_print("Calibration done\r\n");

    /* ── Initialise storage (W25Q80 + SD card auto-detect) ─────────────── */
    {
        uint8_t be = storage_init();
        if (be == STORAGE_BE_NONE) {
            debug_print("Storage: no backend (neither W25Q80 nor SD)\r\n");
        } else {
            if (be & STORAGE_BE_W25Q80) debug_print("Storage: W25Q80 ready\r\n");
            if (be & STORAGE_BE_SDCARD) debug_print("Storage: SD card ready\r\n");
        }
    }

    /* ── Initialise Air780 4G LTE module ─────────────────────────────────── */
    debug_print("Air780 init...\r\n");
    if (air780_init() != 0) {
        set_error(VMN_ERR);   /* reuse error bit for modem fail */
        debug_print("Air780 init failed\r\n");
    } else {
        debug_print("Air780 ready\r\n");
    }

    /* ── Initialise protocol layer ────────────────────────────────────── */
    protocol_init();

    /* ── Initialise cal timers ────────────────────────────────────────── */
    memset(cal_timer, 0, sizeof(cal_timer));

    /* ═══════════════════════════════════════════════════════════════════ */
    /*                         MAIN LOOP                                   */
    /* ═══════════════════════════════════════════════════════════════════ */
    while (1)
    {
        /* ── Vehicle classification (set by TIM4 ISR) ─────────────────── */
        if (cal_class0 > 0) {
            cal_class(0);
            cal_class0 = 0;
        }
        if (cal_class1 > 0) {
            cal_class(1);
            cal_class1 = 0;
        }

        /* ── 1-second tasks ───────────────────────────────────────────── */
        if (timer_1_sec == 1)
        {
            timer_1_sec = 0;

            /* Heartbeat LED toggle (PC13, active-low) */
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

            /* Adaptive baseline: gradually update caldata toward current freq_mean */
            for (int i = 0; i < 4; i++) {
                cal_timer[i]++;
                if (cal_timer[i] > 30) {
                    caldata[i] = (caldata[i] + freq_mean[i]) / 2;
                }
            }

            /* Reset stale lane state machines */
            if (!onloop[0] && !onloop[1]) reset_class(0);
            if (!onloop[2] && !onloop[3]) reset_class(1);

            /* ADC readings */
            read_adc();

            /* ── 5-minute interval boundary ──────────────────────────── */
            if (current_time.second == 0 &&
                (current_time.minute % INTERVAL_PERIOD) == 0)
            {
                cal_interval();
                debug_print("Interval ready\r\n");
            }

            /* ── Error indicators on LEDs ────────────────────────────── */
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, onloop[0] ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, onloop[1] ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, onloop[2] ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, onloop[3] ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_9,
                protocol_is_connected() ? GPIO_PIN_SET : GPIO_PIN_RESET);
        }

        /* ── Protocol task (TCP communication) ───────────────────────── */
        protocol_task();

        /* ── Air780 background task (drain UART RX, process URCs) ───── */
        air780_task();
    }
}

/* ─── load_config ────────────────────────────────────────────────────── */
/**
 * Load configuration from compile-time defaults in config.h.
 * In a production build these could be loaded from Flash (emulated EEPROM)
 * via HAL_FLASH_Program() / HAL_FLASH_Read().
 */
static void load_config(void)
{
    loop[0] = LOOP0_EN;
    loop[1] = LOOP1_EN;
    loop[2] = LOOP2_EN;
    loop[3] = LOOP3_EN;

    loop_distance = LOOP_DISTANCE;
    loop_width    = LOOP_WIDTH;
    MARGINTOP     = MARGIN_TOP;
    MARGINBOT     = MARGIN_BOT;

    LIMX_val   = LIMX;
    LIMA_val   = LIMA;
    LIMB_val   = LIMB;
    LIMC_val   = LIMC;
    LIMD_val   = LIMD;
    LIMITE_val = LIMITE;

    DSPEED1_val = DSPEED1;
    NSPEED1_val = NSPEED1;
    DSPEED2_val = DSPEED2;
    NSPEED2_val = NSPEED2;

    AUTCAL_val = AUTCAL;
    power_type = POWER_TYPE;
    HMM        = 10;

    /* Ensure at least one loop is active */
    if (loop[0]+loop[1]+loop[2]+loop[3] == 0) {
        loop[0] = loop[1] = 1;
    }
}

/* ─── read_adc ───────────────────────────────────────────────────────── */
static void read_adc(void)
{
    vbat  = (int32_t)adc_read_channel(ADC_CHANNEL_0);  /* PA0 */
    solar = (int32_t)adc_read_channel(ADC_CHANNEL_1);  /* PA1 */

    /* Battery low */
    if (vbat < VBAT_LOW_ADC) set_error(LBT_ERR);
    else                      reset_error(LBT_ERR);

    /* Solar / power checks */
    if (power_type == POWER_TYPE_SOLAR) {
        if (current_time.hour > 10 && current_time.hour < 14 &&
            solar < VSOL_LOW_ADC)
            set_error(SOL_ERR);
        if (solar > VSOL_HIGH_ADC)
            reset_error(SOL_ERR);
    }
}

/* ─── debug_print ────────────────────────────────────────────────────── */
static void debug_print(const char *s)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)strlen(s), 100);
}

/* ─────────────────────────────────────────────────────────────────────── */
/*                       HAL IRQ callbacks                                 */
/* ─────────────────────────────────────────────────────────────────────── */

/** TIM4 period elapsed – 1ms tick */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) {
        loop_detector_isr();
    }
}

/** TIM4 and TIM2 IRQ handlers – forward to HAL */
void TIM4_IRQHandler(void)   { HAL_TIM_IRQHandler(&htim4); }
void TIM2_IRQHandler(void)   { HAL_TIM_IRQHandler(&htim2); }

/** USART3 IRQ handler – forward received bytes to Air780 driver */
void USART3_IRQHandler(void) { air780_uart_irq(); }

/* ─────────────────────────────────────────────────────────────────────── */
/*                     Peripheral initialisation                           */
/* ─────────────────────────────────────────────────────────────────────── */

static void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Use 8MHz HSE with PLL × 9 = 72MHz */
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

    /* ── Outputs ──────────────────────────────────────────────────────── */
    /* PA4 = W25Q80 /CS  (active-low, idle HIGH)
     * PA8 = Air780 PWRKEY (idle LOW) */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    GPIO_InitStruct.Pin   = GPIO_PIN_4 | GPIO_PIN_8;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PA11 = Air780 STATUS (input, HIGH = module on) */
    GPIO_InitStruct.Pin  = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* PB0=MUX_A, PB1=MUX_B,
     * PB2=/WP (W25Q80 write-protect, HIGH = disabled),
     * PB3=/HOLD (W25Q80 hold, HIGH = not held),
     * PB5-PB9=status LEDs,
     * PB12=SD card /CS (idle HIGH) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_SET);   /* /WP, /HOLD high */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);                /* SD /CS idle high */
    GPIO_InitStruct.Pin  = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 |
                           GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                           GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed= GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* PC13 = status LED (active-low on Blue Pill) */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    GPIO_InitStruct.Pin  = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed= GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
}

static void MX_TIM2_Init(void)
{
    /* TIM2: 32-bit free-running counter at 1MHz for Input Capture
     * PA2 = TIM2_CH3 (Input Capture) */
    TIM_IC_InitTypeDef   sConfigIC = {0};

    __HAL_RCC_TIM2_CLK_ENABLE();

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = TIM2_PSC;     /* 72MHz/72 = 1MHz */
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period             = 0xFFFFFFFF;   /* 32-bit auto-reload */
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    HAL_TIM_IC_Init(&htim2);

    sConfigIC.ICPolarity  = TIM_ICPOLARITY_RISING;
    sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
    sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
    sConfigIC.ICFilter    = 0x0;
    HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_3);

    /* Configure PA2 as TIM2_CH3 alternate function */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin       = GPIO_PIN_2;
    GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;     /* AF input floating for TIM */
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
}

static void MX_TIM4_Init(void)
{
    /* TIM4: 1ms interrupt tick
     * PSC=71, ARR=999 → 72MHz / 72 / 1000 = 1000 Hz = 1ms */
    __HAL_RCC_TIM4_CLK_ENABLE();

    htim4.Instance               = TIM4;
    htim4.Init.Prescaler         = TIM4_PSC;
    htim4.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim4.Init.Period             = TIM4_ARR;
    htim4.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim4);

    HAL_NVIC_SetPriority(TIM4_IRQn, 0, 0);  /* highest priority for timing */
    HAL_NVIC_EnableIRQ(TIM4_IRQn);
}

static void MX_USART1_UART_Init(void)
{
    /* USART1: PA9=TX, PA10=RX, 115200 8N1 (debug port) */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_9;            /* TX */
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_10;            /* RX */
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
    /* USART3: PB10=TX, PB11=RX, 115200 8N1 → Air780 4G LTE module
     * USART3 is on APB1 (36 MHz) so BRR is calculated from 36MHz. */
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin   = GPIO_PIN_10;           /* TX → Air780 RXD */
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_11;            /* RX ← Air780 TXD */
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = AIR780_UART_BAUD;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl   = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);

    /* Enable RXNE interrupt so air780_uart_irq() is called on each received byte */
    __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
    HAL_NVIC_SetPriority(USART3_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
}

static void MX_SPI1_Init(void)
{
    /* SPI1: PA5=SCK, PA6=MISO, PA7=MOSI → W25Q80 SPI NOR Flash
     * W25Q80 supports CPOL=0 CPHA=0 (SPI Mode 0) at up to 80 MHz.
     * We use prescaler /4 → 18 MHz to stay well within spec. */
    __HAL_RCC_SPI1_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* SCK + MOSI as AF push-pull */
    GPIO_InitStruct.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    /* MISO as input floating */
    GPIO_InitStruct.Pin  = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* CPOL=0 */
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;    /* CPHA=0 */
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;  /* 72/4=18MHz */
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi1);
}

static void MX_SPI2_Init(void)
{
    /* SPI2: PB13=SCK, PB14=MISO, PB15=MOSI → SD card socket
     * SPI2 lives on APB1 (36MHz). For SD init we need ≤400kHz, so
     * prescaler /256 → 140kHz. After successful sd_init() the prescaler
     * is bumped to /4 → 9MHz for normal block transfers (kept inside the
     * SD spec safe range and well below APB1 limits). */
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    /* SCK + MOSI as AF push-pull */
    GPIO_InitStruct.Pin   = GPIO_PIN_13 | GPIO_PIN_15;
    GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    /* MISO as input pull-up (SD spec: DAT0 needs pull-up) */
    GPIO_InitStruct.Pin  = GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    hspi2.Instance               = SPI2;
    hspi2.Init.Mode              = SPI_MODE_MASTER;
    hspi2.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi2.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi2.Init.CLKPolarity       = SPI_POLARITY_LOW;   /* SPI mode 0 */
    hspi2.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi2.Init.NSS               = SPI_NSS_SOFT;
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;  /* slow init */
    hspi2.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode            = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    HAL_SPI_Init(&hspi2);
}

/**
 * Switch SPI2 to high-speed mode after the SD card is initialised.
 * Called by storage_init() once sd_init() returns successfully.
 */
void sd_spi_set_high_speed(void)
{
    __HAL_SPI_DISABLE(&hspi2);
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;  /* 36/4 = 9MHz */
    HAL_SPI_Init(&hspi2);
}

static void MX_ADC1_Init(void)
{
    /* ADC1: CH0=PA0 (VBAT), CH1=PA1 (Solar) */
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

/* ─── HAL MSP (Minimal System Platform) stubs ───────────────────────── */
void HAL_MspInit(void)
{
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_PWR_CLK_ENABLE();
    /* Disable JTAG to free PB3/PB4/PA15 as GPIO */
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
}

/* ─── Error handler ──────────────────────────────────────────────────── */
void Error_Handler(void)
{
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(100);
    }
}
