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
        if (frames >= PLAT_SDL_MUSIC_PREROLL) {
            SDL_AtomicSet(&j->track_ready[prefix->sequence], 1);
            if (prefix->first)
                SDL_AtomicSet(&j->ready, 1);
        }
    } else {
        SDL_AtomicSet(&j->frames, (int)frames);
        if (frames >= PLAT_SDL_MUSIC_PREROLL)
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
typedef struct {
    music_job *jobs, *playing;
    int *count;
    const char *disc;
    double tempo;
    pregrid_audio *pcm;
    menu *owner;
    int enabled, allowed, playing_active;
    char desired[64];
} menu_audio_context;
/* Existing presentation callbacks borrow the app-owned transport state. */
static menu_audio_context *preparing_audio;
static void update_present_midi(menu *m);
/* startStream is synchronous in the script, but decoding is a PC worker.
   Pump publication inside Sync as well as the outer event loop, otherwise
   get_car waits 4+1+3 seconds before its PCM can begin (L14542..14640). */
static void update_present_pcm(menu *m)
{
    if (!m)
        return;
    update_present_midi(m);
    if (!preparing_audio || !preparing_audio->pcm)
        return;
    int volume = 0;
    const char *group = menu_pcm_request(m, &volume);
    if (!group)
        return;
    /* Consume the outgoing BGM fade before publishing the new stream.
       The outer MIDI bookkeeping may still name its old owner. */
    if (*group && music_faded) {
        if (plat_sdl_music_fading())
            return;
        plat_audio_set_music(NULL, NULL);
        music_faded = 0;
    }
    int sequence, repeat;
    unsigned serial;
    menu_pcm_parameters(m, &sequence, &repeat, &serial);
    pregrid_audio_update_gated(preparing_audio->pcm, group, volume, (uint32_t)plat_time_ns(), sequence,
                               repeat, serial, !loading_screen_active());
    pregrid_audio_pause(preparing_audio->pcm, menu_pcm_paused(m));
    menu_pcm_state(m, pregrid_audio_playing(preparing_audio->pcm));
}

static void prepare_menu_audio(menu *m, void *user)
{
    (void)user;
    if (!preparing_audio)
        return;
    int volume = 0;
    const char *group = menu_pcm_request(m, &volume);
    if (group && *group) {
        int sequence, repeat;
        unsigned serial;
        menu_pcm_parameters(m, &sequence, &repeat, &serial);
        pregrid_audio_update_gated(preparing_audio->pcm, group, volume, (uint32_t)plat_time_ns(), sequence,
                                   repeat, serial, 0);
    }
    const char *name = menu_music_name(m);
    if (!preparing_audio->jobs || !name || !*name)
        return;
    preparing_audio->owner = m;
    for (int i = 0; i < *preparing_audio->count; i++)
        if (!strcmp(preparing_audio->jobs[i].name, name))
            return;
    if (*preparing_audio->count >= 32)
        return;
    music_job *job = &preparing_audio->jobs[(*preparing_audio->count)++];
    job->disc = preparing_audio->disc;
    job->tempo = preparing_audio->tempo;
    snprintf(job->name, sizeof job->name, "%s", name);
    SDL_AtomicSet(&job->volume, 120);
    job->thread = SDL_CreateThread(prepare_music, "menu music preparation", job);
    if (!job->thread)
        SDL_AtomicSet(&job->ready, 1);
}

/* Original: func_0023E3D8 +0xAC, func_004616E8 +0x30..+0xF0.
   keepMusic starts at the script call; Sync/panIn must not defer publication
   until the outer UI loop. The same transport serves both update paths. */
static void update_present_midi(menu *m)
{
    menu_audio_context *audio = preparing_audio;
    if (!audio || !m || audio->owner != m)
        return;
    const char *request = menu_music_name(m);
    if (request)
        snprintf(audio->desired, sizeof audio->desired, "%s", request);
    if (music_faded && !plat_sdl_music_fading()) {
        plat_audio_set_music(NULL, NULL);
        audio->playing_active = 0;
        audio->playing = NULL;
        music_faded = 0;
        printf("music: stopped after authored fade\n");
    }

    int pcm_volume = 0;
    const char *pcm = menu_pcm_request(m, &pcm_volume);
    int wants_pcm = pcm && *pcm && !loading_screen_active() &&
                    !(music_faded && plat_sdl_music_fading());
    const char *page = ui_root_name(menu_ui(m), menu_active_root(m));
    int prepare_requested = audio->enabled && audio->allowed && !wants_pcm &&
                            strcmp(page, "MovieRoot") && *audio->desired;
    int wants_music = prepare_requested &&
                      (!loading_screen_active() ||
                       (audio->playing && !strcmp(audio->playing->name, audio->desired)));
    music_job *requested_music = NULL;
    if (prepare_requested) {
        for (int i = 0; i < *audio->count; i++)
            if (!strcmp(audio->jobs[i].name, audio->desired))
                requested_music = &audio->jobs[i];
        if (requested_music && requested_music != audio->playing && requested_music->played &&
            SDL_AtomicGet(&requested_music->ready)) {
            /* func_00461068/00460FE0 saves current sequence + 1;
               func_004616E8 reduces it modulo the disc group count. */
            if (requested_music->playlist) {
                requested_music->sequence = gt4_music_next_sequence((unsigned)SDL_AtomicGet(
                                                &requested_music->current_sequence)) %
                                            requested_music->count;
                SDL_AtomicSet(&requested_music->current_sequence,
                              (int)requested_music->sequence);
                requested_music->music =
                    SDL_AtomicGet(&requested_music->track_ready[requested_music->sequence])
                        ? requested_music->playlist[requested_music->sequence]
                        : NULL;
            } else if (requested_music->count > 1) {
                if (requested_music->thread)
                    SDL_WaitThread(requested_music->thread, NULL);
                requested_music->sequence = gt4_music_next_sequence(requested_music->sequence);
                gt4_music_close(requested_music->music);
                requested_music->music = NULL;
                SDL_AtomicSet(&requested_music->frames, 0);
                SDL_AtomicSet(&requested_music->ready, 0);
                requested_music->thread =
                    SDL_CreateThread(prepare_music, "menu music preparation", requested_music);
                if (!requested_music->thread)
                    SDL_AtomicSet(&requested_music->ready, 1);
            }
            requested_music->played = 0;
        }
        if (!requested_music && *audio->count < 32) {
            requested_music = &audio->jobs[(*audio->count)++];
            requested_music->disc = audio->disc;
            requested_music->tempo = audio->tempo;
            snprintf(requested_music->name, sizeof requested_music->name, "%s", audio->desired);
            SDL_AtomicSet(&requested_music->volume, 120);
            requested_music->thread =
                SDL_CreateThread(prepare_music, "menu music preparation", requested_music);
            if (!requested_music->thread)
                SDL_AtomicSet(&requested_music->ready, 1);
        }
    }
    if (!wants_music)
        requested_music = NULL;
    /* The script's fade duration belongs to the device clock, not the
       next UI update. Keep its producer alive until the final sample. */
    if (audio->playing && music_faded && plat_sdl_music_fading())
        requested_music = audio->playing;
    if (audio->playing && audio->playing != requested_music) {
        plat_audio_release_music(audio->playing);
        audio->playing_active = 0;
        audio->playing = NULL;
        printf("music: stopped\n");
    }
    if (requested_music) {
        if (SDL_AtomicGet(&requested_music->ready) && requested_music->playlist &&
            !audio->playing_active) {
            unsigned current = (unsigned)SDL_AtomicGet(&requested_music->current_sequence);
            requested_music->music = SDL_AtomicGet(&requested_music->track_ready[current])
                                         ? requested_music->playlist[current]
                                         : NULL;
        }
        int volume = (int)menu_option_number(m, "menu_bgm_volume_raw", 120);
        if (volume < 0)
            volume = 0;
        if (volume > 120)
            volume = 120;
        SDL_AtomicSet(&requested_music->volume, volume);
        if (!audio->playing_active && SDL_AtomicGet(&requested_music->ready) &&
            requested_music->music) {
            gt4_music_rewind(requested_music->music);
            audio->playing_active = plat_audio_set_music(render_menu_music, requested_music) == 0;
            if (audio->playing_active) {
                requested_music->played = 1;
                audio->playing = requested_music;
                printf("music: %s sequence=%u, 48000 Hz, tempo %.3f\n", requested_music->name,
                       requested_music->sequence, audio->tempo);
                fflush(stdout);
            }
        }
    }
}
