typedef struct {
    const char *disc;
    char name[64];
    double tempo;
    gt4_music *music;
    unsigned sequence, count;
    int played;
    gt4_music **playlist;
    SDL_atomic_t *track_ready, *track_frames;
    SDL_atomic_t frames;
    SDL_atomic_t ready, volume, current_sequence;
    SDL_Thread *thread;
} music_job;
static void render_menu_music(int16_t *out, int frames, void *user)
{
    music_job *job = user;
    if (job->playlist) {
        unsigned sequence = (unsigned)SDL_AtomicGet(&job->current_sequence);
        gt4_music *tracks[256] = {0};
        unsigned available[256] = {0};
        for (unsigned i = 0; i < job->count; i++)
            if (SDL_AtomicGet(&job->track_ready[i])) {
                tracks[i] = job->playlist[i];
                available[i] = (unsigned)SDL_AtomicGet(&job->track_frames[i]);
            }
        if (!strcmp(job->name, "map"))
            sequence = gt4_music_render_playlist_available(out, frames, tracks, available,
                                                           job->count, sequence);
        else
            gt4_music_render_available(out, frames, tracks[sequence], available[sequence]);
        SDL_AtomicSet(&job->current_sequence, (int)sequence);
    } else
        gt4_music_render_available(out, frames, job->music, (unsigned)SDL_AtomicGet(&job->frames));
    int volume = SDL_AtomicGet(&job->volume);
    if (volume != 120)
        for (int i = 0; i < frames * 2; i++)
            out[i] = (int16_t)((int)out[i] * volume / 120);
}
typedef struct {
    music_job *job;
    unsigned sequence;
    int first;
} music_prefix;
static void publish_music_prefix(void *user, unsigned frames)
{
    music_prefix *prefix = user;
    music_job *j = prefix->job;
    if (j->playlist) {
        SDL_AtomicSet(&j->track_frames[prefix->sequence], (int)frames);
        if (frames >= 4096) {
            SDL_AtomicSet(&j->track_ready[prefix->sequence], 1);
            if (prefix->first)
                SDL_AtomicSet(&j->ready, 1);
        }
    } else {
        SDL_AtomicSet(&j->frames, (int)frames);
        if (frames >= 4096)
            SDL_AtomicSet(&j->ready, 1);
    }
}
static int prepare_music(void *arg)
{
    music_job *j = arg;
    const char *err = NULL;
    gt4_vol *v = gt4_vol_open(j->disc, &err);
    if (v) {
        unsigned count = gt4_music_sequence_count(v, j->name);
        j->count = count;
        j->sequence = count ? j->sequence % count : 0;
        if (count > 1 && count <= 256) {
            j->playlist = calloc(count, sizeof *j->playlist);
            j->track_ready = calloc(count, sizeof *j->track_ready);
            j->track_frames = calloc(count, sizeof *j->track_frames);
            if (j->playlist && j->track_ready && j->track_frames) {
                /* Publish the first track immediately; prefetch the others
                   without delaying startup or allocating in the callback. */
                unsigned first = j->sequence;
                SDL_AtomicSet(&j->current_sequence, (int)first);
                for (unsigned offset = 0; offset < count; offset++) {
                    unsigned i = (first + offset) % count;
                    gt4_music *track = gt4_music_open_sequence(v, j->name, i);
                    j->playlist[i] = track;
                    if (!offset)
                        j->music = track;
                    if (!track || !gt4_music_prepare_begin(track, j->tempo)) {
                        gt4_music_close(track);
                        j->playlist[i] = NULL;
                        if (!offset)
                            j->music = NULL;
                        fprintf(stderr, "music: cannot prepare %s sequence=%u\n", j->name, i);
                        continue;
                    }
                    j->playlist[i] = track;
                }
                /* Round robin: every possible next menu selection receives
                   its startup buffer before any whole song is rendered. */
                int more;
                do {
                    more = 0;
                    for (unsigned offset = 0; offset < count; offset++) {
                        unsigned i = (first + offset) % count;
                        music_prefix prefix = {j, i, !offset};
                        int pending =
                            gt4_music_prepare_step(j->playlist[i], publish_music_prefix, &prefix);
                        more |= pending;
                        if (!pending && j->playlist[i] && SDL_AtomicGet(&j->track_frames[i])) {
                            SDL_AtomicSet(&j->track_ready[i], 1);
                            if (!offset)
                                SDL_AtomicSet(&j->ready, 1);
                        }
                    }
                } while (more);
            } else {
                free(j->playlist);
                j->playlist = NULL;
                free(j->track_ready);
                j->track_ready = NULL;
                free(j->track_frames);
                j->track_frames = NULL;
            }
        } else {
            j->music = gt4_music_open_sequence(v, j->name, j->sequence);
            music_prefix prefix = {j, 0, 1};
            if (j->music &&
                !gt4_music_prepare_stream(j->music, j->tempo, publish_music_prefix, &prefix)) {
                gt4_music_close(j->music);
                j->music = NULL;
            }
        }
        if (!j->playlist)
            SDL_AtomicSet(&j->current_sequence, (int)j->sequence);
        gt4_vol_close(v);
    }
    SDL_AtomicSet(&j->ready, 1);
    return 0;
}

/* The disc requests BGM before the page's authored panIn/Sync. Begin IO
   there, rather than after the synchronous reveal has already completed. */
static music_job *preparing_jobs;
static int *preparing_count;
static const char *preparing_disc;
static double preparing_tempo;
static pregrid_audio *preparing_pcm;
/* startStream is synchronous in the script, but decoding is a PC worker.
   Pump publication inside Sync as well as the outer event loop, otherwise
   get_car waits 4+1+3 seconds before its PCM can begin (L14542..14640). */
static void update_present_pcm(menu *m)
{
    if (!preparing_pcm || !m)
        return;
    int volume = 0;
    const char *group = menu_pcm_request(m, &volume);
    if (!group)
        return;
    /* Consume the outgoing BGM fade before publishing the new stream.
       The outer MIDI bookkeeping may still name its old owner. */
    if (*group && music_faded) {
        plat_audio_set_music(NULL, NULL);
        music_faded = 0;
    }
    int sequence, repeat;
    unsigned serial;
    menu_pcm_parameters(m, &sequence, &repeat, &serial);
    pregrid_audio_update_gated(preparing_pcm, group, volume, (uint32_t)plat_time_ns(), sequence,
                               repeat, serial, !loading_screen_active());
    pregrid_audio_pause(preparing_pcm, menu_pcm_paused(m));
    menu_pcm_state(m, pregrid_audio_playing(preparing_pcm));
}

static void prepare_menu_audio(menu *m, void *user)
{
    (void)user;
    int volume = 0;
    const char *group = menu_pcm_request(m, &volume);
    if (group && *group) {
        int sequence, repeat;
        unsigned serial;
        menu_pcm_parameters(m, &sequence, &repeat, &serial);
        pregrid_audio_update_gated(preparing_pcm, group, volume, (uint32_t)plat_time_ns(), sequence,
                                   repeat, serial, 0);
    }
    const char *name = menu_music_name(m);
    if (!preparing_jobs || !name || !*name)
        return;
    for (int i = 0; i < *preparing_count; i++)
        if (!strcmp(preparing_jobs[i].name, name))
            return;
    if (*preparing_count >= 32)
        return;
    music_job *job = &preparing_jobs[(*preparing_count)++];
    job->disc = preparing_disc;
    job->tempo = preparing_tempo;
    snprintf(job->name, sizeof job->name, "%s", name);
    SDL_AtomicSet(&job->volume, 120);
    job->thread = SDL_CreateThread(prepare_music, "menu music preparation", job);
    if (!job->thread)
        SDL_AtomicSet(&job->ready, 1);
}
