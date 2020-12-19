/*
 *
 *	VBI v4l2 related api header file
 *
 *  drievr vsc internal api from vsc device
 */
#ifndef _VBI_V4L2_API_H
#define _VBI_V4L2_API_H

struct vbi_v4l2_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
};

//v4l2 ioctrl callback api
int vbi_v4l2_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls);
int vbi_v4l2_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls);
int vbi_v4l2_ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl);
int vbi_v4l2_ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl);
int vbi_v4l2_ioctl_s_fmt_vbi_cap   (struct file *file, void *fh, struct v4l2_format *fmt);
int vbi_v4l2_ioctl_g_fmt_vbi_cap   (struct file *file, void *fh, struct v4l2_format *fmt);
int vbi_v4l2_ioctl_g_fmt_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_format *fmt);
int vbi_v4l2_ioctl_s_fmt_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_format *fmt);
int vbi_v4l2_ioctl_streamon(struct file *file, void *fh, enum v4l2_buf_type type);
int vbi_v4l2_ioctl_streamoff(struct file *file, void *fh, enum v4l2_buf_type type);
int vbi_v4l2_get_vbi_service(void);
int vbi_v4l2_open(struct file *file);
int vbi_v4l2_release(struct file *file);
int vbi_v4l2_read(struct file *file, char __user *data, size_t count, loff_t *ppos);
unsigned int vbi_v4l2_poll(struct file *file, poll_table *wait);
int vbi_v4l2_mmap(struct file *file, struct vm_area_struct *vma);

//vbi internal api

#endif
