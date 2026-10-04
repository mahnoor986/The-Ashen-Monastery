/* post.h - low-resolution 3D rendering + the post-process look.
 * The 3D scene is drawn into a small render texture (POST_W x POST_H, nearest filtering), then
 * scaled up to the full virtual screen through assets/shaders/post.fs (colour grade, grain,
 * vignette, chromatic offset, PS1 dither). UI is drawn afterwards at full resolution. */
#ifndef POST_MODULE_H
#define POST_MODULE_H

#include "raylib.h"

void Post_Init(void);
void Post_Shutdown(void);
void Post_BeginScene(void);          /* start drawing the 3D scene (low-res) - NOT inside Screen_Begin */
void Post_EndScene(void);
/* Draw the low-res scene over the whole virtual screen (call inside Screen_Begin).
 * gradeMode: 0 = red horror, 1 = warm gold. pulse: 0..1 red flash. */
void Post_Draw(float time, int gradeMode, float pulse);
void Post_Toggle(void);              /* F2: effect on/off */
bool Post_Enabled(void);

#endif
