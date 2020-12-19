#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "rdvb_dmx_ctrl_internal.h"

bool dmx_ecp_status  = false;
struct task_struct * dmx_guarding = NULL;

static void check_ecp_status(void)
{
	unsigned char ecp_status_temp;
	bool ecp_status;
	RHAL_TP_CheckProtectStatus(TP_TP0, &ecp_status_temp);
	ecp_status = (bool)ecp_status_temp;
	
	if (ecp_status == dmx_ecp_status)
		return;
	if (ecp_status) {
		dmx_crit(NOCH, "%s_%d: ECP ON\n", __func__, __LINE__);
		rdvb_dmx_ctrl_ecp_on();
		dmx_ecp_status = ecp_status;
		return;
	} else {
		dmx_crit(NOCH, "%s_%d: ECP OFF\n", __func__, __LINE__);
		rdvb_dmx_ctrl_ecp_off();
		dmx_ecp_status = ecp_status;
		return;
	}

}
static int dmx_guard_thread(void * param)
{
	while (!kthread_should_stop()){
		set_freezable();
		check_ecp_status();
		msleep(10);
	}
	return 0;
}

void dmx_service_on(void)
{
	dmx_guarding = kthread_run(dmx_guard_thread, NULL, "dmx_service");
	if (IS_ERR(dmx_guarding))
		return ;
	dmx_crit(NOCH, "%s_%d: dmx service ON\n", __func__, __LINE__);
}
void dmx_service_off(void)
{
	if (dmx_guarding)
		kthread_stop(dmx_guarding);
	dmx_guarding = 0;
	dmx_crit(NOCH, "%s_%d: dmx service ON\n", __func__, __LINE__);

}