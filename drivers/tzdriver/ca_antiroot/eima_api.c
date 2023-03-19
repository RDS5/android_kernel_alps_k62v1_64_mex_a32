#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include "eima_api.h"

#define PM_SUSPEND  0x1 /*1 for suspend*/

int send_eima_data(int type, struct m_list_msg *mlist_msg) {
	TEEC_Result ret;
	struct EIMA_AGENT_CHALLENGE challenge;
	struct wakeup_source eima_wake_src;
	mutex_lock(&eima_ca_lock);
	if ((type < 0) || (type > 2) || !mlist_msg){
		antiroot_error("bad param!type = %d!", type);
		mutex_unlock(&eima_ca_lock);
		return -1; 
	}

	wakeup_source_init(&eima_wake_src, "eimaca");
	__pm_wakeup_event(&eima_wake_src, AR_WAKELOCK_TIMEOUT);
	while (PM_SUSPEND == antiroot_sr_flag) {
		antiroot_warning("system is suspend now, try later!");
		msleep(50);
	}

	ret = root_monitor_tee_init();
	if (TEEC_SUCCESS != ret) {
		antiroot_error("root_monitor_tee_init failed 0x%x!", ret);
		mutex_unlock(&eima_ca_lock);
		wakeup_source_trash(&eima_wake_src);
		return ret;
	}

	ret = eima_request_challenge(&challenge);
	if (TEEC_SUCCESS != ret) {
		antiroot_error("eima_request_challenge failed.ret = 0x%x!", ret);
		mutex_unlock(&eima_ca_lock);
		wakeup_source_trash(&eima_wake_src);
		return ret;
	}
	ret = eima_send_response(type, mlist_msg);
	if (TEEC_SUCCESS != ret) {
		antiroot_error("eima_response failed.ret = 0x%x!", ret);
		mutex_unlock(&eima_ca_lock);
		wakeup_source_trash(&eima_wake_src);
		return ret;
	}
	wakeup_source_trash(&eima_wake_src);
	mutex_unlock(&eima_ca_lock);
	return 0;
}
EXPORT_SYMBOL(send_eima_data);
