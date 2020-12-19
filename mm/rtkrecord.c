/*
 *  linux/mm/rtkrecord.c
 *
 *  Copyright (C) 1991, 1992, 1993, 1994  Linus Torvalds
 *
 *  Support for blueprint of dvr zone, I-Chieh Hsu
 */

#include <linux/mm.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/rtkrecord.h>
#include <linux/pageremap.h>
#include <linux/sort.h>
#include <linux/auth.h>

static unsigned long rtk_record_start_time = 0;
static struct radix_tree_root	rtk_tree;
static spinlock_t		rtk_tree_lock;
static unsigned long tree_node_count = 0;
static unsigned long tree_node_tag_count[RADIX_TREE_MAX_TAGS];
static struct workqueue_struct *cma_deferfree_workq;
static struct delayed_work delayed_cma_deferfree_work;

static unsigned long current_low_page_total = 0;
unsigned long cma_low_alloc_peak_value = 0;
static unsigned long current_high_page_total = 0;
unsigned long cma_high_alloc_peak_value = 0;

#ifdef CONFIG_HIBERNATION
extern int in_suspend;
#endif
void *dma_get_allocator(struct device *dev);

void rtk_record_init(void)
{
	int i = 0;
	rtk_record_start_time = jiffies;
	INIT_RADIX_TREE(&rtk_tree, GFP_ATOMIC);
	spin_lock_init(&rtk_tree_lock);

	for (i = 0; i < RADIX_TREE_MAX_TAGS; i++)
		tree_node_tag_count[i] = 0;

	if (need_debug_cma_deferfree()) {
		cma_deferfree_workq = create_singlethread_workqueue("cma_deferfree");
		INIT_DELAYED_WORK(&delayed_cma_deferfree_work, rtk_record_defer_free);
		if (cma_deferfree_workq)
			queue_delayed_work(cma_deferfree_workq, &delayed_cma_deferfree_work, msecs_to_jiffies(2000));
	}
}

void rtk_record_tag_set(rtk_record *ptr, unsigned int tag)
{
	spin_lock_irq(&rtk_tree_lock);

	if (ptr) {
		unsigned long addr = ptr->addr;

		radix_tree_tag_set(&rtk_tree, (addr&0x7fffffff) >> 2, tag);
		if (tag == RTK_RECORD_TAG_DEFER_FREE)
			tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE] ++;
		
		ptr->reserved = (ptr->reserved & 0xfffffff0) | 0x00000001; // defer free set
		ptr->jiffies = (jiffies - rtk_record_start_time) + msecs_to_jiffies(RTK_RECORD_DEFER_FREE_TIMEOUT);
	}

	spin_unlock_irq(&rtk_tree_lock);
}

void rtk_record_tag_clear(unsigned long addr, unsigned int tag)
{
	spin_lock_irq(&rtk_tree_lock);

	radix_tree_tag_clear(&rtk_tree, (addr&0x7fffffff) >> 2, tag);
	if (tag == RTK_RECORD_TAG_DEFER_FREE)
		tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE] --;

	spin_unlock_irq(&rtk_tree_lock);
}

static inline void __record_tag_clear(unsigned long addr, unsigned int tag)
{
	radix_tree_tag_clear(&rtk_tree, (addr&0x7fffffff) >> 2, tag);
	if (tag == RTK_RECORD_TAG_DEFER_FREE)
		tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE] --;
}

int rtk_record_tag_get(unsigned long addr, unsigned int tag)
{
	int ret = 0;
	spin_lock_irq(&rtk_tree_lock);

	ret = radix_tree_tag_get(&rtk_tree, (addr&0x7fffffff) >> 2, tag);

	spin_unlock_irq(&rtk_tree_lock);
	return ret;
}

static inline int __record_tag_get(unsigned long addr, unsigned int tag)
{
	int ret = 0;
	ret = radix_tree_tag_get(&rtk_tree, (addr&0x7fffffff) >> 2, tag);
	return ret;
}

int rtk_record_insert(unsigned long addr, unsigned long type, unsigned long priv, const void *caller)
{
	rtk_record *ptr;
	int error;
	struct page *page = pfn_to_page(addr >> PAGE_SHIFT);
	struct timespec ts;
	get_monotonic_boottime(&ts);

	ptr = kmalloc(sizeof(rtk_record), GFP_KERNEL);
	if (unlikely(!ptr))
		return -ENOMEM;
	ptr->addr = addr;
	ptr->type = type;
	ptr->priv = priv;
	ptr->caller = caller;
	ptr->pid = current->pid;
	ptr->tgid = current->tgid;
	ptr->jiffies = jiffies - rtk_record_start_time;
	ptr->reserved = 0;
	addr = (addr&0x7fffffff) >> 2;	// use WORD as minimum unit
	error = radix_tree_preload(GFP_KERNEL);
	if (error == 0) {
		spin_lock_irq(&rtk_tree_lock);
		error = radix_tree_insert(&rtk_tree, addr, ptr);

		if(!PageHighMem(page))
			current_low_page_total += (type & 0x00ffffff);
		else
			current_high_page_total += (type & 0x00ffffff);

		if(cma_low_alloc_peak_value < current_low_page_total)
			cma_low_alloc_peak_value = current_low_page_total;
		if(cma_high_alloc_peak_value < current_high_page_total)
			cma_high_alloc_peak_value = current_high_page_total;

		if (error) {
			pr_err("radix tree insert error...\n");
			kfree((void *)ptr);
			dump_stack();
		} else 
			tree_node_count ++;

		spin_unlock_irq(&rtk_tree_lock);
		radix_tree_preload_end();
	}

	return error;
}

int rtk_record_lookup(unsigned long addr, unsigned long *priv)
{
	rtk_record *ptr;
	int ret = 0;

	spin_lock_irq(&rtk_tree_lock);
	ptr = radix_tree_lookup(&rtk_tree, (addr&0x7fffffff) >> 2);
	if ((ptr) && (ptr->addr == addr)) {
		ret = ptr->type;
		if (priv)
			*priv = ptr->priv;
	} else
		pr_err("radix tree lookup error...\n");
	spin_unlock_irq(&rtk_tree_lock);

	return ret;
}

int rtk_record_lookup_ex(unsigned long addr, rtk_record **ptr, bool lock)
{
	int ret = 0;
	rtk_record *tmp = NULL;

	if (lock)
		spin_lock_irq(&rtk_tree_lock);

	tmp = radix_tree_lookup(&rtk_tree, (addr&0x7fffffff) >> 2);
	if ((tmp) && (tmp->addr == addr)) {
		ret = tmp->type;
		if (ptr)
			*ptr = tmp;
	} else
		pr_err("radix tree lookup error...\n");

	if (lock)
		spin_unlock_irq(&rtk_tree_lock);

	return ret;
}


void rtk_record_delete(unsigned long addr)
{
	rtk_record *ptr;
	int type;
	struct page *page = pfn_to_page(addr >> PAGE_SHIFT);

	type = rtk_record_lookup(addr,NULL);
	addr = (addr&0x7fffffff) >> 2;	// use WORD as minimum unit
	spin_lock_irq(&rtk_tree_lock);
	ptr = radix_tree_delete(&rtk_tree, addr);
	tree_node_count --;

	if(!PageHighMem(page))
		current_low_page_total -= (type & 0x00ffffff);
	else
		current_high_page_total -= (type & 0x00ffffff);

	spin_unlock_irq(&rtk_tree_lock);
	if (ptr)
		kfree((void *)ptr);
	else
		pr_err("radix tree delete error...\n");
}

static inline void __record_delete(unsigned long addr)
{
	rtk_record *ptr;

	addr = (addr&0x7fffffff) >> 2;	// use WORD as minimum unit
	ptr = radix_tree_delete(&rtk_tree, addr);
	tree_node_count --;

	if (ptr)
		kfree((void *)ptr);
	else
		pr_err("radix tree delete error...\n");
}

void rtk_record_defer_free(struct work_struct *unused)
{
	rtk_record **pRecords = NULL;
	int tmp, cnt, start_index = 0;
	int ret = 0;

	if (need_debug_cma_deferfree() == false)
		return;

	spin_lock(&rtk_tree_lock);

	if (!tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE]) {
		pr_debug("[cma defer_free] rtk record radix tree no tag inside...\n");
		goto out;
	}

	pRecords = (rtk_record **)kmalloc(sizeof(rtk_record *) * tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE], GFP_KERNEL);
	if (!pRecords) {
		ret = -ENOMEM;
		goto out;
	}

	tmp = radix_tree_gang_lookup_tag(&rtk_tree, (void **)pRecords, start_index, tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE], RTK_RECORD_TAG_DEFER_FREE);
	pr_debug("[cma defer_free] tag count(%d/%ld), total(%ld)\n", tmp, tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE], tree_node_count);

	if(tmp != tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE]) {
		pr_err("[cma defer_free] tree node tag(%d) is not equal tag count(%ld)...\n", tmp, tree_node_tag_count[RTK_RECORD_TAG_DEFER_FREE]);
		ret = -1;
		goto out;
	}

	for (cnt = 0; cnt < tmp; cnt++) {
		int type;
		struct page *page = NULL;

		if (!(pRecords[cnt]->reserved & 0x0000000f)) {
			pr_err("[cma defer_free] not defer free\n");
			continue;
		} else if (!__record_tag_get(pRecords[cnt]->addr, RTK_RECORD_TAG_DEFER_FREE)) {
			pr_err("[cma defer_free] not defer free tag\n");
			continue;
		}

//		pr_info("[cma defer_free] cnt=%d, type=%lx, addr=%lx, (%d)\n", cnt, pRecords[cnt]->type, pRecords[cnt]->addr, jiffies_to_msecs(jiffies - pRecords[cnt]->jiffies));

		if (time_before(jiffies - rtk_record_start_time, pRecords[cnt]->jiffies)) { // not timeout
//			pr_info("[cma defer_free] not defer free, timeout\n");
			continue;
		}

		type = pRecords[cnt]->type;
		page = pfn_to_page(pRecords[cnt]->addr >> PAGE_SHIFT);

		if(!PageHighMem(page))
			current_low_page_total -= (type & 0x00ffffff);
		else
			current_high_page_total -= (type & 0x00ffffff);

		dvr_defer_free((rtk_record *)pRecords[cnt]);
		__record_tag_clear(pRecords[cnt]->addr, RTK_RECORD_TAG_DEFER_FREE);
		__record_delete(pRecords[cnt]->addr);
	}

out:
	spin_unlock_irq(&rtk_tree_lock);

	if (pRecords)
		kfree(pRecords);

	if (cma_deferfree_workq)
		queue_delayed_work(cma_deferfree_workq, &delayed_cma_deferfree_work, msecs_to_jiffies(2000));

//	return ret;
	return;
}

int list_all_rtk_memory_allocation_sort(list_rtk_mem_t list_func,int (*cmp_func)(const void *, const void *),void *data)
{
	rtk_record **pRecords = NULL;
	int tmp, start_index = 0;
	int ret;

	if (unlikely(!tree_node_count)){
		pr_err(" rtk record radix tree is empty...\n");
		return 0;
	}

#ifdef CONFIG_HIBERNATION
	if (!in_suspend)
		spin_lock(&rtk_tree_lock);
#else
	spin_lock(&rtk_tree_lock);
#endif

	pRecords = (rtk_record **)kmalloc(sizeof(rtk_record *) * tree_node_count, GFP_KERNEL);
	if (unlikely(!pRecords)){
#ifdef CONFIG_HIBERNATION
		if (!in_suspend)
			spin_unlock(&rtk_tree_lock);
#else
		spin_unlock(&rtk_tree_lock);
#endif		
		return -ENOMEM;
	}

	tmp = radix_tree_gang_lookup(&rtk_tree, (void **)pRecords, start_index, tree_node_count);

	if(tmp != tree_node_count){
		pr_err(" tree node(%d) is not equal tree_node_count(%ld)...\n",tmp,tree_node_count);
		ret = -1;
		goto out;
	}

	if(cmp_func)
		sort(pRecords,tree_node_count,sizeof(rtk_record *),cmp_func,NULL);

	ret = list_func(pRecords,tree_node_count,data);
	printk("allocated peak size at low memory: %8x \n",cma_low_alloc_peak_value << PAGE_SHIFT);
	printk("allocated peak size at high memory: %8x \n",cma_high_alloc_peak_value << PAGE_SHIFT);

out:
#ifdef CONFIG_HIBERNATION
	if (!in_suspend)
		spin_unlock(&rtk_tree_lock);
#else
	spin_unlock(&rtk_tree_lock);
#endif

	kfree(pRecords);
	return ret;

}

void free_all_rtk_memory_allocation(free_rtk_mem_t free_func)
{
	const int record_number = 10;
	rtk_record **pRecords = NULL;
	int tmp, cnt, start_index = 0;

	pRecords = (rtk_record **)kmalloc(sizeof(rtk_record *) * record_number, GFP_KERNEL);
	if (unlikely(!pRecords))
		return;

	printk("===== flush rtk memory =====\n");
	spin_lock(&rtk_tree_lock);
	while (1) {
		tmp = radix_tree_gang_lookup(&rtk_tree, (void **)pRecords, start_index, record_number);
		for (cnt = 0; cnt < tmp; cnt++) {
			printk(" release address: %lx \n", pRecords[cnt]->addr);
			start_index = pRecords[cnt]->addr;
			if (!free_func(pRecords[cnt]))
				__record_delete(pRecords[cnt]->addr);
		}

		if (tmp != record_number)
			break;
		start_index = ((start_index&0x7fffffff) >> 2) + 1;
	}
	spin_unlock(&rtk_tree_lock);

	kfree(pRecords);
}

int cmp_addr_ascending_order(const void *a, const void *b)
{
	rtk_record *tmp_a;
	rtk_record *tmp_b;
	int ret;
	
	tmp_a = (rtk_record *)*(unsigned long *)a;
	tmp_b = (rtk_record *)*(unsigned long *)b;
	
	return (tmp_a->addr - tmp_b->addr);
}


int cmp_pid_ascending_order(const void *a, const void *b)
{
	rtk_record *tmp_a;
	rtk_record *tmp_b;
	int ret;
	
	tmp_a = (rtk_record *)*(unsigned long *)a;
	tmp_b = (rtk_record *)*(unsigned long *)b;
	
	ret = tmp_a->pid - tmp_b->pid;	//fisrt compare pid
	if(ret == 0){
		ret = (tmp_b->type & 0x00ffffff) - (tmp_a->type & 0x00ffffff); //if pid equal, size descending order
	}

	return ret;
}

int cmp_caller_ascending_order(const void *a, const void *b)
{
	rtk_record *tmp_a;
	rtk_record *tmp_b;
	long cmp;
	int ret;
	
	tmp_a = (rtk_record *)*(unsigned long *)a;
	tmp_b = (rtk_record *)*(unsigned long *)b;
	
	cmp = (unsigned long)tmp_a->caller - (unsigned long)tmp_b->caller;	//fisrt compare pid
	if(cmp == 0){
		ret = (tmp_b->type & 0x00ffffff) - (tmp_a->type & 0x00ffffff); //if pid equal, size descending order
	}
	else if(cmp > 0){
		ret = 1;
	}
	else{
		ret = -1;
	}

	return ret;

}

int cmp_size_descending_order(const void *a, const void *b)
{
	rtk_record *tmp_a;
	rtk_record *tmp_b;

	tmp_a = (rtk_record *)*(unsigned long *)a;
	tmp_b = (rtk_record *)*(unsigned long *)b;

	return (tmp_b->type & 0x00ffffff) - (tmp_a->type & 0x00ffffff);
}

int list_mem_generic(rtk_record **pRecords,unsigned long node_count,void *data)
{
	int cnt,ret;
	int total_alloc = 0;

	for(cnt = 0;cnt < node_count; cnt++){
		ret = remap_list_mem((rtk_record *)pRecords[cnt]);
		if (ret > 0) {
			if ( (((pRecords[cnt]->type & ID_MASK) == BUDDY_ID) && need_debug_cma_guardband())  || 
				 (((pRecords[cnt]->type & ID_MASK) == BUDDY_MESG_ID) && need_debug_cma_guardband()) ||
				 (((pRecords[cnt]->type & ID_MASK) == DRIVER_ID) && need_debug_cma_guardband()) )
			{
				dvr_guardband_check(pRecords[cnt]->addr, pRecords[cnt]->type & 0x00ffffff);
			}
		}
		total_alloc += ret;
	}

	pr_info("total allocated: %8x \n", total_alloc);
	return 0;
}

int list_addr_mem(rtk_record **pRecords,unsigned long node_count,void *data)
{
	int cnt,ret;
	int total_alloc = 0;
	int total_tmp = 0;
	unsigned long addr1 = 0,addr2 = 0;
	unsigned long start = 0,end = 0;
	int flag = 0;
	struct mem_bp *bp = NULL;
	phys_addr_t dcu2_start_addr;
	
	if(data){
		sscanf(data,"%x %x",&addr1,&addr2);
		if(addr2 == 0){
			end = start = addr1; 
			flag = 1;
		}
		else{
			start = (addr1 < addr2)?addr1:addr2;
			end = (addr1 < addr2)?addr2:addr1;
		}
		
		for(cnt = 0;cnt < node_count; cnt++){
			if((pRecords[cnt]->addr >=start) 
				&& (pRecords[cnt]->addr  <= end))
				ret = remap_list_mem((rtk_record *)pRecords[cnt]);
			else
				ret = 0;
			total_alloc += ret;
		}
		if(flag)
			pr_info("address(%x) total allocated: %8x \n", start,total_alloc);
		else
			pr_info("address(%x - %x) total allocated: %8x \n", start,end,total_alloc);		
	}
	else{
		bp = (struct mem_bp *)dma_get_allocator(auth_dev);
		dcu2_start_addr = __pfn_to_phys(bp->base_pfn);
		for(cnt = 0;cnt < node_count; cnt++){
			if((pRecords[cnt]->addr >= dcu2_start_addr) && (flag == 0)){
				pr_info("total allocated at DCU1: %8x \n\n", total_tmp);
				flag = 1;
				total_tmp = 0;
			}			
			ret = remap_list_mem((rtk_record *)pRecords[cnt]);
			total_alloc += ret;
			total_tmp += ret;
		}
		pr_info("total allocated at DCU2: %8x \n\n", total_tmp);
		pr_info("total allocated: %8x \n", total_alloc);
	}
	return 0;
}


int list_pid_mem(rtk_record **pRecords,unsigned long node_count,void *data)
{
	int cnt,ret;
	int total_alloc = 0;
	int pid_alloc = 0;
	unsigned long pid = 0;

	if(data){
		sscanf(data,"%d",&pid);
		for(cnt = 0;cnt < node_count; cnt++){
			if(pid == pRecords[cnt]->pid)
				ret = remap_list_mem((rtk_record *)pRecords[cnt]);
			else
				ret = 0;
			total_alloc += ret;
		}
		if(total_alloc)
			pr_info("pid(%ld) total allocated: %8x \n", pid,total_alloc);
		else
			pr_info("pid(%ld) not allocated cma memory\n", pid);	
	}
	else{
		for(cnt = 0;cnt < node_count; cnt++){
			if((pid != ((rtk_record *)pRecords[cnt])->pid) && (pid != 0)){
				pr_info("pid(%ld) total allocated: %8x \n\n",pid,pid_alloc);
				pid_alloc = 0;
			}
			ret = remap_list_mem(pRecords[cnt]);
			pid_alloc += ret;
			total_alloc += ret;
			pid = ((rtk_record *)pRecords[cnt])->pid;
		}
		if(cnt == node_count){
			pr_info("pid(%ld) total allocated: %8x \n\n",pid,pid_alloc);
		}
		pr_info("total allocated: %8x \n", total_alloc);
	}
	
	return 0;

}

int list_mem_size_descending_order(rtk_record **pRecords,unsigned long node_count,void *data)
{
	int cnt,ret;
	int total_alloc = 0;
	unsigned long size1 = 0,size2 = 0;
	unsigned long start = 0,end = 0;
	int flag = 0;
	
	if(data){
		sscanf(data,"%x %x",&size1,&size2);
		if(size2 == 0){
			end = start = size1; 
			flag = 1;
		}
		else{
			start = (size1 < size2)?size1:size2;
			end = (size1 < size2)?size2:size1;
		}
		
		for(cnt = 0;cnt < node_count; cnt++){
			if((((pRecords[cnt]->type & 0x00ffffff)<< PAGE_SHIFT) >=start) 
				&& (((pRecords[cnt]->type & 0x00ffffff)<< PAGE_SHIFT) <= end))
				ret = remap_list_mem((rtk_record *)pRecords[cnt]);
			else
				ret = 0;
			total_alloc += ret;
		}
		if(flag)
			pr_info("size(%x) total allocated: %8x \n", start,total_alloc);
		else
			pr_info("size(%x - %x) total allocated: %8x \n", start,end,total_alloc);		
	}
	else{
		for(cnt = 0;cnt < node_count; cnt++){
			ret = remap_list_mem((rtk_record *)pRecords[cnt]);
			total_alloc += ret;
		}
		pr_info("total allocated: %8x \n", total_alloc);
	}
	return 0;
}

int list_func_mem(rtk_record **pRecords,unsigned long node_count,void *data)
{
	int cnt,ret;
	int total_alloc = 0;
	int func_alloc = 0;
	unsigned char name[128],caller[128];

	memset(name,0,sizeof(name));
	memset(caller,0,sizeof(caller));
	if(data){
		sscanf(data,"%s",name);
		
		for(cnt = 0;cnt < node_count; cnt++){
			snprintf(caller,sizeof(caller) - 1,"%pf",pRecords[cnt]->caller);
			if(strstr(caller,name)){
				ret = remap_list_mem((rtk_record *)pRecords[cnt]);
			}
			else{
				ret = 0;
			}
			total_alloc += ret;
		}
		if(total_alloc)
			pr_info("function has %s total allocated: %8x \n", name,total_alloc);
		else
			pr_info("function has %s not allocated cma memory\n", name);
	}
	else{
		for(cnt = 0;cnt < node_count; cnt++){
			snprintf(caller,sizeof(caller) - 1,"%pf",pRecords[cnt]->caller);
			if((strcmp(caller,name)) && (name[0] != 0)){
				pr_info("func(%s) total allocated: %8x \n\n",name,func_alloc);
				func_alloc = 0;
			}
			ret = remap_list_mem(pRecords[cnt]);
			func_alloc += ret;
			total_alloc += ret;
			snprintf(name,sizeof(name) - 1,"%pf",pRecords[cnt]->caller);
		}
		if(cnt == node_count){
			pr_info("func(%s) total allocated: %8x \n\n",name,func_alloc);
		}
		pr_info("total allocated: %8x \n", total_alloc);
	}

	return 0;
}

