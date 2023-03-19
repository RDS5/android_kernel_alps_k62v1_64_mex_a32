#ifndef _ROOT_AGENT_H
#define _ROOT_AGENT_H

enum tee_rs_mask {
	ROOTSTATE_BIT   = 0,            /* 0    on */
	/* read from fastboot */
	OEMINFO_BIT,                    /* 1    on */
	FBLOCK_YELLOW_BIT,              /* 2    on */
	FBLOCK_RED_BIT,                 /* 3    on */
	FBLOCK_ORANGE_BIT,              /* 4    on */
					/* 5    off */
	/* dy scan result */
	KERNELCODEBIT   = 6,            /* 6    on */
	SYSTEMCALLBIT,                  /* 7    on */
	ROOTPROCBIT,                    /* 8    on */
	SESTATUSBIT,                    /* 9    on */
	SEHOOKBIT       = 10,           /* 10   on */
	SEPOLICYBIT,                    /* 11   off */
	PROCINTERBIT,                   /* 12   off */
	FRAMINTERBIT,                   /* 13   off */
	INAPPINTERBIT,                  /* 14   off */
	NOOPBIT        = 15,            /* 15   on */
	ITIMEOUTBIT,                    /* 16   on */
	EIMABIT,                        /*17    on */
	SETIDBIT,                       /* 18   on */
	CHECKFAILBIT,                   /* 19   on */
};

/**
 * get_tee_status - provide the tee status for caller
 *
 * return the tee status, 0 for not root, other for rooted
 */
uint32_t get_tee_status(void);

#endif
