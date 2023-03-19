#ifndef __PROCESS_RECLAIM_INFO_H
#define __PROCESS_RECLAIM_INFO_H

extern bool process_reclaim_need_abort(struct mm_walk *walk);
extern struct reclaim_result *process_reclaim_result_cache_alloc(gfp_t gfp);
extern void process_reclaim_result_cache_free(struct reclaim_result *result);
extern int process_reclaim_result_read(struct seq_file *m,
					struct pid_namespace *ns,
					struct pid *pid,
					struct task_struct *tsk);
extern void process_reclaim_result_write(struct task_struct *task,
					unsigned nr_reclaimed,
					unsigned nr_writedblock,
					s64 elapsed_centisecs64);

#endif /* __PROCESS_RECLAIM_INFO_H */
