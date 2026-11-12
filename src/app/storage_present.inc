/* Native profile I/O keeps the original access actor running on the UI thread. */
#include "save_package.h"
typedef struct {
    const char *path;
    const uint8_t *data;
    size_t size;
    uint8_t **read_data;
    size_t *read_size;
    SDL_atomic_t done;
    int result;
} storage_io_job;
static int storage_io_worker(void *user)
{
    storage_io_job *job = user;
    job->result = job->read_data ? gt4_save_read(job->path, job->read_data, job->read_size, NULL)
                                 : gt4_save_write(job->path, job->data, job->size);
    SDL_AtomicSet(&job->done, 1);
    return 0;
}
static int storage_io_presented(menu *m, storage_io_job *job)
{
    /* Present access before launching I/O; never add a simulated device delay. */
    present_wait(m, 0, NULL);
    const char *shot = getenv("MOON_STORAGE_SHOT");
    if (shot)
        screenshot(shot);
    SDL_Thread *thread = SDL_CreateThread(storage_io_worker, "profile I/O", job);
    if (!thread)
        storage_io_worker(job);
    while (!SDL_AtomicGet(&job->done))
        present_wait(m, 1.0f / 60.0f, NULL);
    if (thread)
        SDL_WaitThread(thread, NULL);
    return job->result;
}
static int storage_write_presented(const char *path, const uint8_t *data, size_t size, void *user)
{
    storage_io_job job = {0};
    job.path = path;
    job.data = data;
    job.size = size;
    return storage_io_presented(user, &job);
}
static int storage_read_presented(const char *path, uint8_t **data, size_t *size, void *user)
{
    storage_io_job job = {0};
    job.path = path;
    job.read_data = data;
    job.read_size = size;
    return storage_io_presented(user, &job);
}
static int present_load(menu *m, int slot)
{
    return gt4_save_load_slot_with_io(slot, storage_read_presented, storage_write_presented, m);
}
static int present_save(menu *m)
{
    return gt4_save_store_with_writer(storage_write_presented, m);
}

static void package_present_pump(void *user)
{
    present_wait((menu *)user,1.0f/60,NULL);
}
static int present_profile_change(menu *m, int operation, int slot, const char *value)
{
    if (operation==MENU_PROFILE_EXPORT_PACKAGE || operation==MENU_PROFILE_IMPORT_PACKAGE) {
        moon_save_package_report report;
        int ok=operation==MENU_PROFILE_EXPORT_PACKAGE
            ? moon_save_package_export(slot,value,package_present_pump,m,&report)
            : moon_save_package_import(slot,value,storage_write_presented,package_present_pump,m,&report);
        gt4_save_report_error(ok ? "" : report.error);
        if (!ok) fprintf(stderr,"player package: %s\n",report.error);
        return ok;
    }
    switch (operation) {
    case MENU_PROFILE_CREATE:
        return gt4_save_create_with_writer(slot, value, storage_write_presented, m);
    case MENU_PROFILE_RENAME:
        return gt4_save_rename_with_writer(value, storage_write_presented, m);
    case MENU_PROFILE_IMPORT:
        return gt4_save_import_ps2_with_writer(slot, value, storage_write_presented, m);
    default:
        return 0;
    }
}
/* boot.ad LoadRoot L190..198 brackets the actual read with the load indicator.
   This temporary menu is never ticked: its queued boot script cannot start a second load. */
static int present_profile_access_screen(gt4_vol *vol, int slot)
{
    char error[256];
    menu *m = menu_open(vol, "boot", error, sizeof error);
    if (!m) {
        fprintf(stderr, "Profile load screen: %s\n", error);
        return 0;
    }
    ui_project *ui = menu_ui(m);
    int root = -1;
    for (int i = 0; i < ui_root_count(ui); i++)
        if (!strcmp(ui_root_name(ui, i), "LoadRoot"))
            root = i;
    int ok = 0;
    if (root >= 0) {
        menu_enter(m, root);
        menu_set_load_presenter(m, present_load);
        menu_set_save_presenter(m, present_save);
        ok = slot ? menu_save_load(m, slot) : menu_save_store(m);
    }
    menu_close(m);
    return ok;
}

static int present_startup_load(gt4_vol *vol)
{
    int slot = gt4_save_pending_slot();
    return !slot || present_profile_access_screen(vol, slot);
}
