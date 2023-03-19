ifeq ($(CONFIG_HUAWEI_DEVICEBOX_AUDIO),y)
	subdir-ccflags-y += -I$(srctree)/drivers/devkit/audiokit/deviceboxID/
endif
ccflags-y += -Idrivers/devkit/audiokit/
tas2557-objs := tas2557-core.o tas2557-regmap.o tas2557-codec.o tas2557-misc.o tiload.o
obj-$(CONFIG_TAS2557) += tas2557.o
