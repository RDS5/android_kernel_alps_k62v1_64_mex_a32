# Makefile for the TEE root scan client.
#
# Each configuration option enables a list of files.

ifneq ($(TARGET_BUILD_VARIANT),user)
	ccflags-y += -DDEBUG_LOGMASK
endif

obj-$(CONFIG_TEE_ANTIROOT_CLIENT)  += rootagent_api.o\
    rootagent.o rootagent_crypto.o rootagent_mem.o eima_api.o

#obj-$(CONFIG_HUAWEI_EIMA) += eima_api.o

obj-$(CONFIG_HUAWEI_ROOT_SCAN_DUMMY_API) += rscan_dummy.o

ccflags-y +=	-Idrivers/hisi/tzdriver \
		-Idrivers/hisi/tzdriver/libhwsecurec

ccflags-$(CONFIG_TEE_ANTIROOT_CLIENT) +=\
    -Isecurity/hw_root_scan/include

ccflags-$(CONFIG_HUAWEI_EIMA) += -Isecurity/eima/
