/*
 * drivers/cpufreq/cpufreq_interactive.c
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Mike Chan (mike@android.com)
 *
 */

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/tick.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <asm/cputime.h>
#include <linux/kernel.h>

#define CREATE_TRACE_POINTS
#include <trace/events/cpufreq_interactive.h>
#include "cpufreq_governor.h"
#include <mach/platform.h>
#include <rtk_kdriver/rtk_thermal_sensor.h>
#include <linux/freezer.h>
#include <rtk_kdriver/rtk_clock.h>

#ifdef SCPU_FREQ_STEP0
#define STEP0  (SCPU_FREQ_STEP0/1000)
#define STEP1  (SCPU_FREQ_STEP1/1000)
#define STEP2  (SCPU_FREQ_STEP2/1000)
#define STEP3  (SCPU_FREQ_STEP3/1000)
#else
#define STEP0  (700000/1000)
#define STEP1  (850000/1000)
#define STEP2  (1100000/1000)
#define STEP3  (1150000/1000)
#endif
typedef struct cpu_power_table {
	unsigned short core;	
	unsigned short temperature;
	unsigned int load;
	unsigned int freq;
};

#ifdef USE_OLD_TABLE
#define TABLE_SIZE_X	8
#define TABLE_SIZE_Y	5
struct cpu_power_table rtk_pwr_tbl[TABLE_SIZE_X][TABLE_SIZE_Y]=
{
		{ {1,55,50,STEP0}, {1,85,50,STEP0}, {1,105,50,STEP0}, {1,115,50,STEP0}, {1,125,50,STEP0}},

		{ {1,55,90,STEP3}, {2,85,90,STEP0}, {2,105,90,STEP3}, {2,115,90,STEP3}, {2,125,90,STEP3}},

		{ {2,55,150,STEP3},{2,85,150,STEP0},{2,105,150,STEP0},{2,115,150,STEP0},{2,125,150,STEP3}},

		{ {2,55,200,STEP3},{3,85,200,STEP3},{3,105,200,STEP3},{3,115,200,STEP3},{3,125,200,STEP3}},

		{ {3,55,250,STEP3},{3,85,250,STEP3},{3,105,250,STEP3},{3,115,250,STEP3},{3,125,250,STEP3}},

		{ {3,55,300,STEP3},{4,85,300,STEP3},{4,105,300,STEP3},{4,115,300,STEP3},{4,125,300,STEP3}},

		{ {4,55,350,STEP3},{4,85,350,STEP3},{4,105,350,STEP3},{1,115,350,STEP3},{4,125,350,STEP3}},

		{ {4,55,400,STEP3},{4,85,400,STEP3},{4,105,400,STEP3},{4,115,400,STEP3},{4,125,400,STEP3}}
};
#else
#define TABLE_SIZE_X	6
#define TABLE_SIZE_Y	3
struct cpu_power_table rtk_pwr_tbl[TABLE_SIZE_X][TABLE_SIZE_Y]=
{

		{ {2,100,60,STEP0}, {2,120,60,STEP0}, {2,130,60,STEP0}}, 

		{ {3,100,100,STEP0},{3,120,100,STEP0},{3,130,100,STEP0}}, 

		{ {4,100,160,STEP0},{4,120,160,STEP0},{4,130,160,STEP0}},

		{ {3,100,220,STEP3},{3,120,220,STEP3},{3,130,220,STEP3}},

		{ {4,100,280,STEP3},{4,120,280,STEP3},{4,130,280,STEP3}},
};
#endif

struct cpufreq_interactive_cpuinfo {
	struct timer_list cpu_timer;
	struct timer_list cpu_slack_timer;
	spinlock_t load_lock; /* protects the next 4 fields */
	u64 time_in_idle;
	u64 time_in_idle_timestamp;
	u64 cputime_speedadj;
	u64 cputime_speedadj_timestamp;
	struct cpufreq_policy *policy;
	struct cpufreq_frequency_table *freq_table;
	spinlock_t target_freq_lock; /*protects target freq */
	unsigned int target_freq;
	unsigned int floor_freq;
	unsigned int max_freq;
	u64 floor_validate_time;
	u64 hispeed_validate_time;
	struct rw_semaphore enable_sem;
	int governor_enabled;
};

static DEFINE_PER_CPU(struct cpufreq_interactive_cpuinfo, cpuinfo);

/* realtime thread handles frequency scaling */
static struct task_struct *speedchange_task;
static struct task_struct *cpu_up_task;
static cpumask_t speedchange_cpumask;
static unsigned int allowed_cores=4;
static unsigned int saved_allowed_cores=4;
static unsigned int allowed_cores_mask=0;
static int temperature=0;
static spinlock_t speedchange_cpumask_lock;
static struct mutex gov_lock;
static int current_onlines=0;
static int load_accumulate=0;

/* Target load.  Lower values result in higher CPU speeds. */
#define DEFAULT_TARGET_LOAD 90
static unsigned int default_target_loads[] = {DEFAULT_TARGET_LOAD};
static int cpu_load_for_speedchange[NR_CPUS]={-1,-1,-1,-1}; /* for copying to speed task*/
static int cpu_cur_load[NR_CPUS]={-1,-1,-1,-1}; /* for interrupt */
#define DEFAULT_TIMER_RATE (20 * USEC_PER_MSEC)
#define DEFAULT_ABOVE_HISPEED_DELAY DEFAULT_TIMER_RATE
static unsigned int default_above_hispeed_delay[] = {
	DEFAULT_ABOVE_HISPEED_DELAY };

struct cpufreq_interactive_tunables {
	int usage_count;
	/* Hi speed to bump to from lo speed when load burst (default max) */
	unsigned int hispeed_freq;
	/* Go to hi speed when CPU load at or above this value. */
#define DEFAULT_GO_HISPEED_LOAD 99
	unsigned long go_hispeed_load;
	/* Target load. Lower values result in higher CPU speeds. */
	spinlock_t target_loads_lock;
	unsigned int *target_loads;
	int ntarget_loads;
	/*
	 * The minimum amount of time to spend at a frequency before we can ramp
	 * down.
	 */
#define DEFAULT_MIN_SAMPLE_TIME (80 * USEC_PER_MSEC)
	unsigned long min_sample_time;
	/*
	 * The sample rate of the timer used to increase frequency
	 */
	unsigned long timer_rate;
	/*
	 * Wait this long before raising speed above hispeed, by default a
	 * single timer interval.
	 */
	spinlock_t above_hispeed_delay_lock;
	unsigned int *above_hispeed_delay;
	int nabove_hispeed_delay;
	/* Non-zero means indefinite speed boost active */
	int boost_val;
	/* Duration of a boot pulse in usecs */
	int boostpulse_duration_val;
	/* End time of boost pulse in ktime converted to usecs */
	u64 boostpulse_endtime;
	bool boosted;
	/*
	 * Max additional time to wait in idle, beyond timer_rate, at speeds
	 * above minimum before wakeup to reduce speed, or -1 if unnecessary.
	 */
#define DEFAULT_TIMER_SLACK (4 * DEFAULT_TIMER_RATE)
	int timer_slack_val;
	bool io_is_busy;
};

/* For cases where we have single governor instance for system */
static struct cpufreq_interactive_tunables *common_tunables;

static struct attribute_group *get_sysfs_attr(void);
void dump_log(int data)
{
	static long expires_up_down[4]={0, 0 , 0, 0};
	static unsigned long myjiffies[4][10];

	if(data>=4) {
		printk("error\n");
		return;
	}
	myjiffies[data][expires_up_down[data]]=jiffies;
	expires_up_down[data]++;

	if(expires_up_down[data]==10) {
		int i ;
		for(i=0;i<expires_up_down[data];i++)
			printk(" %lu ",myjiffies[data][i]);
		printk("\n");
		expires_up_down[data]=0;
	}
}

static inline cputime64_t get_cpu_idle_time_jiffy(unsigned int cpu,
						  cputime64_t *wall)
{
	u64 idle_time;
	u64 cur_wall_time;
	u64 busy_time;

	cur_wall_time = jiffies64_to_cputime64(get_jiffies_64());

	busy_time  = kcpustat_cpu(cpu).cpustat[CPUTIME_USER];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SYSTEM];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_IRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_SOFTIRQ];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_STEAL];
	busy_time += kcpustat_cpu(cpu).cpustat[CPUTIME_NICE];

	idle_time = cur_wall_time - busy_time;
	if (wall)
		*wall = jiffies_to_usecs(cur_wall_time);

	return jiffies_to_usecs(idle_time);
}

static void cpufreq_interactive_timer_resched(
	struct cpufreq_interactive_cpuinfo *pcpu)
{
	struct cpufreq_interactive_tunables *tunables =
		pcpu->policy->governor_data;
	unsigned long expires;
	unsigned long flags;

	spin_lock_irqsave(&pcpu->load_lock, flags);
	pcpu->time_in_idle =
		get_cpu_idle_time(smp_processor_id(),
				  &pcpu->time_in_idle_timestamp,
				  tunables->io_is_busy);
	pcpu->cputime_speedadj = 0;
	pcpu->cputime_speedadj_timestamp = pcpu->time_in_idle_timestamp;
	expires = jiffies + usecs_to_jiffies(tunables->timer_rate);
	mod_timer_pinned(&pcpu->cpu_timer, expires);

	if (tunables->timer_slack_val >= 0 &&
	    pcpu->target_freq > pcpu->policy->min) {
		expires += usecs_to_jiffies(tunables->timer_slack_val);
		mod_timer_pinned(&pcpu->cpu_slack_timer, expires);
	}

	spin_unlock_irqrestore(&pcpu->load_lock, flags);
}

/* The caller shall take enable_sem write semaphore to avoid any timer race.
 * The cpu_timer and cpu_slack_timer must be deactivated when calling this
 * function.
 */
static void cpufreq_interactive_timer_start(
	struct cpufreq_interactive_tunables *tunables, int cpu)
{
	struct cpufreq_interactive_cpuinfo *pcpu = &per_cpu(cpuinfo, cpu);
	unsigned long expires = jiffies +
		usecs_to_jiffies(tunables->timer_rate);
	unsigned long flags;

	pcpu->cpu_timer.expires = expires;
	add_timer_on(&pcpu->cpu_timer, cpu);
	if (tunables->timer_slack_val >= 0 &&
	    pcpu->target_freq > pcpu->policy->min) {
		expires += usecs_to_jiffies(tunables->timer_slack_val);
		pcpu->cpu_slack_timer.expires = expires;
		add_timer_on(&pcpu->cpu_slack_timer, cpu);
	}

	spin_lock_irqsave(&pcpu->load_lock, flags);
	pcpu->time_in_idle =
		get_cpu_idle_time(cpu, &pcpu->time_in_idle_timestamp,
				  tunables->io_is_busy);
	pcpu->cputime_speedadj = 0;
	pcpu->cputime_speedadj_timestamp = pcpu->time_in_idle_timestamp;
	spin_unlock_irqrestore(&pcpu->load_lock, flags);
}

static unsigned int freq_to_above_hispeed_delay(
	struct cpufreq_interactive_tunables *tunables,
	unsigned int freq)
{
	int i;
	unsigned int ret;
	unsigned long flags;

	spin_lock_irqsave(&tunables->above_hispeed_delay_lock, flags);

	for (i = 0; i < tunables->nabove_hispeed_delay - 1 &&
			freq >= tunables->above_hispeed_delay[i+1]; i += 2)
		;

	ret = tunables->above_hispeed_delay[i];
	spin_unlock_irqrestore(&tunables->above_hispeed_delay_lock, flags);
	return ret;
}

static unsigned int freq_to_targetload(
	struct cpufreq_interactive_tunables *tunables, unsigned int freq)
{
	int i;
	unsigned int ret;
	unsigned long flags;

	spin_lock_irqsave(&tunables->target_loads_lock, flags);

	for (i = 0; i < tunables->ntarget_loads - 1 &&
		    freq >= tunables->target_loads[i+1]; i += 2)
		;

	ret = tunables->target_loads[i];
	spin_unlock_irqrestore(&tunables->target_loads_lock, flags);
	return ret;
}

/*
 * If increasing frequencies never map to a lower target load then
 * choose_freq() will find the minimum frequency that does not exceed its
 * target load given the current load.
 */
static unsigned int choose_freq(struct cpufreq_interactive_cpuinfo *pcpu,
		unsigned int loadadjfreq)
{
	unsigned int freq = pcpu->policy->cur;
	unsigned int prevfreq, freqmin, freqmax;
	unsigned int tl;
	int index;

	freqmin = 0;
	freqmax = UINT_MAX;

	do {
		prevfreq = freq;
		tl = freq_to_targetload(pcpu->policy->governor_data, freq);

		/*
		 * Find the lowest frequency where the computed load is less
		 * than or equal to the target load.
		 */

		if (cpufreq_frequency_table_target(
			    pcpu->policy, pcpu->freq_table, loadadjfreq / tl,
			    CPUFREQ_RELATION_L, &index))
			break;
		freq = pcpu->freq_table[index].frequency;

		if (freq > prevfreq) {
			/* The previous frequency is too low. */
			freqmin = prevfreq;

			if (freq >= freqmax) {
				/*
				 * Find the highest frequency that is less
				 * than freqmax.
				 */
				if (cpufreq_frequency_table_target(
					    pcpu->policy, pcpu->freq_table,
					    freqmax - 1, CPUFREQ_RELATION_H,
					    &index))
					break;
				freq = pcpu->freq_table[index].frequency;

				if (freq == freqmin) {
					/*
					 * The first frequency below freqmax
					 * has already been found to be too
					 * low.  freqmax is the lowest speed
					 * we found that is fast enough.
					 */
					freq = freqmax;
					break;
				}
			}
		} else if (freq < prevfreq) {
			/* The previous frequency is high enough. */
			freqmax = prevfreq;

			if (freq <= freqmin) {
				/*
				 * Find the lowest frequency that is higher
				 * than freqmin.
				 */
				if (cpufreq_frequency_table_target(
					    pcpu->policy, pcpu->freq_table,
					    freqmin + 1, CPUFREQ_RELATION_L,
					    &index))
					break;
				freq = pcpu->freq_table[index].frequency;

				/*
				 * If freqmax is the first frequency above
				 * freqmin then we have already found that
				 * this speed is fast enough.
				 */
				if (freq == freqmax)
					break;
			}
		}

		/* If same frequency chosen as previous then done. */
	} while (freq != prevfreq);

	return freq;
}

static u64 update_load(int cpu)
{
	struct cpufreq_interactive_cpuinfo *pcpu = &per_cpu(cpuinfo, cpu);
	struct cpufreq_interactive_tunables *tunables =
		pcpu->policy->governor_data;
	u64 now;
	u64 now_idle;
	unsigned int delta_idle;
	unsigned int delta_time;
	u64 active_time;

	now_idle = get_cpu_idle_time(cpu, &now, tunables->io_is_busy);
	delta_idle = (unsigned int)(now_idle - pcpu->time_in_idle);
	delta_time = (unsigned int)(now - pcpu->time_in_idle_timestamp);

	if (delta_time <= delta_idle)
		active_time = 0;
	else
		active_time = delta_time - delta_idle;

	pcpu->cputime_speedadj += active_time * pcpu->policy->cur;

	pcpu->time_in_idle = now_idle;
	pcpu->time_in_idle_timestamp = now;
	return now;
}

#define BOUNCING_COUNT 5 
#if (BOUNCING_COUNT >31)
#error BOUNCING_COUNT over 31 in cpufreq_interactive.c
#endif
static int cpu_load_value[32];
static int cpu_load_idx=0;
static int bouncing_count=BOUNCING_COUNT;

void cpufreq_interactive_bouncing(int count)
{
	if(count >=BOUNCING_COUNT && count<=31)
		bouncing_count=count;
	else
		bouncing_count=BOUNCING_COUNT;
	printk("set bouncing count:%d\n",bouncing_count);
}

#define RTK_FREQ_DRIVER
static void cpufreq_interactive_timer(unsigned long data)
{

#ifdef RTK_FREQ_DRIVER
	u64 now;
	unsigned int delta_time;
	u64 cputime_speedadj;
	int cpu_load;
	struct cpufreq_interactive_cpuinfo *pcpu;
	struct cpufreq_interactive_tunables *tunables;
	unsigned int new_freq;
	unsigned int cores;
	unsigned int loadadjfreq;
	unsigned int index;
	unsigned long flags;

	int t, p,online_index=0;

	if(data!=0) //no need to deal with 1,2,3
	{
		return;
	}

	cpu_load=0;
	for_each_possible_cpu(data){
		cpu_cur_load[data]=-1;
		if(cpu_online(data)) {
			online_index++;
			pcpu =&per_cpu(cpuinfo, data);
			tunables =pcpu->policy->governor_data;


			if (!down_read_trylock(&pcpu->enable_sem))
				return;
			if (!pcpu->governor_enabled) {
				up_read(&pcpu->enable_sem);
				continue;
			}

			spin_lock_irqsave(&pcpu->load_lock, flags);
			now = update_load(data);
			delta_time = (unsigned int)(now - pcpu->cputime_speedadj_timestamp);
			cputime_speedadj = pcpu->cputime_speedadj;
			spin_unlock_irqrestore(&pcpu->load_lock, flags);

			//if (WARN_ON_ONCE(!delta_time))
			//	goto rearm;

			spin_lock_irqsave(&pcpu->target_freq_lock, flags);
			do_div(cputime_speedadj, delta_time);
			loadadjfreq = (unsigned int)cputime_speedadj * 100;
			cpu_load+=loadadjfreq / pcpu->policy->cur;
			cpu_cur_load[data]=loadadjfreq / pcpu->policy->cur;
			tunables->boosted = tunables->boost_val || now < tunables->boostpulse_endtime;
			spin_unlock_irqrestore(&pcpu->target_freq_lock, flags);

			up_read(&pcpu->enable_sem);
		}
	}
	data=0;

	pcpu =&per_cpu(cpuinfo, data);

	if (!down_read_trylock(&pcpu->enable_sem))
		return;
	if (!pcpu->governor_enabled)
		goto exit;


wakeup_proc:
	spin_lock_irqsave(&speedchange_cpumask_lock, flags);

{
	int calc_tempidx=0;
	int calc_loadidx=0;
	struct cpu_power_table *prtk_pwr_tbl;

	if(temperature <= rtk_pwr_tbl[0][0].temperature)
	{
		calc_tempidx=0;
	}
	else if(temperature > rtk_pwr_tbl[0][0].temperature && temperature <= rtk_pwr_tbl[0][1].temperature )
	{
		calc_tempidx=1;
	}
	else if(temperature > rtk_pwr_tbl[0][1].temperature && temperature <= rtk_pwr_tbl[0][2].temperature )
	{
		calc_tempidx=2;
	}
#if (TABLE_SIZE_Y>=5)	
	else if(temperature > rtk_pwr_tbl[0][2].temperature && temperature <= rtk_pwr_tbl[0][3].temperature )
	{
		calc_tempidx=3;
	}
	else if(temperature > rtk_pwr_tbl[0][3].temperature && temperature <= rtk_pwr_tbl[0][4].temperature )
	{
		calc_tempidx=4;
	}
	else
	{
		calc_tempidx=4;
	}
#else
	else
	{
		calc_tempidx=2;
	}
#endif	

	prtk_pwr_tbl=&rtk_pwr_tbl[0][calc_tempidx];
	if(cpu_load<=rtk_pwr_tbl[0][calc_tempidx].load)
	{
		saved_allowed_cores=rtk_pwr_tbl[0][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[0][calc_tempidx].freq;
	}
	else if(cpu_load>rtk_pwr_tbl[0][calc_tempidx].load && cpu_load<=rtk_pwr_tbl[1][calc_tempidx].load)
	{
		saved_allowed_cores=rtk_pwr_tbl[1][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[1][calc_tempidx].freq;
	}
	else if(cpu_load>rtk_pwr_tbl[1][calc_tempidx].load && cpu_load<=rtk_pwr_tbl[2][calc_tempidx].load)
	{
		saved_allowed_cores=rtk_pwr_tbl[2][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[2][calc_tempidx].freq;
	}
	else if(cpu_load>rtk_pwr_tbl[2][calc_tempidx].load && cpu_load<=rtk_pwr_tbl[3][calc_tempidx].load)
	{
		saved_allowed_cores=rtk_pwr_tbl[3][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[3][calc_tempidx].freq;
	}
	else if(cpu_load>rtk_pwr_tbl[3][calc_tempidx].load && cpu_load<=rtk_pwr_tbl[4][calc_tempidx].load)
	{
		saved_allowed_cores=rtk_pwr_tbl[4][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[4][calc_tempidx].freq;
	}
#if #if (TABLE_SIZE_X>=8)		
	else if(cpu_load>rtk_pwr_tbl[4][calc_tempidx].load && cpu_load<=rtk_pwr_tbl[5][calc_tempidx].load)
	{
		saved_allowed_cores=rtk_pwr_tbl[5][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[5][calc_tempidx].freq;
	}
	else if(cpu_load>rtk_pwr_tbl[5][calc_tempidx].load && cpu_load<=rtk_pwr_tbl[6][calc_tempidx].load)
	{
		saved_allowed_cores=rtk_pwr_tbl[6][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[6][calc_tempidx].freq;
	}
	else if(cpu_load>rtk_pwr_tbl[6][calc_tempidx].load && cpu_load<=rtk_pwr_tbl[7][calc_tempidx].load)
	{
		saved_allowed_cores=rtk_pwr_tbl[7][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[7][calc_tempidx].freq;
	}
	else
	{
		saved_allowed_cores=rtk_pwr_tbl[7][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[7][calc_tempidx].freq;
	}
#else
	else
	{
		saved_allowed_cores=rtk_pwr_tbl[4][calc_tempidx].core;
		pcpu->target_freq =rtk_pwr_tbl[4][calc_tempidx].freq;
			
	}
#endif	
	pcpu->target_freq*=1000;
}

//bouncing	start
	cpu_load_value[cpu_load_idx++]=cpu_load;
	if(cpu_load_idx>=bouncing_count)
		cpu_load_idx=0;

	if(saved_allowed_cores < allowed_cores)
			allowed_cores_mask|=1;
	else { //up cpus, clear mask
		allowed_cores=saved_allowed_cores;
		allowed_cores_mask=0;
	}
		
	allowed_cores_mask &=((1UL<<bouncing_count) -1);
	
	if(allowed_cores_mask==((1UL<<bouncing_count) -1)) {//confirm power down cores
//	   	printk("online_cpu_num:%d load:%d %d %d %d %d allowed_cores:%d current:%d bouncing:%d\n",online_index,cpu_cur_load[0],cpu_cur_load[1],cpu_cur_load[2],cpu_cur_load[3],cpu_load_value[4],allowed_cores,saved_allowed_cores,((1UL<<bouncing_count) -1));	   			
		if(online_index>saved_allowed_cores)
			allowed_cores=online_index - 1;
		else
		   	allowed_cores=saved_allowed_cores;
	   	allowed_cores_mask=0;

	}
	   	
	allowed_cores_mask<<=1;
//bouncing end
	
//	printk("load %d %d  %d %d\n",data,cpu_load,temperature,saved_allowed_cores,pcpu->target_freq);

	cpumask_set_cpu(data, &speedchange_cpumask);
	memcpy(cpu_load_for_speedchange,cpu_cur_load,sizeof(cpu_load_for_speedchange));
	spin_unlock_irqrestore(&speedchange_cpumask_lock, flags);
	
	wake_up_process(speedchange_task);

rearm_if_notmax:
	/*
	 * Already set max speed and don't see a need to change that,
	 * wait until next idle to re-evaluate, don't need timer.
	 */
	//if (pcpu->target_freq == pcpu->policy->max)
	//	goto exit;

rearm:
	if (!timer_pending(&pcpu->cpu_timer))
		cpufreq_interactive_timer_resched(pcpu);

exit:
	up_read(&pcpu->enable_sem);
	return;
#else //original
	u64 now;
	unsigned int delta_time;
	u64 cputime_speedadj;
	int cpu_load;
	struct cpufreq_interactive_cpuinfo *pcpu =
		&per_cpu(cpuinfo, data);
	struct cpufreq_interactive_tunables *tunables =
		pcpu->policy->governor_data;
	unsigned int new_freq;
	unsigned int loadadjfreq;
	unsigned int index;
	unsigned long flags;

	if (!down_read_trylock(&pcpu->enable_sem))
		return;
	if (!pcpu->governor_enabled)
		goto exit;

	spin_lock_irqsave(&pcpu->load_lock, flags);
	now = update_load(data);
	delta_time = (unsigned int)(now - pcpu->cputime_speedadj_timestamp);
	cputime_speedadj = pcpu->cputime_speedadj;
	spin_unlock_irqrestore(&pcpu->load_lock, flags);

	if (WARN_ON_ONCE(!delta_time))
		goto rearm;

	spin_lock_irqsave(&pcpu->target_freq_lock, flags);
	do_div(cputime_speedadj, delta_time);
	loadadjfreq = (unsigned int)cputime_speedadj * 100;
	cpu_load = loadadjfreq / pcpu->policy->cur;
	tunables->boosted = tunables->boost_val || now < tunables->boostpulse_endtime;

	if (cpu_load >= tunables->go_hispeed_load || tunables->boosted) {
		if (pcpu->target_freq < tunables->hispeed_freq) {
			new_freq = tunables->hispeed_freq;
		} else {
			new_freq = choose_freq(pcpu, loadadjfreq);

			if (new_freq < tunables->hispeed_freq)
				new_freq = tunables->hispeed_freq;
		}
	} else {
		new_freq = choose_freq(pcpu, loadadjfreq);
		if (new_freq > tunables->hispeed_freq &&
				pcpu->target_freq < tunables->hispeed_freq)
			new_freq = tunables->hispeed_freq;
	}

	if (pcpu->target_freq >= tunables->hispeed_freq &&
	    new_freq > pcpu->target_freq &&
	    now - pcpu->hispeed_validate_time <
	    freq_to_above_hispeed_delay(tunables, pcpu->target_freq)) {
		trace_cpufreq_interactive_notyet(
			data, cpu_load, pcpu->target_freq,
			pcpu->policy->cur, new_freq);
		spin_unlock_irqrestore(&pcpu->target_freq_lock, flags);
		goto rearm;
	}

	pcpu->hispeed_validate_time = now;

	if (cpufreq_frequency_table_target(pcpu->policy, pcpu->freq_table,
					   new_freq, CPUFREQ_RELATION_L,
					   &index)) {
		spin_unlock_irqrestore(&pcpu->target_freq_lock, flags);
		goto rearm;
	}

	new_freq = pcpu->freq_table[index].frequency;

	/*
	 * Do not scale below floor_freq unless we have been at or above the
	 * floor frequency for the minimum sample time since last validated.
	 */
	if (new_freq < pcpu->floor_freq) {
		if (now - pcpu->floor_validate_time <
				tunables->min_sample_time) {
			trace_cpufreq_interactive_notyet(
				data, cpu_load, pcpu->target_freq,
				pcpu->policy->cur, new_freq);
			spin_unlock_irqrestore(&pcpu->target_freq_lock, flags);
			goto rearm;
		}
	}

	/*
	 * Update the timestamp for checking whether speed has been held at
	 * or above the selected frequency for a minimum of min_sample_time,
	 * if not boosted to hispeed_freq.  If boosted to hispeed_freq then we
	 * allow the speed to drop as soon as the boostpulse duration expires
	 * (or the indefinite boost is turned off).
	 */

	if (!tunables->boosted || new_freq > tunables->hispeed_freq) {
		pcpu->floor_freq = new_freq;
		pcpu->floor_validate_time = now;
	}

	if (pcpu->target_freq == new_freq &&
			pcpu->target_freq <= pcpu->policy->cur) {
		trace_cpufreq_interactive_already(
			data, cpu_load, pcpu->target_freq,
			pcpu->policy->cur, new_freq);
		spin_unlock_irqrestore(&pcpu->target_freq_lock, flags);
		goto rearm_if_notmax;
	}

	trace_cpufreq_interactive_target(data, cpu_load, pcpu->target_freq,
					 pcpu->policy->cur, new_freq);

	pcpu->target_freq = new_freq;
	spin_unlock_irqrestore(&pcpu->target_freq_lock, flags);
	spin_lock_irqsave(&speedchange_cpumask_lock, flags);
	cpumask_set_cpu(data, &speedchange_cpumask);
	spin_unlock_irqrestore(&speedchange_cpumask_lock, flags);
	wake_up_process(speedchange_task);

rearm_if_notmax:
	/*
	 * Already set max speed and don't see a need to change that,
	 * wait until next idle to re-evaluate, don't need timer.
	 */
	if (pcpu->target_freq == pcpu->policy->max)
		goto exit;

rearm:
	if (!timer_pending(&pcpu->cpu_timer))
		cpufreq_interactive_timer_resched(pcpu);

exit:
	up_read(&pcpu->enable_sem);
	return;
#endif
}

static void cpufreq_interactive_idle_start(void)
{
	struct cpufreq_interactive_cpuinfo *pcpu =
		&per_cpu(cpuinfo, smp_processor_id());
	int pending;

	if (!down_read_trylock(&pcpu->enable_sem))
		return;
	if (!pcpu->governor_enabled) {
		up_read(&pcpu->enable_sem);
		return;
	}

	pending = timer_pending(&pcpu->cpu_timer);

	if (pcpu->target_freq != pcpu->policy->min) {
		/*
		 * Entering idle while not at lowest speed.  On some
		 * platforms this can hold the other CPU(s) at that speed
		 * even though the CPU is idle. Set a timer to re-evaluate
		 * speed so this idle CPU doesn't hold the other CPUs above
		 * min indefinitely.  This should probably be a quirk of
		 * the CPUFreq driver.
		 */
		if (!pending)
			cpufreq_interactive_timer_resched(pcpu);
	}

	up_read(&pcpu->enable_sem);
}

static void cpufreq_interactive_idle_end(void)
{
	struct cpufreq_interactive_cpuinfo *pcpu =
		&per_cpu(cpuinfo, smp_processor_id());

	if (!down_read_trylock(&pcpu->enable_sem))
		return;
	if (!pcpu->governor_enabled) {
		up_read(&pcpu->enable_sem);
		return;
	}

	/* Arm the timer for 1-2 ticks later if not already. */
	if (!timer_pending(&pcpu->cpu_timer)) {
		cpufreq_interactive_timer_resched(pcpu);
	} else if (time_after_eq(jiffies, pcpu->cpu_timer.expires)) {
		del_timer(&pcpu->cpu_timer);
		del_timer(&pcpu->cpu_slack_timer);
		cpufreq_interactive_timer(smp_processor_id());
	}

	up_read(&pcpu->enable_sem);
}

#if 0
static int cpufreq_interactive_cpu_up_task(void *data)
{
	while(1)
	{
		unsigned long jiffies1,jiffies2,jiffies3;
		set_current_state(TASK_INTERRUPTIBLE);
		schedule();

		if (kthread_should_stop())
			break;
		set_current_state(TASK_RUNNING);

		jiffies1=jiffies2=jiffies3=jiffies;
		if(!is_platform_high_voltage())
		{
			if(!cpu_online(2)) {
				cpu_up(2);
				jiffies2=jiffies;
			}
			if(!cpu_online(3)) {
				cpu_up(3);
				jiffies3=jiffies;
			}
		}
		if(jiffies1!=jiffies2 || jiffies1!=jiffies3)
			printk("test: %lu %lu\n",jiffies2-jiffies1,jiffies3-jiffies1);
	}

}
#endif

extern struct device *get_cpu_device(unsigned cpu);
#define CPU_GO_OFFLINE(cpu_id)	\ 
	{if(cpu_online(cpu_id)) {	\
			dev = get_cpu_device(cpu_id);\
			device_offline(dev);\
	}}

#define CPU_GO_ONLINE(cpu_id)	\	
	{if(!cpu_online(cpu_id)) {	\
			dev = get_cpu_device(cpu_id);\
			device_online(dev);\
	}}

static DEFINE_MUTEX(interactive_pufreq_lock);
static int cpufreq_interactive_speedchange_task(void *data)
{

#ifdef RTK_FREQ_DRIVER
	unsigned int cpu;
	cpumask_t tmp_mask;
	unsigned long flags;
	unsigned int cores;
	unsigned int online_cores;
	int local_cpu_load[NR_CPUS];	
	struct cpufreq_interactive_cpuinfo *pcpu;
	current->flags &= ~PF_NOFREEZE;
	struct cpumask interactive_cpumask;
	struct device *dev;
//	cpumask_clear(&interactive_cpumask);
//	cpumask_set_cpu(0, &interactive_cpumask); // run task in core 3
//	cpumask_set_cpu(1, &interactive_cpumask); // run task in core 3
//	sched_setaffinity(0, &interactive_cpumask);
	while (1) {
		if (freezing(current))
		{
			int ix;
			//up all cpus
			for_each_possible_cpu(ix) {
				CPU_GO_ONLINE(ix);
			}
			try_to_freeze();
		}
		set_current_state(TASK_INTERRUPTIBLE);
		temperature=rtk_get_thermal_value();

		spin_lock_irqsave(&speedchange_cpumask_lock, flags);
		cores=allowed_cores;
		memcpy(local_cpu_load,cpu_load_for_speedchange,sizeof(local_cpu_load));
		if (cpumask_empty(&speedchange_cpumask)) {
			spin_unlock_irqrestore(&speedchange_cpumask_lock,
					       flags);

			schedule();

			if (kthread_should_stop())
				break;

			spin_lock_irqsave(&speedchange_cpumask_lock, flags);
		}

		set_current_state(TASK_RUNNING);
		tmp_mask = speedchange_cpumask;
		cpumask_clear(&speedchange_cpumask);

		spin_unlock_irqrestore(&speedchange_cpumask_lock, flags);

		for_each_cpu(cpu, &tmp_mask) {
			unsigned int j;
			unsigned int max_freq = 0;

			pcpu = &per_cpu(cpuinfo, cpu);
			if (!down_read_trylock(&pcpu->enable_sem))
				continue;
			if (!pcpu->governor_enabled) {
				up_read(&pcpu->enable_sem);
				continue;
			}

			for_each_cpu(j, pcpu->policy->cpus) {
				struct cpufreq_interactive_cpuinfo *pjcpu =
					&per_cpu(cpuinfo, j);

				if (pjcpu->target_freq > max_freq)
					max_freq = pjcpu->target_freq;
			}

			if ( pcpu->policy->cur!=pcpu->target_freq) {
				if(pcpu->target_freq>pcpu->policy->cur)
				__cpufreq_driver_target(pcpu->policy,
							pcpu->target_freq,
							CPUFREQ_RELATION_H);
				else
				__cpufreq_driver_target(pcpu->policy,
							pcpu->target_freq,
							CPUFREQ_RELATION_L);
			}

			trace_cpufreq_interactive_setspeed(cpu,
						     pcpu->target_freq,
						     pcpu->policy->cur);

			up_read(&pcpu->enable_sem);
		}
		if(cores==1) /* cores controlled by interrupt */
		{
			int ix=0;
			for_each_possible_cpu(ix) {			
				if(ix!=0 && cpu_online(ix))
					CPU_GO_OFFLINE(ix);
			}
		}
		else if(cores==2) //active 2 cores
		{
			int ix=0;
			int tmp;
			online_cores=0;//current online number
			for_each_possible_cpu(ix) { 		
				if(cpu_online(ix))
					online_cores++;
			}

			switch(online_cores)
			{
				case 4:
				case 3:
					if((local_cpu_load[1]!=-1) && (local_cpu_load[1]>=local_cpu_load[2]) && (local_cpu_load[1]>=local_cpu_load[3]))
					{
						CPU_GO_OFFLINE(2);
						CPU_GO_OFFLINE(3);
					}					
					else if((local_cpu_load[2]!=-1) && (local_cpu_load[2]>=local_cpu_load[1]) && (local_cpu_load[2]>=local_cpu_load[3]))
					{
						CPU_GO_OFFLINE(1);
						CPU_GO_OFFLINE(3);
					}
					else if((local_cpu_load[3]!=-1) && (local_cpu_load[3]>=local_cpu_load[1]) && (local_cpu_load[3]>=local_cpu_load[2]))
					{
						CPU_GO_OFFLINE(1);
						CPU_GO_OFFLINE(2);
					}				
					else
						pr_err("invalid cpu load on interactive step0\n");
					break;
					
				case 1:					
					if(!cpu_online(1))
					{	
						CPU_GO_ONLINE(1);					
					}
					else if(!cpu_online(2))
					{
						CPU_GO_ONLINE(2);											
					}
					else if(!cpu_online(3))
					{
						CPU_GO_ONLINE(3);																
					}
					break;
				default:
					break;
			}
		}
		else if(cores==3) //active 3 cores
		{	
			int ix=0;
			online_cores=0;
			for_each_possible_cpu(ix) { 		
				if(cpu_online(ix))
					online_cores++;
			}	
			switch(online_cores)
			{
				case 4: //choose lowest load to power down
					if((local_cpu_load[1]!=-1) && (local_cpu_load[1]<=local_cpu_load[2]) && (local_cpu_load[1]<=local_cpu_load[3]))
					{
						CPU_GO_OFFLINE(1);
					}					
					else if((local_cpu_load[2]!=-1) && (local_cpu_load[2]<=local_cpu_load[1]) && (local_cpu_load[2]<=local_cpu_load[3]))
					{
						CPU_GO_OFFLINE(2);					
					}
					else if((local_cpu_load[3]!=-1) && (local_cpu_load[3]<=local_cpu_load[1]) && (local_cpu_load[3]<=local_cpu_load[2]))
					{
						CPU_GO_OFFLINE(3);					
					}				
					else
						pr_err("invalid cpu load on interactive step2\n");
					break;	
				case 2:
					if(!cpu_online(1))
					{
						CPU_GO_ONLINE(1);					
					}
					else if(!cpu_online(2))
					{
						CPU_GO_ONLINE(2);											
					}
					else if(!cpu_online(3))
					{
						CPU_GO_ONLINE(3);
					}
					else
						pr_err("invalid cpu load on interactive step3\n");
					break;
				case 1:
					if(cpu_online(1))
					{
						CPU_GO_ONLINE(1);					
					}
					if(cpu_online(2))
					{
						CPU_GO_ONLINE(2);											
					}
					if((!cpu_online(1)) || (!cpu_online(2)) && (!cpu_online(3)))
					{
						CPU_GO_ONLINE(3);	
					}
					break;
			}
		}
		else	//active all cores
		{
			int ix=0;
			for_each_possible_cpu(ix) {			
				if(ix!=0 && (!cpu_online(ix)))
					CPU_GO_ONLINE(ix);
			}		
		}

	}
	do_exit(0);
#else
	unsigned int cpu;
	cpumask_t tmp_mask;
	unsigned long flags;
	struct cpufreq_interactive_cpuinfo *pcpu;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		spin_lock_irqsave(&speedchange_cpumask_lock, flags);

		if (cpumask_empty(&speedchange_cpumask)) {
			spin_unlock_irqrestore(&speedchange_cpumask_lock,
					       flags);
			schedule();

			if (kthread_should_stop())
				break;

			spin_lock_irqsave(&speedchange_cpumask_lock, flags);
		}

		set_current_state(TASK_RUNNING);
		tmp_mask = speedchange_cpumask;
		cpumask_clear(&speedchange_cpumask);
		spin_unlock_irqrestore(&speedchange_cpumask_lock, flags);

		for_each_cpu(cpu, &tmp_mask) {
			unsigned int j;
			unsigned int max_freq = 0;

			pcpu = &per_cpu(cpuinfo, cpu);
			if (!down_read_trylock(&pcpu->enable_sem))
				continue;
			if (!pcpu->governor_enabled) {
				up_read(&pcpu->enable_sem);
				continue;
			}

			for_each_cpu(j, pcpu->policy->cpus) {
				struct cpufreq_interactive_cpuinfo *pjcpu =
					&per_cpu(cpuinfo, j);

				if (pjcpu->target_freq > max_freq)
					max_freq = pjcpu->target_freq;
			}

			if (max_freq != pcpu->policy->cur)
				__cpufreq_driver_target(pcpu->policy,
							max_freq,
							CPUFREQ_RELATION_H);
			trace_cpufreq_interactive_setspeed(cpu,
						     pcpu->target_freq,
						     pcpu->policy->cur);

			up_read(&pcpu->enable_sem);
		}
	}
#endif
	return 0;
}

static void cpufreq_interactive_boost(struct cpufreq_interactive_tunables *tunables)
{
	int i;
	int anyboost = 0;
	unsigned long flags[2];
	struct cpufreq_interactive_cpuinfo *pcpu;

	tunables->boosted = true;

	spin_lock_irqsave(&speedchange_cpumask_lock, flags[0]);

	for_each_online_cpu(i) {
		pcpu = &per_cpu(cpuinfo, i);
		if (tunables != pcpu->policy->governor_data)
			continue;

		spin_lock_irqsave(&pcpu->target_freq_lock, flags[1]);
		if (pcpu->target_freq < tunables->hispeed_freq) {
			pcpu->target_freq = tunables->hispeed_freq;
			cpumask_set_cpu(i, &speedchange_cpumask);
			pcpu->hispeed_validate_time =
				ktime_to_us(ktime_get());
			anyboost = 1;
		}

		/*
		 * Set floor freq and (re)start timer for when last
		 * validated.
		 */

		pcpu->floor_freq = tunables->hispeed_freq;
		pcpu->floor_validate_time = ktime_to_us(ktime_get());
		spin_unlock_irqrestore(&pcpu->target_freq_lock, flags[1]);
	}

	spin_unlock_irqrestore(&speedchange_cpumask_lock, flags[0]);

	if (anyboost)
		wake_up_process(speedchange_task);
}

static int cpufreq_interactive_notifier(
	struct notifier_block *nb, unsigned long val, void *data)
{
	struct cpufreq_freqs *freq = data;
	struct cpufreq_interactive_cpuinfo *pcpu;
	int cpu;
	unsigned long flags;

	if (val == CPUFREQ_POSTCHANGE) {
		pcpu = &per_cpu(cpuinfo, freq->cpu);
		if (!down_read_trylock(&pcpu->enable_sem))
			return 0;
		if (!pcpu->governor_enabled) {
			up_read(&pcpu->enable_sem);
			return 0;
		}

		for_each_cpu(cpu, pcpu->policy->cpus) {
			struct cpufreq_interactive_cpuinfo *pjcpu =
				&per_cpu(cpuinfo, cpu);
			if (cpu != freq->cpu) {
				if (!down_read_trylock(&pjcpu->enable_sem))
					continue;
				if (!pjcpu->governor_enabled) {
					up_read(&pjcpu->enable_sem);
					continue;
				}
			}
			spin_lock_irqsave(&pjcpu->load_lock, flags);
			update_load(cpu);
			spin_unlock_irqrestore(&pjcpu->load_lock, flags);
			if (cpu != freq->cpu)
				up_read(&pjcpu->enable_sem);
		}

		up_read(&pcpu->enable_sem);
	}
	return 0;
}

static struct notifier_block cpufreq_notifier_block = {
	.notifier_call = cpufreq_interactive_notifier,
};

static unsigned int *get_tokenized_data(const char *buf, int *num_tokens)
{
	const char *cp;
	int i;
	int ntokens = 1;
	unsigned int *tokenized_data;
	int err = -EINVAL;

	cp = buf;
	while ((cp = strpbrk(cp + 1, " :")))
		ntokens++;

	if (!(ntokens & 0x1))
		goto err;

	tokenized_data = kmalloc(ntokens * sizeof(unsigned int), GFP_KERNEL);
	if (!tokenized_data) {
		err = -ENOMEM;
		goto err;
	}

	cp = buf;
	i = 0;
	while (i < ntokens) {
		if (sscanf(cp, "%u", &tokenized_data[i++]) != 1)
			goto err_kfree;

		cp = strpbrk(cp, " :");
		if (!cp)
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	*num_tokens = ntokens;
	return tokenized_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}

static ssize_t show_target_loads(
	struct cpufreq_interactive_tunables *tunables,
	char *buf)
{
	int i;
	ssize_t ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&tunables->target_loads_lock, flags);

	for (i = 0; i < tunables->ntarget_loads; i++)
		ret += sprintf(buf + ret, "%u%s", tunables->target_loads[i],
			       i & 0x1 ? ":" : " ");

	sprintf(buf + ret - 1, "\n");
	spin_unlock_irqrestore(&tunables->target_loads_lock, flags);
	return ret;
}

static ssize_t store_target_loads(
	struct cpufreq_interactive_tunables *tunables,
	const char *buf, size_t count)
{
	int ntokens;
	unsigned int *new_target_loads = NULL;
	unsigned long flags;

	new_target_loads = get_tokenized_data(buf, &ntokens);
	if (IS_ERR(new_target_loads))
		return PTR_RET(new_target_loads);

	spin_lock_irqsave(&tunables->target_loads_lock, flags);
	if (tunables->target_loads != default_target_loads)
		kfree(tunables->target_loads);
	tunables->target_loads = new_target_loads;
	tunables->ntarget_loads = ntokens;
	spin_unlock_irqrestore(&tunables->target_loads_lock, flags);
	return count;
}

static ssize_t show_above_hispeed_delay(
	struct cpufreq_interactive_tunables *tunables, char *buf)
{
	int i;
	ssize_t ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&tunables->above_hispeed_delay_lock, flags);

	for (i = 0; i < tunables->nabove_hispeed_delay; i++)
		ret += sprintf(buf + ret, "%u%s",
			       tunables->above_hispeed_delay[i],
			       i & 0x1 ? ":" : " ");

	sprintf(buf + ret - 1, "\n");
	spin_unlock_irqrestore(&tunables->above_hispeed_delay_lock, flags);
	return ret;
}

static ssize_t store_above_hispeed_delay(
	struct cpufreq_interactive_tunables *tunables,
	const char *buf, size_t count)
{
	int ntokens;
	unsigned int *new_above_hispeed_delay = NULL;
	unsigned long flags;

	new_above_hispeed_delay = get_tokenized_data(buf, &ntokens);
	if (IS_ERR(new_above_hispeed_delay))
		return PTR_RET(new_above_hispeed_delay);

	spin_lock_irqsave(&tunables->above_hispeed_delay_lock, flags);
	if (tunables->above_hispeed_delay != default_above_hispeed_delay)
		kfree(tunables->above_hispeed_delay);
	tunables->above_hispeed_delay = new_above_hispeed_delay;
	tunables->nabove_hispeed_delay = ntokens;
	spin_unlock_irqrestore(&tunables->above_hispeed_delay_lock, flags);
	return count;

}

static ssize_t show_hispeed_freq(struct cpufreq_interactive_tunables *tunables,
		char *buf)
{
	return sprintf(buf, "%u\n", tunables->hispeed_freq);
}

static ssize_t store_hispeed_freq(struct cpufreq_interactive_tunables *tunables,
		const char *buf, size_t count)
{
	int ret;
	long unsigned int val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->hispeed_freq = val;
	return count;
}

static ssize_t show_go_hispeed_load(struct cpufreq_interactive_tunables
		*tunables, char *buf)
{
	return sprintf(buf, "%lu\n", tunables->go_hispeed_load);
}

static ssize_t store_go_hispeed_load(struct cpufreq_interactive_tunables
		*tunables, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->go_hispeed_load = val;
	return count;
}

static ssize_t show_min_sample_time(struct cpufreq_interactive_tunables
		*tunables, char *buf)
{
	return sprintf(buf, "%lu\n", tunables->min_sample_time);
}

static ssize_t store_min_sample_time(struct cpufreq_interactive_tunables
		*tunables, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->min_sample_time = val;
	return count;
}

static ssize_t show_timer_rate(struct cpufreq_interactive_tunables *tunables,
		char *buf)
{
	return sprintf(buf, "%lu\n", tunables->timer_rate);
}

static ssize_t store_timer_rate(struct cpufreq_interactive_tunables *tunables,
		const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->timer_rate = val;
	return count;
}

static ssize_t show_timer_slack(struct cpufreq_interactive_tunables *tunables,
		char *buf)
{
	return sprintf(buf, "%d\n", tunables->timer_slack_val);
}

static ssize_t store_timer_slack(struct cpufreq_interactive_tunables *tunables,
		const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtol(buf, 10, &val);
	if (ret < 0)
		return ret;

	tunables->timer_slack_val = val;
	return count;
}

static ssize_t show_boost(struct cpufreq_interactive_tunables *tunables,
			  char *buf)
{
	return sprintf(buf, "%d\n", tunables->boost_val);
}

static ssize_t store_boost(struct cpufreq_interactive_tunables *tunables,
			   const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->boost_val = val;

	if (tunables->boost_val) {
		trace_cpufreq_interactive_boost("on");
		if (!tunables->boosted)
			cpufreq_interactive_boost(tunables);
	} else {
		tunables->boostpulse_endtime = ktime_to_us(ktime_get());
		trace_cpufreq_interactive_unboost("off");
	}

	return count;
}

static ssize_t store_boostpulse(struct cpufreq_interactive_tunables *tunables,
				const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->boostpulse_endtime = ktime_to_us(ktime_get()) +
		tunables->boostpulse_duration_val;
	trace_cpufreq_interactive_boost("pulse");
	if (!tunables->boosted)
		cpufreq_interactive_boost(tunables);
	return count;
}

static ssize_t show_boostpulse_duration(struct cpufreq_interactive_tunables
		*tunables, char *buf)
{
	return sprintf(buf, "%d\n", tunables->boostpulse_duration_val);
}

static ssize_t store_boostpulse_duration(struct cpufreq_interactive_tunables
		*tunables, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables->boostpulse_duration_val = val;
	return count;
}

static ssize_t show_io_is_busy(struct cpufreq_interactive_tunables *tunables,
		char *buf)
{
	return sprintf(buf, "%u\n", tunables->io_is_busy);
}

static ssize_t store_io_is_busy(struct cpufreq_interactive_tunables *tunables,
		const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;
	tunables->io_is_busy = val;
	return count;
}

/*
 * Create show/store routines
 * - sys: One governor instance for complete SYSTEM
 * - pol: One governor instance per struct cpufreq_policy
 */
#define show_gov_pol_sys(file_name)					\
static ssize_t show_##file_name##_gov_sys				\
(struct kobject *kobj, struct attribute *attr, char *buf)		\
{									\
	return show_##file_name(common_tunables, buf);			\
}									\
									\
static ssize_t show_##file_name##_gov_pol				\
(struct cpufreq_policy *policy, char *buf)				\
{									\
	return show_##file_name(policy->governor_data, buf);		\
}

#define store_gov_pol_sys(file_name)					\
static ssize_t store_##file_name##_gov_sys				\
(struct kobject *kobj, struct attribute *attr, const char *buf,		\
	size_t count)							\
{									\
	return store_##file_name(common_tunables, buf, count);		\
}									\
									\
static ssize_t store_##file_name##_gov_pol				\
(struct cpufreq_policy *policy, const char *buf, size_t count)		\
{									\
	return store_##file_name(policy->governor_data, buf, count);	\
}

#define show_store_gov_pol_sys(file_name)				\
show_gov_pol_sys(file_name);						\
store_gov_pol_sys(file_name)

show_store_gov_pol_sys(target_loads);
show_store_gov_pol_sys(above_hispeed_delay);
show_store_gov_pol_sys(hispeed_freq);
show_store_gov_pol_sys(go_hispeed_load);
show_store_gov_pol_sys(min_sample_time);
show_store_gov_pol_sys(timer_rate);
show_store_gov_pol_sys(timer_slack);
show_store_gov_pol_sys(boost);
store_gov_pol_sys(boostpulse);
show_store_gov_pol_sys(boostpulse_duration);
show_store_gov_pol_sys(io_is_busy);

#define gov_sys_attr_rw(_name)						\
static struct global_attr _name##_gov_sys =				\
__ATTR(_name, 0644, show_##_name##_gov_sys, store_##_name##_gov_sys)

#define gov_pol_attr_rw(_name)						\
static struct freq_attr _name##_gov_pol =				\
__ATTR(_name, 0644, show_##_name##_gov_pol, store_##_name##_gov_pol)

#define gov_sys_pol_attr_rw(_name)					\
	gov_sys_attr_rw(_name);						\
	gov_pol_attr_rw(_name)

gov_sys_pol_attr_rw(target_loads);
gov_sys_pol_attr_rw(above_hispeed_delay);
gov_sys_pol_attr_rw(hispeed_freq);
gov_sys_pol_attr_rw(go_hispeed_load);
gov_sys_pol_attr_rw(min_sample_time);
gov_sys_pol_attr_rw(timer_rate);
gov_sys_pol_attr_rw(timer_slack);
gov_sys_pol_attr_rw(boost);
gov_sys_pol_attr_rw(boostpulse_duration);
gov_sys_pol_attr_rw(io_is_busy);

static struct global_attr boostpulse_gov_sys =
	__ATTR(boostpulse, 0200, NULL, store_boostpulse_gov_sys);

static struct freq_attr boostpulse_gov_pol =
	__ATTR(boostpulse, 0200, NULL, store_boostpulse_gov_pol);

/* One Governor instance for entire system */
static struct attribute *interactive_attributes_gov_sys[] = {
	&target_loads_gov_sys.attr,
	&above_hispeed_delay_gov_sys.attr,
	&hispeed_freq_gov_sys.attr,
	&go_hispeed_load_gov_sys.attr,
	&min_sample_time_gov_sys.attr,
	&timer_rate_gov_sys.attr,
	&timer_slack_gov_sys.attr,
	&boost_gov_sys.attr,
	&boostpulse_gov_sys.attr,
	&boostpulse_duration_gov_sys.attr,
	&io_is_busy_gov_sys.attr,
	NULL,
};

static struct attribute_group interactive_attr_group_gov_sys = {
	.attrs = interactive_attributes_gov_sys,
	.name = "interactive",
};

/* Per policy governor instance */
static struct attribute *interactive_attributes_gov_pol[] = {
	&target_loads_gov_pol.attr,
	&above_hispeed_delay_gov_pol.attr,
	&hispeed_freq_gov_pol.attr,
	&go_hispeed_load_gov_pol.attr,
	&min_sample_time_gov_pol.attr,
	&timer_rate_gov_pol.attr,
	&timer_slack_gov_pol.attr,
	&boost_gov_pol.attr,
	&boostpulse_gov_pol.attr,
	&boostpulse_duration_gov_pol.attr,
	&io_is_busy_gov_pol.attr,
	NULL,
};

static struct attribute_group interactive_attr_group_gov_pol = {
	.attrs = interactive_attributes_gov_pol,
	.name = "interactive",
};

static struct attribute_group *get_sysfs_attr(void)
{
	if (have_governor_per_policy())
		return &interactive_attr_group_gov_pol;
	else
		return &interactive_attr_group_gov_sys;
}

static int cpufreq_interactive_idle_notifier(struct notifier_block *nb,
					     unsigned long val,
					     void *data)
{
	switch (val) {
	case IDLE_START:
		cpufreq_interactive_idle_start();
		break;
	case IDLE_END:
		cpufreq_interactive_idle_end();
		break;
	}

	return 0;
}

static struct notifier_block cpufreq_interactive_idle_nb = {
	.notifier_call = cpufreq_interactive_idle_notifier,
};

static int cpufreq_governor_interactive(struct cpufreq_policy *policy,
		unsigned int event)
{
	int rc;
	unsigned int j;
	struct cpufreq_interactive_cpuinfo *pcpu;
	struct cpufreq_frequency_table *freq_table;
	struct cpufreq_interactive_tunables *tunables;
	unsigned long flags;

	if (have_governor_per_policy())
		tunables = policy->governor_data;
	else
		tunables = common_tunables;

	WARN_ON(!tunables && (event != CPUFREQ_GOV_POLICY_INIT));

	switch (event) {
	case CPUFREQ_GOV_POLICY_INIT:
		if (have_governor_per_policy()) {
			WARN_ON(tunables);
		} else if (tunables) {
			tunables->usage_count++;
			policy->governor_data = tunables;
			return 0;
		}

		tunables = kzalloc(sizeof(*tunables), GFP_KERNEL);
		if (!tunables) {
			pr_err("%s: POLICY_INIT: kzalloc failed\n", __func__);
			return -ENOMEM;
		}

		tunables->usage_count = 1;
		tunables->above_hispeed_delay = default_above_hispeed_delay;
		tunables->nabove_hispeed_delay =
			ARRAY_SIZE(default_above_hispeed_delay);
		tunables->go_hispeed_load = DEFAULT_GO_HISPEED_LOAD;
		tunables->target_loads = default_target_loads;
		tunables->ntarget_loads = ARRAY_SIZE(default_target_loads);
		tunables->min_sample_time = DEFAULT_MIN_SAMPLE_TIME;
		tunables->timer_rate = DEFAULT_TIMER_RATE;
		tunables->boostpulse_duration_val = DEFAULT_MIN_SAMPLE_TIME;
		tunables->timer_slack_val = DEFAULT_TIMER_SLACK;

		spin_lock_init(&tunables->target_loads_lock);
		spin_lock_init(&tunables->above_hispeed_delay_lock);

		policy->governor_data = tunables;
		if (!have_governor_per_policy())
			common_tunables = tunables;

		rc = sysfs_create_group(get_governor_parent_kobj(policy),
				get_sysfs_attr());
		if (rc) {
			kfree(tunables);
			policy->governor_data = NULL;
			if (!have_governor_per_policy())
				common_tunables = NULL;
			return rc;
		}

		if (!policy->governor->initialized) {
			idle_notifier_register(&cpufreq_interactive_idle_nb);
			cpufreq_register_notifier(&cpufreq_notifier_block,
					CPUFREQ_TRANSITION_NOTIFIER);
		}

		break;

	case CPUFREQ_GOV_POLICY_EXIT:
		if (!--tunables->usage_count) {
			if (policy->governor->initialized == 1) {
				cpufreq_unregister_notifier(&cpufreq_notifier_block,
						CPUFREQ_TRANSITION_NOTIFIER);
				idle_notifier_unregister(&cpufreq_interactive_idle_nb);
			}

			sysfs_remove_group(get_governor_parent_kobj(policy),
					get_sysfs_attr());
			kfree(tunables);
			common_tunables = NULL;
		}

		policy->governor_data = NULL;
		break;

	case CPUFREQ_GOV_START:
		mutex_lock(&gov_lock);

		freq_table = cpufreq_frequency_get_table(policy->cpu);
		if (!tunables->hispeed_freq)
			tunables->hispeed_freq = policy->max;

		for_each_cpu(j, policy->cpus) {
			pcpu = &per_cpu(cpuinfo, j);
			pcpu->policy = policy;
			pcpu->target_freq = policy->cur;
			pcpu->freq_table = freq_table;
			pcpu->floor_freq = pcpu->target_freq;
			pcpu->floor_validate_time =
				ktime_to_us(ktime_get());
			pcpu->hispeed_validate_time =
				pcpu->floor_validate_time;
			pcpu->max_freq = policy->max;
			down_write(&pcpu->enable_sem);
			del_timer_sync(&pcpu->cpu_timer);
			del_timer_sync(&pcpu->cpu_slack_timer);
			cpufreq_interactive_timer_start(tunables, j);
			pcpu->governor_enabled = 1;
			up_write(&pcpu->enable_sem);
		}

		mutex_unlock(&gov_lock);
		break;

	case CPUFREQ_GOV_STOP:
		mutex_lock(&gov_lock);
		for_each_cpu(j, policy->cpus) {
			pcpu = &per_cpu(cpuinfo, j);
			down_write(&pcpu->enable_sem);
			pcpu->governor_enabled = 0;
			del_timer_sync(&pcpu->cpu_timer);
			del_timer_sync(&pcpu->cpu_slack_timer);
			up_write(&pcpu->enable_sem);
		}

		mutex_unlock(&gov_lock);
		break;

	case CPUFREQ_GOV_LIMITS:
		if (policy->max < policy->cur)
			__cpufreq_driver_target(policy,
					policy->max, CPUFREQ_RELATION_H);
		else if (policy->min > policy->cur)
			__cpufreq_driver_target(policy,
					policy->min, CPUFREQ_RELATION_L);
		for_each_cpu(j, policy->cpus) {
			pcpu = &per_cpu(cpuinfo, j);

			down_read(&pcpu->enable_sem);
			if (pcpu->governor_enabled == 0) {
				up_read(&pcpu->enable_sem);
				continue;
			}

			spin_lock_irqsave(&pcpu->target_freq_lock, flags);
			if (policy->max < pcpu->target_freq)
				pcpu->target_freq = policy->max;
			else if (policy->min > pcpu->target_freq)
				pcpu->target_freq = policy->min;

			spin_unlock_irqrestore(&pcpu->target_freq_lock, flags);
			up_read(&pcpu->enable_sem);

			/* Reschedule timer only if policy->max is raised.
			 * Delete the timers, else the timer callback may
			 * return without re-arm the timer when failed
			 * acquire the semaphore. This race may cause timer
			 * stopped unexpectedly.
			 */

			if (policy->max > pcpu->max_freq) {
				down_write(&pcpu->enable_sem);
				del_timer_sync(&pcpu->cpu_timer);
				del_timer_sync(&pcpu->cpu_slack_timer);
				cpufreq_interactive_timer_start(tunables, j);
				up_write(&pcpu->enable_sem);
			}

			pcpu->max_freq = policy->max;
		}
		break;
	}
	return 0;
}

#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_INTERACTIVE
static
#endif
struct cpufreq_governor cpufreq_gov_interactive = {
	.name = "interactive",
	.governor = cpufreq_governor_interactive,
	.max_transition_latency = 10000000,
	.owner = THIS_MODULE,
};

static void cpufreq_interactive_nop_timer(unsigned long data)
{
}

static int __init cpufreq_interactive_init(void)
{
	unsigned int i;
	struct cpufreq_interactive_cpuinfo *pcpu;
	struct sched_param param = { .sched_priority = MAX_RT_PRIO-1 };

	/* Initalize per-cpu timers */
	for_each_possible_cpu(i) {
		pcpu = &per_cpu(cpuinfo, i);
		init_timer_deferrable(&pcpu->cpu_timer);
		pcpu->cpu_timer.function = cpufreq_interactive_timer;
		pcpu->cpu_timer.data = i;
		init_timer(&pcpu->cpu_slack_timer);
		pcpu->cpu_slack_timer.function = cpufreq_interactive_nop_timer;
		spin_lock_init(&pcpu->load_lock);
		spin_lock_init(&pcpu->target_freq_lock);
		init_rwsem(&pcpu->enable_sem);
	}

	spin_lock_init(&speedchange_cpumask_lock);
	mutex_init(&gov_lock);
	speedchange_task =
		kthread_create(cpufreq_interactive_speedchange_task, NULL,
			       "cfinteractive");
#if 0
	cpu_up_task =
		kthread_create(cpufreq_interactive_cpu_up_task, NULL,
			       "cpu_up_task");
#endif

	if (IS_ERR(speedchange_task))
		return PTR_ERR(speedchange_task);

	sched_setscheduler_nocheck(speedchange_task, SCHED_FIFO, &param);
	get_task_struct(speedchange_task);

	/* NB: wake up so the thread does not look hung to the freezer */
	wake_up_process(speedchange_task);
#if 0
	if (IS_ERR(cpu_up_task))
		return PTR_ERR(cpu_up_task);

	sched_setscheduler_nocheck(cpu_up_task, SCHED_FIFO, &param);
	get_task_struct(cpu_up_task);


	/* NB: wake up so the thread does not look hung to the freezer */
	wake_up_process(cpu_up_task);
#endif

	return cpufreq_register_governor(&cpufreq_gov_interactive);
}

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_INTERACTIVE
fs_initcall(cpufreq_interactive_init);
#else
module_init(cpufreq_interactive_init);
#endif

static void __exit cpufreq_interactive_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_interactive);
	kthread_stop(speedchange_task);
	put_task_struct(speedchange_task);
}

module_exit(cpufreq_interactive_exit);

MODULE_AUTHOR("Mike Chan <mike@android.com>");
MODULE_DESCRIPTION("'cpufreq_interactive' - A cpufreq governor for "
	"Latency sensitive workloads");
MODULE_LICENSE("GPL");
