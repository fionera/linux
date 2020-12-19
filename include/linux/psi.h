#ifndef _LINUX_PSI_H
#define _LINUX_PSI_H

#include <linux/psi_types.h>
#include <linux/sched.h>

struct seq_file;
struct css_set;
struct mem_cgroup;

#ifdef CONFIG_PSI

extern bool psi_disabled;

void psi_init(void);

void psi_task_change(struct task_struct *task, int clear, int set);

void psi_memstall_tick(struct task_struct *task, int cpu);
void psi_memstall_enter(unsigned long *flags);
void psi_memstall_leave(unsigned long *flags);

int psi_show(struct seq_file *s, struct psi_group *group, enum psi_res res);
int get_psi_info(unsigned long (*avg)[3], u64 *total);

#ifdef CONFIG_MEMCG
int psi_mem_cgroup_alloc(struct mem_cgroup *memcg);
void psi_mem_cgroup_free(struct mem_cgroup *memcg);
void cgroup_move_task(struct task_struct *p, struct css_set *to);

#else
static inline void cgroup_move_task(struct task_struct *p, struct css_set *to)
{
	rcu_assign_pointer(p->cgroups, to);
}
#endif /* CONFIG_MEMCG */

#else /* CONFIG_PSI */

static inline void psi_init(void) {}

static inline void psi_memstall_enter(unsigned long *flags) {}
static inline void psi_memstall_leave(unsigned long *flags) {}

static inline int psi_mem_cgroup_alloc(struct mem_cgroup *memcg)
{
	return 0;
}
static inline void psi_mem_cgroup_free(struct mem_cgroup *memcg)
{
}
static inline void cgroup_move_task(struct task_struct *p, struct css_set *to)
{
	rcu_assign_pointer(p->cgroups, to);
}

static inline int get_psi_info(unsigned long (*avg)[3], u64 *total)
{
	return 0;
}
#endif /* CONFIG_PSI */

#endif /* _LINUX_PSI_H */
