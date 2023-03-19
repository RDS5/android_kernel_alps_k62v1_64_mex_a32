ccflags-y += -I$(srctree)/drivers/misc/mediatek/connectivity/fm/inc
ifneq ($(TARGET_BUILD_VARIANT),user)
    EXTRA_CFLAGS += -DCONFIG_HUAWEI_HEADSET_DEBUG
    EXTRA_CFLAGS += -Idrivers/devkit/audiokit/headset_debug/
endif
ccflags-y += -Idrivers/huawei_platform/audio/hs_auto_calib/

obj-$(CONFIG_MTK_ACCDET) := accdet.o

