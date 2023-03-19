/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2016-2019. All rights reserved.
 * Description: Function declaration for smc cmd send.
 * Author: qiqingchao  q00XXXXXX
 * Create: 2016-06-21
 */
#ifndef _SMC_H_
#define _SMC_H_

#include <linux/of_device.h>
#include "teek_client_constants.h"
#include "teek_ns_client.h"

enum TC_NS_CMD_type {
	TC_NS_CMD_TYPE_INVALID = 0,
	TC_NS_CMD_TYPE_NS_TO_SECURE,
	TC_NS_CMD_TYPE_SECURE_TO_NS,
	TC_NS_CMD_TYPE_SECURE_TO_SECURE,
	TC_NS_CMD_TYPE_SECURE_CONFIG = 0xf,
	TC_NS_CMD_TYPE_MAX
};

#ifdef CONFIG_TEE_SMP
struct pending_entry {
	atomic_t users;
	pid_t pid;
	wait_queue_head_t wq;
	atomic_t run;
	struct list_head list;
};
#endif

extern struct session_crypto_info *g_session_root_key;


int smc_init_data(struct device *class_dev);
void smc_free_data(void);
int tc_ns_smc(tc_ns_smc_cmd *cmd);
int tc_ns_smc_with_no_nr(tc_ns_smc_cmd *cmd);
int teeos_log_exception_archive(unsigned int eventid, const char *exceptioninfo);
void set_cmd_send_state(void);
int init_smc_svc_thread(void);
int smc_wakeup_ca(pid_t ca);
int smc_wakeup_broadcast(void);
int smc_shadow_exit(pid_t ca);
int smc_queue_shadow_worker(uint64_t target);
void fiq_shadow_work_func(uint64_t target);
struct pending_entry *find_pending_entry(pid_t pid);
void foreach_pending_entry(void (*func)(struct pending_entry *));
void put_pending_entry(struct pending_entry *pe);
void show_cmd_bitmap(void);
static inline int switch_low_temperature_mode(unsigned int mode)
{
	return -EINVAL;
}
void wakeup_tc_siq(void);

#endif
