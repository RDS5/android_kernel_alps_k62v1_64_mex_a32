
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/mm.h>

#include "teek_client_api.h"
#include "teek_client_id.h"

#include "rootagent_mem.h"
#include "rootagent_api.h"
#include "rootagent_common.h"
#include "rootagent_crypto.h"
#include "rootagent.h"


static TEEC_Context gContext;
static TEEC_Session gSession;
static int gInited; /*init flag*/
static int gCount; /*init count*/
static TEEC_UUID uuid = UUID_RM_TA; /*TA service ID*/
static TEEC_TempMemoryReference gMem;	/* for invokecmd params[0].tmpref.buffer */
static DEFINE_MUTEX(root_monitor_lock);

static TEEC_TempMemoryReference m_swap_mem;	/* for cbc crypto buffer: rm_command_in_out */
static uint8_t m_chal_req_key[CHALLENGE_REQ_KEY_LENGTH];
static uint8_t m_chal_key[CHALLENGE_KEY_LENGTH];
static uint8_t m_nounce[CHALLENGE_NOUNCE_LENGTH];

static uint8_t eima_nounce[CHALLENGE_NOUNCE_LENGTH];
static int root_flag = REV_NOT_ROOT;
uint32_t tee_scan_status = REV_NOT_ROOT;

/* only consider rootscan and eima bits form tee(but rootproc)
 *
 */
uint32_t tee_valid_bits =  ( 1 << KERNELCODEBIT | 1 << SYSTEMCALLBIT | 1 << SESTATUSBIT | 1 << SEHOOKBIT | \
                      1 << SETIDBIT | 1 << EIMABIT );


uint32_t get_tee_status(void)
{
	return tee_scan_status & tee_valid_bits;
}

void root_monitor_finalize(void)
{
	 mutex_lock(&root_monitor_lock);
	 rm_mem_destroy();
	 gMem.buffer = NULL;
	 gMem.size = 0;
	 m_swap_mem.buffer = NULL;
	 m_swap_mem.size = 0;
	 TEEK_CloseSession(&gSession);
	 TEEK_FinalizeContext(&gContext);
	 gInited = 0;
	 gCount = 0;
	 mutex_unlock(&root_monitor_lock);
}

TEEC_Result root_monitor_tee_init(void)
{
	TEEC_Result result;
	uint32_t origin;
	TEEC_Operation operation = {0};
	u8 package_name[] = "antiroot-ca";
	u32 root_id = 0;

	if (0 != get_ro_secure() && REV_NOT_ROOT != root_flag) {
		antiroot_error("device is rooted!\n");
		return TEE_ERROR_ANTIROOT_RSP_FAIL; /*lint !e570*/
	}

	mutex_lock(&root_monitor_lock);
	if (gInited) {
		antiroot_debug(ROOTAGENT_DEBUG_API,
				"RootAgent has already initialized");
		mutex_unlock(&root_monitor_lock);
		return TEEC_SUCCESS;
	}
	result = rm_mem_init();
	if (result) {
		antiroot_error("rm_mem_init failed\n");
		mutex_unlock(&root_monitor_lock);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	if (initial_rm_shared_mem(&gMem, &m_swap_mem)) {
		antiroot_error("Initial swap or share memory failed\n");
		mutex_unlock(&root_monitor_lock);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	result = TEEK_InitializeContext(NULL, &gContext);
	if (TEEC_SUCCESS != result) {
		antiroot_error("root_monitor_tee_init failed\n");
		goto cleanup_2;
	}

	operation.started = 1;
	operation.cancel_flag = 0;
	operation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_NONE,
			TEEC_NONE,
			TEEC_MEMREF_TEMP_INPUT,
			TEEC_MEMREF_TEMP_INPUT);
	operation.params[2].tmpref.buffer = (void *)(&root_id);
	operation.params[2].tmpref.size = sizeof(root_id);
	operation.params[3].tmpref.buffer = (void *)(package_name);
	operation.params[3].tmpref.size = strlen(package_name) + 1; /*lint !e64*/
	result = TEEK_OpenSession(&gContext, &gSession,
		&uuid, TEEC_LOGIN_IDENTIFY, NULL, &operation, &origin);
	if (TEEC_SUCCESS != result) {
		antiroot_error("root agent open session failed\n");
		goto cleanup_1;
	}
	gInited = 1;
	gCount++;
	antiroot_debug(ROOTAGENT_DEBUG_API,
			"root_monitor_tee_init ok, initialized count: %d, gInited: %d\n",
			gCount, gInited);
	mutex_unlock(&root_monitor_lock);
	return TEEC_SUCCESS;

cleanup_1:
	TEEK_FinalizeContext(&gContext);
cleanup_2:
	rm_mem_destroy();
	mutex_unlock(&root_monitor_lock);
	return result;
}

TEEC_Result setting_config(struct RAGENT_CONFIG *config,
		struct RAGENT_ROOTPROC *root_proc)
{
	uint32_t origin;
	TEEC_Result result;
	TEEC_Operation operation;
	struct RAGENT_COMMAND *rm_command;
	int s_ret = 0;

	if (!gInited || !(gMem.buffer)) {
		antiroot_error("Agent should be initialized first!\n");
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	if (!config || !root_proc || !(root_proc->procid)) {
		antiroot_error("Bad params!\n");
		return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
	}

	s_ret = memset_s(gMem.buffer, RM_PRE_ALLOCATE_SIZE,
			0x0, gMem.size);
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memset_s(&operation, sizeof(TEEC_Operation),
			0, sizeof(TEEC_Operation));
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	operation.started = 1;
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);
	operation.params[0].tmpref.buffer = gMem.buffer;
	operation.params[0].tmpref.size = gMem.size;
	rm_command = (struct RAGENT_COMMAND *)gMem.buffer;
	rm_command->magic = MAGIC;
	rm_command->version = VERSION;
	rm_command->interface = ROOTAGENT_CONFIG_ID;
	rm_command->content.config.white_list.dStatus =
		config->white_list.dStatus;
	rm_command->content.config.white_list.selinux =
		config->white_list.selinux;
	rm_command->content.config.white_list.proclength = root_proc->length;
	rm_command->content.config.white_list.setid = 0;
	s_ret = memcpy_s(m_chal_req_key,
			CHALLENGE_REQ_KEY_LENGTH,
			config->cipher_key.cha_req_key,
			CHALLENGE_REQ_KEY_LENGTH);
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memcpy_s(rm_command->content.config.cipher_key.cha_req_key,
			CHALLENGE_REQ_KEY_LENGTH,
			config->cipher_key.cha_req_key,
			CHALLENGE_REQ_KEY_LENGTH);
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memcpy_s(m_chal_key, CHALLENGE_KEY_LENGTH,
			config->cipher_key.cha_key, CHALLENGE_KEY_LENGTH);
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memcpy_s(rm_command->content.config.cipher_key.cha_key,
			CHALLENGE_KEY_LENGTH,
			config->cipher_key.cha_key, CHALLENGE_KEY_LENGTH);
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}

	s_ret = memcpy_s(rm_command->content.config.white_list.kcodes,
			KERNEL_CODE_LENGTH_SHA,
			config->white_list.kcodes,
			KERNEL_CODE_LENGTH_SHA);
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memcpy_s(rm_command->content.config.white_list.syscalls,
			SYSTEM_CALL_LENGTH_SHA,
			config->white_list.syscalls,
			SYSTEM_CALL_LENGTH_SHA);
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memcpy_s(rm_command->content.config.white_list.sehooks,
			SELINUX_HOOKS_LENGTH_SHA,
			config->white_list.sehooks,
			SELINUX_HOOKS_LENGTH_SHA);
	if (EOK != s_ret) {
		antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}

	antiroot_debug(ROOTAGENT_DEBUG_API, "setting_config-------proclength:%d\n",
			rm_command->content.config.white_list.proclength);
	if ((root_proc->length > 0)
			&& (root_proc->length < ROOTAGENT_RPROCS_MAX_LENGTH)
			&& (NULL != root_proc->procid)) {
		s_ret = memcpy_s(gMem.buffer + sizeof(struct RAGENT_COMMAND),
				RM_PRE_ALLOCATE_SIZE - sizeof(struct RAGENT_COMMAND),
				root_proc->procid, root_proc->length);
		if (EOK != s_ret) {
			antiroot_error("setting_config _s fail. line=%d, s_ret=%d.\n",
					__LINE__, s_ret);
			return TEEC_ERROR_GENERIC; /*lint !e570*/
		}
	} else {
		antiroot_error("root_proc is NULL!\n");
		return TEEC_ERROR_OUT_OF_MEMORY; /*lint !e570*/
	}

	result = TEEK_InvokeCommand(&gSession,
			ROOTAGENT_CONFIG_ID, &operation, &origin);
	/*
	 * if TA read rootstatus is already rooted, then TA
	 * will NOT setWhitelist, and return TEEC_SUCCESS directly,
	 * with value.a = TEE_ERROR_ANTIROOT_RSP_FAIL.
	 */
	if (TEEC_SUCCESS != result) {
		antiroot_error("Setting_config failed\n");
	} else {
		tee_scan_status = operation.params[1].value.b;
			if (REV_NOT_ROOT != operation.params[1].value.a) {
				antiroot_debug(ROOTAGENT_DEBUG_ERROR, "Setting_config failed due to R.\n");
				result = TEE_ERROR_ANTIROOT_RSP_FAIL; /*lint !e570*/
			}
	}

	return result;
}

TEEC_Result request_challenge(struct RAGENT_CHALLENGE *ichallenge)
{
	uint32_t origin;
	TEEC_Result result;
	TEEC_Operation operation;
	char *rm_command_out_in;
	char *rm_command_in_out;
	struct RAGENT_COMMAND *rm_command;
	int s_ret = 0;
	int ret;

	if (!gInited || !(gMem.buffer)) {
		antiroot_error("Agent should be initialized first!\n");
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	if (!ichallenge) {
		antiroot_error("Bad params!\n");
		return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
	}
	s_ret = memset_s(gMem.buffer, RM_PRE_ALLOCATE_SIZE, 0x0, gMem.size);
	if (EOK != s_ret) {
		antiroot_error("request_challenge _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memset_s(m_swap_mem.buffer, RM_PRE_ALLOCATE_SIZE,
			0x0, m_swap_mem.size);
	if (EOK != s_ret) {
		antiroot_error("request_challenge _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}

	s_ret = memset_s(&operation, sizeof(TEEC_Operation),
			0, sizeof(TEEC_Operation));
	if (EOK != s_ret) {
		antiroot_error("request_challenge _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	operation.started = 1;
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);
	operation.params[0].tmpref.buffer = gMem.buffer;
	operation.params[0].tmpref.size = gMem.size;

	get_random_bytes(gMem.buffer, IV_SIZE);
	rm_command_out_in = (char *)(gMem.buffer + IV_SIZE);
	rm_command_in_out = (char *)m_swap_mem.buffer;
	if (!rm_command_in_out) {
		antiroot_error("malloc failed!\n");
		return TEEC_ERROR_OUT_OF_MEMORY; /*lint !e570*/
	}
	rm_command = (struct RAGENT_COMMAND *)rm_command_in_out;
	rm_command->magic = MAGIC;
	rm_command->version = VERSION;
	rm_command->interface = ROOTAGENT_CHALLENGE_ID;
	rm_command->content.challenge.cpuload = ichallenge->cpuload;
	rm_command->content.challenge.battery = ichallenge->battery;
	rm_command->content.challenge.charging = ichallenge->charging;
	rm_command->content.challenge.time = ichallenge->time;
	rm_command->content.challenge.timezone = ichallenge->timezone;
	s_ret = memset_s(rm_command->content.challenge.nounce,
			CHALLENGE_NOUNCE_LENGTH,
			0, CHALLENGE_NOUNCE_LENGTH);
	if (EOK != s_ret) {
		antiroot_error("request_challenge _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memset_s(m_nounce, CHALLENGE_NOUNCE_LENGTH,
			0, CHALLENGE_NOUNCE_LENGTH);
	if (EOK != s_ret) {
		antiroot_error("request_challenge _s fail. line=%d, s_ret=%d.\n",
			__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	ret = do_aes256_cbc((u8 *)rm_command_out_in, (u8 *)rm_command_in_out,
		gMem.buffer, m_chal_req_key,
		ANTIROOT_SRC_LEN, ENCRYPT);
	if (ret) {
		antiroot_error("do_aes256_cbc failed, ret = %d.\n", ret);
		return (TEEC_Result)TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	result = TEEK_InvokeCommand(&gSession, ROOTAGENT_CHALLENGE_ID,
			&operation, &origin);
	if (TEEC_SUCCESS != result) {
		antiroot_error("Request_challenge failed!\n");
	} else if (REV_NOT_ROOT != operation.params[1].value.a) {
		antiroot_debug(ROOTAGENT_DEBUG_ERROR,
				"Request_challenge failed due to R!\n");
		root_flag = operation.params[1].value.a;
		tee_scan_status = operation.params[1].value.b;
		result = TEE_ERROR_ANTIROOT_RSP_FAIL; /*lint !e570*/
	} else {
		tee_scan_status = operation.params[1].value.b;
		ret = do_aes256_cbc((u8 *)rm_command_in_out, (u8 *)rm_command_out_in,
			gMem.buffer, m_chal_key,
			ANTIROOT_SRC_LEN, DECRYPT);
		if (ret) {
			antiroot_error("do_aes256_cbc failed, ret = %d.\n", ret);
			return (TEEC_Result)TEEC_ERROR_GENERIC;
		}
		s_ret = memcpy_s(m_nounce, CHALLENGE_NOUNCE_LENGTH,
				rm_command->content.challenge.nounce,
				CHALLENGE_NOUNCE_LENGTH);
		if (EOK != s_ret) {
			antiroot_error("request_challenge _s fail. line=%d, s_ret=%d.\n",
					__LINE__, s_ret);
			return TEEC_ERROR_GENERIC; /*lint !e570*/
		}
		s_ret = memcpy_s(ichallenge->challengeid,
				CHALLENGE_MAX_LENGTH*sizeof(uint32_t),
				rm_command->content.challenge.challengeid,
				CHALLENGE_MAX_LENGTH*sizeof(uint32_t));
		if (EOK != s_ret) {
			antiroot_error("request_challenge _s fail. line=%d, s_ret=%d.\n",
					__LINE__, s_ret);
			return TEEC_ERROR_GENERIC; /*lint !e570*/
		}
		antiroot_debug(ROOTAGENT_DEBUG_API, "Request_challenge successful\n");
	}
	return result;
}
TEEC_Result feedback_response(struct RAGENT_RESPONSE *response,
		struct RAGENT_ROOTPROC *root_proc)
{
	uint32_t origin;
	int size;
	TEEC_Result result;
	TEEC_Operation operation;
	void *rm_command_out;
	void *rm_command_in;
	struct RAGENT_COMMAND *rm_command;
	struct RAGENT_RESPONSE *rsp;
	int s_ret = 0;
	int ret;

	if (!gInited || !(gMem.buffer)) {
		antiroot_error("Agent should be initialized first!\n");
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}

	if (!response || !root_proc) {
		antiroot_error("Bad params!\n");
		return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
	}

	s_ret = memset_s(gMem.buffer, RM_PRE_ALLOCATE_SIZE, 0x0, gMem.size);
	if (EOK != s_ret) {
		antiroot_error("feedback_response _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memset_s(m_swap_mem.buffer, RM_PRE_ALLOCATE_SIZE,
			0x0, m_swap_mem.size);
	if (EOK != s_ret) {
		antiroot_error("feedback_response _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}

	s_ret = memset_s(&operation, sizeof(TEEC_Operation),
			0, sizeof(TEEC_Operation));
	if (EOK != s_ret) {
		antiroot_error("feedback_response _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	operation.started = 1;
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT,
			TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE);
	operation.params[0].tmpref.buffer = gMem.buffer;
	operation.params[0].tmpref.size = gMem.size;
	get_random_bytes(gMem.buffer, IV_SIZE);
	rm_command_out = (void *)(gMem.buffer + IV_SIZE);
	rm_command_in = (void *)m_swap_mem.buffer;
	if (!rm_command_in) {
		antiroot_error("response kmalloc failed!\n");
		return TEEC_ERROR_OUT_OF_MEMORY; /*lint !e570*/
	}
	rm_command = (struct RAGENT_COMMAND *)rm_command_in;
	rsp = &(rm_command->content.response);
	rm_command->magic = MAGIC;
	rm_command->version = VERSION;
	rm_command->interface = ROOTAGENT_RESPONSE_ID;
	rsp->proc_integrated = response->proc_integrated;
	rsp->noop = response->noop;
	rsp->runtime_white_list.selinux = response->runtime_white_list.selinux;
	rsp->runtime_white_list.proclength = root_proc->length;
	rsp->runtime_white_list.setid = response->runtime_white_list.setid;
	s_ret = memcpy_s(rsp->runtime_white_list.kcodes,
			KERNEL_CODE_LENGTH_SHA,
			response->runtime_white_list.kcodes,
			KERNEL_CODE_LENGTH_SHA);
	if (EOK != s_ret) {
		antiroot_error("feedback_response _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memcpy_s(rsp->runtime_white_list.syscalls,
			SYSTEM_CALL_LENGTH_SHA,
			response->runtime_white_list.syscalls,
			SYSTEM_CALL_LENGTH_SHA);
	if (EOK != s_ret) {
		antiroot_error("feedback_response _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	s_ret = memcpy_s(rsp->runtime_white_list.sehooks,
			SELINUX_HOOKS_LENGTH_SHA,
			response->runtime_white_list.sehooks,
			SELINUX_HOOKS_LENGTH_SHA);
	if (EOK != s_ret) {
		antiroot_error("feedback_response _s fail. line=%d, s_ret=%d.\n",
				__LINE__, s_ret);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	if ((NULL != root_proc->procid)
			&& (root_proc->length > 0)
			&& (root_proc->length < ROOTAGENT_RPROCS_MAX_LENGTH)) {
		size = sizeof(struct RAGENT_COMMAND) + root_proc->length;
		if (size > (ANTIROOT_SRC_LEN)) {
			antiroot_error("response is oom!!\n");
			return TEEC_ERROR_OUT_OF_MEMORY; /*lint !e570*/
		}
		s_ret = memcpy_s((char *)rm_command + sizeof(struct RAGENT_COMMAND),
				RM_PRE_ALLOCATE_SIZE - sizeof(struct RAGENT_COMMAND),
				root_proc->procid, root_proc->length);
		if (EOK != s_ret) {
			antiroot_error("feedback_response _s fail. line=%d, s_ret=%d.\n",
					__LINE__, s_ret);
			return TEEC_ERROR_GENERIC; /*lint !e570*/
		}
	}
	ret = do_aes256_cbc(rm_command_out, rm_command_in, gMem.buffer,
		m_nounce, ANTIROOT_SRC_LEN, ENCRYPT);
	if (ret) {
		antiroot_error("do_aes256_cbc failed, ret = %d.\n", ret);
		return (TEEC_Result)TEEC_ERROR_GENERIC;
	}
	result = TEEK_InvokeCommand(&gSession,
		ROOTAGENT_RESPONSE_ID, &operation, &origin);
	if (TEEC_SUCCESS != result) {
		antiroot_error("Feedback_response failed result = 0x%x\n",
				result);
	} else if (REV_NOT_ROOT != operation.params[1].value.a) {
		antiroot_debug(ROOTAGENT_DEBUG_ERROR,
				"feedback_response failed due to R!\n");
		root_flag = operation.params[1].value.a;
		result =  TEE_ERROR_ANTIROOT_RSP_FAIL; /*lint !e570*/
		tee_scan_status = operation.params[1].value.b;
	} else {
		tee_scan_status = operation.params[1].value.b;
	}
	return result;
}
static TEEC_Result eima_meminit(TEEC_Operation *operation)
{
    int s_ret;

    if (!gInited || !(gMem.buffer)) {
		antiroot_error("eima Agent should be initialized first!");
		return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
	}

    if(RM_PRE_ALLOCATE_SIZE != gMem.size)
    {
        return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
    }

	s_ret = memset_s(gMem.buffer, RM_PRE_ALLOCATE_SIZE, 0x0, gMem.size);
	if (EOK != s_ret) {
		antiroot_error("eima_request_challenge error.memset_s.line=%d, s_ret=%d.", __LINE__, s_ret);
		return TEEC_ERROR_SECURITY;/*lint !e570*/
	}

    if(RM_PRE_ALLOCATE_SIZE != m_swap_mem.size)
    {
        return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
    }

	s_ret = memset_s(m_swap_mem.buffer, RM_PRE_ALLOCATE_SIZE, 0x0, m_swap_mem.size);
	if (EOK != s_ret) {
		antiroot_error("eima_request_challenge error.memset_s.line=%d, s_ret=%d.", __LINE__, s_ret);
		return TEEC_ERROR_SECURITY;/*lint !e570*/
	}

	s_ret = memset_s(operation, sizeof(TEEC_Operation), 0, sizeof(TEEC_Operation));
	if (EOK != s_ret) {
		antiroot_error("eima_request_challenge error.memset_s.line=%d, s_ret=%d.", __LINE__, s_ret);
		return TEEC_ERROR_SECURITY;/*lint !e570*/
	}
    return TEEC_SUCCESS;
}

TEEC_Result eima_request_challenge(struct EIMA_AGENT_CHALLENGE *echallenge) {

	uint32_t origin;
	TEEC_Result result;
	TEEC_Operation operation;
	char *cm_in; /*ca*/
	char *cm_out; /*ta*/
	struct RAGENT_COMMAND *cm;
	int s_ret;
	int ret;
	uint8_t nounce[CHALLENGE_NOUNCE_LENGTH] = {0};

	ret = eima_meminit(&operation);
	if (TEEC_SUCCESS != ret) {
		antiroot_error("eima_meminit faild!. ret=%d.", ret);
		return TEEC_ERROR_SECURITY;/*lint !e570*/
	}

	operation.started = 1;
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT, TEEC_VALUE_OUTPUT,
		TEEC_NONE, TEEC_NONE);
	operation.params[0].tmpref.buffer = gMem.buffer;
	operation.params[0].tmpref.size = gMem.size;

	get_random_bytes(gMem.buffer, IV_SIZE);
	cm_out = (char*)(gMem.buffer + IV_SIZE);
	cm_in = (char*)(m_swap_mem.buffer);

	cm = (struct RAGENT_COMMAND*)cm_out;
	cm->magic = MAGIC;
	cm->version = VERSION;
	cm->interface = EIMA_CHALLENGE;

	get_random_bytes(nounce, CHALLENGE_NOUNCE_LENGTH);
	s_ret = memcpy_s(cm->content.eima_challenge.nounce, CHALLENGE_NOUNCE_LENGTH,
		nounce, CHALLENGE_NOUNCE_LENGTH);
	if (EOK != s_ret) {
		antiroot_error("eima_request_challenge error.memset_s.line=%d, s_ret=%d.", __LINE__, s_ret);
		return TEEC_ERROR_SECURITY; /*lint !e570*/
	}

	result = TEEK_InvokeCommand(&gSession, EIMA_CHALLENGE, &operation, &origin);
	if (TEEC_SUCCESS != result) {
		antiroot_error("eima_request_challenge failed!");
	} else if (REV_NOT_ROOT != operation.params[1].value.a) {
		antiroot_error("eima_request_challenge failed due to rooted!");
		root_flag = operation.params[1].value.a;
		tee_scan_status = operation.params[1].value.b;
		result = TEE_ERROR_ANTIROOT_RSP_FAIL; /*lint !e570*/
	} else {
		tee_scan_status = operation.params[1].value.b;
		ret = do_aes256_cbc((u8*)cm_in, (u8*)cm_out, gMem.buffer,
			nounce, ANTIROOT_SRC_LEN, DECRYPT);
		if (ret) {
			antiroot_error("do_aes256_cbc failed,ret = %d!", ret);
			return (TEEC_Result)TEEC_ERROR_GENERIC;
		}
		s_ret = memset_s(eima_nounce, CHALLENGE_NOUNCE_LENGTH, 0x0, CHALLENGE_NOUNCE_LENGTH);
		if (EOK != s_ret) {
			antiroot_error("eima_request_challenge failed.line = %d, s_ret = %d", __LINE__, s_ret);
			return TEEC_ERROR_SECURITY; /*lint !e570*/
		}
		cm = (struct RAGENT_COMMAND *)cm_in;
		s_ret = memcpy_s(eima_nounce, CHALLENGE_NOUNCE_LENGTH,
			cm->content.eima_challenge.nounce, CHALLENGE_NOUNCE_LENGTH);

		if (EOK != s_ret) {
			antiroot_error("eima_request_challenge failed.line = %d, s_ret = %d", __LINE__, s_ret);
			return TEEC_ERROR_GENERIC; /*lint !e570*/
		}
		antiroot_debug(ROOTAGENT_DEBUG_API,"eima_request_challenge successful");
	}
	return result;
}

static inline bool sanity_check_len(struct m_entry *entry, uint16_t index, uint32_t total) {

	uint16_t entry_total;

	entry_total = sizeof(struct m_entry) - sizeof(entry->fn) + entry->fn_len;
	return entry_total > UINT16_MAX - index || total < index + entry_total;
}

static int parase_msgentry(char *buf, uint16_t *index, struct m_entry *entry) {

	uint16_t tmp_index;
	errno_t s_ret;
	uint16_t fn_len;
	if (NULL == buf || NULL == index || NULL == entry) {
		antiroot_error("eima parase_msgentry:parase_msgentry: bad params!");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	tmp_index = *index;
	/*judg the length*/
	if (sanity_check_len(entry, tmp_index, ANTIROOT_SRC_LEN)) {
		antiroot_error("parase_msgentry: length is error!");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	/*copy type of struct msglist*/
	if (entry->type != 0 && entry->type != 1) {
		antiroot_error("parase_msgentry: failed! type is %d", entry->type);
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	*(uint8_t *)(buf + tmp_index) = entry->type;
	tmp_index += sizeof(uint8_t);

	/*copy hash len of struct msglist*/
	*(uint8_t *)(buf + tmp_index) = entry->hash_len;
	tmp_index += sizeof(uint8_t);

	/*copy hash  of struct msglist*/
	s_ret = memcpy_s(buf + tmp_index, sizeof(entry->hash), entry->hash, sizeof(entry->hash));
	if (EOK != s_ret) {
		antiroot_error("parase_msgentry: copy failed! s_ret = %d",s_ret);
		return TEEC_ERROR_SECURITY;
	}
	tmp_index += sizeof(entry->hash);

	/*copy file_len len of struct msglist*/
	fn_len = entry->fn_len;
	if (0 == fn_len || fn_len > 256) {
		antiroot_error("parase_msgentry: fn len error! fn_len = 0x%x", fn_len);
		return TEEC_ERROR_SECURITY;
	}
	*(uint16_t *)(buf + tmp_index) = fn_len;
	tmp_index += sizeof(uint16_t);

	/*copy file context of struct msglist*/
	s_ret = memcpy_s(buf + tmp_index, 256, entry->fn, fn_len);
	if (EOK != s_ret) {
		antiroot_error("parase_msgentry: failed! s_ret = %d",s_ret);
		return TEEC_ERROR_SECURITY;
	}
	tmp_index += fn_len;
	*index = tmp_index;
	return TEEC_SUCCESS;
}

static TEEC_Result eima_package_data(struct m_list_msg *rsp, uint8_t *cm) {

	TEEC_Result ret;
	uint16_t index = sizeof(struct RAGENT_COMMAND);
	uint32_t i;
	struct m_entry *entry;
	errno_t s_ret;

	if (NULL == rsp || NULL == cm) {
		antiroot_error("eima package data bad params!");
		return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
	}

	if (rsp->num > EIMA_MAX_MEASURE_OBJ_CNT || 0 == rsp->num) {
		antiroot_error("eima package data bad param! num = %d", rsp->num);
		return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
	}

	s_ret = memcpy_s(cm + index, UNAME_LENTH, rsp->usecase, UNAME_LENTH - 1);
	if (EOK != s_ret) {
		antiroot_error("eima_copy failed! s_ret = 0x%x", s_ret);
		return TEEC_ERROR_SECURITY; /*lint !e570*/
	}

	index += UNAME_LENTH;

	*((uint32_t*) (cm + index)) = rsp->num;

	index += sizeof(uint32_t);

	for (i = 0; i < rsp->num; i++) {

		entry = rsp->m_list + i;
		ret = parase_msgentry(cm, &index, entry);
		if (TEEC_SUCCESS != ret) {
			antiroot_error("eima_package_data: parse the data error, ret = 0x%x", ret);
			return ret;
		}
	}
	return TEEC_SUCCESS;
}

TEEC_Result eima_send_response(int type,struct m_list_msg * response) {
	uint32_t origin;
	TEEC_Result result;
	TEEC_Operation operation;
	void *cm_out;
	void *cm_in;
	struct RAGENT_COMMAND *cm;
	int ret;

    if (!response) {
		antiroot_error("eima response Bad params!");
		return TEEC_ERROR_BAD_PARAMETERS; /*lint !e570*/
	}

	ret = eima_meminit(&operation);
	if (TEEC_SUCCESS != ret) {
		antiroot_error("eima_meminit faild!. ret=%d.", ret);
		return TEEC_ERROR_SECURITY; /*lint !e570*/
	}

	operation.started = 1;
	operation.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INOUT, TEEC_VALUE_OUTPUT,
		TEEC_NONE, TEEC_NONE);
	operation.params[0].tmpref.buffer = gMem.buffer;
	operation.params[0].tmpref.size = gMem.size;

	get_random_bytes(gMem.buffer, IV_SIZE);
	cm_out =(void*)(gMem.buffer + IV_SIZE);
	cm_in = (void*)m_swap_mem.buffer;
	if (!cm_in) {
		antiroot_error("eima response kmalloc failed!");
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	cm = (struct RAGENT_COMMAND*)cm_in;
	cm->magic =	MAGIC;
	cm->version = VERSION;
	cm->interface = EIMA_RESPONSE;
	cm->content.eima_response.msg_type = type;
	result = eima_package_data(response, (uint8_t *)cm);
	if (result != TEEC_SUCCESS) {
		antiroot_error("eima_response failed!result = 0x%x!",result);
		return TEEC_ERROR_GENERIC; /*lint !e570*/
	}
	ret = do_aes256_cbc(cm_out, cm_in, gMem.buffer,
		eima_nounce, ANTIROOT_SRC_LEN, ENCRYPT);
	if (ret) {
		antiroot_error("do_aes256_cbc failed,ret = %d.",ret);
		return (TEEC_Result)TEEC_ERROR_GENERIC;
	}
	result = TEEK_InvokeCommand(&gSession, EIMA_RESPONSE, &operation, &origin);
	if (TEEC_SUCCESS != result) {
		antiroot_error("eima_response failed!result = 0x%x",result);
	} else {
		tee_scan_status = operation.params[1].value.b;
		if (REV_NOT_ROOT != operation.params[1].value.a) {
			antiroot_debug(ROOTAGENT_DEBUG_ERROR,"eima_response failed due to rooted!");
			root_flag = operation.params[1].value.a;
			tee_scan_status = operation.params[1].value.b;
			result = TEE_ERROR_ANTIROOT_RSP_FAIL; /*lint !e570*/
		}

	}

	return result;
}
