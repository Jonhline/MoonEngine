/* Private layout shared by the menu implementation fragments. */
#ifndef MOON_MENU_INTERNAL_H
#define MOON_MENU_INTERNAL_H
#include "favorites.h"

typedef struct menu_actor menu_actor;

typedef struct {
    ah_obj *obj;
    int count;
} watcher;

struct menu {
    int (*profile_picker)(menu *);
    int (*photo_saver)(void *);
    void *photo_saver_user;
    gt4_vol *vol;
    gt4_favorite_labels favorite_labels;
    char project_label[32];
    gt4_disc_replay *disc_replays;
    ui_project *ui;
    ah_vm *vm;
    ah_obj *project; /* e.g. ArcadeProject */
    ah_obj *context;
    char *entered; /* per root: onInitialize ran */
    char *loaded;  /* per root: onLoad ran (once per project load) */
    watcher *watchers;
    int nwatchers;
    int pages[32], depth;
    ui_widget **focus;
    int pending_page, pending_kind;
    int page_transition_depth;
    int pending_more[8], pending_mores; /* startPage's further pages, pushed in order */
    ah_obj *events[128];
    int nevents;
    char next_project[64];
    char labo_arg[32]; /* the labomode page or card mode this project asked for */
    /* lists whose items the script fills (labomode.ad's visible_callback /
       update_callback): the ones to show again, and items asking for more
       updates (setItemUpdate, e.g. a thumbnail still loading) */
    ui_widget *item_lists[8];
    int nitem_lists;
    struct {
        ui_widget *list;
        int index;
    } item_updates[64];
    int nitem_updates;
    unsigned pad_held; /* the pad buttons held (getPadButtonState) */
    unsigned idle_frames;
    double tick_remainder, animation_ticks;
    uint32_t loading_seed;
    menu_actor *actors;
    gt4_sound *sound;
    char pcm_group[64];
    int pcm_requested, pcm_volume, pcm_sequence, pcm_repeat, pcm_playing;
    unsigned pcm_serial;
    int pcm_paused; /* sound.pauseStream */
    uint8_t *course_db, *course_labels;
    size_t course_db_size, course_labels_size;
    int preview_courses, course_load_attempted;
    uint8_t *car_labels, *car_db, *car_names, *car_strings;
    size_t car_labels_size, car_db_size, car_names_size, car_strings_size;
    int car_load_attempted;
    uint8_t *car_links, *car_variants;
    size_t car_links_size, car_variants_size;
    uint8_t *race_labels, *race_db;
    size_t race_labels_size, race_db_size;
    int race_load_attempted;
    int car_color; /* CarFace.setColorIndex, -1 before any */
    int gtmode_save_entry, new_career_entry, quit_requested;
    char music_name[64];
    int music_set;
    void (*music_fader)(float);
    void (*audio_preparer)(menu *, void *);
    void *audio_preparer_user;
    int options_dirty;
    int race_requested; /* finish() with a race sequence */
    gt4_arcade *arcade; /* the Arcade's SpecDB answers, opened on use */
    int (*rate_cars)(void *, const uint64_t *, int, float *);
    void *rate_ctx;
    char *entry_car[6];
    int entry_color[6]; /* game.setEntryCarCode */
    uint64_t quick_result_generation;
    uint64_t quick_tune_generation; /* the race its QuickTune was prepared for */
    ah_obj *quick_work;             /* MQuickWork: this VM's view of the quick race */
    char quick_command[32];         /* its selectedCommand at finish() */
    void (*present_wait)(menu *, float, void *);
    void *present_user;
    int (*present_dialog)(menu *, const char *, const char *, int, const char *, const char *);
    void (*present_progress)(menu *, int, const char *, int, float);
    int (*present_event)(menu *, void *);
    void *event_user;
    int (*load_car)(menu *, const ui_widget *, void *);
    void *car_loader_user;
    unsigned char event_loops[32];
    int event_depth;
    int render_context_count;
    int initializing;
    float pending_fade_in;
    gt4_photo_mode photo;
    float photo_apertures[17];
    int photo_apertures_ready;
    gt4_album_slide album_layer[2];
    int album_playing, album_shuffle, album_front, album_at, album_count,
        album_order[GT4_ALBUM_CAPACITY];
    uint32_t album_random;
    int album_preview_w, album_preview_h;
    int album_music_ready;
    gt4_music_playlist album_music_snapshot;
};

#endif
