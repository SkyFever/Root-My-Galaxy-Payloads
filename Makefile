API ?= 35
TARGET ?= pa3q-S938NKSUACZF1
OUTDIR ?= build/$(TARGET)

TARGET_HEADER := src/targets/$(TARGET)/target.h
TARGET_INCLUDE := targets/$(TARGET)/target.h
TARGET_CC := $(ANDROID_NDK_HOME)/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android$(API)-clang

ifeq ($(wildcard $(TARGET_CC)),)
$(error set ANDROID_NDK_HOME to an Android NDK containing $(TARGET_CC))
endif

PRELOAD := $(OUTDIR)/cve-2026-43499
APP_PRELOAD := $(OUTDIR)/cve-2026-43499-app.so
APP_RELEASE := $(OUTDIR)/cve-2026-43499-app.release.so
APP_RELEASE_SIZE := 104128
ROOT_HELPER := $(OUTDIR)/cve-2026-43499-root

PRELOAD_SRCS := \
  src/main.c \
  src/util.c \
  src/slide.c \
  src/fops.c \
  src/pipe.c \
  src/root.c \
  src/preload.c

APP_PRELOAD_SRCS := \
  src/main.c \
  src/util.c \
  src/slide_app.c \
  src/fops.c \
  src/pipe.c \
  src/root.c \
  src/preload.c

T870_FASTRPC_SRCS := \
  src/t870_fastrpc/t870_payload_bridge.c \
  src/t870_fastrpc/t870_dsp_asset.c \
  src/t870_fastrpc/t870_dsp_asset.S \
  src/t870_fastrpc/t870_fastrpc_bridge_adapter.c \
  src/t870_fastrpc/t870_fastrpc_session.c \
  src/t870_fastrpc/t870_bridge.c \
  src/t870_fastrpc/t870_detached_owner.c \
  src/t870_fastrpc/t870_cmsg_wave.c \
  src/t870_fastrpc/t870_pipe_wave.c
T870_FASTRPC_DEPS := \
  $(wildcard src/t870_fastrpc/*.h) \
  src/t870_fastrpc/libt870_map_collision_skel.so

ifeq ($(TARGET),gts7lwifi-T870XXS8DXH1)
APP_RELEASE_SIZE := 112048
APP_PRELOAD_SRCS := $(filter-out src/slide_app.c,$(APP_PRELOAD_SRCS))
APP_PRELOAD_SRCS += src/slide.c
APP_PRELOAD_SRCS += $(T870_FASTRPC_SRCS)
TARGET_APP_DEPS += $(T870_FASTRPC_DEPS)
TARGET_APP_CFLAGS += -DT870_FASTRPC_BRIDGE=1
TARGET_APP_LIBS += -ldl
endif

COMMON_CFLAGS := \
  -O2 -g0 -Wall -Wextra \
  -Wno-unused-parameter -Wno-sign-compare \
  -Isrc -DTARGET_HEADER='"$(TARGET_INCLUDE)"'

.DEFAULT_GOAL := all

.PHONY: all clean info release

all: $(PRELOAD) $(APP_PRELOAD) $(ROOT_HELPER)

release: $(APP_RELEASE)

$(OUTDIR):
	mkdir -p $@

$(PRELOAD): $(PRELOAD_SRCS) $(TARGET_HEADER) src/offset.h src/common.h src/kernelsnitch/*.h | $(OUTDIR)
	$(TARGET_CC) -fPIC $(COMMON_CFLAGS) $(PRELOAD_SRCS) \
	  -shared -pthread -o $@

$(ROOT_HELPER): src/su_daemon.c | $(OUTDIR)
	$(TARGET_CC) -fPIE -pie -O2 -g0 -Wall -Wextra $< -ldl -o $@

$(APP_PRELOAD): $(APP_PRELOAD_SRCS) $(TARGET_APP_DEPS) $(TARGET_HEADER) src/offset.h src/common.h src/kernelsnitch/*.h | $(OUTDIR)
	$(TARGET_CC) -DAPP_PAYLOAD=1 $(TARGET_APP_CFLAGS) -fPIC \
	  $(COMMON_CFLAGS) $(APP_PRELOAD_SRCS) -shared -pthread \
	  $(TARGET_APP_LIBS) -o $@

$(APP_RELEASE): $(APP_PRELOAD_SRCS) $(TARGET_APP_DEPS) $(TARGET_HEADER) src/offset.h src/common.h src/kernelsnitch/*.h | $(OUTDIR)
	$(TARGET_CC) -DAPP_PAYLOAD=1 $(TARGET_APP_CFLAGS) -fPIC -Oz -g0 \
	  -fno-unwind-tables -fno-asynchronous-unwind-tables \
	  -ffunction-sections -fdata-sections \
	  -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare \
	  -Isrc -DTARGET_HEADER='"$(TARGET_INCLUDE)"' \
	  $(APP_PRELOAD_SRCS) -shared -pthread $(TARGET_APP_LIBS) \
	  -Wl,--gc-sections -Wl,--icf=all -s -o $@
	@test $$(stat -c %s $@) -le $(APP_RELEASE_SIZE)
	truncate -s $(APP_RELEASE_SIZE) $@

info:
	@echo "TARGET=$(TARGET)"
	@echo "TARGET_CC=$(TARGET_CC)"
	@echo "PRELOAD=$(PRELOAD)"
	@echo "APP_PRELOAD=$(APP_PRELOAD)"
	@echo "APP_RELEASE=$(APP_RELEASE)"
	@echo "ROOT_HELPER=$(ROOT_HELPER)"

clean:
	rm -rf $(OUTDIR)
