#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include "huawei_thp.h"

#include <linux/time.h>
#include <linux/syscalls.h>

#define GOODIX_IC_NAME "goodix"
#define THP_GOODIX_DEV_NODE_NAME "goodix_thp"

#define MODULE_INFO_READ_RETRY       5
#define FW_INFO_READ_RETRY           3
#define SEND_COMMAND_RETRY           3
#define CHECK_COMMAND_RETRY 3

#define GOODIX_FRAME_ADDR_DEFAULT      0x8C05
#define GOODIX_CMD_ADDR_DEFAULT        0x58BF
#define GOODIX_FW_STATE_ADDR_DEFAULT   0xBFDE
#define GOODIX_FW_STATE_LEN_DEFAULT    10
#define GOODIX_MODULE_INFO_ADDR        0x452C
#define GOODIX_PROJECT_ID_ADDR         0xBDB4
#define GOODIX_AFE_INFO_ADDR           0x6D20
#define GOODIX_MAX_AFE_LEN             300
#define GOODIX_FRAME_LEN_OFFSET        20
#define GOODIX_FRAME_ADDR_OFFSET       22
#define GOODIX_FW_STATUS_LEN_OFFSET    91
#define GOODIX_FW_STATUS_ADDR_OFFSET   93
#define GOODIX_CMD_ADDR_OFFSET         102
/* GT9896 default addr */
#define GOODIX_FRAME_ADDR_DEFAULT_GT9896 0x4280
#define GOODIX_CMD_ADDR_DEFAULT_GT9896 0x4168
#define GOODIX_MODULE_INFO_ADDR_GT9896 0x4018
#define GOODIX_PROJECT_ID_ADDR_GT9896 0x4114
#define GOODIX_AFE_INFO_ADDR_GT9896 0x4014
#define GOODIX_MAX_AFE_LEN_GT9896 300
#define GOODIX_FRAME_LEN_OFFSET_GT9896 77
#define GOODIX_FRAME_ADDR_OFFSET_GT9896 79
#define GOODIX_CMD_ADDR_OFFSET_GT9896 67
#define GOODIX_READ_WRITE_BYTE_OFFSET_GT9896 5
#define GOODIX_MASK_ID_FOR_GT9896 "YELSTO"
#define SPI_ARGS_WRITE_RETRY 5
#define GOODIX_SPI_TRANSFER_ARGS_ADDR 0x3082
#define SPI_TRANSFER_ARGS 0x0F
#define GOODIX_FRAME_LEN_MAX_GT9896 2048

#define CMD_ACK_BUF_OVERFLOW    0x01
#define CMD_ACK_CHECKSUM_ERROR  0x02
#define CMD_ACK_BUSY            0x04
#define CMD_ACK_OK              0x80
#define CMD_ACK_IDLE            0xFF
#define CMD_ACK_UNKNOWN 0x00
#define CMD_ACK_ERROR (-1)

#define CMD_FRAME_DATA          0x90
#define CMD_HOVER               0x91
#define CMD_FORCE               0x92
#define CMD_SLEEP               0x96
#define CMD_SCREEN_ON_OFF       0x96

#define CMD_PT_MODE             0x05
#define CMD_GESTURE_MODE        0xB5

#define IC_STATUS_GESTURE       0x01
#define IC_STATUS_POWEROF       0x02
#define IC_STATUS_UDFP          0x04
#define IC_STATUS_PT_TEST       0x08

#define PT_MODE                 0

#define FEATURE_ENABLE          1
#define FEATURE_DISABLE         0

#define SPI_FLAG_WR             0xF0
#define SPI_FLAG_RD             0xF1
#define MASK_8BIT               0xFF
#define MASK_1BIT               0x01

#define GOODIX_MASK_ID           "NOR_G1"
#define GOODIX_MASK_ID_LEN       6
#define MOVE_8BIT                8
#define MOVE_16BIT               16
#define MOVE_24BIT               24
#define MAX_FW_STATUS_DATA_LEN   64

#define SEND_COMMAND_WAIT_DELAY  10
#define SEND_COMMAND_END_DELAY    2
#define SEND_COMMAND_SPI_READ_LEN 1
#define COMMAND_BUF_LEN           5
#define COMMUNI_CHECK_RETRY_DELAY 10
#define DELAY_1MS                 1
#define NO_DELAY                  0
#define GET_AFE_INFO_RETRY_DELAY 10
#define DEBUG_AFE_DATA_BUF_LEN    20
#define DEBUG_AFE_DATA_BUF_OFFSET DEBUG_AFE_DATA_BUF_LEN

#define INFO_ADDR_BUF_LEN          2
#define IRQ_EVENT_TYPE_FRAME       0x80
#define IRQ_EVENT_TYPE_GESTURE     0x81

#define GESTURE_EVENT_HEAD_LEN               6
#define GESTURE_TYPE_DOUBLE_TAP              0x01
#define GOODIX_CUSTOME_INFO_LEN              30
#define GOODIX_GET_PROJECT_ID_RETRY          3
#define GOODIX_GET_PROJECT_RETRY_DELAY_10MS  10
#define IDLE_WAKE_FLAG                       0xF0
#define IDLE_SPI_SPEED                       3500
#define ACTIVE_SPI_SPEED                     7500000
#define READ_WRITE_BYTE_OFFSET               1
#define READ_CMD_BUF_LEN                     3
#define CMD_TOUCH_REPORT                     0xAC
#define WAIT_FOR_SPI_BUS_RESUMED_DELAY       20
#define WAIT_FOR_SPI_BUS_READ_DELAY          5
#define GOODIX_SPI_READ_XFER_LEN             2
#define SPI_WAKEUP_RETRY                     8
#define GOODIX_LCD_EFFECT_FLAG_POSITION 12
#define DEBUG_BUF_LEN 160
#define GET_AFE_BUF_LEN 2

#pragma pack(1)
struct goodix_module_info {
	u8 mask_id[6];
	u8 mask_vid[3];
	u8 pid[8];
	u8 vid[4];
	u8 sensor_id;
	u8 reserved[49];
	u8 checksum;
};
#pragma pack()

#pragma pack(1)
struct goodix_module_info_for_gt9896 {
	u8 mask_id[6];
	u8 mask_vid[4];
	u8 pid[8];
	u8 cid;
	u8 vid[4];
	u8 sensor_id;
	u8 reserved[2];
	u16 checksum;
};
#pragma pack()
enum goodix_ic_type {
	GT9886,
	GT9896 = 1,
};
struct goodix_module_info module_info;
struct goodix_module_info_for_gt9896 module_info_for_gt9896;
static int goodix_frame_addr = GOODIX_FRAME_ADDR_DEFAULT;
static int goodix_frame_len;
static int goodix_cmd_addr = GOODIX_CMD_ADDR_DEFAULT;
static int goodix_fw_status_addr = GOODIX_FW_STATE_ADDR_DEFAULT;
static int goodix_fw_status_len = GOODIX_FW_STATE_LEN_DEFAULT;
static unsigned int g_thp_udfp_status;
static unsigned int goodix_stylus_status;
static unsigned int work_status;
static u8 *goodix_spi_tx_buf;

static int touch_driver_gesture_event(struct thp_device *tdev,
	unsigned int *gesture_wakeup_value);
static int touch_driver_switch_cmd(struct thp_device *tdev, u8 cmd, u8 status);
static int touch_driver_idle_wakeup(struct thp_device *tdev);
static int touch_driver_spi_read(struct thp_device *tdev, unsigned int addr,
	u8 *data, unsigned int len)
{
	struct spi_device *spi = NULL;
	u8 *rx_buf = NULL;
	u8 *tx_buf = NULL;
	u8 *start_cmd_buf = goodix_spi_tx_buf;
	struct spi_transfer xfers[GOODIX_SPI_READ_XFER_LEN];
	struct spi_message spi_msg;
	int ret;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (data == NULL) || (tdev->tx_buff == NULL) ||
		(tdev->rx_buff == NULL) || (tdev->thp_core == NULL) ||
		(tdev->thp_core->sdev == NULL)) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	if (len + READ_WRITE_BYTE_OFFSET > THP_MAX_FRAME_SIZE) {
		THP_LOG_ERR("%s:invalid len:%d\n", __func__, len);
		return -EINVAL;
	}
	if (start_cmd_buf == NULL) {
		THP_LOG_ERR("%s:start_cmd_buf null\n", __func__);
		return -EINVAL;
	}
	spi = tdev->thp_core->sdev;
	rx_buf = tdev->rx_buff;
	tx_buf = tdev->tx_buff;
	cd = tdev->thp_core;
	spi_message_init(&spi_msg);
	memset(xfers, 0, sizeof(xfers));
	if (cd->support_vendor_ic_type == GT9896) {
		tx_buf[0] = SPI_FLAG_RD; /* 0xF1 start read flag */
		tx_buf[1] = (addr >> MOVE_8BIT) & MASK_8BIT;
		tx_buf[2] = addr & MASK_8BIT;
		tx_buf[3] = MASK_8BIT;
		tx_buf[4] = MASK_8BIT;

		xfers[0].tx_buf = tx_buf;
		xfers[0].rx_buf = rx_buf;
		xfers[0].len = len + GOODIX_READ_WRITE_BYTE_OFFSET_GT9896;
		xfers[0].cs_change = 1;
		spi_message_add_tail(&xfers[0], &spi_msg);
	} else {
		start_cmd_buf[0] = SPI_FLAG_WR; /* 0xF0 start write flag */
		start_cmd_buf[1] = (addr >> MOVE_8BIT) & MASK_8BIT;
		start_cmd_buf[2] = addr & MASK_8BIT;

		xfers[0].tx_buf = start_cmd_buf;
		xfers[0].len = READ_CMD_BUF_LEN;
		xfers[0].cs_change = 1;
		spi_message_add_tail(&xfers[0], &spi_msg);

		tx_buf[0] = SPI_FLAG_RD; /* 0xF1 start read flag */
		xfers[1].tx_buf = tx_buf;
		xfers[1].rx_buf = rx_buf;
		xfers[1].len = len + READ_WRITE_BYTE_OFFSET;
		xfers[1].cs_change = 1;
		spi_message_add_tail(&xfers[1], &spi_msg);
	}
	ret = thp_bus_lock();
	if (ret < 0) {
		THP_LOG_ERR("%s:get lock failed:%d\n", __func__, ret);
		return ret;
	}
	ret = thp_spi_sync(spi, &spi_msg);
	thp_bus_unlock();
	if (ret < 0) {
		THP_LOG_ERR("Spi transfer error:%d\n", ret);
		return ret;
	}
	if (cd->support_vendor_ic_type == GT9896)
		memcpy(data, &rx_buf[GOODIX_READ_WRITE_BYTE_OFFSET_GT9896], len);
	else
		memcpy(data, &rx_buf[READ_WRITE_BYTE_OFFSET], len);

	return ret;
}

static int touch_driver_spi_write(struct thp_device *tdev,
	unsigned int addr, u8 *data, unsigned int len)
{
	struct spi_device *spi = NULL;
	u8 *tx_buf = NULL;
	struct spi_transfer xfers;
	struct spi_message spi_msg;
	int ret;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (data == NULL) || (tdev->tx_buff == NULL) ||
		(tdev->rx_buff == NULL) || (tdev->thp_core == NULL) ||
		(tdev->thp_core->sdev == NULL)) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	spi = tdev->thp_core->sdev;
	tx_buf = tdev->tx_buff;
	cd = tdev->thp_core;
	spi_message_init(&spi_msg);
	memset(&xfers, 0, sizeof(xfers));

	if (addr || (cd->support_vendor_ic_type == GT9896)) {
		tx_buf[0] = SPI_FLAG_WR; /* 0xF1 start read flag */
		tx_buf[1] = (addr >> MOVE_8BIT) & MASK_8BIT;
		tx_buf[2] = addr & MASK_8BIT;
		memcpy(&tx_buf[3], data, len);
		xfers.len = len + 3;
	} else {
		memcpy(&tx_buf[0], data, len);
		xfers.len = len;
	}

	xfers.tx_buf = tx_buf;
	xfers.cs_change = 1;
	spi_message_add_tail(&xfers, &spi_msg);
	ret = thp_bus_lock();
	if (ret < 0) {
		THP_LOG_ERR("%s:get lock failed:%d\n", __func__, ret);
		return ret;
	}
	ret = thp_spi_sync(spi, &spi_msg);
	thp_bus_unlock();
	if (ret < 0)
		THP_LOG_ERR("Spi transfer error:%d, addr 0x%x\n", ret, addr);

	return ret;
}

static int touch_driver_get_cmd_ack(struct thp_device *tdev)
{
	int ret;
	int i;
	u8 cmd_ack = 0;

	for (i = 0; i < CHECK_COMMAND_RETRY; i++) {
		/* check command result */
		ret = touch_driver_spi_read(tdev, goodix_cmd_addr +
			GOODIX_READ_WRITE_BYTE_OFFSET_GT9896,
			&cmd_ack, 1);
		if (ret < 0) {
			THP_LOG_ERR("%s: failed read cmd ack info, ret %d\n",
				__func__, ret);
			return -EINVAL;
		}
		if (cmd_ack != CMD_ACK_OK) {
			ret = CMD_ACK_ERROR;
			if (cmd_ack == CMD_ACK_BUF_OVERFLOW) {
				mdelay(10); /* delay 10 ms */
				break;
			} else if ((cmd_ack == CMD_ACK_BUSY) ||
				(cmd_ack == CMD_ACK_UNKNOWN)) {
				mdelay(1); /* delay 1 ms */
				continue;
			}
			mdelay(1); /* delay 1 ms */
			break;
		}
		ret = 0;
		mdelay(SEND_COMMAND_END_DELAY);
		goto exit;
	}
exit:
	return ret;
}

#define GOODIX_SWITCH_CMD_BUF_LEN 6
static int touch_driver_switch_cmd_for_gt9896(struct thp_device *tdev,
	u8 cmd, u16 data)
{
	int ret;
	int i;
	u8 cmd_buf[GOODIX_SWITCH_CMD_BUF_LEN] = {0};
	u16 checksum = 0;

	checksum += cmd;
	checksum += data >> MOVE_8BIT;
	checksum += data & 0xFF;
	cmd_buf[0] = cmd;
	cmd_buf[1] = (u8)(data >> MOVE_8BIT);
	cmd_buf[2] = (u8)(data & 0xFF);
	cmd_buf[3] = (u8)(checksum >> MOVE_8BIT);
	cmd_buf[4] = (u8)(checksum & 0xFF);
	cmd_buf[5] = 0; /* cmd_buf[5] is used to clear cmd ack data */

	for (i = 0; i < SEND_COMMAND_RETRY; i++) {
		ret = touch_driver_spi_write(tdev, goodix_cmd_addr, cmd_buf,
			sizeof(cmd_buf));
		if (ret < 0) {
			THP_LOG_ERR("%s: failed send command, ret %d\n",
				__func__, ret);
			return -EINVAL;
		}
		ret = touch_driver_get_cmd_ack(tdev);
		if (ret == CMD_ACK_ERROR) {
			THP_LOG_ERR("%s: cmd ack info is error\n", __func__);
			continue;
		} else {
			break;
		}
	}
	return ret;
}

#define CMD_ACK_OFFSET 4
static int touch_driver_switch_cmd(struct thp_device *tdev,
	u8 cmd, u8 status)
{
	int ret = 0;
	int retry = 0;
	u8 cmd_buf[COMMAND_BUF_LEN];
	u8 cmd_ack = 0;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (tdev->thp_core == NULL)) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	if (cd->support_vendor_ic_type == GT9896)
		return touch_driver_switch_cmd_for_gt9896(tdev, cmd, status);

	if (touch_driver_idle_wakeup(tdev)) {
		THP_LOG_ERR("failed wakeup idle before send command\n");
		return -EINVAL;
	}
	cmd_buf[0] = cmd;
	cmd_buf[1] = status;
	cmd_buf[2] = 0 - cmd_buf[1] - cmd_buf[0]; /* checksum */
	cmd_buf[3] = 0;
	cmd_buf[4] = 0; /* use to clear cmd ack flag */
	while (retry++ < SEND_COMMAND_RETRY) {
		ret = touch_driver_spi_write(tdev, goodix_cmd_addr,
					cmd_buf, sizeof(cmd_buf));
		if (ret < 0) {
			THP_LOG_ERR("%s: failed send command, ret %d\n",
				__func__, ret);
			return -EINVAL;
		}

		ret = touch_driver_spi_read(tdev,
					goodix_cmd_addr + CMD_ACK_OFFSET,
					&cmd_ack, SEND_COMMAND_SPI_READ_LEN);
		if (ret < 0) {
			THP_LOG_ERR("%s: failed read command ack info, ret %d\n",
				__func__, ret);
			return -EINVAL;
		}

		if (cmd_ack != CMD_ACK_OK) {
			ret = -EINVAL;
			THP_LOG_DEBUG("%s: command state ack info 0x%x\n",
					__func__, cmd_ack);
			if (cmd_ack == CMD_ACK_BUF_OVERFLOW ||
				cmd_ack == CMD_ACK_BUSY)
				mdelay(SEND_COMMAND_WAIT_DELAY);
		} else {
			ret = 0;
			mdelay(SEND_COMMAND_END_DELAY);
			break;
		}
	}
	return ret;
}

static int thp_parse_feature_ic_config(struct device_node *thp_node,
	struct thp_core_data *cd)
{
	int rc;

	rc = of_property_read_u32(thp_node, "cmd_gesture_mode",
		&cd->cmd_gesture_mode);
	if (rc)
		cd->cmd_gesture_mode = CMD_GESTURE_MODE;
	THP_LOG_INFO("%s:cd->cmd_gesture_mode = 0x%X, rc = %d\n",
			__func__, cd->cmd_gesture_mode, rc);
	return rc;
}

static int touch_driver_init(struct thp_device *tdev)
{
	int rc;
	struct thp_core_data *cd = NULL;
	struct device_node *goodix_node = NULL;

	if (tdev == NULL) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	goodix_node = of_get_child_by_name(cd->thp_node,
					THP_GOODIX_DEV_NODE_NAME);

	THP_LOG_INFO("%s: called\n", __func__);

	if (!goodix_node) {
		THP_LOG_INFO("%s: dev not config in dts\n", __func__);
		return -ENODEV;
	}

	rc = thp_parse_spi_config(goodix_node, cd);
	if (rc)
		THP_LOG_ERR("%s: spi config parse fail\n", __func__);

	rc = thp_parse_timing_config(goodix_node, &tdev->timing_config);
	if (rc)
		THP_LOG_ERR("%s: timing config parse fail\n", __func__);

	rc = thp_parse_feature_config(goodix_node, cd);
	if (rc)
		THP_LOG_ERR("%s: feature_config fail\n", __func__);

	rc = thp_parse_feature_ic_config(goodix_node, cd);
	if (rc)
		THP_LOG_ERR("%s: feature_ic_config fail, use default\n",
			__func__);

	if (cd->support_gesture_mode) {
		cd->easy_wakeup_info.sleep_mode = TS_POWER_OFF_MODE;
		cd->easy_wakeup_info.easy_wakeup_gesture = false;
		cd->easy_wakeup_info.off_motion_on = false;
	}
	rc = thp_parse_trigger_config(goodix_node, cd);
	if (rc)
		THP_LOG_ERR("%s: trigger_config fail\n", __func__);
	return 0;
}

static u8 checksum_u8(u8 *data, u32 size)
{
	u8 checksum = 0;
	u32 i;
	int zero_count = 0;

	for (i = 0; i < size; i++) {
		checksum += data[i];
		if (!data[i])
			zero_count++;
	}
	return zero_count == size ? MASK_8BIT : checksum;
}

static u16 checksum16_cmp(u8 *data, u32 size)
{
	u16 cal_checksum = 0;
	u16 checksum;
	u32 i;

	if (size < sizeof(u16)) {
		THP_LOG_ERR("%s:inval parm\n", __func__);
		return 1;
	}
	for (i = 0; i < size - sizeof(u16); i++)
		cal_checksum += data[i];
	checksum = (data[size - sizeof(u16)] << MOVE_8BIT) + data[size - 1];
	return cal_checksum == checksum ? 0 : 1;
}

static int touch_driver_communication_check_for_gt9896(
	struct thp_device *tdev)
{
	int ret;
	int len;
	int retry;
	u8 temp_buf[GOODIX_MASK_ID_LEN + 1] = {0};
	u8 wr_val;
	u8 rd_val;

	len = sizeof(module_info_for_gt9896);
	memset(&module_info_for_gt9896, 0, len);

	wr_val = SPI_TRANSFER_ARGS;
	for (retry = 0; retry < SPI_ARGS_WRITE_RETRY; retry++) {
		ret = touch_driver_spi_write(tdev,
			GOODIX_SPI_TRANSFER_ARGS_ADDR, &wr_val, 1);
		if (ret < 0) {
			THP_LOG_ERR("%s:  spi write failed, ret %d\n",
				__func__, ret);
			return -EINVAL;
		}
		ret = touch_driver_spi_read(tdev, GOODIX_SPI_TRANSFER_ARGS_ADDR,
			&rd_val, 1);
		if (ret < 0) {
			THP_LOG_ERR("%s: spi read fail, ret %d\n",
				__func__, ret);
			return -EINVAL;
		}
		if (wr_val == rd_val)
			break;
		THP_LOG_INFO("%s:wr:0x%X, rd:0x%X", __func__, wr_val, rd_val);
	}
	for (retry = 0; retry < MODULE_INFO_READ_RETRY; retry++) {
		ret = touch_driver_spi_read(tdev,
			GOODIX_MODULE_INFO_ADDR_GT9896,
			(u8 *)&module_info_for_gt9896, len);
		/* print 32*3 data (rowsize=32,groupsize=3),0:no ascii */
		print_hex_dump(KERN_INFO, "[I/THP] module info: ",
			DUMP_PREFIX_NONE, 32, 3,
			(u8 *)&module_info_for_gt9896, len, 0);
		if (!ret)
			break;
		/* retry need delay 10ms */
		msleep(10);
	}
	THP_LOG_INFO("hw info: ret %d, retry %d\n", ret, retry);
	if (retry == MODULE_INFO_READ_RETRY) {
		THP_LOG_ERR("%s: failed read module info\n", __func__);
		return -EINVAL;
	}
	if (memcmp(module_info_for_gt9896.mask_id, GOODIX_MASK_ID_FOR_GT9896,
		GOODIX_MASK_ID_LEN)) {
		memcpy(temp_buf, module_info_for_gt9896.mask_id,
			GOODIX_MASK_ID_LEN);
		THP_LOG_ERR("%s: invalied mask id %s != %s\n", __func__,
			temp_buf, GOODIX_MASK_ID_FOR_GT9896);
		return -EINVAL;
	}
	THP_LOG_INFO("%s: communication check passed\n", __func__);
	memcpy(temp_buf, module_info_for_gt9896.mask_id, GOODIX_MASK_ID_LEN);
	temp_buf[GOODIX_MASK_ID_LEN] = '\0';
	THP_LOG_INFO("MASK_ID %s : ver %*ph\n", temp_buf,
		(u32)sizeof(module_info_for_gt9896.mask_vid),
		module_info_for_gt9896.mask_vid);
	THP_LOG_INFO("PID %s : ver %*ph\n", module_info_for_gt9896.pid,
		(u32)sizeof(module_info_for_gt9896.vid),
		module_info_for_gt9896.vid);
	return 0;
}

static int touch_driver_communication_check(struct thp_device *tdev)
{
	int ret = 0;
	int len;
	int retry;
	u8 temp_buf[GOODIX_MASK_ID_LEN + 1] = {0};
	u8 checksum = 0;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (tdev->thp_core == NULL)) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	if (cd->support_vendor_ic_type == GT9896)
		return touch_driver_communication_check_for_gt9896(tdev);

	len = sizeof(module_info);
	memset(&module_info, 0, len);

	for (retry = 0; retry < MODULE_INFO_READ_RETRY; retry++) {
		ret = touch_driver_spi_read(tdev, GOODIX_MODULE_INFO_ADDR,
				(u8 *)&module_info, len);
		print_hex_dump(KERN_INFO, "[I/THP] module info: ",
				DUMP_PREFIX_NONE,
				32, 3, (u8 *)&module_info, len, 0);
		checksum = checksum_u8((u8 *)&module_info, len);
		if (!ret && !checksum)
			break;

		mdelay(COMMUNI_CHECK_RETRY_DELAY);
	}

	THP_LOG_INFO("hw info: ret %d, checksum 0x%x, retry %d\n", ret,
		checksum, retry);
	if (retry == MODULE_INFO_READ_RETRY) {
		THP_LOG_ERR("%s:failed read module info\n", __func__);
		return -EINVAL;
	}

	if (memcmp(module_info.mask_id, GOODIX_MASK_ID,
			sizeof(GOODIX_MASK_ID) - 1)) {
		memcpy(temp_buf, module_info.mask_id, GOODIX_MASK_ID_LEN);
		THP_LOG_ERR("%s: invalied mask id %s != %s\n",
			__func__, temp_buf, GOODIX_MASK_ID);
		return -EINVAL;
	}

	THP_LOG_INFO("%s: communication check passed\n", __func__);
	memcpy(temp_buf, module_info.mask_id, GOODIX_MASK_ID_LEN);
	temp_buf[GOODIX_MASK_ID_LEN] = '\0';
	THP_LOG_INFO("MASK_ID %s : ver %*ph\n", temp_buf,
		(u32)sizeof(module_info.mask_vid), module_info.mask_vid);
	THP_LOG_INFO("PID %s : ver %*ph\n", module_info.pid,
		(u32)sizeof(module_info.vid), module_info.vid);
	return 0;
}

static int touch_driver_power_init(void)
{
	int ret;

	ret = thp_power_supply_get(THP_VCC);
	ret |= thp_power_supply_get(THP_IOVDD);
	if (ret)
		THP_LOG_ERR("%s: fail to get power\n", __func__);

	return 0;
}

static void touch_driver_power_release(void)
{
	thp_power_supply_put(THP_VCC);
	thp_power_supply_put(THP_IOVDD);
}

static int touch_driver_power_on(struct thp_device *tdev)
{
	int ret;

	THP_LOG_INFO("%s:called\n", __func__);
	gpio_set_value(tdev->gpios->cs_gpio, GPIO_HIGH);

	ret = thp_power_supply_ctrl(THP_VCC, THP_POWER_ON, 0);
	ret |= thp_power_supply_ctrl(THP_IOVDD, THP_POWER_ON, 1);
	if (ret)
		THP_LOG_ERR("%s:power ctrl fail\n", __func__);
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_HIGH);
	return ret;
}

#define AFTER_VCC_POWERON_DELAY 65
#define AFTER_FIRST_RESET_GPIO_HIGH_DELAY 40
static int touch_driver_power_on_for_gt9896(struct thp_device *tdev)
{
	int ret;

	if (tdev == NULL) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	THP_LOG_INFO("%s:called\n", __func__);
	gpio_set_value(tdev->gpios->cs_gpio, GPIO_HIGH);
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_LOW);
	ret = thp_power_supply_ctrl(THP_VCC, THP_POWER_ON, NO_DELAY);
	if (ret)
		THP_LOG_ERR("%s:vcc fail\n", __func__);
	udelay(AFTER_VCC_POWERON_DELAY);
	ret = thp_power_supply_ctrl(THP_IOVDD, THP_POWER_ON, 3); /* 3ms */
	if (ret)
		THP_LOG_ERR("%s:vddio fail\n", __func__);

	THP_LOG_INFO("%s pull up tp ic reset\n", __func__);
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_HIGH);
	return ret;
}

static int touch_driver_power_off(struct thp_device *tdev)
{
	int ret;

	if (tdev == NULL) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	THP_LOG_INFO("%s pull down tp ic reset\n", __func__);
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_LOW);
	thp_do_time_delay(tdev->timing_config.suspend_reset_after_delay_ms);

	ret = thp_power_supply_ctrl(THP_IOVDD, THP_POWER_OFF, NO_DELAY);
	ret |= thp_power_supply_ctrl(THP_VCC, THP_POWER_OFF, NO_DELAY);
	if (ret)
		THP_LOG_ERR("%s:power ctrl fail\n", __func__);
	gpio_set_value(tdev->gpios->cs_gpio, GPIO_LOW);

	return ret;
}

static void touch_driver_timing_work(struct thp_device *tdev)
{
	if (tdev == NULL) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return;
	}
	THP_LOG_INFO("%s:called,do hard reset\n", __func__);
	gpio_direction_output(tdev->gpios->rst_gpio, THP_RESET_HIGH);
	thp_do_time_delay(tdev->timing_config.boot_reset_after_delay_ms);
}

static u16 checksum_16(u8 *data, int size)
{
	int i;
	int non_zero_count = 0;
	u16 checksum = 0;

	if ((data == NULL) || (size <= sizeof(u16))) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return MASK_8BIT;
	}

	for (i = 0; i < (size - sizeof(u16)); i++) {
		checksum += data[i];
		if (data[i])
			non_zero_count++;
	}

	checksum += (data[i] << MOVE_8BIT) + data[i + 1];

	return non_zero_count ? checksum : MASK_8BIT;
}

static u32 checksum_32(u8 *data, int size)
{
	int i;
	int non_zero_count = 0;
	u32 checksum = 0;

	if ((data == NULL) || (size <= sizeof(u32))) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return MASK_8BIT;
	}

	for (i = 0; i < (size - sizeof(u32)); i += 2) {
		checksum += ((data[i] << MOVE_8BIT) | data[i + 1]);
		if (data[i] || data[i + 1])
			non_zero_count++;
	}

	checksum += (data[i] << MOVE_24BIT) + (data[i + 1] << MOVE_16BIT) +
		    (data[i + 2] << MOVE_8BIT) + data[i + 3];

	return non_zero_count ? checksum : MASK_8BIT;
}

static void touch_driver_show_afe_debug_info(struct thp_device *tdev)
{
	u8 debug_buf[DEBUG_BUF_LEN] = {0};
	int ret;

	ret = touch_driver_spi_read(tdev,
		GOODIX_AFE_INFO_ADDR_GT9896,
		debug_buf, sizeof(debug_buf));
	if (ret) {
		THP_LOG_ERR("%s: failed read debug buf, ret %d\n",
			__func__, ret);
		return;
	}
	THP_LOG_INFO("debug_buf[0-20] %*ph\n",
		DEBUG_AFE_DATA_BUF_LEN, debug_buf); /* offset 0 */
	THP_LOG_INFO("debug_buf[20-40] %*ph\n",
		DEBUG_AFE_DATA_BUF_LEN, debug_buf +
		DEBUG_AFE_DATA_BUF_OFFSET); /* offset 20 */
	THP_LOG_INFO("debug_buf[40-60] %*ph\n",
		DEBUG_AFE_DATA_BUF_LEN, debug_buf +
		DEBUG_AFE_DATA_BUF_OFFSET * 2); /* offset 40 */
	THP_LOG_INFO("debug_buf[60-80] %*ph\n",
		DEBUG_AFE_DATA_BUF_LEN, debug_buf +
		DEBUG_AFE_DATA_BUF_OFFSET * 3); /* offset 60 */
	THP_LOG_INFO("debug_buf[80-100] %*ph\n",
		DEBUG_AFE_DATA_BUF_LEN, debug_buf +
		DEBUG_AFE_DATA_BUF_OFFSET * 4); /* offset 80 */
	THP_LOG_INFO("debug_buf[100-120] %*ph\n",
		DEBUG_AFE_DATA_BUF_LEN, debug_buf +
		DEBUG_AFE_DATA_BUF_OFFSET * 5); /* offset 100 */
	THP_LOG_INFO("debug_buf[120-140] %*ph\n",
		DEBUG_AFE_DATA_BUF_LEN, debug_buf +
		DEBUG_AFE_DATA_BUF_OFFSET * 6); /* offset 120 */
	THP_LOG_INFO("debug_buf[140-160] %*ph\n",
		DEBUG_AFE_DATA_BUF_LEN, debug_buf +
		DEBUG_AFE_DATA_BUF_OFFSET * 7); /* offset 140 */
}

static void touch_driver_get_afe_addr(const u8 * const afe_data_buf)
{
	THP_LOG_INFO("%s: try get useful info from afe data\n",
		__func__);
	goodix_frame_addr =
		(afe_data_buf[GOODIX_FRAME_ADDR_OFFSET_GT9896] <<
		MOVE_8BIT) +
		afe_data_buf[GOODIX_FRAME_ADDR_OFFSET_GT9896 + 1];
	goodix_frame_len =
		(afe_data_buf[GOODIX_FRAME_LEN_OFFSET_GT9896] <<
		MOVE_8BIT) +
		afe_data_buf[GOODIX_FRAME_LEN_OFFSET_GT9896 + 1];
	goodix_cmd_addr =
		(afe_data_buf[GOODIX_CMD_ADDR_OFFSET_GT9896] <<
		MOVE_8BIT) +
		afe_data_buf[GOODIX_CMD_ADDR_OFFSET_GT9896 + 1];

	THP_LOG_INFO("%s: frame addr 0x%x, len %d, cmd addr 0x%x\n",
		__func__, goodix_frame_addr,
		goodix_frame_len, goodix_cmd_addr);
}

static int touch_driver_get_afe_info_for_gt9896(struct thp_device *tdev)
{
	int ret;
	int retry;
	int afe_data_len;
	u8 buf[GET_AFE_BUF_LEN] = {0};
	u8 afe_data_buf[GOODIX_MAX_AFE_LEN_GT9896] = {0};

	for (retry = 0; retry < FW_INFO_READ_RETRY; retry++) {
		ret = touch_driver_spi_read(tdev, GOODIX_AFE_INFO_ADDR_GT9896,
			buf, sizeof(buf));
		if (ret) {
			THP_LOG_ERR("%s: failed read afe data length, ret %d\n",
				__func__, ret);
			goto error;
		}

		afe_data_len = (buf[0] << MOVE_8BIT) | buf[1];
		/* data len must be equal or less than GOODIX_MAX_AFE_LEN */
		if ((afe_data_len == 0) ||
			(afe_data_len > GOODIX_MAX_AFE_LEN_GT9896)) {
			THP_LOG_ERR("%s: invalid afe_data_len %d\n",
				__func__, afe_data_len);
			mdelay(GET_AFE_INFO_RETRY_DELAY);
			continue;
		}

		ret = touch_driver_spi_read(tdev, GOODIX_AFE_INFO_ADDR_GT9896,
			afe_data_buf, afe_data_len);
		if (ret) {
			THP_LOG_ERR("%s: failed read afe data, ret %d\n",
				__func__, ret);
			goto error;
		}
		if (!checksum16_cmp(afe_data_buf, afe_data_len)) {
			THP_LOG_INFO("%s: successfuly read afe data\n",
				__func__);
			break;
		}
		THP_LOG_ERR("%s: afe data checksum error\n", __func__);
		touch_driver_show_afe_debug_info(tdev);
		mdelay(GET_AFE_INFO_RETRY_DELAY);
	}
	if (retry != FW_INFO_READ_RETRY) {
		touch_driver_get_afe_addr(afe_data_buf);
		return 0;
	}
error:
	THP_LOG_ERR("%s: failed get afe info, use default\n", __func__);
	goodix_frame_addr = GOODIX_FRAME_ADDR_DEFAULT_GT9896;
	goodix_frame_len = GOODIX_FRAME_LEN_MAX_GT9896;
	goodix_cmd_addr = GOODIX_CMD_ADDR_DEFAULT_GT9896;
	THP_LOG_ERR("%s: afe data checksum error\n", __func__);
	return -EINVAL;
}

static int touch_driver_get_afe_info(struct thp_device *tdev)
{
	int ret = 0;
	int retry = 0;
	unsigned int afe_data_len;
	u8 buf[INFO_ADDR_BUF_LEN] = {0};
	u8 afe_data_buf[GOODIX_MAX_AFE_LEN] = {0};
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (tdev->thp_core == NULL)) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	if (cd->support_vendor_ic_type == GT9896)
		return touch_driver_get_afe_info_for_gt9896(tdev);

	for (retry = 0; retry < FW_INFO_READ_RETRY; retry++) {
		ret = touch_driver_spi_read(tdev, GOODIX_AFE_INFO_ADDR,
				buf, sizeof(buf));
		if (ret) {
			THP_LOG_ERR("%s: failed read afe data length, ret %d\n",
				__func__, ret);
			goto err_out;
		}

		afe_data_len = (buf[0] << MOVE_8BIT) | buf[1];
		/* data len must be equal or less than GOODIX_MAX_AFE_LEN */
		if ((afe_data_len == 0) || (afe_data_len > GOODIX_MAX_AFE_LEN)
			|| (afe_data_len & MASK_1BIT)) {
			THP_LOG_ERR("%s: invalied afe_data_len 0x%x retry\n",
				__func__, afe_data_len);
			mdelay(GET_AFE_INFO_RETRY_DELAY);
			continue;
		}
		THP_LOG_INFO("%s: got afe data len %d\n",
				__func__, afe_data_len);
		ret = touch_driver_spi_read(tdev,
			GOODIX_AFE_INFO_ADDR + INFO_ADDR_BUF_LEN,
			afe_data_buf, afe_data_len);
		if (ret) {
			THP_LOG_ERR("%s: failed read afe data, ret %d\n",
				__func__, ret);
			goto err_out;
		}

		if (!checksum_32(afe_data_buf, afe_data_len)) {
			THP_LOG_INFO("%s: successfuly read afe data\n",
				__func__);
			break;
		}
		THP_LOG_ERR("%s: afe data checksum error, checksum 0x%x, retry\n",
			__func__, checksum_32(afe_data_buf, afe_data_len));
		THP_LOG_ERR("afe_data_buf[0-20] %*ph\n",
			DEBUG_AFE_DATA_BUF_LEN, afe_data_buf);
		THP_LOG_ERR("afe_data_buf[20-40] %*ph\n",
			DEBUG_AFE_DATA_BUF_LEN, afe_data_buf +
			DEBUG_AFE_DATA_BUF_OFFSET);
		THP_LOG_ERR("afe_data_buf[40-60] %*ph\n",
			DEBUG_AFE_DATA_BUF_LEN, afe_data_buf +
			DEBUG_AFE_DATA_BUF_OFFSET * 2);
		THP_LOG_ERR("afe_data_buf[60-80] %*ph\n",
			DEBUG_AFE_DATA_BUF_LEN, afe_data_buf +
			DEBUG_AFE_DATA_BUF_OFFSET * 3);
		THP_LOG_ERR("afe_data_buf[80-100] %*ph\n",
			DEBUG_AFE_DATA_BUF_LEN, afe_data_buf +
			DEBUG_AFE_DATA_BUF_OFFSET * 4);
		mdelay(GET_AFE_INFO_RETRY_DELAY);
	}

	if (retry != FW_INFO_READ_RETRY) {
		THP_LOG_INFO("%s: try get useful info from afe data\n", __func__);
		goodix_frame_addr =
			(afe_data_buf[GOODIX_FRAME_ADDR_OFFSET] << MOVE_8BIT) +
			afe_data_buf[GOODIX_FRAME_ADDR_OFFSET + 1];
		goodix_frame_len =
			(afe_data_buf[GOODIX_FRAME_LEN_OFFSET] << MOVE_8BIT) +
			afe_data_buf[GOODIX_FRAME_LEN_OFFSET + 1];
		goodix_cmd_addr =
			(afe_data_buf[GOODIX_CMD_ADDR_OFFSET] << MOVE_8BIT) +
			afe_data_buf[GOODIX_CMD_ADDR_OFFSET + 1];
		goodix_fw_status_addr =
			(afe_data_buf[GOODIX_FW_STATUS_ADDR_OFFSET] << MOVE_8BIT) +
			afe_data_buf[GOODIX_FW_STATUS_ADDR_OFFSET + 1];
		goodix_fw_status_len =
			(afe_data_buf[GOODIX_FW_STATUS_LEN_OFFSET] << MOVE_8BIT) +
			afe_data_buf[GOODIX_FW_STATUS_LEN_OFFSET + 1];
		ret = 0;
	} else {
		THP_LOG_ERR("%s: failed get afe info, use default\n", __func__);
		ret = -EINVAL;
	}

err_out:
	THP_LOG_INFO("%s: frame addr 0x%x, len %d, cmd addr 0x%x\n", __func__,
			goodix_frame_addr, goodix_frame_len, goodix_cmd_addr);
	THP_LOG_INFO("%s: fw status addr 0x%x, len %d\n", __func__,
			goodix_fw_status_addr, goodix_fw_status_len);
	return ret;
}

int touch_driver_chip_detect(struct thp_device *tdev)
{
	int ret;
	struct thp_core_data *cd = NULL;

	THP_LOG_INFO("%s: called\n", __func__);
	if (tdev == NULL) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	goodix_spi_tx_buf = kzalloc(READ_CMD_BUF_LEN, GFP_KERNEL);
	if (goodix_spi_tx_buf == NULL) {
		THP_LOG_ERR("%s: goodix_spi_tx_buf null\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}
	cd = tdev->thp_core;
	touch_driver_power_init();
	if (cd->support_vendor_ic_type == GT9896)
		ret = touch_driver_power_on_for_gt9896(tdev);
	else
		ret = touch_driver_power_on(tdev);
	if (ret)
		THP_LOG_ERR("%s: power on failed\n", __func__);

	touch_driver_timing_work(tdev);

	if (touch_driver_communication_check(tdev)) {
		THP_LOG_ERR("%s:communication check fail\n", __func__);
		touch_driver_power_off(tdev);
		/* check old panel */
		cd->support_vendor_ic_type = GT9886;
		ret = touch_driver_power_on(tdev);
		if (ret)
			THP_LOG_ERR("%s: power on failed\n", __func__);
		touch_driver_timing_work(tdev);
		if (touch_driver_communication_check(tdev)) {
			THP_LOG_ERR("%s:check old fail\n", __func__);
			touch_driver_power_off(tdev);
			touch_driver_power_release();
			ret = -ENODEV;
			goto exit;
		}
	}

	if (touch_driver_get_afe_info(tdev))
		THP_LOG_ERR("%s: failed get afe addr info\n", __func__);
	return 0;
exit:
	kfree(goodix_spi_tx_buf);
	goodix_spi_tx_buf = NULL;
	if (tdev->thp_core->fast_booting_solution) {
		kfree(tdev->tx_buff);
		tdev->tx_buff = NULL;
		kfree(tdev->rx_buff);
		tdev->rx_buff = NULL;
		kfree(tdev);
		tdev = NULL;
	}
	return ret;
}

static int touch_driver_get_frame(struct thp_device *tdev,
	char *buf, unsigned int len)
{
	if ((tdev == NULL) || (buf == NULL)) {
		THP_LOG_INFO("%s: input dev null\n", __func__);
		return -ENOMEM;
	}

	if (!len) {
		THP_LOG_INFO("%s: read len illegal\n", __func__);
		return -ENOMEM;
	}

	return touch_driver_spi_read(tdev, goodix_frame_addr, buf, len);
}

static int touch_driver_gesture_report(struct thp_device *tdev,
	unsigned int *gesture_wakeup_value)
{
	int retval;

	THP_LOG_INFO("%s\n", __func__);
	retval = touch_driver_gesture_event(tdev, gesture_wakeup_value);
	if (retval != 0) {
		THP_LOG_INFO("[%s] ->get event failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int touch_driver_wakeup_gesture_enable_switch(
	struct thp_device *tdev, u8 switch_value)
{
	int retval = NO_ERR;
	int result;

	if (!tdev) {
		THP_LOG_ERR("%s: input dev is null\n", __func__);
		return -EINVAL;
	}

	if (switch_value) {
		result = touch_driver_switch_cmd(tdev,
			tdev->thp_core->cmd_gesture_mode, FEATURE_ENABLE);
		if (result)
			THP_LOG_ERR("failed enable gesture mode\n");

		retval = touch_driver_switch_cmd(tdev, CMD_SCREEN_ON_OFF,
				FEATURE_ENABLE);
		if (result || retval) {
			THP_LOG_ERR("failed enable gesture mode\n");
		} else {
			work_status |= IC_STATUS_GESTURE;
			THP_LOG_INFO("enable gesture mode\n");
		}
	} else {
		retval =  touch_driver_switch_cmd(tdev,
			tdev->thp_core->cmd_gesture_mode, FEATURE_DISABLE);
		if (retval) {
			THP_LOG_ERR("failed disable gesture mode\n");
		} else {
			THP_LOG_INFO("disable gesture mode\n");
			work_status &= ~IC_STATUS_GESTURE;
		}
	}

	THP_LOG_INFO("%s, write TP IC\n", __func__);
	return retval;
}

static int touch_driver_wrong_touch(struct thp_device *tdev)
{
	if (!tdev) {
		THP_LOG_ERR("%s: input dev is null\n", __func__);
		return -EINVAL;
	}

	if (tdev->thp_core->support_gesture_mode) {
		mutex_lock(&tdev->thp_core->thp_wrong_touch_lock);
		tdev->thp_core->easy_wakeup_info.off_motion_on = true;
		mutex_unlock(&tdev->thp_core->thp_wrong_touch_lock);
		THP_LOG_INFO("[%s] ->done\n", __func__);
	}
	return 0;
}

/* call this founction when TPIC in gesture mode
 *  return: if get valied gesture type 0 is returened
 */
#define GESTURE_EVENT_RETRY_TIME 10
static int touch_driver_gesture_event(struct thp_device *tdev,
	unsigned int *gesture_wakeup_value)
{
	u8 sync_flag = 0;
	int retval = -1;
	u16 gesture_type;
	u8 gesture_event_head[GESTURE_EVENT_HEAD_LEN + 1] = {0};
	int i = 0;
	int result;

	if (tdev == NULL) {
		THP_LOG_ERR("%s: input dev is null\n", __func__);
		return -EINVAL;
	}
	if (!(work_status & IC_STATUS_GESTURE)) {
		THP_LOG_INFO("%s:please enable gesture mode first\n", __func__);
		retval = -EINVAL;
		goto err_out;
	}
	msleep(WAIT_FOR_SPI_BUS_RESUMED_DELAY);
	/* wait spi bus resume */
	for (i = 0; i < GESTURE_EVENT_RETRY_TIME; i++) {
		retval = touch_driver_spi_read(tdev, goodix_frame_addr,
				gesture_event_head, sizeof(gesture_event_head));
		if (retval == 0) {
			break;
		} else {
			THP_LOG_ERR("%s: spi not work normal, ret %d retry\n",
				__func__, retval);
			msleep(WAIT_FOR_SPI_BUS_READ_DELAY);
		}
	}
	if (retval) {
		THP_LOG_ERR("%s: failed read gesture head info, ret %d\n",
			__func__, retval);
		return -EINVAL;
	}

	THP_LOG_INFO("gesute_data:%*ph\n", (u32)sizeof(gesture_event_head),
			gesture_event_head);
	if (gesture_event_head[0] != IRQ_EVENT_TYPE_GESTURE) {
		THP_LOG_ERR("%s: not gesture irq event, event_type 0x%x\n",
			__func__, gesture_event_head[0]);
		retval = -EINVAL;
		goto err_out;
	}

	if (checksum_16(gesture_event_head + 1, GESTURE_EVENT_HEAD_LEN)) {
		THP_LOG_ERR("gesture data checksum error\n");
		retval = -EINVAL;
		goto err_out;
	}

	gesture_type = (gesture_event_head[1] << MOVE_8BIT) +
			gesture_event_head[2];
	if (gesture_type == GESTURE_TYPE_DOUBLE_TAP) {
		THP_LOG_INFO("found valid gesture type\n");
		mutex_lock(&tdev->thp_core->thp_wrong_touch_lock);
		if (tdev->thp_core->easy_wakeup_info.off_motion_on == true) {
			tdev->thp_core->easy_wakeup_info.off_motion_on = false;
			*gesture_wakeup_value = TS_DOUBLE_CLICK;
		}
		mutex_unlock(&tdev->thp_core->thp_wrong_touch_lock);
		retval = 0;
	} else {
		THP_LOG_ERR("found invalid gesture type:0x%x\n", gesture_type);
		retval = -EINVAL;
	}
	/* clean sync flag */
	touch_driver_spi_write(tdev, goodix_frame_addr, &sync_flag, 1);
	return retval;

err_out:
	/* clean sync flag */
	touch_driver_spi_write(tdev, goodix_frame_addr, &sync_flag, 1);
	/* resend gesture command */
	result = touch_driver_switch_cmd(tdev, CMD_SCREEN_ON_OFF,
		FEATURE_ENABLE);
	if (result)
		THP_LOG_ERR("resend SCREEN_ON_OFF command\n");

	retval = touch_driver_switch_cmd(tdev,
		tdev->thp_core->cmd_gesture_mode, FEATURE_ENABLE);
	work_status |= IC_STATUS_GESTURE;
	if (result || retval)
		THP_LOG_ERR("resend gesture command\n");
	else
		THP_LOG_INFO("success resend gesture command\n");
	return -EINVAL;
}

#define CMD_SWITCH_INT_PIN 0xA9
#define CMD_SWITCH_INT_AP 0x0
#define CMD_SWITCH_INT_SH 0x1
static int touch_driver_resume_ap(struct thp_device *tdev)
{
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_LOW);
	mdelay(tdev->timing_config.resume_reset_after_delay_ms);
	gpio_set_value(tdev->gpios->rst_gpio, GPIO_HIGH);
	return 0;
}

#ifdef CONFIG_HUAWEI_SHB_THP
#define NEED_RESET 1
static int touch_driver_resume_shb(struct thp_device *tdev)
{
	int ret;

	if ((!tdev) || (!tdev->thp_core) || (!tdev->gpios)) {
		THP_LOG_ERR("%s: have null ptr\n", __func__);
		return -EINVAL;
	}
	if (tdev->thp_core->need_resume_reset == NEED_RESET) {
		gpio_set_value(tdev->gpios->rst_gpio, GPIO_LOW);
		mdelay(tdev->timing_config.resume_reset_after_delay_ms);
		gpio_set_value(tdev->gpios->rst_gpio, GPIO_HIGH);
		THP_LOG_INFO("%s:reset when resume\n", __func__);
		return 0;
	}
	ret = touch_driver_switch_cmd(tdev, CMD_SWITCH_INT_PIN,
		CMD_SWITCH_INT_AP);
	if (ret) {
		THP_LOG_ERR("%s:touch_driver_switch_cmd send err, ret = %d\n",
			__func__, ret);
		return ret;
	}
	THP_LOG_INFO("%s:touch_driver_switch_cmd send: switch int ap\n",
		__func__);
	return ret;
}
#endif

static int touch_driver_resume(struct thp_device *tdev)
{
	int ret = 0;

	if (tdev == NULL) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	/* if pt mode, we should reset */
	if (is_pt_test_mode(tdev)) {
		ret = touch_driver_resume_ap(tdev);
		if (tdev->thp_core->support_vendor_ic_type == GT9896)
			thp_do_time_delay(
				tdev->timing_config.boot_reset_after_delay_ms);
		THP_LOG_INFO("%s:pt mode called end\n", __func__);
		return ret;
	}
	if (g_thp_udfp_status ||
		(tdev->thp_core->easy_wakeup_info.sleep_mode ==
		TS_GESTURE_MODE) || goodix_stylus_status ||
		tdev->thp_core->aod_touch_status) {
#ifdef CONFIG_HUAWEI_SHB_THP
		if (tdev->thp_core->support_shb_thp)
			ret = touch_driver_resume_shb(tdev);
		else
#endif
			ret = touch_driver_resume_ap(tdev);
		THP_LOG_INFO("%s: called end\n", __func__);
		return ret;
	}
	if (tdev->thp_core->support_vendor_ic_type == GT9896)
		ret = touch_driver_power_on_for_gt9896(tdev);
	else
		ret = touch_driver_power_on(tdev);
	THP_LOG_INFO("%s: called end\n", __func__);
	return ret;
}

static int touch_driver_after_resume(struct thp_device *tdev)
{
	int ret = 0;

	THP_LOG_INFO("%s: called\n", __func__);
	if (tdev == NULL) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	thp_do_time_delay(tdev->timing_config.boot_reset_after_delay_ms);
	if (!g_thp_udfp_status && !is_pt_test_mode(tdev)) {
		/* Turn off sensorhub report when
		 * fingerprintud  isn't in work state.
		 */
		ret = touch_driver_switch_cmd(tdev, CMD_TOUCH_REPORT, 0);
		if (ret)
			THP_LOG_ERR("failed send CMD_TOUCH_REPORT mode\n");
	}

	return ret;
}

static int touch_driver_suspend_ap(struct thp_device *tdev)
{
	int ret;

	ret =  touch_driver_switch_cmd(tdev, CMD_SCREEN_ON_OFF, 1);
	if (ret)
		THP_LOG_ERR("failed to screen_on off\n");
	if (tdev->thp_core->easy_wakeup_info.sleep_mode == TS_GESTURE_MODE) {
		THP_LOG_INFO("%s TS_GESTURE_MODE\n", __func__);
		ret = touch_driver_wakeup_gesture_enable_switch(tdev,
			FEATURE_ENABLE);
		if (ret)
			THP_LOG_ERR("failed to wakeup gesture enable\n");
		mutex_lock(&tdev->thp_core->thp_wrong_touch_lock);
		tdev->thp_core->easy_wakeup_info.off_motion_on = true;
		mutex_unlock(&tdev->thp_core->thp_wrong_touch_lock);
	}

	return ret;
}

#ifdef CONFIG_HUAWEI_SHB_THP
static int touch_driver_suspend_shb(struct thp_device *tdev)
{
	int ret;

	ret = touch_driver_switch_cmd(tdev, CMD_SWITCH_INT_PIN,
		CMD_SWITCH_INT_SH);
	if (ret) {
		THP_LOG_ERR("%s:touch_driver_switch_cmd send err, ret = %d\n",
			__func__, ret);
		return ret;
	}
	THP_LOG_INFO("%s:touch_driver_switch_cmd send: switch int shb\n",
		__func__);
	return ret;
}

#define INPUTHUB_POWER_SWITCH_START_BIT 9
#define INPUTHUB_POWER_SWITCH_START_OFFSET 1
static void touch_driver_poweroff_status(void)
{
	struct thp_core_data *cd = thp_get_core_data();

	cd->poweroff_function_status =
		((cd->double_click_switch << THP_DOUBLE_CLICK_ON) |
		(g_thp_udfp_status << THP_TPUD_ON) |
		(cd->support_ring_feature << THP_RING_SUPPORT) |
		(cd->ring_switch << THP_RING_ON) |
		(goodix_stylus_status << THP_PEN_MODE_ON) |
		(cd->phone_status << THP_PHONE_STATUS) |
		(cd->single_click_switch << THP_SINGLE_CLICK_ON) |
		(cd->volume_side_status << THP_VOLUME_SIDE_STATUS_LEFT));

	if ((cd->power_switch >= POWER_KEY_OFF_CTRL) &&
		(cd->power_switch < POWER_MAX_STATUS))
	/*
	 * cd->poweroff_function_status 0~8 bit saved function flag
	 * eg:double_click, finger_status, ring_switch,and so on.
	 * cd->poweroff_function_status 9~16 bit saved screen-on-off reason flag
	 * cd->power_switch is a value(1~8) which stand for screen-on-off reason
	 */
		cd->poweroff_function_status |=
			(1 << (INPUTHUB_POWER_SWITCH_START_BIT +
			cd->power_switch - INPUTHUB_POWER_SWITCH_START_OFFSET));
}
#endif

static int touch_driver_suspend(struct thp_device *tdev)
{
	int ret = 0;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (tdev->thp_core == NULL)) {
		THP_LOG_ERR("%s: invalid input\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	g_thp_udfp_status = thp_get_status(THP_STATUS_UDFP);
	goodix_stylus_status = thp_get_status(THP_STATUS_STYLUS);
	THP_LOG_INFO("%s:gesture_status:%d,finger_status:%u\n",
		__func__, cd->easy_wakeup_info.sleep_mode, g_thp_udfp_status);
	THP_LOG_INFO("%s:ring_support:%u,ring_switch:%u,phone_status:%u\n",
		__func__, cd->support_ring_feature, cd->ring_switch,
		cd->phone_status);
	if (is_pt_test_mode(tdev)) {
		THP_LOG_INFO("%s: suspend PT mode\n", __func__);
		ret =  touch_driver_switch_cmd(tdev, CMD_PT_MODE, PT_MODE);
		if (ret)
			THP_LOG_ERR("failed enable PT mode\n");
	} else if (g_thp_udfp_status || cd->support_ring_feature ||
		(cd->easy_wakeup_info.sleep_mode == TS_GESTURE_MODE) ||
		goodix_stylus_status || cd->aod_touch_status) {
		if (cd->support_shb_thp) {
#ifdef CONFIG_HUAWEI_SHB_THP
			touch_driver_poweroff_status();
			ret = touch_driver_suspend_shb(tdev);
#endif
		} else {
			ret = touch_driver_suspend_ap(tdev);
		}
	} else {
		if (tdev->thp_core->support_shb_thp)
			/* 0:all function was closed */
			tdev->thp_core->poweroff_function_status = 0;
		ret = touch_driver_power_off(tdev);
		THP_LOG_INFO("enter poweroff mode: ret = %d\n", ret);
	}
	THP_LOG_INFO("%s: called end\n", __func__);
	return ret;
}

static void touch_driver_get_oem_info(struct thp_device *tdev,
	const char *buff)
{
	struct thp_core_data *cd = thp_get_core_data();
	char lcd_effect_flag;
	int ret;

	if ((!tdev) || (!buff) || (!tdev->thp_core)) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return;
	}
	if (tdev->thp_core->support_oem_info == THP_OEM_INFO_LCD_EFFECT_TYPE) {
		/* The 12th byte is lcd_effect_flag, and 0xaa is valid */
		lcd_effect_flag = buff[GOODIX_LCD_EFFECT_FLAG_POSITION - 1];
		memset(cd->oem_info_data, 0, sizeof(cd->oem_info_data));
		ret = snprintf(cd->oem_info_data, OEM_INFO_DATA_LENGTH,
			"0x%x", lcd_effect_flag);
		if (ret < 0)
			THP_LOG_INFO("%s:snprintf error\n", __func__);
		THP_LOG_INFO("%s:lcd effect flag :%s\n", __func__,
			cd->oem_info_data);
		return;
	}
	THP_LOG_INFO("%s:not support oem info\n", __func__);
}

static int touch_driver_is_valid_project_id(const char *id)
{
	int i;

	if (id == NULL)
		return false;
	for (i = 0; i < THP_PROJECT_ID_LEN; i++) {
		if (!isascii(*id) || !isalnum(*id))
			return false;
		id++;
	}
	return true;
}

static int touch_driver_get_project_id_for_gt9896(
	struct thp_device *tdev, char *buf, unsigned int len)
{

	char proj_id[THP_PROJECT_ID_LEN + 1] = {0};
	int ret;
	struct thp_core_data *cd = NULL;

	cd = tdev->thp_core;
	ret = touch_driver_spi_read(tdev, GOODIX_PROJECT_ID_ADDR_GT9896,
		proj_id, THP_PROJECT_ID_LEN);
	if (ret)
		THP_LOG_ERR("%s:Project_id Read ERR\n", __func__);
	proj_id[THP_PROJECT_ID_LEN] = '\0';
	THP_LOG_INFO("PROJECT_ID[0-9] %*ph\n", THP_PROJECT_ID_LEN, proj_id);

	if (touch_driver_is_valid_project_id(proj_id)) {
		strncpy(buf, proj_id, len);
	} else {
		THP_LOG_ERR("%s:get project id fail\n", __func__);
		if (cd->project_id_dummy) {
			THP_LOG_ERR("%s:use dummy prj id\n", __func__);
			strncpy(buf, cd->project_id_dummy, len);
			return 0;
		}
		return -EIO;
	}
	THP_LOG_INFO("%s call end\n", __func__);
	return 0;
}

static int touch_driver_get_project_id(struct thp_device *tdev,
	char *buf, unsigned int len)
{
	int retry;
	char proj_id[GOODIX_CUSTOME_INFO_LEN + 1] = {0};
	int ret = 0;
	struct thp_core_data *cd = NULL;

	if ((tdev == NULL) || (buf == NULL)) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	cd = tdev->thp_core;
	if (cd->support_vendor_ic_type == GT9896)
		return touch_driver_get_project_id_for_gt9896(tdev, buf, len);

	for (retry = 0; retry < GOODIX_GET_PROJECT_ID_RETRY; retry++) {
		ret = touch_driver_spi_read(tdev, GOODIX_PROJECT_ID_ADDR,
			proj_id, GOODIX_CUSTOME_INFO_LEN);
		if (ret) {
			THP_LOG_ERR("Project_id Read ERR\n");
			return -EIO;
		}

		if (!checksum_u8(proj_id, GOODIX_CUSTOME_INFO_LEN)) {
			proj_id[THP_PROJECT_ID_LEN] = '\0';
			if (!is_valid_project_id(proj_id)) {
				THP_LOG_ERR("get project id fail\n");
				return -EIO;
			}
			strncpy(buf, proj_id, len);
			THP_LOG_INFO("%s:get project id:%s\n", __func__, buf);
			touch_driver_get_oem_info(tdev, proj_id);
			return 0;
		}
		THP_LOG_ERR("proj_id[0-30] %*ph\n",
				GOODIX_CUSTOME_INFO_LEN, proj_id);
		THP_LOG_ERR("%s:get project id fail, retry\n", __func__);
		mdelay(GOODIX_GET_PROJECT_RETRY_DELAY_10MS);
	}

	return -EIO;
}

static void touch_driver_exit(struct thp_device *tdev)
{
	THP_LOG_INFO("%s: called\n", __func__);
	if (!tdev) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return;
	}
	kfree(tdev->tx_buff);
	kfree(tdev->rx_buff);
	kfree(tdev);
	kfree(goodix_spi_tx_buf);
	goodix_spi_tx_buf = NULL;
}

static int touch_driver_afe_notify_callback(struct thp_device *tdev,
	unsigned long event)
{
	if (tdev == NULL) {
		THP_LOG_ERR("%s: tdev null\n", __func__);
		return -EINVAL;
	}
	return touch_driver_get_afe_info(tdev);
}

#define GOODIX_SPI_ACTIVE_DELAY 1000
int touch_driver_spi_active(struct thp_device *tdev)
{
	int ret;
	u8 wake_flag = IDLE_WAKE_FLAG;

	ret = thp_set_spi_max_speed(IDLE_SPI_SPEED);
	if (ret) {
		THP_LOG_ERR("failed set spi speed to %dHz, ret %d\n",
			IDLE_SPI_SPEED, ret);
		return ret;
	}

	ret = touch_driver_spi_write(tdev, 0, &wake_flag, sizeof(wake_flag));
	if (ret)
		THP_LOG_ERR("failed write wakeup flag %x, ret %d",
				wake_flag, ret);
	THP_LOG_INFO("[%s] tdev->sdev->max_speed_hz-> %d\n", __func__,
			tdev->sdev->max_speed_hz);

	ret = thp_set_spi_max_speed(ACTIVE_SPI_SPEED);
	if (ret) {
		THP_LOG_ERR("failed reset speed to %dHz, ret %d\n",
			tdev->sdev->max_speed_hz, ret);
		return ret;
	}

	udelay(GOODIX_SPI_ACTIVE_DELAY);
	return ret;
}

#define FW_STAUTE_DATA_MASK  0x04
#define FW_STAUTE_DATA_FLAG  0xAA
static int touch_driver_idle_wakeup(struct thp_device *tdev)
{
	int ret;
	int i;
	u8 fw_status_data[MAX_FW_STATUS_DATA_LEN] = {0};

	THP_LOG_DEBUG("%s\n", __func__);

	if (!goodix_fw_status_addr || !goodix_fw_status_len ||
		goodix_fw_status_len > MAX_FW_STATUS_DATA_LEN) {
		THP_LOG_ERR("%s: invalid fw status reg, length: %d\n",
			__func__, goodix_fw_status_len);
		return 0;
	}
	ret = touch_driver_spi_read(tdev, goodix_fw_status_addr,
		fw_status_data, goodix_fw_status_len);
	if (ret) {
		THP_LOG_ERR("failed read fw status info data, ret %d\n", ret);
		return -EIO;
	}

	if (!checksum_16(fw_status_data, goodix_fw_status_len) &&
		!(fw_status_data[0] & FW_STAUTE_DATA_MASK) &&
		(fw_status_data[goodix_fw_status_len - 3] ==
			FW_STAUTE_DATA_FLAG))
		return 0;

	THP_LOG_DEBUG("fw status data:%*ph\n",
			goodix_fw_status_len, fw_status_data);
	THP_LOG_DEBUG("need do spi wakeup\n");
	for (i = 0; i < SPI_WAKEUP_RETRY; i++) {
		ret = touch_driver_spi_active(tdev);
		if (ret) {
			THP_LOG_DEBUG("failed write spi active flag, ret %d\n",
				ret);
			continue;
		}
		/* recheck spi state */
		ret = touch_driver_spi_read(tdev, goodix_fw_status_addr,
			fw_status_data, goodix_fw_status_len);
		if (ret) {
			THP_LOG_ERR("[recheck]failed read fw status info data, ret %d\n",
				ret);
			continue;
		}

		if (!checksum_16(fw_status_data, goodix_fw_status_len) &&
			!(fw_status_data[0] & FW_STAUTE_DATA_MASK) &&
			(fw_status_data[goodix_fw_status_len - 3] ==
				FW_STAUTE_DATA_FLAG))
			return 0;

		THP_LOG_DEBUG("fw status data:%*ph\n", goodix_fw_status_len,
						fw_status_data);
		THP_LOG_DEBUG("failed wakeup form idle retry %d\n", i);
	}
	return -EIO;
}

#ifdef CONFIG_HUAWEI_SHB_THP
static int touch_driver_switch_int_sh(struct thp_device *tdev)
{
	int retval;

	THP_LOG_INFO("%s: called\n", __func__);
	retval = touch_driver_switch_cmd(tdev, CMD_SWITCH_INT_PIN,
		CMD_SWITCH_INT_SH);
	if (retval != 0) {
		THP_LOG_INFO("%s: switch int to shb failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}

static int touch_driver_switch_int_ap(struct thp_device *tdev)
{
	int retval;

	THP_LOG_INFO("%s: called\n", __func__);
	retval = touch_driver_switch_cmd(tdev, CMD_SWITCH_INT_PIN,
		CMD_SWITCH_INT_AP);
	if (retval != 0) {
		THP_LOG_INFO("%s: switch int to ap failed\n", __func__);
		return -EINVAL;
	}
	return 0;
}
#endif

struct thp_device_ops goodix_dev_ops = {
	.init = touch_driver_init,
	.detect = touch_driver_chip_detect,
	.get_frame = touch_driver_get_frame,
	.resume = touch_driver_resume,
	.after_resume = touch_driver_after_resume,
	.suspend = touch_driver_suspend,
	.get_project_id = touch_driver_get_project_id,
	.exit = touch_driver_exit,
	.afe_notify = touch_driver_afe_notify_callback,
	.chip_wakeup_gesture_enable_switch =
		touch_driver_wakeup_gesture_enable_switch,
	.chip_wrong_touch = touch_driver_wrong_touch,
	.chip_gesture_report = touch_driver_gesture_report,
#ifdef CONFIG_HUAWEI_SHB_THP
	.switch_int_sh = touch_driver_switch_int_sh,
	.switch_int_ap = touch_driver_switch_int_ap,
#endif
};

static int __init touch_driver_module_init(void)
{
	int rc;
	struct thp_device *dev = kzalloc(sizeof(struct thp_device), GFP_KERNEL);
	struct thp_core_data *cd = thp_get_core_data();

	if (dev == NULL) {
		THP_LOG_ERR("%s: thp device malloc fail\n", __func__);
		return -ENOMEM;
	}

	dev->tx_buff = kzalloc(THP_MAX_FRAME_SIZE, GFP_KERNEL);
	dev->rx_buff = kzalloc(THP_MAX_FRAME_SIZE, GFP_KERNEL);
	if ((dev->tx_buff == NULL) || (dev->rx_buff == NULL)) {
		THP_LOG_ERR("%s: out of memory\n", __func__);
		rc = -ENOMEM;
		goto err;
	}

	dev->ic_name = GOODIX_IC_NAME;
	dev->dev_node_name = THP_GOODIX_DEV_NODE_NAME;
	dev->ops = &goodix_dev_ops;
	if (cd && cd->fast_booting_solution) {
		thp_send_detect_cmd(dev, NO_SYNC_TIMEOUT);
		/*
		 * The thp_register_dev will be called later to complete
		 * the real detect action.If it fails, the detect function will
		 * release the resources requested here.
		 */
		return 0;
	}
	rc = thp_register_dev(dev);
	if (rc) {
		THP_LOG_ERR("%s: register fail\n", __func__);
		goto err;
	}

	return rc;
err:
	kfree(dev->tx_buff);
	dev->tx_buff = NULL;
	kfree(dev->rx_buff);
	dev->rx_buff = NULL;
	kfree(dev);
	dev = NULL;
	return rc;
}

static void __exit touch_driver_module_exit(void)
{
	THP_LOG_INFO("%s: called\n", __func__);
};

module_init(touch_driver_module_init);
module_exit(touch_driver_module_exit);
MODULE_AUTHOR("goodix");
MODULE_DESCRIPTION("goodix driver");
MODULE_LICENSE("GPL");
