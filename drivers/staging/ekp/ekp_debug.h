#ifndef __EKP_DEBUG_H
#define __EKP_DEBUG_H

#ifdef CONFIG_EKP_DEBUGFS
extern struct dentry *ekp_debugfs;

#ifdef CONFIG_SECURITY_EKP
void __init ekp_lsm_debugfs_init(void);
#else
static inline void ekp_lsm_debugfs_init(void) { }
#endif

#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
bool check_ekp_security(struct task_struct *task);
#endif

#ifdef CONFIG_EKP_DEBUGFS_KERNEL_INFO
bool cpu_is_ro_table(unsigned long addr);
void cpu_pt_dump(pgd_t *pgd);
void __weak arch_module_s2_info_prepare(void);
#endif

bool cpu_pgd_walk_for_checking_pxn(pgd_t *pgd);
void cpu_report(void);

#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS
void __init ekp_test_helpers_init(void);
#else
static inline void ekp_test_helpers_init(void) { }
#endif

#ifdef CONFIG_EKP_DEBUGFS_TEST_HELPERS_FOR_HACKING
/* For {swapper,user}_modify_{pgd,pmd,pte} entries under ekp debugfs */
enum EKP_TEST_PT_LEVEL {
        PGD_LEVEL = 0,
        PMD_LEVEL,
        PTE_LEVEL,
};
struct ekp_test_pt_modify {
        pgd_t *pgd;
        enum EKP_TEST_PT_LEVEL level;
        unsigned long addr;
        bool set;
        u64 val;
};

void cpu_test_pt_modify(struct ekp_test_pt_modify *info);
void cpu_test_attack_ops(unsigned long target);
#endif
#endif	/* CONFIG_EKP_DEBUGFS */

#endif	/* __EKP_DEBUG_H */
