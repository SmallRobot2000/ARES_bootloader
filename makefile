###############################################################################
# Toolchain and target architecture
###############################################################################

TOOLCHAIN_PREFIX ?= riscv64-unknown-elf-

RISCV_ARCH ?= rv32imafc_zifencei_zicbom
RISCV_ABI  ?= ilp32

LIBC_SPECS ?= --specs=picolibc.specs

EXTRA_CFLAGS += \
	-Wno-unused-variable \
	-march=$(RISCV_ARCH) \
	-mabi=$(RISCV_ABI)

# Link kernel / dtb into ELF
CONFIG_KERNEL_EMBEDDED ?= n

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

endif
EXTRA_CFLAGS+= -Wno-unused-variable -march=rv32imafc_zifencei_zicbom -mabi=ilp32

# Options
BASE_ADDRESS      = 0x80000000
PLATFORM_LDFLAGS  = -nostartfiles -nodefaultlibs -nostdlib -lgcc -T./flash.ld -march=$(RISCV_ARCH) \
	-mabi=$(RISCV_ABI) \
	-mcmodel=medany


OPT        ?= 2
CFLAGS := $(LIBC_SPECS) \
          -Ttext $(BASE_ADDRESS) \
          -O$(OPT) \
          -g \
          -Wall \
          $(patsubst %,-I%,$(SRC_DIR)) \
          $(EXTRA_CFLAGS)
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
