#ifndef __DSM_IOCACHE_H
#define __DSM_IOCACHE_H
#include <dsm/dsm_pub.h>

extern struct dsm_client *f2fs_dclient;

#define MSG_MAX_SIZE 200
#define DSM_IOCACHE_LOG(no, fmt, a...) \
	do { \
		char msg[MSG_MAX_SIZE]; \
		snprintf(msg, MSG_MAX_SIZE-1, fmt, ## a); \
		if (f2fs_dclient && !dsm_client_ocuppy(f2fs_dclient)) { \
			dsm_client_record(f2fs_dclient, msg); \
			dsm_client_notify(f2fs_dclient, no); \
		} \
	} while (0)

#endif
