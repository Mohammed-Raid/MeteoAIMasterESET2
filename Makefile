##############################################################################
# TP2 ETRS923 - NUCLEO-N657X0-Q
# Compilation en ligne de commande (arm-none-eabi-gcc) :
#   make            -> partie choisie dans Core/Inc/main.h (PARTIE_TP)
#   make PARTIE=1   -> force la partie 1 (idem 2 ou 3)
#   make clean
# Resultat : build/TP2_NUCLEO_N657.elf / .bin / .hex
##############################################################################
TARGET = TP2_NUCLEO_N657

# Chaque configuration a son propre dossier d'objets. Sans cela, changer
# PARTIE ne declencherait aucune recompilation : les .o ne gardent pas trace
# des -D, make les croirait a jour, et on obtiendrait silencieusement le
# binaire de la partie precedente.
BUILD_DIR = build/partie$(or $(PARTIE),defaut)$(if $(TEMOIN),-temoin,)$(if $(CONSO),-conso,)
PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

# Sources -------------------------------------------------------------------
C_SOURCES = $(wildcard Core/Src/*.c) \
            $(wildcard Drivers/STM32N6xx_HAL_Driver/Src/*.c) \
            Drivers/BSP/Components/lan8742/lan8742.c \
            $(wildcard Drivers/STMems/*.c) \
            $(wildcard Middlewares/Third_Party/FreeRTOS/Source/*.c) \
            Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM55_NTZ/non_secure/port.c \
            Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM55_NTZ/non_secure/portasm.c \
            Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c \
            $(wildcard Middlewares/Third_Party/LwIP/src/core/*.c) \
            $(wildcard Middlewares/Third_Party/LwIP/src/core/ipv4/*.c) \
            $(wildcard Middlewares/Third_Party/LwIP/src/api/*.c) \
            Middlewares/Third_Party/LwIP/src/netif/ethernet.c \
            $(wildcard LWIP/Target/*.c)
ASM_SOURCES = Core/Startup/startup_stm32n657x0hxq_fsbl.s

# Options ---------------------------------------------------------------------
MCU = -mcpu=cortex-m55 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard -mcmse

C_DEFS = -DUSE_HAL_DRIVER -DSTM32N657xx
ifdef PARTIE
C_DEFS += -DPARTIE_TP=$(PARTIE)
endif
# TP3 §3.d : make PARTIE=4 TEMOIN=1 produit le programme temoin, identique
# en tout point mais sans l'execution du reseau de neurones.
ifdef TEMOIN
C_DEFS += -DTEMOIN_SANS_IA=$(TEMOIN)
endif
# TP3 §3.d : CONSO=1 donne la configuration de mesure du courant : LEDs et
# console coupees, classifications enchainees. Les deux binaires a comparer
# sont donc :  make PARTIE=4 CONSO=1   et   make PARTIE=4 CONSO=1 TEMOIN=1
ifdef CONSO
C_DEFS += -DMESURE_CONSO=$(CONSO)
endif

C_INCLUDES = -ICore/Inc \
             -IDrivers/STM32N6xx_HAL_Driver/Inc \
             -IDrivers/STM32N6xx_HAL_Driver/Inc/Legacy \
             -IDrivers/CMSIS/Device/ST/STM32N6xx/Include \
             -IDrivers/CMSIS/Include \
             -IDrivers/BSP/Components/lan8742 \
             -IDrivers/STMems \
             -IMiddlewares/Third_Party/FreeRTOS/Source/include \
             -IMiddlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM55_NTZ/non_secure \
             -IMiddlewares/Third_Party/LwIP/src/include \
             -ILWIP/Target

OPT = -Os -g3
CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -std=gnu11 -Wall \
         -ffunction-sections -fdata-sections -fstack-usage -MMD -MP
ASFLAGS = $(MCU) $(C_DEFS) $(OPT) -Wall -ffunction-sections -fdata-sections

LDSCRIPT = STM32N657X0HXQ_AXISRAM2_fsbl.ld
LDFLAGS = $(MCU) -T$(LDSCRIPT) --specs=nano.specs --specs=nosys.specs \
          -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections \
          -Wl,--print-memory-usage -Wl,--no-warn-rwx-segments \
          -Wl,--start-group -lc -lm -Wl,--end-group

# Regles ----------------------------------------------------------------------
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).bin $(BUILD_DIR)/$(TARGET).hex

OBJECTS = $(addprefix $(BUILD_DIR)/,$(C_SOURCES:.c=.o)) \
          $(addprefix $(BUILD_DIR)/,$(ASM_SOURCES:.s=.o))

$(BUILD_DIR)/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile
	@mkdir -p $(dir $@)
	$(AS) -c $(ASFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) $(LDSCRIPT)
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	$(SZ) $@

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf
	$(CP) -O binary -S $< $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf
	$(CP) -O ihex $< $@

clean:
	rm -rf build

-include $(wildcard $(BUILD_DIR)/**/*.d)

.PHONY: all clean
