#ifndef __TEE_RDR_REGISTER_H__
#define __TEE_RDR_REGISTER_H__

#define RDR_MEM_SIZE (SZ_4K * 64)
#define RDR_MEM_ORDER_MAX get_order(RDR_MEM_SIZE)

int tc_ns_register_rdr_mem(void);
int teeos_register_exception(void);
int tee_rdr_init(void);
unsigned long tc_ns_get_rdr_mem_addr(void);
unsigned int tc_ns_get_rdr_mem_len(void);

#endif
