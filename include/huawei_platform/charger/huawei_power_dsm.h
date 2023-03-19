#ifndef _POWER_DSM_H_
#define _POWER_DSM_H_

#include <dsm/dsm_pub.h>

/* define dsm buffer size */
#define POWER_DSM_BUF_SIZE_0016 (16)
#define POWER_DSM_BUF_SIZE_0128 (128)
#define POWER_DSM_BUF_SIZE_0256 (256)
#define POWER_DSM_BUF_SIZE_0512 (512)
#define POWER_DSM_BUF_SIZE_1024 (1024)
#define POWER_DSM_BUF_SIZE_2048 (2048)

typedef enum
{
	/* type begin anchor, can not modify */
	POWER_DSM_TYPE_BEGIN = 0,

	POWER_DSM_USB = POWER_DSM_TYPE_BEGIN,
	POWER_DSM_BATTERY_DETECT,
	POWER_DSM_BATTERY,
	POWER_DSM_CHARGE_MONITOR,
	POWER_DSM_SMPL,
	POWER_DSM_PD,
	POWER_DSM_USCP,
	POWER_DSM_PMU_OCP,
	POWER_DSM_PMU_IRQ,
	VIBR_DSM_PMU_IRQ,

	/* type end anchor, can not modify */
	POWER_DSM_TYPE_END,
} power_dsm_type;

struct power_dsm_data_info {
	power_dsm_type type;
	const char *name;
	struct dsm_client *client;
	struct dsm_dev *dev;
};

struct dsm_info {
	int error_no;
	bool notify_enable;
	bool notify_always;
	void (*dump)(char *buf, unsigned int buf_len);
	bool (*check_error)(void);
};

#ifdef CONFIG_HUAWEI_DSM
struct dsm_client *power_dsm_get_dclient(power_dsm_type type);

int power_dsm_dmd_report(power_dsm_type type, int err_no, void *buf);

#ifdef CONFIG_HUAWEI_DATA_ACQUISITION
int power_dsm_bigdata_report(power_dsm_type type, int err_no, void *msg);
#else
static inline int power_dsm_bigdata_report(power_dsm_type type, int err_no, void *msg)
{
	return 0;
}
#endif /* end of CONFIG_HUAWEI_DATA_ACQUISITION */

#define power_dsm_dmd_report_format(type, err_no, fmt, args...) \
do { \
	if (power_dsm_get_dclient(type)) { \
		if(!dsm_client_ocuppy(power_dsm_get_dclient(type))) { \
			dsm_client_record(power_dsm_get_dclient(type), fmt, ##args); \
			dsm_client_notify(power_dsm_get_dclient(type), err_no); \
			pr_info("[%s]: report type:%d, err_no:%d\n", __func__, type, err_no); \
		} else { \
			pr_err("[%s]: power dsm client is busy!\n", __func__); \
		} \
	} \
} while (0)

#define DSM_PMU_WLED_OVP_ERR_NO DSM_PMU_OCPLDO2_ERROR_NO
#define DSM_PMU_WLED_OCP_ERR_NO DSM_PMU_OCPLDO1_ERROR_NO

#else
static inline struct dsm_client *power_dsm_get_dclient(power_dsm_type type)
{
	return NULL;
}

static inline int power_dsm_dmd_report(power_dsm_type type, int err_no, void *buf)
{
	return 0;
}

static inline int power_dsm_bigdata_report(power_dsm_type type, int err_no, void *msg)
{
	return 0;
}

#define power_dsm_dmd_report_format(type, err_no, fmt, args...)

#endif /* end of CONFIG_HUAWEI_DSM */

#endif /* end of _POWER_DSM_H_ */

