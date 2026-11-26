#ifndef GPU_VIS_H
#define GPU_VIS_H
/* GPU visualizer path (Piglet ES2). Compiled always, called only when
 * VIS_GPU_TAKEOVER is defined, and then only after gpu_vis_init() has reported a
 * usable surface; it otherwise fails closed and the canvas draws instead. The
 * env-var gate this comment used to describe no longer exists - the console has
 * no shell to set one. See gpu_vis.c. */
int  gpu_vis_init(int w, int h);
void gpu_vis_set_surface(void *egl_surface);
void gpu_vis_select(int style);
void gpu_vis_frame(float dt, float level);
void gpu_vis_shutdown(void);
int  gpu_vis_look_count(void);
const char *gpu_vis_look_name(int i);
#endif
