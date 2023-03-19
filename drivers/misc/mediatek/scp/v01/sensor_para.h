#ifndef __SENSOR_PARA_H
#define __SENSOR_PARA_H

struct als_para_t{
	uint8_t tp_type;
	uint8_t tp_color;
	int cali_target[2];
	uint8_t als_extend_para[50];
	uint8_t product_name;
};

struct ps_para_t{
	uint8_t threshold_h;
	uint8_t threshold_l;
	uint8_t min_prox;
	uint8_t pwave;
	uint8_t pwindow;
	uint8_t product_name;
};

struct acc_para_t{
 	uint8_t axis_map_x;
 	uint8_t axis_map_y;
 	uint8_t axis_map_z;
 	uint8_t negate_x;
 	uint8_t negate_y;
 	uint8_t negate_z;
	uint8_t direction;
};

struct sar_para_t{
	uint8_t reg[10];
	uint8_t reg_val[10];
	uint32_t product_name;
};

struct mag_para_t{
 	uint8_t axis_map_x;
 	uint8_t axis_map_y;
 	uint8_t axis_map_z;
 	uint8_t negate_x;
 	uint8_t negate_y;
 	uint8_t negate_z;
};

struct finger_para_t {
	short fg_data[256]; // fg sense need data size
	int head;
	int tail;
	int finger_ready;
};

struct sensor_para_t{
	struct als_para_t als_para;
	struct ps_para_t ps_para;
	struct acc_para_t acc_para;
	struct sar_para_t sar_para;
	struct mag_para_t mag_para;
	struct finger_para_t fg_para;
};

enum sh_mem_type_t {
	SHR_MEM_TYPE_ALL,
	SHR_MEM_TYPE_ACC,
	SHR_MEM_TYPE_GYR,
	SHR_MEM_TYPE_ALS,
	SHR_MEM_TYPE_PROX,
	SHR_MEM_TYPE_FINGER,
	SHR_MEM_TYPE_MAX,
};

void *get_sensor_share_mem_addr(enum sh_mem_type_t type);

extern struct sensor_para_t *sensor_para;
#endif
