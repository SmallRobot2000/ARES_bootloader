
# Toolchain path/prefix
TOOLCHAIN_PREFIX ?= riscv32-unknown-elf-

# Link kernel / dtb into ELF
CONFIG_KERNEL_EMBEDDED ?= n

# [if KERNEL_EMBEDDED=n]
CONFIG_DTB_SRC         ?= 0x88500000
CONFIG_DTB_DST         ?= 0x82300000
CONFIG_KERNEL_SRC      ?= 0x88600000
CONFIG_KERNEL_DST      ?= 0x80400000
CONFIG_DTB_SIZE        ?= 16384
CONFIG_KERNEL_SIZE     ?= 6500000

###############################################################################
# Variables: Safe defaults (no changes required)
###############################################################################
# Target name
ELF_NAME    ?= riscv-linux-boot.elf
BIN_NAME    ?= riscv-linux-boot.bin

# Source files
SRC_DIR      = .

# Directories
ELF_DIR     ?=
OBJ_DIR     ?= $(abspath ./obj)/


###############################################################################
# Variables: Source files
###############################################################################
ifeq ($(CONFIG_KERNEL_EMBEDDED),y)
SRC :=   boot.S \
		 $(foreach src,$(SRC_DIR),$(filter-out $(src)/boot.S, $(wildcard $(src)/*.S))) \
		 $(foreach src,$(SRC_DIR),$(wildcard $(src)/*.c))
else
SRC :=   boot.S \
		 $(foreach src,$(SRC_DIR),$(wildcard $(src)/*.c))
endif

###############################################################################
# Variables: Compiler Options
###############################################################################
EXTRA_CFLAGS =
ifeq ($(CONFIG_KERNEL_EMBEDDED),y)
  EXTRA_CFLAGS+= -DCONFIG_KERNEL_EMBEDDED
  EXTRA_CFLAGS+= -DPAYLOAD_BINARY=\"$(PAYLOAD)\"
  EXTRA_CFLAGS+= -DDTB_BINARY=\"$(DTB)\"
else
  EXTRA_CFLAGS+= -DCONFIG_DTB_SRC=$(CONFIG_DTB_SRC)
  EXTRA_CFLAGS+= -DCONFIG_DTB_DST=$(CONFIG_DTB_DST)
  EXTRA_CFLAGS+= -DCONFIG_DTB_SIZE=$(CONFIG_DTB_SIZE)
  EXTRA_CFLAGS+= -DCONFIG_KERNEL_SRC=$(CONFIG_KERNEL_SRC)
  EXTRA_CFLAGS+= -DCONFIG_KERNEL_DST=$(CONFIG_KERNEL_DST)
  EXTRA_CFLAGS+= -DCONFIG_KERNEL_SIZE=$(CONFIG_KERNEL_SIZE)
endif
EXTRA_CFLAGS+= -Wno-unused-variable  -march=rv32imafc_zicbom -mabi=ilp32

# Options
BASE_ADDRESS      = 0x80000000
PLATFORM_LDFLAGS  = -nostartfiles -nodefaultlibs -nostdlib -lgcc -T./flash.ld


OPT        ?= 2
CFLAGS	   := -Ttext $(BASE_ADDRESS) -O$(OPT) -g -Wall $(patsubst %,-I%,$(SRC_DIR)) $(EXTRA_CFLAGS)
ASFLAGS    := 
LDFLAGS    := $(PLATFORM_LDFLAGS) -Wl,--defsym=BASE_ADDRESS=$(BASE_ADDRESS)

###############################################################################
# Variables: Toolchain
###############################################################################
CC          = $(TOOLCHAIN_PREFIX)gcc
AS          = $(TOOLCHAIN_PREFIX)as
LD          = $(TOOLCHAIN_PREFIX)ld
OBJDUMP     = $(TOOLCHAIN_PREFIX)objdump
OBJCOPY     = $(TOOLCHAIN_PREFIX)objcopy

###############################################################################
# Variables: SRC / Object list
###############################################################################
src2obj = $(OBJ_DIR)$(patsubst %$(suffix $(1)),%.o,$(notdir $(1)))
OBJ    := $(foreach src,$(SRC),$(call src2obj,$(src)))

###############################################################################
# Rules
###############################################################################
all: $(ELF_DIR)$(ELF_NAME) $(ELF_DIR)$(BIN_NAME)

$(OBJ_DIR) $(ELF_DIR):
	@mkdir -p $@

clean:
	@echo "# Cleaning"
	@rm -rf $(OBJ_DIR) $(OBJ) $(ELF_DIR)$(ELF_NAME) $(PAYLOAD) $(DTB)

define template_c
$(call src2obj,$(1)): $(1) $(PAYLOAD) $(DTB) | $(OBJ_DIR)
	@echo "# CC $(notdir $$<)"
	@$(CC) $(CFLAGS) -c $$< -o $$@
endef

$(foreach src,$(SRC),$(eval $(call template_c,$(src))))

$(ELF_DIR)$(ELF_NAME): $(OBJ) | $(ELF_DIR)
	@echo "# LD $(notdir $@)"
	@$(CC) $(OBJ) -o $@ $(LDFLAGS)


$(ELF_DIR)$(BIN_NAME): $(ELF_DIR)$(ELF_NAME)
	@echo "# OBJCOPY $(notdir $@)"
	@$(OBJCOPY) -O binary $< $@
