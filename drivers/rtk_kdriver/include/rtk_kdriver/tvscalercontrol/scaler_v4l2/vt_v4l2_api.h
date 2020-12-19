/*
 *
 *	VSC v4l2 related api header file
 *
 *  drievr vsc internal api from vsc device
 */
#ifndef _VT_V4L2_API_H
#define _VT_V4L2_API_H


struct vt_v4l2_device {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd;
};

//v4l2 ioctrl callback api
int vt_v4l2_ioctl_s_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls);
int vt_v4l2_ioctl_g_ext_ctrls(struct file *file, void *fh, struct v4l2_ext_controls *ctrls);
int vt_v4l2_ioctl_s_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl);
int vt_v4l2_ioctl_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl);
int vt_v4l2_ioctl_subscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub);
int vt_v4l2_ioctl_unsubscribe_event(struct v4l2_fh *fh, const struct v4l2_event_subscription *sub);

//vt internal api

int vt_v4l2_ioctl_querycap(struct file *file, void *fh, struct v4l2_capability *cap);
int vt_v4l2_ioctl_reqbufs(struct file *file, void *fh, struct v4l2_requestbuffers *b);
int vt_v4l2_ioctl_querybuf(struct file *file, void *fh, struct v4l2_buffer *b);
int vt_v4l2_ioctl_qbuf(struct file *file, void *fh, struct v4l2_buffer *b);
int vt_v4l2_ioctl_dqbuf  (struct file *file, void *fh, struct v4l2_buffer *b);
int vt_v4l2_ioctl_streamon(struct file *file, void *fh, enum v4l2_buf_type i);
int vt_v4l2_ioctl_streamoff(struct file *file, void *fh, enum v4l2_buf_type i);
int vt_v4l2_ioctl_s_fmt_vid_cap    (struct file *file, void *fh, struct v4l2_format *f);
int vt_v4l2_ioctl_g_fmt_vid_cap    (struct file *file, void *fh, struct v4l2_format *f);


#endif
