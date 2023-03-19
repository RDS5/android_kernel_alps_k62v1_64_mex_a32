/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2016-2019. All rights reserved.
 * Description: Function definition for rdr memory register.
 * Author: qiqingchao  q00XXXXXX
 * Create: 2016-06-21
 */
#include "tee_rdr_register.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/stat.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
//#include <linux/hisi/rdr_pub.h>
#include <securec.h>
#include "teek_client_constants.h"
#include "tc_ns_client.h"
#include "teek_ns_client.h"
#include "tc_ns_log.h"
#include "smc.h"
#include "mailbox_mempool.h"

//#define TEEOS_MODID HISI_BB_MOD_TEE_START
//#define TEEOS_MODID_END  HISI_BB_MOD_TEE_END

struct rdr_register_module_result {
  u64   log_addr;
  u32   log_len;
};

struct rdr_register_module_result current_rdr_info;
//static const u64 current_core_id = RDR_TEEOS;

#if 0
void tee_fn_dump(u32 modid,
		 u32 etype,
		 u64 coreid,
		 char *pathname,
		 pfn_cb_dump_done pfn_cb)
{
	u32 l_modid = 0;

	l_modid = modid;
	pfn_cb(l_modid, current_core_id);
}

int tee_rdr_register_core(void)
{
	struct rdr_module_ops_pub s_module_ops = {0};
	int ret = -1;

	s_module_ops.ops_dump = tee_fn_dump;
	s_module_ops.ops_reset = NULL;

	ret = rdr_register_module_ops(current_core_id,
				      &s_module_ops, &current_rdr_info);
	if (ret) {
	    tloge("register rdr mem failed.\n");
	}

	return ret;
}


int teeos_register_exception(void)
{
	struct rdr_exception_info_s einfo;
	int ret = -1;
	errno_t ret_s;
	const char tee_module_name[] = "RDR_TEEOS";
	const char tee_module_desc[] = "RDR_TEEOS crash";

	ret_s = memset_s(&einfo, sizeof(struct rdr_exception_info_s),
			0, sizeof(struct rdr_exception_info_s));
	if (ret_s) {
	    tloge("memset einfo failed.\n");
	    return ret_s;
	}

	einfo.e_modid = (unsigned int)TEEOS_MODID;
	einfo.e_modid_end = (unsigned int)TEEOS_MODID_END;
	einfo.e_process_priority = RDR_ERR;
	einfo.e_reboot_priority = RDR_REBOOT_WAIT;
	einfo.e_notify_core_mask = RDR_TEEOS | RDR_AP;
	einfo.e_reset_core_mask = RDR_TEEOS | RDR_AP;
	einfo.e_reentrant = (unsigned int)RDR_REENTRANT_ALLOW;
	einfo.e_exce_type = TEE_S_EXCEPTION;
	einfo.e_from_core = RDR_TEEOS;
	einfo.e_upload_flag = (unsigned int)RDR_UPLOAD_YES;
	ret_s = memcpy_s(einfo.e_from_module, sizeof(einfo.e_from_module),
		tee_module_name, sizeof(tee_module_name));
	if (ret_s) {
		tloge("memcpy einfo.e_from_module failed.\n");
		return ret_s;
	}
	ret_s = memcpy_s(einfo.e_desc, sizeof(einfo.e_desc),
		tee_module_desc, sizeof(tee_module_desc));
	if (ret_s) {
		tloge("memcpy einfo.e_desc failed.\n");
		return ret_s;
	}
	ret_rdr = rdr_register_exception(&einfo);
	if (ret_rdr == 0) {
		tloge("register exception mem failed.");
		ret = -1;
	} else {
		ret = 0;
	}
	return ret;
}
#endif

/* Register rdr memory */
int tc_ns_register_rdr_mem(void)
{
	tc_ns_smc_cmd smc_cmd = { {0}, 0 };
	int ret = 0;
	u64 rdr_mem_addr;
	unsigned int rdr_mem_len;
	struct mb_cmd_pack *mb_pack = NULL;

#if 0
	ret = tee_rdr_register_core();
	if (ret) {
		current_rdr_info.log_addr = 0x0;
		current_rdr_info.log_len = 0;
		return ret;
	}

	rdr_mem_addr = current_rdr_info.log_addr;
	rdr_mem_len = current_rdr_info.log_len;

#endif

	current_rdr_info.log_addr  = (void *)__get_free_pages(GFP_KERNEL, RDR_MEM_ORDER_MAX);
	if (!current_rdr_info.log_addr) {
  		tloge("fail to alloc rdr mem\n");
		return -ENOMEM;
        }
	current_rdr_info.log_len = RDR_MEM_SIZE;

	rdr_mem_addr = virt_to_phys(current_rdr_info.log_addr);
	rdr_mem_len = current_rdr_info.log_len;

	mb_pack = mailbox_alloc_cmd_pack();
	if (mb_pack == NULL) {
		current_rdr_info.log_addr = 0x0;
		current_rdr_info.log_len = 0;
		return -ENOMEM;
	}

	smc_cmd.global_cmd = true;
	smc_cmd.cmd_id = GLOBAL_CMD_ID_REGISTER_RDR_MEM;
	mb_pack->operation.paramtypes = TEE_PARAM_TYPE_VALUE_INPUT |
		TEE_PARAM_TYPE_VALUE_INPUT << TEE_PARAM_NUM;
	mb_pack->operation.params[0].value.a = rdr_mem_addr;
	mb_pack->operation.params[0].value.b = rdr_mem_addr >> ADDR_TRANS_NUM;
	mb_pack->operation.params[1].value.a = rdr_mem_len;
	smc_cmd.operation_phys = virt_to_phys(&mb_pack->operation);
	smc_cmd.operation_h_phys = virt_to_phys(&mb_pack->operation) >> ADDR_TRANS_NUM;
	ret = tc_ns_smc(&smc_cmd);
	mailbox_free(mb_pack);
	if (ret)
		tloge("Send rdr mem info failed.\n");
	return ret;
}

unsigned long tc_ns_get_rdr_mem_addr(void)
{
	return current_rdr_info.log_addr;
}

unsigned int tc_ns_get_rdr_mem_len(void)
{
	return current_rdr_info.log_len;
}
