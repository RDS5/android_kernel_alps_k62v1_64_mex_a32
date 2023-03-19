KCOV_INSTRUMENT := n
CONFIG_TEE_SMP := y

ifneq ($(TARGET_BUILD_VARIANT),user)
ccflags-y += -DDEF_ENG
endif

ccflags-y += -DDEF_MONITOR
ifeq ($(CONFIG_TEE_SMP), y)
ccflags-y += -DCONFIG_TEE_SMP
obj-$(CONFIG_TZDRIVER) += smc_smp.o
else
obj-$(CONFIG_TZDRIVER) += smc.o
endif

obj-$(CONFIG_TZDRIVER) += tc_client_driver.o tc_client_sub_driver.o teek_client_api.o mailbox_mempool.o
obj-$(CONFIG_TZDRIVER) += security_auth_enhance.o agent.o gp_ops.o mem.o cmdmonitor.o tzdebug.o tz_spi_notify.o
obj-$(CONFIG_TEELOG) += tlogger.o tee_rdr_register.o
ifeq ($(CONFIG_TZDRIVER), y)
obj-$(CONFIG_TZDRIVER) += agent_rpmb.o
endif

#obj-$(CONFIG_TEE_TUI) += tui.o

obj-$(CONFIG_TEE_ANTIROOT_CLIENT) += ca_antiroot/

ccflags-y += 	-Idrivers/video/hisi/dss \
		-Idrivers/video/fbdev/hisi/dss \
		-Idrivers/staging/android \
		-Idrivers/tzdriver/libhwsecurec
		
ifeq ($(CONFIG_MEDIATEK_SOLUTION),y)
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ccflags-y += -Idrivers/mmc/core
ccflags-y += -Idrivers/mmc/card
ccflags-y += -Idrivers/misc/mediatek/include
ccflags-y += -Idrivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -Idrivers/misc/mediatek/include/mt-plat
ccflags-y += -Idrivers/misc/mediatek/base/power/$(MTK_PLATFORM)
ccflags-y += -Idrivers/misc/mediatek/base/power/include
ccflags-y += -Idrivers/devfreq
ccflags-y += -Idrivers/scsi/ufs

ccflags-y += -Idrivers/mmc/host/mediatek/ComboA
ifeq ($(CONFIG_MTK_PLATFORM), "mt6761")
ccflags-y += -Idrivers/mmc/host/mediatek/ComboA/mt6765
else
ccflags-y += -Idrivers/mmc/host/mediatek/ComboA/$(MTK_PLATFORM)
endif
ccflags-y += -Idrivers/mmc/host/mediatek/$(MTK_PLATFORM)
ccflags-y += -Idrivers/mmc/host/mediatek/$(MTK_PLATFORM)/$(MTK_PLATFORM)
endif


