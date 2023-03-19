#ifndef _EIMA_API_H_
#define _EIMA_API_H_

#include "rootagent_common.h"
#include "rootagent_api.h"

extern int antiroot_sr_flag;
extern struct mutex eima_ca_lock;

int send_eima_data(int type, struct m_list_msg *mlist_msg);

#endif
