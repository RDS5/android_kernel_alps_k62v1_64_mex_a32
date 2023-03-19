

#ifndef _TEE_CLIENT_TYPE_H_
#define _TEE_CLIENT_TYPE_H_
#define SECURITY_AUTH_ENHANCE
#include "teek_client_list.h"
#include "teek_client_constants.h"
#define TOKEN_SAVE_LEN	24
#define CLOCK_NODE_LEN	8

/*
 * @ingroup teec_common_data
 * nullֵ�Ķ���
 */
#ifndef NULL
#define NULL 0
#endif

/*
 * @ingroup teec_common_data
 * ��������ֵ���Ͷ���
 *
 * ���ڱ�ʾ�������ؽ��
 */
typedef uint32_t teec_result;

/*
 * @ingroup teec_common_data
 * uuid���Ͷ���
 *
 * uuid������ѭrfc4122 [2]�����ڱ�ʶ��ȫ����
 */
typedef struct {
	uint32_t time_low;
	/* < ʱ����ĵ�4�ֽ�  */
	uint16_t time_mid;
	/* < ʱ�������2�ֽ�  */
	uint16_t timehi_and_version;
	/* < ʱ����ĸ�2�ֽ���汾�ŵ����  */
	uint8_t clockseq_and_node[CLOCK_NODE_LEN];
	/* < ʱ��������ڵ��ʶ�������  */
} teec_uuid;

/*
 * @ingroup teec_common_data
 * teec_context�ṹ�����Ͷ���
 *
 * �����ͻ���Ӧ���밲ȫ����֮�佨�������ӻ���
 */
typedef struct {
	void *dev;
	uint8_t *ta_path;
	struct list_node session_list;
	/* < �Ự����  */
	struct list_node shrd_mem_list;
	/* < �����ڴ�����  */
} teec_context;

/*
 * @ingroup teec_common_data
 * teec_session�ṹ�����Ͷ���
 * �����ͻ���Ӧ���밲ȫ����֮�佨���ĻỰ
 */
typedef struct {
	uint32_t session_id;
	/* < �Ựid���ɰ�ȫ���緵��  */
	teec_uuid service_id;
	/* < ��ȫ�����uuid��ÿ����ȫ����ӵ��Ψһ��uuid  */
	uint32_t ops_cnt;
	/* < �ڻỰ�ڵĲ�����  */
	struct list_node head;
	/* < �Ự����ͷ  */
	teec_context *context;
	/* < ָ��Ự������tee����  */
#ifdef SECURITY_AUTH_ENHANCE
	/* token_save_len  24byte = token  16byte + timestamp  8byte */
	uint8_t teec_token[TOKEN_SAVE_LEN];
#endif
} teec_session;

/*
 * @ingroup teec_common_data
 * teec_sharedmemory�ṹ�����Ͷ���
 * ����һ�鹲���ڴ棬����ע�ᣬҲ���Է���
 */
typedef struct {
	void *buffer;
	/* < �ڴ�ָ��  */
	size_t size;
	/* < �ڴ��С  */
	uint32_t flags;
	/* < �ڴ��ʶ����������������������ȡֵ��ΧΪ#teec_sharedmemctl  */
	uint32_t ops_cnt;
	/* < �ڴ������  */
	bool is_allocated;
	/* < �ڴ�����ʾ��������������ע��ģ����Ƿ����  */
	struct list_node head;
	/* < �����ڴ�����ͷ  */
	teec_context *context;
	/* < ָ��������tee���� */
} teec_sharedmemory;

/*
 * @ingroup teec_common_data
 * teec_tempmemory_reference�ṹ�����Ͷ���
 * ����һ����ʱ������ָ��\n
 * ��������#teec_parameter�����ͣ��������Ӧ�����Ϳ�����
 * #teec_memref_temp_input�� #teec_memref_temp_output����#teec_memref_temp_inout
 */
typedef struct {
	void *buffer;
	/* < ��ʱ������ָ��  */
	size_t size;
	/* < ��ʱ��������С  */
} teec_tempmemory_reference;

/*
 * @ingroup teec_common_data
 * teec_registeredmemory_reference�ṹ�����Ͷ���
 * ���������ڴ�ָ�룬ָ������ע������õĹ����ڴ�\n
 * ��������#teec_parameter�����ͣ��������Ӧ�����Ϳ�����
 * #teec_memref_whole�� #teec_memref_partial_input��
 * #teec_memref_partial_output���� #teec_memref_partial_inout
 */
typedef struct {
	teec_sharedmemory *parent;
	/* < �����ڴ�ָ��  */
	size_t size;
	/* < �����ڴ��ʹ�ô�С  */
	size_t offset;
	/* < �����ڴ��ʹ��ƫ��  */
} teec_registeredmemory_reference;

/*
 * @ingroup teec_common_data
 * teec_value�ṹ�����Ͷ���
 * ��������������\n
 * ��������#teec_parameter�����ͣ��������Ӧ�����Ϳ�����
 * #teec_value_input�� #teec_value_output����#teec_value_inout
 */
typedef struct {
	uint32_t a;
	/* < ��������a  */
	uint32_t b;
	/* < ��������b  */
} teec_value;

typedef struct {
	int ion_share_fd;  /* < ��������a  */
	size_t ion_size;  /* < ��������b  */
} teec_ion_reference;

/*
 * @ingroup teec_common_data
 * teec_parameter���������Ͷ���
 * ����#teec_operation����Ӧ�Ĳ�������
 */
typedef union {
	teec_tempmemory_reference tmpref;
	/* < ����#teec_tempmemory_reference����  */
	teec_registeredmemory_reference memref;
	/* < ����#teec_registeredmemory_reference����  */
	teec_value value;
	/* < ����#teec_value����  */
	teec_ion_reference ionref;
} teec_parameter;

typedef struct {
	uint32_t event_type;
	/* tui event type */
	uint32_t value;
	/* return value, is keycode if tui event is getkeycode */
	uint32_t notch;  /* notch size of phone */
	uint32_t width;  /* width of foldable screen */
	uint32_t height; /* height of foldable screen */
	uint32_t fold_state;    /* state of foldable screen */
	uint32_t display_state; /* one state of folded state */
	uint32_t phy_width;     /* real width of the mobile */
	uint32_t phy_height;    /* real height of the mobile */
} teec_tui_parameter;

/*
 * @ingroup teec_common_data
 * teec_operation�ṹ�����Ͷ���
 *
 * �򿪻Ự��������ʱ�Ĳ�������ͨ��������������
 * ȡ������Ҳ����ʹ�ô˲���
 */
typedef struct {
	uint32_t started;
	/* < ��ʶ�Ƿ���ȡ��������0��ʾȡ������  */
	uint32_t paramtypes;
	/* ����params�Ĳ�������#teec_paramtype,
	 * ��Ҫʹ�ú�#teec_param_types��ϲ�����?	 */
	teec_parameter params[4];
	/* < �������ݣ�����Ϊ#teec_parameter  */
	teec_session *session;
	bool cancel_flag;
} teec_operation;

/* begin: for KERNEL-HAL out interface */
/*
 * @ingroup TEEC_COMMON_DATA
 * ��������ֵ���Ͷ���
 *
 * ���ڱ�ʾ�������ؽ��
 */
typedef uint32_t TEEC_Result;

/*
 * @ingroup TEEC_COMMON_DATA
 * UUID���Ͷ���
 *
 * UUID������ѭRFC4122 [2]�����ڱ�ʶ��ȫ����
 */
typedef struct {
	uint32_t timeLow;
	/* < ʱ����ĵ�4�ֽ�  */
	uint16_t timeMid;
	/* < ʱ�������2�ֽ�  */
	uint16_t timeHiAndVersion;
	/* < ʱ����ĸ�2�ֽ���汾�ŵ����  */
	uint8_t clockSeqAndNode[CLOCK_NODE_LEN];
	/* < ʱ��������ڵ��ʶ�������  */
} TEEC_UUID;

/*
 * @ingroup TEEC_COMMON_DATA
 * TEEC_Context�ṹ�����Ͷ���
 *
 * �����ͻ���Ӧ���밲ȫ����֮�佨�������ӻ���
 */
typedef struct {
	void *dev;
	uint8_t *ta_path;
	struct list_node session_list;
	/* < �Ự����  */
	struct list_node shrd_mem_list;
	/* < �����ڴ�����  */
} TEEC_Context;

/*
 * @ingroup TEEC_COMMON_DATA
 * TEEC_Session�ṹ�����Ͷ���
 *
 * �����ͻ���Ӧ���밲ȫ����֮�佨���ĻỰ
 */
typedef struct {
	uint32_t session_id;
	/* < �ỰID���ɰ�ȫ���緵��  */
	TEEC_UUID service_id;
	/* < ��ȫ�����UUID��ÿ����ȫ����ӵ��Ψһ��UUID  */
	uint32_t ops_cnt;
	/* < �ڻỰ�ڵĲ�����  */
	struct list_node head;
	/* < �Ự����ͷ  */
	TEEC_Context *context;
	/* < ָ��Ự������TEE����  */
#ifdef SECURITY_AUTH_ENHANCE
	/* TOKEN_SAVE_LEN  24byte = token  16byte + timestamp  8byte */
	uint8_t teec_token[TOKEN_SAVE_LEN];
#endif
} TEEC_Session;

/*
 * @ingroup TEEC_COMMON_DATA
 * TEEC_SharedMemory�ṹ�����Ͷ���
 *
 * ����һ�鹲���ڴ棬����ע�ᣬҲ���Է���
 */
typedef struct {
	void *buffer;
	/* < �ڴ�ָ��  */
	size_t size;
	/* < �ڴ��С  */
	uint32_t flags;
	/* < �ڴ��ʶ����������������������ȡֵ��ΧΪ#TEEC_SharedMemCtl  */
	uint32_t ops_cnt;
	/* < �ڴ������  */
	bool is_allocated;
	/* < �ڴ�����ʾ��������������ע��ģ����Ƿ����  */
	struct list_node head;
	/* < �����ڴ�����ͷ  */
	TEEC_Context *context;
	/* < ָ��������TEE���� */
} TEEC_SharedMemory;

/*
 * @ingroup TEEC_COMMON_DATA
 * TEEC_TempMemoryReference�ṹ�����Ͷ���
 *
 * ����һ����ʱ������ָ��\n
 * ��������#TEEC_Parameter�����ͣ��������Ӧ�����Ϳ�����
 * #TEEC_MEMREF_TEMP_INPUT�� #TEEC_MEMREF_TEMP_OUTPUT����#TEEC_MEMREF_TEMP_INOUT
 */
typedef struct {
	void *buffer;
	/* < ��ʱ������ָ��  */
	size_t size;
	/* < ��ʱ��������С  */
} TEEC_TempMemoryReference;

/*
 * @ingroup TEEC_COMMON_DATA
 * TEEC_RegisteredMemoryReference�ṹ�����Ͷ���
 *
 * ���������ڴ�ָ�룬ָ������ע������õĹ����ڴ�\n
 * ��������#TEEC_Parameter�����ͣ��������Ӧ�����Ϳ�����
 * #TEEC_MEMREF_WHOLE�� #TEEC_MEMREF_PARTIAL_INPUT��
 * #TEEC_MEMREF_PARTIAL_OUTPUT���� #TEEC_MEMREF_PARTIAL_INOUT
 */
typedef struct {
	TEEC_SharedMemory *parent;
	/* < �����ڴ�ָ��  */
	size_t size;
	/* < �����ڴ��ʹ�ô�С  */
	size_t offset;
	/* < �����ڴ��ʹ��ƫ��  */
} TEEC_RegisteredMemoryReference;

/*
 * @ingroup TEEC_COMMON_DATA
 * TEEC_Value�ṹ�����Ͷ���
 *
 * ��������������\n
 * ��������#TEEC_Parameter�����ͣ��������Ӧ�����Ϳ�����
 * #TEEC_VALUE_INPUT�� #TEEC_VALUE_OUTPUT����#TEEC_VALUE_INOUT
 */
typedef struct {
	uint32_t a;
	/* < ��������a  */
	uint32_t b;
	/* < ��������b  */
} TEEC_Value;

typedef struct {
	int ion_share_fd;  /* < ��������a  */
	size_t ion_size;   /* < ��������b  */
} TEEC_IonReference;

/*
 * @ingroup TEEC_COMMON_DATA
 * TEEC_Parameter���������Ͷ���
 *
 * ����#TEEC_Operation����Ӧ�Ĳ�������
 */
typedef union {
	TEEC_TempMemoryReference tmpref;
	/* < ����#TEEC_TempMemoryReference����  */
	TEEC_RegisteredMemoryReference memref;
	/* < ����#TEEC_RegisteredMemoryReference����  */
	TEEC_Value value;
	/* < ����#TEEC_Value����  */
	TEEC_IonReference ionref;
} TEEC_Parameter;

typedef struct {
	uint32_t event_type;
	/* Tui event type */
	uint32_t value;
	/* return value, is keycode if tui event is getKeycode */
	uint32_t notch;   /* notch size of phone */
	uint32_t width;   /* width of foldable screen */
	uint32_t height;  /* height of foldable screen */
	uint32_t fold_state;    /* state of foldable screen */
	uint32_t display_state; /* one state of folded state */
	uint32_t phy_width;     /* real width of the mobile */
	uint32_t phy_height;    /* real height of the mobile */
} TEEC_TUI_Parameter;

/*
 * @ingroup TEEC_COMMON_DATA
 * TEEC_Operation�ṹ�����Ͷ���
 *
 * �򿪻Ự��������ʱ�Ĳ�������ͨ��������������
 * ȡ������Ҳ����ʹ�ô˲���
 */
typedef struct {
	uint32_t started;
	/*  ��ʶ�Ƿ���ȡ��������0��ʾȡ������*/
	uint32_t paramTypes;
	/* ����params�Ĳ�������#TEEC_ParamType,
	 * ��Ҫʹ�ú�#TEEC_PARAM_TYPES��ϲ�������
	 */
	TEEC_Parameter params[4];
	/*  �������ݣ�����Ϊ#TEEC_Parameter  */
	TEEC_Session *session;
	bool cancel_flag;
} TEEC_Operation;

/* end: for KERNEL-HAL out interface */
#endif
