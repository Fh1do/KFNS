# Algun dia tendre que documentar todo el proyecto, me gustaria hacerle tambien su / sus diagramas de flujo,
# pero por ahora no tengo tiempo para eso, o sera que me da flojera hacerlo, no lo se realmente?

ifeq ($(strip $(DEVKITPRO)),)
    $(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

APP_TITLE         := Khowel
APP_AUTHOR        := Fh1do
APP_VERSION       := v1.1.0
APP_ICON          := $(DEVKITPRO)/libnx/default_icon.jpg
TARGET            := Khowel
EXTENSION         := ovl
OUT               := out
BUILD             := build
ULTRAHAND_DIR     := libs/libultrahand-v2.5.4

SYSMODULE_DIR     := src/sysmodule
SYSMODULE_TARGET  := sysmodule
SYSMODULE_TITLEID := 20A10F2910600D0C
SYSMODULE_CONFIG  := $(SYSMODULE_DIR)/$(SYSMODULE_TARGET).json

PORTLIBS          := $(DEVKITPRO)/portlibs/switch
LIBNX             := $(DEVKITPRO)/libnx
LIBS              := $(LIBNX) $(PORTLIBS)

export PATH      := $(DEVKITPRO)/tools/bin:$(DEVKITPRO)/devkitA64/bin:$(PORTLIBS)/bin:$(PATH)

OVERLAY_SOURCES   := src/overlay $(ULTRAHAND_DIR)/common $(ULTRAHAND_DIR)/libultra/source $(ULTRAHAND_DIR)/libtesla/source
INCLUDES          := $(ULTRAHAND_DIR)/common $(ULTRAHAND_DIR)/libultra/include $(ULTRAHAND_DIR)/libtesla/include

SYSMODULE_ELF     := $(BUILD)/sysmodule/$(SYSMODULE_TARGET).elf
SYSMODULE_NSO     := $(OUT)/$(SYSMODULE_TARGET).nso
SYSMODULE_NPDM    := $(OUT)/$(SYSMODULE_TARGET).npdm
SYSMODULE_NSP     := $(OUT)/$(SYSMODULE_TARGET).nsp
SYSMODULE_EXEFS   := $(BUILD)/sysmodule/exefs
STAGE_DIR         := $(OUT)/KFNS-$(APP_VERSION)
STAGE_DIR_WINDOWS := $(shell cygpath -w "$(abspath $(STAGE_DIR))")
PATCH_DIR         := $(STAGE_DIR)/atmosphere/exefs_patches/mastervolume_patches
PATCH_DIR_WINDOWS := $(shell cygpath -w "$(abspath $(PATCH_DIR))")
PATCH_TOOL        := misc/MasterVolumePatcher/MasterVolumePatcher.py
PATCH_SOURCE_DIR  := misc/MasterVolumePatcher/patches
SYSMODULE_STAGE   := $(STAGE_DIR)/atmosphere/contents/$(SYSMODULE_TITLEID)
SYSMODULE_PACKAGE := $(SYSMODULE_STAGE)/exefs.nsp
SYSMODULE_FLAG    := $(SYSMODULE_STAGE)/flags/boot2.flag

DEFINES           := __SWITCH__ APP_VERSION=\"$(APP_VERSION)\" APP_AUTHOR=\"$(APP_AUTHOR)\"
ARCH              := -march=armv8-a+crc+crypto+simd -mtune=cortex-a57 -mtp=soft -fpie
SYSMODULE_ARCH    := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
FLAGS             := -Wall -pipe -g -O2 -ffunction-sections -fdata-sections
CFLAGS            := -std=gnu11
CXXFLAGS          := -std=gnu++23 -fno-exceptions -fno-rtti
LDFLAGS           := -Wl,-pie -Wl,--gc-sections -Wl,--as-needed -specs=$(DEVKITPRO)/libnx/switch.specs -g
LINKS             := -lcurl -lz -lminizip -lmbedtls -lmbedx509 -lmbedcrypto -lnx

PREFIX            := aarch64-none-elf-
CC                := $(PREFIX)gcc
CXX               := $(PREFIX)g++
LD                := $(PREFIX)g++
NM                := $(PREFIX)gcc-nm

OVERLAY_CFILES    := $(shell find $(OVERLAY_SOURCES) -name '*.c')
OVERLAY_CPPFILES  := $(shell find $(OVERLAY_SOURCES) -name '*.cpp')
OVERLAY_OFILES    := $(OVERLAY_CFILES:%=$(BUILD)/%.o) $(OVERLAY_CPPFILES:%=$(BUILD)/%.o)
DEPENDENCY_FILES  := $(OVERLAY_OFILES:.o=.d)
SYSMODULE_CPPFILES := $(shell find $(SYSMODULE_DIR) -name '*.cpp')
SYSMODULE_OFILES  := $(patsubst $(SYSMODULE_DIR)/%.cpp,$(BUILD)/sysmodule/%.cpp.o,$(SYSMODULE_CPPFILES))
SYSMODULE_DFILES  := $(SYSMODULE_OFILES:.o=.d)

NX_TARGET         := $(OUT)/$(TARGET).$(EXTENSION)
ELF_TARGET        := $(OUT)/$(TARGET).elf
NACP_TARGET       := $(OUT)/$(TARGET).nacp
TEST_OVERLAY      := $(STAGE_DIR)/switch/.overlays/$(TARGET).$(EXTENSION)

DEFINE_FLAGS      := $(addprefix -D,$(DEFINES)) -DAPP_TITLE='"$(APP_TITLE)"'
INCLUDE_FLAGS     := $(addprefix -I$(CURDIR)/,$(INCLUDES)) $(foreach dir,$(LIBS),-I$(dir)/include)
LIB_FLAGS         := $(foreach dir,$(LIBS),-L$(dir)/lib)
SYSMODULE_CPPFLAGS := -D__SWITCH__ -I$(CURDIR)/$(SYSMODULE_DIR) -I$(CURDIR)/$(SYSMODULE_DIR)/services -I$(CURDIR)/src/ipc -I$(LIBNX)/include
SYSMODULE_CXXFLAGS := -g -Wall -O2 -ffunction-sections -fdata-sections $(SYSMODULE_ARCH) -std=gnu++20 -fno-rtti -fno-exceptions

NROFLAGS          := --icon=$(APP_ICON) --nacp=$(NACP_TARGET)

.SUFFIXES:
.DEFAULT_GOAL := help
.PHONY: help all overlay sysmodule patch clean __clean-all __clean-out __clean-build out build

help:
	@printf "==== Help Menu ====\n"
	@printf "make help\n"
	@printf "make all\n"
	@printf "make overlay\n"
	@printf "make sysmodule\n"
	@printf "make patch\n"
	@printf "make clean all\n"
	@printf "make clean out\n"
	@printf "make clean build\n"
	@printf "===================\n"

ifneq ($(filter clean,$(MAKECMDGOALS)),)
all:
	@:
else
all: overlay sysmodule patch
	@printf "\nThe Overlay, Sysmodule, IPC, and Patches are now ready at: %s\n" "$(STAGE_DIR_WINDOWS)"
endif

overlay: $(NX_TARGET) $(TEST_OVERLAY)
sysmodule: $(SYSMODULE_FLAG)
patch:
	@printf '\n'
	@printf "Generating Master Volume IPS patch...\n"
	@py -3 "$(PATCH_TOOL)"
	@found=0; for patch in "$(PATCH_SOURCE_DIR)"/*.ips; do if [ -f "$$patch" ]; then found=1; break; fi; done; \
	if [ "$$found" -eq 1 ]; then \
		printf "Copying IPS patches to: %s\n" "$(PATCH_DIR_WINDOWS)"; \
		mkdir -p "$(PATCH_DIR)"; \
		cp -f "$(PATCH_SOURCE_DIR)"/*.ips "$(PATCH_DIR)/"; \
	else \
		printf "It is not possible to copy the patches, as no patches have been generated in the patches folder.\n"; \
	fi

$(NX_TARGET): $(APP_ICON) $(NACP_TARGET) $(ELF_TARGET)
	@echo " OVL " $@
	@mkdir -p $(dir $@)
	@elf2nro $(ELF_TARGET) $@ $(NROFLAGS) > /dev/null
	@printf 'ULTR' >> $@
	@echo "Built" $(notdir $@)
ifeq ($(filter all,$(MAKECMDGOALS)),all)
	@printf '\n'
endif

$(TEST_OVERLAY): $(NX_TARGET)
	@mkdir -p $(dir $@)
	@cp $(NX_TARGET) $@
ifneq ($(filter all,$(MAKECMDGOALS)),all)
	@printf "\nThe overlay is ready in: %s\n" "$(STAGE_DIR_WINDOWS)"
endif

$(SYSMODULE_ELF): $(SYSMODULE_OFILES)
	@echo " LD   " $@
	@mkdir -p $(dir $@)
	@$(CXX) $(SYSMODULE_ARCH) -specs=$(DEVKITPRO)/libnx/switch.specs -g -Wl,-Map,$(BUILD)/$(SYSMODULE_TARGET).map $(SYSMODULE_OFILES) -L$(LIBNX)/lib -lnx -o $@
	@$(NM) -CSn $@ > $(BUILD)/$(SYSMODULE_TARGET).lst

$(BUILD)/sysmodule/%.cpp.o: $(SYSMODULE_DIR)/%.cpp
	@echo " CXX  " $@
	@mkdir -p $(dir $@)
	@$(CXX) -MMD -MP -MF $(@:.o=.d) $(SYSMODULE_CPPFLAGS) $(SYSMODULE_CXXFLAGS) -c $< -o $@

$(SYSMODULE_NSO): $(SYSMODULE_ELF)
	@echo " NSO  " $@
	@mkdir -p $(dir $@)
	@elf2nso $< $@

$(SYSMODULE_NPDM): $(SYSMODULE_CONFIG)
	@echo " NPDM " $@
	@mkdir -p $(dir $@)
	@npdmtool $< $@ > $@.log 2>&1; status=$$?; sed '/^Failed to get .* (field not present)\./d' $@.log; rm -f $@.log; exit $$status

$(SYSMODULE_NSP): $(SYSMODULE_NSO) $(SYSMODULE_NPDM)
	@mkdir -p $(SYSMODULE_EXEFS)
	@cp $(SYSMODULE_NSO) $(SYSMODULE_EXEFS)/main
	@cp $(SYSMODULE_NPDM) $(SYSMODULE_EXEFS)/main.npdm
	@build_pfs0 $(SYSMODULE_EXEFS) $@
	@echo "Built" $(notdir $@)

$(SYSMODULE_PACKAGE): $(SYSMODULE_NSP)
	@mkdir -p $(dir $@)
	@cp $(SYSMODULE_NSP) $@

$(SYSMODULE_FLAG): $(SYSMODULE_PACKAGE)
	@mkdir -p $(dir $@)
	@touch $@

ifneq ($(filter all,$(MAKECMDGOALS)),all)
	@printf "The sysmodule is staged for Atmosphere in: %s\n" "$(STAGE_DIR_WINDOWS)"
endif

$(ELF_TARGET): $(OVERLAY_OFILES)
	@echo " LD  " $@
	@mkdir -p $(dir $@)
	@$(LD) $(ARCH) $(LDFLAGS) -Wl,-Map,$(BUILD)/$(TARGET).map $(LIB_FLAGS) $(OVERLAY_OFILES) $(LINKS) -o $@
	@$(NM) -CSn $@ > $(BUILD)/$(TARGET).lst

$(BUILD)/%.c.o: %.c
	@echo " CC  " $@
	@mkdir -p $(dir $@)
	@$(CC) -MMD -MP $(ARCH) $(FLAGS) $(CFLAGS) $(DEFINE_FLAGS) $(INCLUDE_FLAGS) -c $(CURDIR)/$< -o $@

$(BUILD)/%.cpp.o: %.cpp
	@echo " CXX " $@
	@mkdir -p $(dir $@)
	@$(CXX) -MMD -MP $(ARCH) $(FLAGS) $(CXXFLAGS) $(DEFINE_FLAGS) $(INCLUDE_FLAGS) -c $(CURDIR)/$< -o $@

%.nacp: Makefile
	@echo " NACP" $@
	@mkdir -p $(dir $@)
	@nacptool --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" $@

ifneq ($(filter clean,$(MAKECMDGOALS)),)
ifneq ($(filter all,$(MAKECMDGOALS)),)
clean: __clean-all
else ifneq ($(filter out,$(MAKECMDGOALS)),)
clean: __clean-out
else ifneq ($(filter build,$(MAKECMDGOALS)),)
clean: __clean-build
else
clean:
	@printf "Choose a clean target: make clean all, make clean out, or make clean build.\n"
	@false
endif

out build:
	@:
endif

__clean-all: __clean-out __clean-build

__clean-out:
	@echo "Cleaning $(OUT)..."
	@rm -rf $(OUT)

__clean-build:
	@echo "Cleaning $(BUILD)..."
	@rm -rf $(BUILD)

ifeq ($(filter clean,$(MAKECMDGOALS)),)
-include $(DEPENDENCY_FILES) $(SYSMODULE_DFILES)
endif
