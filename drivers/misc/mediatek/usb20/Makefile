#
# Copyright (C) 2017 MediaTek Inc.
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

#
# for USB OTG silicon based on Mentor Graphics INVENTRA designs
#

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat

#Platform
obj-$(CONFIG_MACH_MT6768) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT8167) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT6739) += $(subst ",,$(CONFIG_MTK_PLATFORM))/
obj-$(CONFIG_MACH_MT6765) += $(subst ",,$(CONFIG_MTK_PLATFORM))/

obj-$(CONFIG_USB_MTK_HDRC) += musb_hdrc.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC) := musb_core.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC) += musb_gadget_ep0.o musb_gadget.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC) += musb_virthub.o musb_host.o
musb_hdrc-$(CONFIG_USB_MTK_HDRC) += musbhsdma.o
musb_hdrc-$(CONFIG_DEBUG_FS) += musb_debugfs.o
# QMU Realted
obj-$(CONFIG_MTK_MUSB_QMU_SUPPORT) += mtk_qmu.o musb_qmu.o
ifeq ($(CONFIG_MTK_MUSB_QMU_SUPPORT),y)
subdir-ccflags-$(CONFIG_MACH_MT8167) += -DMUSB_QMU_LIMIT_SUPPORT -DMUSB_QMU_LIMIT_RXQ_NUM=4 -DMUSB_QMU_LIMIT_TXQ_NUM=4
endif

obj-$(CONFIG_DUAL_ROLE_USB_INTF) += mtk_dual_role.o
obj-$(CONFIG_Y_CABLE) += hw_ycable.o
