/*
 * EKP LSM hooks
 *
 * Copyright (C) 2018 LG Electronics Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 */

#include <linux/binfmts.h>
#include <linux/cred.h>
#include <linux/debugfs.h>
#include <linux/ekp.h>
#include <linux/fs.h>
#include <linux/lsm_hooks.h>
#include <linux/mount.h>
#include <linux/path.h>
#include <linux/rbtree.h>
#include <linux/rcupdate.h>
#include <linux/uidgid.h>
#include <linux/romempool.h>

#include "ekp_debug.h"

enum ekp_lsm_violation {
	EKP_BPRM_COMMIT_CREDS_VIOLATION = 0,
	EKP_TASK_FIX_SETUID_VIOLATION,
	EKP_TASK_FIX_SETGID_VIOLATION,
	EKP_TASK_CREATE_VIOLATION,
	EKP_LKM_FROM_FILE_VIOLATION,
	EKP_LSM_VIOLATION_MAX
};

#ifdef CONFIG_EKP_VIOLATION_FAULT_NOTIFIER
#include <linux/fault_notifier.h>

static uint64_t ekp_lsm_fault_count[EKP_LSM_VIOLATION_MAX];

#define ekp_lsm_fault_handler(v, r) \
	__ekp_lsm_fault_handler(v, __stringify(v), r)

static int __ekp_lsm_fault_handler(enum ekp_lsm_violation v,
				   const char *msg, const char *reason)
{
	if (ekp_lsm_fault_count[v] == ~0UL)
		ekp_lsm_fault_count[v] = 0;

	ekp_lsm_fault_count[v]++;

	if (reason) {
		fn_kernel_notify(FN_TYPE_ALL, "%s [%lu] : %s",
				 msg, ekp_lsm_fault_count[v], reason);
	} else {
		fn_kernel_notify(FN_TYPE_ALL, "%s [%lu]",
				 msg, ekp_lsm_fault_count[v]);
	}

	WARN_ON(1);

	return 0;
}
#else
static inline int ekp_lsm_fault_handler(enum ekp_lsm_violation v, const char *reason)
{
	return 1;
}
#endif

#ifdef CONFIG_SECURITY_EKP_MODULE_VALIDATION
static inline bool mnt_root_is_valid(unsigned long data)
{
	return !ekp_tunnel(EKP_CHECK_MOD_ROOT, data,  0, 0, 0, 0, 0);
}

#ifdef CONFIG_WEBOS
static bool root_inited = false;

static int ekp_sb_pivotroot(struct path *old_path, struct path *new_path)
{
	if (root_inited)
		return 0;

	if (!__mnt_is_readonly(old_path->mnt))
		return 0;

	if (mnt_root_is_valid((unsigned long)dget(new_path->mnt->mnt_root)))
		root_inited = true;

	return 0;
}
#endif

static int ekp_kernel_module_from_file(struct file *file)
{
	if (!file || !file->f_path.mnt || !file->f_path.mnt->mnt_root) {
		ekp_lsm_fault_handler(EKP_LKM_FROM_FILE_VIOLATION,
				      "Invalid file");
		return -EPERM;
	}

	if (!mnt_root_is_valid((unsigned long)file->f_path.mnt->mnt_root)) {
		ekp_lsm_fault_handler(EKP_LKM_FROM_FILE_VIOLATION,
				      "Invalid mnt_root");
		return -EPERM;
	}

	return 0;
}
#endif

#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
static uid_t jail_uid_begin = 5000;
static uid_t jail_uid_end = 7000;
static gid_t jail_gid = 5000;

static bool is_prisoner_id(const struct cred *cred)
{
	struct user_namespace *ns = current_user_ns();

	/* "UID_BEGIN <= current uid < UID_END" means "I'm in the jail". */
	return uid_gte(cred->uid, make_kuid(ns, jail_uid_begin)) &&
	       uid_lt(cred->uid, make_kuid(ns, jail_uid_end)) &&
	       gid_eq(cred->gid, make_kgid(ns, jail_gid));
}

static inline kuid_t root_uid(void)
{
	return make_kuid(current_user_ns(), 0);
}

static inline kgid_t root_gid(void)
{
	return make_kgid(current_user_ns(), 0);
}

static void reset_uids(const struct cred *old, struct cred *new)
{
	pr_warn("EKP:\tfixup UIDs (%d,%d,%d,%d) ==> (%d,%d,%d,%d)\n",
		__kuid_val(new->uid), __kuid_val(new->euid),
		__kuid_val(new->suid), __kuid_val(new->fsuid),
		__kuid_val(old->uid), __kuid_val(old->euid),
		__kuid_val(old->suid), __kuid_val(old->fsuid));

	/* Reset new UIDs to old ones forcedly */
	new->uid = old->uid;
	new->euid = old->euid;
	new->suid = old->suid;
	new->fsuid = old->fsuid;
}

static void reset_gids(const struct cred *old, struct cred *new)
{
	pr_warn("EKP:\tfixup GIDs (%d,%d,%d,%d) ==> (%d,%d,%d,%d)\n",
		__kgid_val(new->gid), __kgid_val(new->egid),
		__kgid_val(new->sgid), __kgid_val(new->fsgid),
		__kgid_val(old->gid), __kgid_val(old->egid),
		__kgid_val(old->sgid), __kgid_val(old->fsgid));

	/* Reset new GIDs to old ones forcedly */
	new->gid = old->gid;
	new->egid = old->egid;
	new->sgid = old->sgid;
	new->fsgid = old->fsgid;
}

static void ekp_bprm_committing_creds(struct linux_binprm *bprm)
{
	const struct cred *old = current_cred();
	struct cred *new = bprm->cred;

	if (!is_prisoner_id(old))
		return;

	if (uid_eq(new->euid, root_uid())) {
		pr_warn("EKP: a prisoner cannot execute %s with (owner=root, perm=S_ISUID)\n",
			bprm->filename);
		reset_uids(old, new);
		ekp_lsm_fault_handler(EKP_BPRM_COMMIT_CREDS_VIOLATION,
				      "Invalid EUID");
	}

	if (gid_eq(new->egid, root_gid())) {
		pr_warn("EKP: a prisoner cannot execute %s with (group=root, perm=S_ISGID)\n",
			bprm->filename);
		reset_gids(old, new);
		ekp_lsm_fault_handler(EKP_BPRM_COMMIT_CREDS_VIOLATION,
				      "Invalid EGID");
	}
}

/*
 * TODO:
 * Mainline kernel can invoke multiple hooks, but only support one
 * cred->security pointer. In order words, if EKP LSM uses cred->security,
 * any other LSMs cannot use it.
 */
struct ekp_security_struct {
	bool prisoner;
	struct task_struct *task;
	kuid_t original_uid;
};

static struct kmem_cache *ess_cache;

static bool is_cap_cleared(const struct cred *cred)
{
	if (!cap_isclear(cred->cap_inheritable) ||
	    !cap_isclear(cred->cap_permitted) ||
	    !cap_isclear(cred->cap_effective) ||
	    !cap_isclear(cred->cap_ambient))
		return false;
	return true;
}

static void show_caps(kernel_cap_t old, kernel_cap_t new)
{
	unsigned int __capi;

	CAP_FOR_EACH_U32(__capi)
		printk(KERN_CONT "%08x", old.cap[CAP_LAST_U32 - __capi]);
	printk(KERN_CONT " ==> ");
	CAP_FOR_EACH_U32(__capi)
		printk(KERN_CONT "%08x", new.cap[CAP_LAST_U32 - __capi]);
	printk(KERN_CONT "\n");
}

static int ekp_task_create(unsigned long clone_flags)
{
	const struct cred *old = current_cred();
	struct cred *new;
	struct ekp_security_struct *ess = old->security;

	if (!ekp_is_romem(__pa(ess)))
		panic("EKP LSM security pointer is writable.\n");
#ifdef CONFIG_EKP_CRED_PROTECTION
	if (ess->task != current)
		panic("EKP security pointer is not an original one.\n");
#endif

	/* This task is not a prisoner. */
	if (!ess->prisoner)
		return 0;
	/* This task is a prisoner, but it is still in the jail. */
	if (is_prisoner_id(old) && is_cap_cleared(old))
		return 0;

	ekp_lsm_fault_handler(EKP_TASK_CREATE_VIOLATION, NULL);

	pr_warn("EKP: An escaped prisoner is brought back\n");
	pr_warn("EKP:\t%s (PID: %d, UID: %d, GID: %d)\n",
		current->comm, current->pid, __kuid_val(task_uid(current)),
		__kgid_val(task_cred_xxx(current, gid)));

	new = prepare_creds();
	if (!new)
		return -ENOMEM;

	new->uid = ess->original_uid;
	new->euid = new->suid = new->fsuid = new->uid;

	new->gid = make_kgid(current_user_ns(), jail_gid);
	new->egid = new->sgid = new->fsgid = new->gid;

	/*
	 * TODO: This "clearing capabilities forcedly" can be available
	 * since we assume that current system doesn't support any
	 * file capabilities. If not, this should be re-considered.
	 */
	cap_clear(new->cap_inheritable);
	cap_clear(new->cap_permitted);
	cap_clear(new->cap_effective);
	cap_clear(new->cap_ambient);

	pr_warn("EKP:\tfixup UIDs (%d,%d,%d,%d) ==> (%d,%d,%d,%d)\n",
		__kuid_val(old->uid), __kuid_val(old->euid),
		__kuid_val(old->suid), __kuid_val(old->fsuid),
		__kuid_val(new->uid), __kuid_val(new->euid),
		__kuid_val(new->suid), __kuid_val(new->fsuid));
	pr_warn("EKP:\tfixup GIDs (%d,%d,%d,%d) ==> (%d,%d,%d,%d)\n",
		__kgid_val(old->gid), __kgid_val(old->egid),
		__kgid_val(old->sgid), __kgid_val(old->fsgid),
		__kgid_val(new->gid), __kgid_val(new->egid),
		__kgid_val(new->sgid), __kgid_val(new->fsgid));
	pr_warn("EKP:\tfixup capabilities\n");
	pr_warn("EKP:\t\tCapInh:\t");
	show_caps(old->cap_inheritable, new->cap_inheritable);
	pr_warn("EKP:\t\tCapPrm:\t");
	show_caps(old->cap_permitted, new->cap_permitted);
	pr_warn("EKP:\t\tCapEff:\t");
	show_caps(old->cap_effective, new->cap_effective);
	pr_warn("EKP:\t\tCapBnd:\t");
	show_caps(old->cap_bset, new->cap_bset);
	pr_warn("EKP:\t\tCapAmb:\t");
	show_caps(old->cap_ambient, new->cap_ambient);

	return commit_creds(new);
}

static void ekp_cred_free(struct cred *cred)
{
	struct ekp_security_struct *ess = cred->security;

	BUG_ON(!ess);
	BUG_ON(!ekp_is_romem(__pa(ess)));
	kmem_cache_free(ess_cache, ess);

	/* cred->security = NULL */
	ekp_clear_romem(__pa(&(cred->security)), sizeof(void *));
	BUG_ON(cred->security != NULL);
}

static int set_ekp_security(struct cred *new, const struct cred *old,
			    gfp_t gfp, bool arrestment)
{
	struct ekp_security_struct *ess, *ess_old = NULL;
	struct ekp_security_struct temp;
	long ret;

	ess = new->security;
	if (!ess) {
		ess = kmem_cache_zalloc(ess_cache, gfp);
		if (!ess)
			return -ENOMEM;
	}

	if (old)
		ess_old = old->security;
	if (ess_old)
		memcpy(&temp, ess_old, sizeof(struct ekp_security_struct));
	else {
		temp.prisoner = arrestment;
		temp.task = current;
		temp.original_uid = new->uid;
	}

	ret = ekp_tunnel(EKP_CRED_SECURITY, __pa(ess), __pa(&temp),
			 sizeof(struct ekp_security_struct), 0, 0, 0);
	if (ret != 0)
		return ret;

	if (!new->security)
		new->security = ess;

	return 0;
}

static int ekp_cred_prepare(struct cred *new, const struct cred *old,
			    gfp_t gfp)
{
	return set_ekp_security(new, old, gfp, false);
}

static int arrestment(struct cred *cred)
{
#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION_LOG
	pr_info("EKP: This process is a suspect.\n");
	pr_info("EKP:\t%s (PID: %d, UID: %d, GID: %d) ==> (UID: %d, GID: %d)\n",
		current->comm, current->pid, __kuid_val(task_uid(current)),
		__kgid_val(task_cred_xxx(current, gid)),
		__kuid_val(cred->uid), __kgid_val(cred->gid));
#endif

	BUG_ON(!cred->security);

	return set_ekp_security(cred, NULL, 0, true);
}

static void show_jailbreaker(struct cred *new)
{
	pr_warn("EKP: This prisoner attempts to break out of the jail.\n");
	pr_warn("EKP:\t%s (PID: %d, UID: %d,%d,%d,%d, GID: %d,%d,%d,%d)\n",
		current->comm, current->pid,
		__kuid_val(new->uid), __kuid_val(new->euid),
		__kuid_val(new->suid), __kuid_val(new->fsuid),
		__kgid_val(new->gid), __kgid_val(new->egid),
		__kgid_val(new->sgid), __kgid_val(new->fsgid));
}

static int ekp_task_fix_setuid(struct cred *new, const struct cred *old,
			       int flags)
{
	bool try_to_escape = false;
	const char *reason = NULL;

	if (!is_prisoner_id(old)) {
		if (is_prisoner_id(new))
			return arrestment(new);
		return 0;
	}

	/*
	 * Even if a prisoner has CAP_SETUID, "0" (which means root privilege)
	 * is not allowed for new UID/EUID/SUID/FSUID.
	 */
	switch (flags) {
	case LSM_SETID_RES:
		if (uid_eq(new->suid, root_uid())) {
			try_to_escape = true;
			reason = "Invalid SUID";
		}
		/* fallback to LSM_SETID_{RE,ID} to also check UID/EUID */
	case LSM_SETID_RE:
		if (uid_eq(new->euid, root_uid())) {
			try_to_escape = true;
			reason = "Invalid EUID";
		}
		/* fallback to LSM_SETID_ID to also check UID */
	case LSM_SETID_ID:
		if (uid_eq(new->uid, root_uid())) {
			try_to_escape = true;
			reason = "Invalid UID";
		}
		break;
	case LSM_SETID_FS:
		if (uid_eq(new->fsuid, root_uid())) {
			try_to_escape = true;
			reason = "Invalid FSUID";
		}
		break;
	default:
		return -EINVAL;
	}

	if (try_to_escape) {
		show_jailbreaker(new);
		reset_uids(old, new);
		ekp_lsm_fault_handler(EKP_TASK_FIX_SETUID_VIOLATION, reason);
	}

	/*
	 * We've already dropped "root" privilege forcedly for a prisoner,
	 * so we don't need to return -EPERM.
	 */
	return 0;
}

static int ekp_task_fix_setgid(struct cred *new, const struct cred *old,
			       int flags)
{
	bool try_to_escape = false;
	const char *reason = NULL;

	if (!is_prisoner_id(old))
		return 0;

	/*
	 * Even if a prisoner has CAP_SETGID, "0" (which means root privilege)
	 * is not allowed for new GID/EGID/SGID/FSGID.
	 */
	switch (flags) {
	case LSM_SETID_RES:
		if (gid_eq(new->sgid, root_gid())) {
			try_to_escape = true;
			reason = "Invalid SGID";
		}
		/* fallback to LSM_SETID_{RE,ID} to also check GID/EGID */
	case LSM_SETID_RE:
		if (gid_eq(new->egid, root_gid())) {
			try_to_escape = true;
			reason = "Invalid EGID";
		}
		/* fallback to LSM_SETID_ID to also check GID */
	case LSM_SETID_ID:
		if (gid_eq(new->gid, root_gid())) {
			try_to_escape = true;
			reason = "Invalid GID";
		}
		break;
	case LSM_SETID_FS:
		if (gid_eq(new->fsgid, root_gid())) {
			try_to_escape = true;
			reason = "Invalid FSGID";
		}
		break;
	default:
		return -EINVAL;
	}

	if (try_to_escape) {
		show_jailbreaker(new);
		reset_gids(old, new);
		ekp_lsm_fault_handler(EKP_TASK_FIX_SETGID_VIOLATION, reason);
	}

	/*
	 * We've already dropped "root" privilege forcedly for a prisoner,
	 * so we don't need to return -EPERM.
	 */
	return 0;
}

static int __init ekp_lsm_root_restriction_init(void)
{
	/* RO slab cache for struct ekp_security_struct */
	ess_cache = kmem_cache_create("ess_cache",
				      sizeof(struct ekp_security_struct),
				      0, SLAB_PANIC | SLAB_READONLY, NULL);

	/* Initialize security pointer for init task */
	return set_ekp_security((struct cred *)current_cred(), NULL,
				GFP_KERNEL, false);
}

/* /sys/kernel/ekp_lsm */
#ifdef CONFIG_SYSFS
#include <linux/kobject.h>

#define EKP_LSM_ATTR_RO(_name) \
        static struct kobj_attribute _name##_attr = \
                __ATTR(_name, 0400, _name##_show, NULL)

#define EKP_LSM_ATTR(_name) \
        static struct kobj_attribute _name##_attr = \
                __ATTR(_name, 0600, _name##_show, _name##_store)

static ssize_t jail_uid_begin_show(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "%u\n", jail_uid_begin);
}
static ssize_t jail_uid_begin_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	uid_t temp;
	int err;

	err = kstrtouint(buf, 10, &temp);
	if (err || temp > UINT_MAX)
		return -EINVAL;

	jail_uid_begin = temp;

	return count;
}
EKP_LSM_ATTR(jail_uid_begin);

static ssize_t jail_uid_end_show(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "%u\n", jail_uid_end);
}
static ssize_t jail_uid_end_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	uid_t temp;
	int err;

	err = kstrtouint(buf, 10, &temp);
	if (err || temp > UINT_MAX)
		return -EINVAL;

	jail_uid_end = temp;

	return count;
}
EKP_LSM_ATTR(jail_uid_end);

static ssize_t jail_gid_show(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   char *buf)
{
	return sprintf(buf, "%u\n", jail_gid);
}
static ssize_t jail_gid_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	gid_t temp;
	int err;

	err = kstrtouint(buf, 10, &temp);
	if (err || temp > UINT_MAX)
		return -EINVAL;

	jail_gid = temp;

	return count;
}
EKP_LSM_ATTR(jail_gid);

static struct attribute *ekp_lsm_attrs[] = {
	&jail_uid_begin_attr.attr,
	&jail_uid_end_attr.attr,
	&jail_gid_attr.attr,
	NULL,
};

static struct attribute_group ekp_lsm_attr_group = {
	.attrs = ekp_lsm_attrs,
	.name = "ekp_lsm",
};

static int __init ekp_lsm_root_restriction_sysfs_init(void)
{
	return sysfs_create_group(kernel_kobj, &ekp_lsm_attr_group);
}
late_initcall(ekp_lsm_root_restriction_sysfs_init);
#endif	/* CONFIG_SYSFS */
#else
static inline int ekp_lsm_root_restriction_init(void) { return 0; }
#endif	/* CONFIG_SECURITY_EKP_ROOT_RESTRICTION */

static struct security_hook_list ekp_lsm_hooks[] __ro_after_init = {
#ifdef CONFIG_SECURITY_EKP_MODULE_VALIDATION
#ifdef CONFIG_WEBOS
	LSM_HOOK_INIT(sb_pivotroot, ekp_sb_pivotroot),
#endif
	LSM_HOOK_INIT(kernel_module_from_file, ekp_kernel_module_from_file),
#endif
#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
	LSM_HOOK_INIT(bprm_committing_creds, ekp_bprm_committing_creds),
	LSM_HOOK_INIT(task_create, ekp_task_create),
	LSM_HOOK_INIT(cred_free, ekp_cred_free),
	LSM_HOOK_INIT(cred_prepare, ekp_cred_prepare),
	LSM_HOOK_INIT(task_fix_setuid, ekp_task_fix_setuid),
	LSM_HOOK_INIT(task_fix_setgid, ekp_task_fix_setgid),
#endif
};

static int __init ekp_lsm_init(void)
{
	int ret;

	if (!ekp_initialized())
		return 0;

	ret = ekp_lsm_root_restriction_init();
	if (ret < 0) {
		pr_err("EKP: Fail to start EKP Linux Security Module (%d)\n", ret);
		return ret;
	}

	security_add_hooks(ekp_lsm_hooks, ARRAY_SIZE(ekp_lsm_hooks));

	pr_info("EKP: Starting EKP Linux Security Module\n");

	return 0;
}
security_initcall(ekp_lsm_init);

#ifdef CONFIG_EKP_DEBUGFS
#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
bool check_ekp_security(struct task_struct *task)
{
	struct ekp_security_struct *ess = task->cred->security;

	if (!ekp_is_romem(__pa(ess)))
		return false;
#ifdef CONFIG_EKP_CRED_PROTECTION
	if (ess->task != task)
		return false;
#endif
	return true;
}

static int roll_call(void)
{
	struct task_struct *p, *t;
	const struct cred *pcred, *tcred;
	struct ekp_security_struct *ess;
	int count = 0;

	pr_info("EKP: ======================================\n");

	read_lock(&tasklist_lock);
	for_each_process(p) {
		pcred = get_task_cred(p);
		ess = pcred->security;

		if (ess->prisoner) {
			pr_info("EKP: %s (PID: %d, UID: %d, GID: %d)\n",
				p->comm, p->pid, __kuid_val(task_uid(p)),
				__kgid_val(task_cred_xxx(p, gid)));

			for_each_thread(p, t) {
				if (p == t)
					continue;

				tcred = get_task_cred(t);
				ess = tcred->security;

				pr_info("EKP:\t%s (PID: %d, UID: %d, GID: %d)\n",
					t->comm, t->pid, __kuid_val(task_uid(t)),
					__kgid_val(task_cred_xxx(t, gid)));

				put_cred(tcred);

				count++;
			}

			count++;
		}

		put_cred(pcred);
	}
	read_unlock(&tasklist_lock);

	pr_info("EKP: ======================================\n");

	return count;
}

static int root_restriction_roll_call_get(void *data, u64 *val)
{
	/* Return a count of prisoners in the jail. */
	*val = roll_call();
	return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(root_restriction_roll_call_fops,
			root_restriction_roll_call_get, NULL, "%llu\n");
#endif	/* CONFIG_SECURITY_EKP_ROOT_RESTRICTION */

void __init ekp_lsm_debugfs_init(void)
{
	struct dentry *lsm_debugfs;

	lsm_debugfs = debugfs_create_dir("lsm", ekp_debugfs);
	if (IS_ERR_OR_NULL(lsm_debugfs))
		return;

#ifdef CONFIG_SECURITY_EKP_ROOT_RESTRICTION
	/* roll_call */
	debugfs_create_file("root_restriction_roll_call", S_IRUSR,
			    lsm_debugfs, NULL,
			    &root_restriction_roll_call_fops);
#endif
}
#endif	/* CONFIG_EKP_DEBUGFS */
