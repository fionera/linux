/* URB Trace fuction for URB double submit debug */

//#define URB_TRACE

struct urb_action_log
{
    int             cpu_id;
    unsigned long   time_stamp;
    struct urb *    urb;
    struct urb      urb_snap_shot;
    unsigned long   action;
    unsigned long   hcpriv;
    int             pipe;
    int             transfer_flags;
    int             arg;
    __le16          idVendor;   // back up urb device vid
    __le16          idProduct;  // back up urb device vid
        
#define CREATE_URB     (1)
#define SUBMIT_URB     (1<<1)
#define CANCEL_URB     (1<<2)
#define FREE_URB       (1<<3)
#define DESTROY_URB    (1<<4)
#define QUEUE_URB      (1<<5)
#define GIVEBACK_URB   (1<<6)
#define ERROR_URB      (1<<7)
#define INIT_URB       (1<<8)
#define XHCI_QUEUE_URB (1<<9)
#define XHCI_ALLOC_HCDPRIV (1<<10)
};

#define MAX_URB_LOG_SIZE        2048

#define urb_warning(fmt, args...)  printk(KERN_WARNING "[URB][WARN]" fmt, ## args)

#ifdef URB_TRACE

#define urb_trace(fmt, args...)  printk("[URB][TRACE]" fmt, ## args)
extern void back_trace_urb_action(struct urb* urb);
extern void log_urb_action(struct urb* urb, unsigned int action);
extern void log_urb_action_ex(struct urb* urb, unsigned int action, unsigned long arg);
extern const char* urb_action_str(unsigned int action);

#else

#define urb_trace(fmt, args...)  do {} while(0) 
#define back_trace_urb_action(urb)
#define log_urb_action(urb, action)
#define log_urb_action_ex(urb, action, arg)
#define urb_action_str(action)

#endif
