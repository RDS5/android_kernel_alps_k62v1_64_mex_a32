#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

obj-$(CONFIG_MTK_LEDS) := mtk_leds.o

ccflags-y += -I$(srctree)/drivers/misc/mediatek/video/include

ifeq ($(CONFIG_LCD_KIT_DRIVER),y)
ccflags-y += -I$(srctree)/drivers/devkit/lcdkit/lcdkit3.0/kernel/common/include
ccflags-y += -I$(srctree)/drivers/devkit/lcdkit/lcdkit3.0/kernel/mtk/include
ccflags-y += -I$(srctree)/drivers/devkit/lcdkit/lcdkit3.0/kernel/mtk/adapt
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lcm/inc
endif
