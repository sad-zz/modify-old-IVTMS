# ─────────────────────────────────────────────────────────────────────────
#  RATCX1-STM32  –  bare-metal Makefile (no STM32CubeIDE required)
#
#  Build:   make
#  Output:  build/RATCX1-STM32.elf  .hex  .bin
#  Flash:   make flash          (needs st-flash from stlink-tools)
#       or: STM32CubeProgrammer / ST-Link Utility on the .hex / .bin
#  Clean:   make clean
#
#  Toolchain: arm-none-eabi-gcc  (sudo apt install gcc-arm-none-eabi)
# ─────────────────────────────────────────────────────────────────────────

TARGET  = RATCX1-STM32
BUILD   = build

# ─── Toolchain ───────────────────────────────────────────────────────────
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
AS      = $(PREFIX)gcc -x assembler-with-cpp
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

# ─── MCU ─────────────────────────────────────────────────────────────────
CPU     = -mcpu=cortex-m3
MCU     = $(CPU) -mthumb -mfloat-abi=soft
LDSCRIPT = STM32F103C8TX_FLASH.ld

# ─── Defines ─────────────────────────────────────────────────────────────
C_DEFS  = -DUSE_HAL_DRIVER -DSTM32F103xB

# ─── Include paths ───────────────────────────────────────────────────────
C_INCLUDES = \
  -ICore/Inc \
  -IDrivers/STM32F1xx_HAL_Driver/Inc \
  -IDrivers/STM32F1xx_HAL_Driver/Inc/Legacy \
  -IDrivers/CMSIS/Device/ST/STM32F1xx/Include \
  -IDrivers/CMSIS/Include

# ─── Sources ─────────────────────────────────────────────────────────────
# Application sources (exclude the legacy W5500 driver – replaced by Air780)
C_SOURCES = \
  Core/Src/main.c \
  Core/Src/stm32f1xx_it.c \
  Core/Src/loop_detector.c \
  Core/Src/classification.c \
  Core/Src/interval.c \
  Core/Src/protocol.c \
  Core/Src/air780_tcp.c \
  Core/Src/storage.c \
  Core/Src/w25q80.c \
  Core/Src/sd_spi.c \
  Core/Src/console.c \
  Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c

# HAL driver sources (all of them; --gc-sections drops the unused ones)
C_SOURCES += $(wildcard Drivers/STM32F1xx_HAL_Driver/Src/*.c)
# Drop the template file (not a real translation unit we want twice)
C_SOURCES := $(filter-out Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_msp_template.c \
                          Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_timebase_tim_template.c \
                          Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_timebase_rtc_alarm_template.c, \
                          $(C_SOURCES))

ASM_SOURCES = Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/gcc/startup_stm32f103xb.s

# ─── Flags ───────────────────────────────────────────────────────────────
OPT     = -Og
CFLAGS  = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections -g3 -std=gnu11
ASFLAGS = $(MCU) $(OPT) -Wall -fdata-sections -ffunction-sections -g3
LDFLAGS = $(MCU) -specs=nano.specs -specs=nosys.specs -T$(LDSCRIPT) \
          -Wl,-Map=$(BUILD)/$(TARGET).map,--cref -Wl,--gc-sections \
          -Wl,--start-group -lc -lm -Wl,--end-group

# ─── Objects ─────────────────────────────────────────────────────────────
OBJECTS  = $(addprefix $(BUILD)/,$(C_SOURCES:.c=.o))
OBJECTS += $(addprefix $(BUILD)/,$(ASM_SOURCES:.s=.o))

# ─── Rules ───────────────────────────────────────────────────────────────
all: $(BUILD)/$(TARGET).elf $(BUILD)/$(TARGET).hex $(BUILD)/$(TARGET).bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD)/%.o: %.s
	@mkdir -p $(dir $@)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@

$(BUILD)/%.hex: $(BUILD)/%.elf
	$(OBJCOPY) -O ihex $< $@

$(BUILD)/%.bin: $(BUILD)/%.elf
	$(OBJCOPY) -O binary -S $< $@

flash: $(BUILD)/$(TARGET).bin
	st-flash --reset write $< 0x08000000

clean:
	rm -rf $(BUILD)

.PHONY: all clean flash
