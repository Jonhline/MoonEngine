/* P: the PC renderer's frame profiler, bench timing and speed post-effect
   (video_gl.c, vk_*.cpp) have no deko3d counterpart yet. The shared race code
   calls them, so the Switch build answers with nothing measured and no effect. */
#include <stdint.h>

void video_profile_bucket(int bucket, uint64_t ns)
{
    (void)bucket;
    (void)ns;
}
void video_set_bench_finish(int on)
{
    (void)on;
}
uint64_t video_bench_finish_ns(void)
{
    return 0;
}
void video_bench_reset(void)
{
}
uint64_t video_frame_work_ns(void)
{
    return 0;
}
void video_set_speed_post(int blur, int amount, int vignette)
{
    (void)blur;
    (void)amount;
    (void)vignette;
}
void moon_vk_gpu_mark(const char *label)
{
    (void)label;
}
