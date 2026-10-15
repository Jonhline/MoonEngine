/* PC preparation: no artwork before disc validation; Options assets and
   widget measurements are loaded at runtime from the user's disc. */
#include "launcher.h"
#include "launcher_photos.h"
#include "launcher_icons.h"
#include "launcher_skin.h"
#include "launcher_audio.h"
#include "launcher_release.h"
#include "launcher_update.h"
#include "save.h"
#include "miniz.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <windowsx.h>

enum { HOME, VIDEO, AUDIO, CONTROLS };
enum { ROW = 100, BACK = 200, APPLY, DEFAULTS, TEST_AUDIO, PORT, UPDATE, UPDATE_CHECK, MUSIC_MUTE };
typedef struct {
    launcher_startup *start;
    launcher_options current, draft;
    HWND window, rows[8], fields[24], labels[24], footer[4], port_choice;
    HFONT normal, title, font_small, section;
    wchar_t label_text[24][80];
    RECT label_rect[24];
    HBRUSH panel;
    HBITMAP backdrop, icons, large_icons;
    uint32_t *backdrop_pixels;
    launcher_photos photos;
    launcher_skin skin;
    HCURSOR cursor;
    launcher_audio *menu_audio;
    float highlight[15];
    launcher_update_job *update_job;
    launcher_update_manifest update_manifest;
    HWND update_button, update_check, music_mute;
    int update_downloading;
    int disc_valid;
    HWND hovered_control;
    ULONGLONG last_tick;
    int page, selection, port, count, result, sdl_ready, hover, pad_mode, probing;
    unsigned nav_previous;
    char status[256], device[192];
    SDL_GameController *pads[2];
    SDL_AudioDeviceID audio;
    float tone[24000];
} launcher_window;
static const wchar_t *const row_names[] = {
    L"Play",     L"Game disc",   L"Video",           L"Audio",
    L"Controls", L"Player data", L"Show at startup", L"Exit"};
static const wchar_t *const descriptions[] = {
    L"Start the game with your selected disc and settings.",
    L"Locate your own disc image. It stays in its current folder.",
    L"Configure your display before starting the game.",
    L"Set music and sound volumes, and test the audio device.",
    L"Assign controller buttons and sticks for players 1 and 2.",
    L"Choose the folder used for profiles, replays and photographs.",
    L"Remember this screen on startup. Open it again with --launcher.",
    L"Close OpenGT4."};
/* Same function ordering as option.ad KeyconfigRoot/DS2 L17..88. New PC
   controls present names rather than using the disc's controller glyphs. */
static const int binding_actions[] = {-2,
                                      GT4_PAD_ACCEL,
                                      GT4_PAD_BRAKE,
                                      GT4_PAD_REVERSE,
                                      GT4_PAD_SIDE,
                                      GT4_PAD_SHIFTUP,
                                      GT4_PAD_SHIFTDOWN,
                                      GT4_PAD_BACKVIEW,
                                      GT4_PAD_PLAYVIEW,
                                      GT4_PAD_SPECIAL,
                                      GT4_PAD_GHOST};
static const wchar_t *const binding_names[] = {
    L"Steering",       L"Gas",       L"Brake",       L"Reverse", L"E-brake", L"(MT) Upshift",
    L"(MT) Downshift", L"Rear View", L"Change View", L"Special", L"Ghost"};
static const wchar_t *const key_choices[] = {L"NONE",
                                             L"SELECT / Back",
                                             L"L3",
                                             L"R3",
                                             L"START",
                                             L"up",
                                             L"right",
                                             L"down",
                                             L"left",
                                             L"L2 / LT",
                                             L"R2 / RT",
                                             L"L1 / LB",
                                             L"R1 / RB",
                                             L"Triangle / Y",
                                             L"Circle / B",
                                             L"Cross / A",
                                             L"Square / X",
                                             L"left analog stick (UP)",
                                             L"left analog stick (DOWN)",
                                             L"right analog stick (UP)",
                                             L"right analog stick (DOWN)"};
static const int resolutions[][2] = {{1280, 720},  {1280, 960},  {1600, 900},
                                     {1920, 1080}, {2560, 1440}, {3840, 2160}};
static int wide(const char *text, wchar_t *out, int size)
{
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, out, size);
}
static void paint_text(HDC dc, HFONT font, int x, int y, int w, int h, COLORREF color,
                       const wchar_t *text, UINT flags)
{
    HFONT old = SelectObject(dc, font);
    SetTextColor(dc, color);
    SetBkMode(dc, TRANSPARENT);
    RECT r = {x, y, x + w, y + h};
    DrawTextW(dc, text, -1, &r, flags | DT_NOPREFIX);
    SelectObject(dc, old);
}
static void gradient(HDC dc, RECT r, COLORREF top, COLORREF bottom)
{
    int h = r.bottom - r.top;
    if (h < 1)
        return;
    for (int y = 0; y < h; y++) {
        int red = (GetRValue(top) * (h - y) + GetRValue(bottom) * y) / h;
        int green = (GetGValue(top) * (h - y) + GetGValue(bottom) * y) / h;
        int blue = (GetBValue(top) * (h - y) + GetBValue(bottom) * y) / h;
        HBRUSH b = CreateSolidBrush(RGB(red, green, blue));
        RECT line = {r.left, r.top + y, r.right, r.top + y + 1};
        FillRect(dc, &line, b);
        DeleteObject(b);
    }
}
static void gradient_h(HDC dc, RECT r, COLORREF left, COLORREF right)
{
    int w = r.right - r.left;
    if (w < 1)
        return;
    for (int x = 0; x < w; x++) {
        int red = (GetRValue(left) * (w - x) + GetRValue(right) * x) / w,
            green = (GetGValue(left) * (w - x) + GetGValue(right) * x) / w,
            blue = (GetBValue(left) * (w - x) + GetBValue(right) * x) / w;
        HBRUSH b = CreateSolidBrush(RGB(red, green, blue));
        RECT line = {r.left + x, r.top, r.left + x + 1, r.bottom};
        FillRect(dc, &line, b);
        DeleteObject(b);
    }
}
static LRESULT CALLBACK combo_proc(HWND win,UINT message,WPARAM w,LPARAM l)
{
    WNDPROC original=(WNDPROC)GetPropW(win,L"OpenGT4ComboProc");
    launcher_window *s=(launcher_window *)GetPropW(win,L"OpenGT4Launcher");
    LRESULT result=CallWindowProcW(original,win,message,w,l);
    if(message==WM_NCDESTROY) {
        RemovePropW(win,L"OpenGT4ComboProc"); RemovePropW(win,L"OpenGT4Launcher");
        return result;
    }
    if(s && s->disc_valid && (message==WM_PAINT || message==WM_PRINTCLIENT)) {
        HDC dc=message==WM_PAINT?GetDC(win):(HDC)w;
        RECT r; GetClientRect(win,&r);
        int selected=(int)SendMessageW(win,CB_GETCURSEL,0,0);
        wchar_t text[256]=L"";
        if(selected>=0) SendMessageW(win,CB_GETLBTEXT,selected,(LPARAM)text);
        launcher_skin_field(&s->skin,dc,r,GetFocus()==win);
        r.left+=8; r.right-=30;
        launcher_skin_control_text(&s->skin,dc,r,text,0);
        if(message==WM_PAINT) ReleaseDC(win,dc);
    }
    return result;
}
static HWND control(launcher_window *s, const wchar_t *cls, const wchar_t *name, DWORD style,
                    int id, int x, int y, int w, int h)
{
    HWND win = CreateWindowExW(0, cls, name, WS_CHILD | WS_VISIBLE | style, x, y, w, h, s->window,
                               (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(win, WM_SETFONT, (WPARAM)s->normal, TRUE);
    if(!wcscmp(cls,L"COMBOBOX")) {
        SetPropW(win,L"OpenGT4Launcher",s);
        WNDPROC previous=(WNDPROC)SetWindowLongPtrW(win,GWLP_WNDPROC,(LONG_PTR)combo_proc);
        SetPropW(win,L"OpenGT4ComboProc",(HANDLE)previous);
    }
    return win;
}
static void destroy_page(launcher_window *s)
{
    for (int i = 0; i < 24; i++) {
        if (s->fields[i])
            DestroyWindow(s->fields[i]);
        if (s->labels[i])
            DestroyWindow(s->labels[i]);
        s->fields[i] = s->labels[i] = NULL;
        s->label_text[i][0] = 0;
    }
    for (int i = 0; i < 4; i++) {
        if (s->footer[i])
            DestroyWindow(s->footer[i]);
        s->footer[i] = NULL;
    }
    if (s->port_choice)
        DestroyWindow(s->port_choice);
    s->port_choice = NULL;
    s->count = 0;
}
static void set_label(launcher_window *s, int k, const wchar_t *text, int x, int y, int w, int h)
{
    wcsncpy(s->label_text[k], text, 79);
    s->label_text[k][79] = 0;
    s->label_rect[k] = (RECT){x, y, x + w, y + h};
}
static void combo(launcher_window *s, int k, const wchar_t *label, const wchar_t *const *choices,
                  int n, int selected, int x, int y, int width)
{
    set_label(s, k, label, x, y, width, 24);
    s->fields[k] =
        control(s, L"COMBOBOX", L"",
                CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL | WS_TABSTOP,
                300 + k, x, y + 25, width, 300);
    for (int i = 0; i < n; i++)
        SendMessageW(s->fields[k], CB_ADDSTRING, 0, (LPARAM)choices[i]);
    SendMessageW(s->fields[k], CB_SETCURSEL, selected, 0);
}
static RECT authored_rect(double x, double y, double w, double h)
{
    /* The authored Options canvas is 640x480; the PC window is 900x700. */
    return (RECT){(LONG)lround(x*900/640), (LONG)lround(y*700/480),
                  (LONG)lround((x+w)*900/640), (LONG)lround((y+h)*700/480)};
}
static void layout_rows(launcher_window *s)
{
    double origin[4]={26,62,520,370}, item[4]={0,0,102,91}, group[4]={0,0,520,30};
    if (s->disc_valid) {
        mp_numbers(launcher_skin_widget(&s->skin,"ScrollWindow","ScrollWindow"),"geometry",origin,4);
        mp_numbers(launcher_skin_widget(&s->skin,"Composite","Item"),"geometry",item,4);
        mp_numbers(launcher_skin_widget(&s->skin,"Composite","Group"),"geometry",group,4);
    }
    for (int i=0;i<8;i++) {
        RECT r;
        if (!s->disc_valid) r=(RECT){320,350,580,394};
        else if (i==7) {
            double exit[4]={567,390,58,40};
            mp_numbers(launcher_skin_widget(&s->skin,"Composite","Exit"),"geometry",exit,4);
            r=authored_rect(exit[0],exit[1],exit[2],exit[3]);
        } else {
            /* Original: option.adc OptionRoot.onInitialize, five columns;
               rows are separated by Item.h+4 and Group.h. PC actions replace
               the original option entries, keeping their slot geometry. */
            int row=i<5?0:1, col=i<5?i:i-5;
            r=authored_rect(origin[0]+col*item[2],origin[1]+group[3]+
                           row*(item[3]+4+group[3]),item[2],item[3]);
        }
        MoveWindow(s->rows[i],r.left,r.top,r.right-r.left,r.bottom-r.top,TRUE);
        ShowWindow(s->rows[i],s->page==HOME && (s->disc_valid || i==1)?SW_SHOW:SW_HIDE);
        SetWindowTextW(s->rows[i],!s->disc_valid&&i==1?L"Select disc image":row_names[i]);
    }
}
static void menu_volume(launcher_window *s,int music,int effects)
{
    /* P: launcher-only mute retains the game's configured volume. */
    launcher_audio_volume(s->menu_audio,s->start->preferences.mute_music?0:music,effects);
}
static void refresh_disc(launcher_window *s)
{
    launcher_audio_close(s->menu_audio); s->menu_audio=NULL;
    if (s->cursor) { SetCursor(LoadCursor(NULL,IDC_ARROW)); DestroyCursor(s->cursor); s->cursor=NULL; }
    launcher_skin_close(&s->skin);
    s->disc_valid=launcher_validate_disc(s->start->preferences.disc,s->status,sizeof s->status);
    if (s->disc_valid && !launcher_skin_open(&s->skin,s->start->preferences.disc,NULL)) {
        s->disc_valid=0;
        strcpy(s->status,"Could not read the Options artwork. Select a readable disc image.");
    }
    if (s->disc_valid) {
        launcher_skin_filter(&s->skin);
        s->cursor=launcher_skin_cursor(&s->skin);
        s->menu_audio=launcher_audio_open(s->start->preferences.disc,1);
        menu_volume(s,s->current.volume[0],s->current.volume[1]);
    }
    memset(s->highlight,0,sizeof s->highlight);
    if (!s->disc_valid) s->selection=1;
    s->hover=-1;
}
static void page(launcher_window *s, int which)
{
    /* P: rebuild native children while painting is suspended, then present
       the complete page through the composited parent window. */
    int visible=(GetWindowLongPtrW(s->window,GWL_STYLE)&WS_VISIBLE)!=0;
    if (visible) SendMessageW(s->window,WM_SETREDRAW,FALSE,0);
    if(s->update_button) ShowWindow(s->update_button,
        which==HOME && s->update_manifest.version[0] && s->start->preferences.check_updates?SW_SHOW:SW_HIDE);
    if(s->music_mute) ShowWindow(s->music_mute,s->disc_valid?SW_SHOW:SW_HIDE);
    destroy_page(s);
    if (!s->disc_valid) which=HOME;
    s->page = which;
    s->draft = s->current;
    menu_volume(s,s->current.volume[0],s->current.volume[1]);
    layout_rows(s);
    if (which != HOME) {
        s->footer[0] =
            control(s, L"BUTTON", L"Cancel", BS_OWNERDRAW | WS_TABSTOP, BACK, 112, 572, 192, 34);
        s->footer[1] =
            control(s, L"BUTTON", L"OK", BS_OWNERDRAW | WS_TABSTOP, APPLY, 588, 572, 192, 34);
        s->footer[2] = control(s, L"BUTTON", L"Default", BS_OWNERDRAW | WS_TABSTOP, DEFAULTS, 350,
                               572, 192, 34);
    }
    if (which == VIDEO) {
        const wchar_t *ratio[] = {L"Original 4:3", L"Widescreen 16:9"},
                      *display[] = {L"Windowed", L"Full screen"}, *on[] = {L"Off", L"On"};
        const wchar_t *res[7];
        wchar_t text[7][64];
        int selected = 6;
        for (int i = 0; i < 6; i++) {
            swprintf(text[i], 64, L"%d x %d", resolutions[i][0], resolutions[i][1]);
            res[i] = text[i];
            if (s->draft.width == resolutions[i][0] && s->draft.height == resolutions[i][1])
                selected = i;
        }
        swprintf(text[6], 64, L"%d x %d (current)", s->draft.width, s->draft.height);
        res[6] = text[6];
        const wchar_t *filters[] = {L"Smooth", L"Sharp", L"Original pixels"};
        const wchar_t *sharp[] = {L"0%", L"25%", L"50%", L"75%", L"100%"};
        const wchar_t *scale[] = {L"Native resolution", L"FSR Ultra quality", L"FSR Quality",
                                  L"FSR Balanced", L"FSR Performance"};
        combo(s, 0, L"Aspect ratio", ratio, 2, s->draft.aspect, 140, 234, 280);
        combo(s, 1, L"Display mode", display, 2, s->draft.fullscreen, 480, 234, 280);
        combo(s, 2, L"Resolution", res, 7, selected, 140, 314, 280);
        combo(s, 3, L"Vertical synchronization", on, 2, s->draft.vsync, 480, 314, 280);
        combo(s, 4, L"Interface filtering", filters, 3, s->draft.filter, 140, 394, 280);
        combo(s, 5, L"Text clarity", sharp, 5, (s->draft.text + 12) / 25, 480, 394, 280);
        combo(s, 6, L"3D scaling", scale, 5, s->draft.upscale, 310, 474, 280);
        s->count = 7;
    } else if (which == AUDIO) {
        const wchar_t *labels[] = {L"Menu music",      L"Menu effects", L"Race music",
                                   L"Race effects",    L"Replay music", L"Replay effects",
                                   L"Slide show music"};
        const wchar_t *choices[129];
        wchar_t text[129][24];
        for (int i = 0; i <= 128; i++) {
            swprintf(text[i], 24, L"%d%%", (i * 100 + 64) / 128);
            choices[i] = text[i];
        }
        for (int i = 0; i < 7; i++)
            combo(s, i, labels[i], choices, 129, s->draft.volume[i], i==6?310:140+(i%2)*340,
                  234+(i/2)*80,280);
        s->footer[3] = control(s, L"BUTTON", L"Test audio", BS_OWNERDRAW | WS_TABSTOP, TEST_AUDIO,
                               350, 532, 200, 34);
        s->count = 7;
    } else if (which == CONTROLS) {
        s->port_choice =
            control(s, L"COMBOBOX", L"",
                    CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_TABSTOP, PORT, 640,
                    132, 150, 100);
        const wchar_t *ports[] = {L"Player 1", L"Player 2"};
        for (int i = 0; i < 2; i++)
            SendMessageW(s->port_choice, CB_ADDSTRING, 0, (LPARAM)ports[i]);
        SendMessageW(s->port_choice, CB_SETCURSEL, s->port, 0);
        gt4_pad_config *c = &s->draft.pad[s->port];
        for (int i = 0; i < 11; i++) {
            int y = 174 + i * 36, select = 0;
            set_label(s, i, binding_names[i], 108, y + 5, 275, 28);
            s->fields[i] = control(s, L"COMBOBOX", L"",
                                   CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS |
                                       WS_TABSTOP | WS_VSCROLL,
                                   300 + i, 406, y, 374, 400);
            if (!i) {
                const wchar_t *steering[] = {L"NONE", L"left analog stick", L"right analog stick",
                                             L"left / right buttons"};
                for (int k = 0; k < 4; k++)
                    SendMessageW(s->fields[i], CB_ADDSTRING, 0, (LPARAM)steering[k]);
                if (c->analog[GT4_PAD_LX] == -2)
                    select = 1;
                else if (c->analog[GT4_PAD_RX] == -2)
                    select = 2;
                else if (c->button[GT4_PAD_LEFT] == GT4_PAD_LSTEER &&
                         c->button[GT4_PAD_RIGHT] == GT4_PAD_RSTEER)
                    select = 3;
            } else {
                for (int k = 0; k < 21; k++)
                    SendMessageW(s->fields[i], CB_ADDSTRING, 0, (LPARAM)key_choices[k]);
                for (int k = 0; k < 16; k++)
                    if (c->button[k] == binding_actions[i]) {
                        select = k + 1;
                        break;
                    }
                if (!select)
                    for (int k = 2; k < 6; k++)
                        if (c->analog[k] == binding_actions[i]) {
                            select = k + 15;
                            break;
                        }
            }
            SendMessageW(s->fields[i], CB_SETCURSEL, select, 0);
        }
        s->count = 11;
    }
    SetFocus(which == HOME ? s->rows[s->selection] : s->fields[0]);
    if (visible) SendMessageW(s->window,WM_SETREDRAW,TRUE,0);
    RedrawWindow(s->window,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
}
static int value(launcher_window *s, int n)
{
    return (int)SendMessageW(s->fields[n], CB_GETCURSEL, 0, 0);
}
static int apply(launcher_window *s)
{
    int groups = 0;
    if (s->page == VIDEO) {
        s->draft.aspect = value(s, 0);
        s->draft.fullscreen = value(s, 1);
        int r = value(s, 2);
        if (r < 6) {
            s->draft.width = resolutions[r][0];
            s->draft.height = resolutions[r][1];
        }
        s->draft.vsync = value(s, 3);
        s->draft.filter = value(s, 4);
        s->draft.text = value(s, 5) * 25;
        s->draft.upscale = value(s, 6);
        groups = 1;
    } else if (s->page == AUDIO) {
        for (int i = 0; i < 7; i++)
            s->draft.volume[i] = value(s, i);
        groups = 2;
    } else if (s->page == CONTROLS) {
        int selected[11];
        for (int i = 0; i < 11; i++)
            selected[i] = value(s, i);
        for (int i = 1; i < 11; i++)
            for (int j = i + 1; j < 11; j++)
                if (selected[i] && selected[i] == selected[j]) {
                    strcpy(s->status,
                           "Two functions use the same button. Choose a different assignment.");
                    InvalidateRect(s->window, NULL, FALSE);
                    return 0;
                }
        if (selected[0] == 3)
            for (int i = 1; i < 11; i++)
                if (selected[i] == GT4_PAD_LEFT + 1 || selected[i] == GT4_PAD_RIGHT + 1) {
                    strcpy(s->status,
                           "Left / right buttons are assigned to steering. Choose another button.");
                    InvalidateRect(s->window, NULL, FALSE);
                    return 0;
                }
        gt4_pad_config *c = &s->draft.pad[s->port];
        for (int i = 0; i < 11; i++) {
            int action = binding_actions[i];
            for (int k = 0; k < 16; k++)
                if (i ? c->button[k] == action
                      : c->button[k] == GT4_PAD_LSTEER || c->button[k] == GT4_PAD_RSTEER)
                    c->button[k] = -1;
            for (int k = 0; k < 6; k++)
                if (c->analog[k] == action)
                    c->analog[k] = -1;
        }
        if (selected[0] == 1)
            c->analog[GT4_PAD_LX] = -2;
        if (selected[0] == 2)
            c->analog[GT4_PAD_RX] = -2;
        /* The original DS2 default also enables digital left/right alongside
           the stick. Retain that available fallback for the new PC editor. */
        if (selected[0]) {
            c->button[GT4_PAD_LEFT] = GT4_PAD_LSTEER;
            c->button[GT4_PAD_RIGHT] = GT4_PAD_RSTEER;
        }
        for (int i = 1; i < 11; i++) {
            if (selected[i] >= 1 && selected[i] <= 16)
                c->button[selected[i] - 1] = binding_actions[i];
            if (selected[i] >= 17)
                c->analog[selected[i] - 15] = binding_actions[i];
        }
        groups = 4 << s->port;
    }
    if (!groups)
        return 0;
    int ok =
        launcher_options_save(&s->draft, s->start->video_file, s->start->preferences.saves, groups);
    if (ok) {
        s->current = s->draft;
        strcpy(s->status, "Settings saved.");
        page(s, HOME);
    } else {
        strcpy(s->status,
               "Could not save all settings. Check access to the settings and player folders.");
        MessageBoxW(s->window, L"Could not save all settings. Check folder permissions.",
                    L"OpenGT4", MB_ICONERROR);
    }
    return ok;
}
static int choose_path(launcher_window *s, char *out, int folder)
{
    wchar_t selected[LAUNCHER_PATH] = L"";
    if (folder) {
        BROWSEINFOW b = {0};
        b.hwndOwner = s->window;
        b.lpszTitle = L"Choose the OpenGT4 player data folder";
        b.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&b);
        int ok = id && SHGetPathFromIDListW(id, selected);
        CoTaskMemFree(id);
        if (!ok)
            return 0;
    } else {
        OPENFILENAMEW open = {0};
        open.lStructSize = sizeof open;
        open.hwndOwner = s->window;
        open.lpstrFilter = L"PlayStation 2 disc image (*.iso)\0*.iso\0All files\0*.*\0";
        open.lpstrFile = selected;
        open.nMaxFile = LAUNCHER_PATH;
        open.lpstrTitle = L"OpenGT4 - Select game disc";
        open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetOpenFileNameW(&open))
            return 0;
    }
    return WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, selected, -1, out, LAUNCHER_PATH,
                               NULL, NULL) != 0;
}
static void pad_navigate(launcher_window *s, int key);
static void activate(launcher_window *s, int n)
{
    launcher_audio_play(s->menu_audio,"select");
    char selected[LAUNCHER_PATH];
    if (!s->disc_valid && n!=1 && n!=7) return;
    s->selection = n;
    if (n == 0) {
        if (!launcher_validate_disc(s->start->preferences.disc, s->status, sizeof s->status)) {
            refresh_disc(s); page(s,HOME);
            InvalidateRect(s->window, NULL, FALSE);
            return;
        }
        if (!launcher_preferences_save(&s->start->preferences)) {
            strcpy(s->status, "Could not save launcher preferences.");
            InvalidateRect(s->window, NULL, FALSE);
            return;
        }
        s->result = 1;
        DestroyWindow(s->window);
    } else if (n == 1 || n == 5) {
        if (choose_path(s, selected, n == 5)) {
            launcher_preferences draft = s->start->preferences;
            if (n == 1) {
                strcpy(draft.disc, selected);
            } else
                strcpy(draft.saves, selected);
            if (launcher_preferences_save(&draft)) {
                s->start->preferences = draft;
                launcher_options_load(&s->current, s->start->video_file, draft.saves);
                if (n==1) { refresh_disc(s); page(s,HOME); }
                if (n == 5)
                    strcpy(s->status, "Player data folder selected. Existing files stay in place.");
            } else
                strcpy(s->status, "Could not save launcher preferences.");
            InvalidateRect(s->window, NULL, FALSE);
        }
    } else if (n >= 2 && n <= 4)
        page(s, n - 1);
    else if (n == 6) {
        launcher_preferences p = s->start->preferences;
        p.show = !p.show;
        if (launcher_preferences_save(&p)) {
            s->start->preferences = p;
            strcpy(s->status, "Startup preference saved.");
        } else
            strcpy(s->status, "Could not save launcher preferences.");
        InvalidateRect(s->rows[6], NULL, TRUE);
        InvalidateRect(s->window, NULL, FALSE);
    } else
        DestroyWindow(s->window);
}
static void test_audio(launcher_window *s)
{
    if (s->menu_audio) {
        menu_volume(s,value(s,0),value(s,1));
        launcher_audio_play(s->menu_audio,"ok");
        strcpy(s->status,"Playing the menu confirmation sound.");
        InvalidateRect(s->window,NULL,FALSE);
        return;
    }
    if (s->audio) {
        SDL_CloseAudioDevice(s->audio);
        s->audio = 0;
    }
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
        goto failed;
    SDL_AudioSpec want = {0}, got;
    want.freq = 48000;
    want.format = AUDIO_F32SYS;
    want.channels = 1;
    want.samples = 1024;
    s->audio = SDL_OpenAudioDevice(NULL, 0, &want, &got, 0);
    if (!s->audio)
        goto failed;
    /* P: generated device-check tone, never an imitation of a game asset. */
    float gain = (float)value(s, 1) / 128.f;
    for (int i = 0; i < 24000; i++) {
        float fade = (float)fmin(i / 480.0, (24000 - i) / 2400.0);
        if (fade > 1)
            fade = 1;
        s->tone[i] = .15f * gain * fade * (float)sin(i * 6.283185307179586 * 440 / 48000);
    }
    if (SDL_QueueAudio(s->audio, s->tone, sizeof s->tone) < 0)
        goto failed;
    SDL_PauseAudioDevice(s->audio, 0);
    strcpy(s->status, "Device test: generated tone at the menu effects volume.");
    InvalidateRect(s->window, NULL, FALSE);
    return;
failed:
    snprintf(s->status, sizeof s->status, "Audio device unavailable: %.160s", SDL_GetError());
    InvalidateRect(s->window, NULL, FALSE);
}
static void controller_tick(launcher_window *s)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
    }
    if (!IsWindow(s->window)) return; /* SDL may dispatch Play while pumping events. */
    for (int p = 0; p < 2; p++)
        if (s->pads[p] && !SDL_GameControllerGetAttached(s->pads[p])) {
            SDL_GameControllerClose(s->pads[p]);
            s->pads[p] = NULL;
        }
    for (int i = 0; i < SDL_NumJoysticks(); i++)
        if (SDL_IsGameController(i)) {
            SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(i);
            int used = 0;
            for (int p = 0; p < 2; p++)
                if (s->pads[p] &&
                    SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(s->pads[p])) == id)
                    used = 1;
            if (!used)
                for (int p = 0; p < 2; p++)
                    if (!s->pads[p]) {
                        s->pads[p] = SDL_GameControllerOpen(i);
                        break;
                    }
        }
    SDL_GameController *nav = s->pads[0];
    unsigned buttons = 0;
    const SDL_GameControllerButton navigation[] = {
        SDL_CONTROLLER_BUTTON_DPAD_UP,   SDL_CONTROLLER_BUTTON_DPAD_DOWN,
        SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
        SDL_CONTROLLER_BUTTON_A,         SDL_CONTROLLER_BUTTON_B};
    const int keys[] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_RETURN, VK_ESCAPE};
    if (nav)
        for (int i = 0; i < 6; i++)
            if (SDL_GameControllerGetButton(nav, navigation[i]))
                buttons |= 1u << i;
    unsigned pressed = buttons & ~s->nav_previous;
    s->nav_previous = buttons;
    if (GetForegroundWindow() == s->window)
        for (int i = 0; i < 6; i++)
            if (pressed & (1u << i)) {
                if (s->page == HOME && i < 4) {
                    pad_navigate(s, keys[i]);
                    continue;
                }
                if (s->page == HOME && i == 4) {
                    activate(s, s->hover >= 0 ? s->hover : s->selection);
                    continue;
                }
                HWND focus = GetFocus();
                PostMessageW(focus ? focus : s->window, WM_KEYDOWN, keys[i], 0);
            }
    SDL_GameController *pad = s->pads[s->port];
    char before[192];
    strcpy(before, s->device);
    if (pad)
        snprintf(s->device, sizeof s->device, "%.80s     LX %d   LY %d   LT %d   RT %d",
                 SDL_GameControllerName(pad),
                 SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX),
                 SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY),
                 SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT),
                 SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT));
    else
        strcpy(s->device, "No controller connected. Keyboard and mouse are available.");
    if (strcmp(before, s->device) && s->page == CONTROLS) {
        RECT r = {80, 647, 820, 678};
        InvalidateRect(s->window, &r, FALSE);
    }
}
static void load_photos(launcher_window *s)
{
    launcher_photos_close(&s->photos);
    char base[LAUNCHER_PATH], cache[LAUNCHER_PATH + 64], album[LAUNCHER_PATH + 16];
    strcpy(base, s->start->preferences.file);
    char *a = strrchr(base, '/'), *b = strrchr(base, '\\'), *end = a;
    if (b && (!a || b > a))
        end = b;
    if (end)
        *end = 0;
    else
        strcpy(base, ".");
    snprintf(cache, sizeof cache, "%s/launcher_photos/photos", base);
    snprintf(album, sizeof album, "%s/photos", s->start->preferences.saves);
    launcher_photos_open(&s->photos, cache, album, 900, 700);
}
static void compose_backdrop(launcher_window *s, HDC dc)
{
    if (!s->backdrop) {
        BITMAPINFO info={0}; info.bmiHeader.biSize=sizeof info.bmiHeader;
        info.bmiHeader.biWidth=900; info.bmiHeader.biHeight=-700;
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32;
        s->backdrop=CreateDIBSection(dc,&info,DIB_RGB_COLORS,(void **)&s->backdrop_pixels,NULL,0);
    }
    if (!s->backdrop) return;
    HDC back=CreateCompatibleDC(dc); HBITMAP old=SelectObject(back,s->backdrop);
    if (s->disc_valid) launcher_skin_background(&s->skin,back,900,700);
    else {
        /* P: disc-independent preparation screen. No game artwork is
           available before validation; this background is procedural. */
        gradient(back,(RECT){0,0,900,700},RGB(48,49,51),RGB(9,10,12));
        for (int y=0;y<700;y+=16) for (int x=0;x<900;x+=16) {
            int shade=3*(700-y)/700;
            if (((x+y)/16)&1) continue;
            int base=48-(39*y)/700;
            HBRUSH b=CreateSolidBrush(RGB(base+shade,base+1+shade,base+3+shade));
            RECT cell={x,y,x+16,y+16}; FillRect(back,&cell,b); DeleteObject(b);
        }
        gradient(back,(RECT){0,176,900,267},RGB(22,23,25),RGB(4,5,7));
        gradient_h(back,(RECT){170,267,730,268},RGB(61,62,64),RGB(164,165,167));
    }
    if (s->disc_valid && s->page!=HOME) {
        RECT r={80,118,820,632}; launcher_skin_image_draw(&s->skin,back,1,r);
    }
    SelectObject(back,old); DeleteDC(back);
}
static void update_tick(launcher_window *s)
{
    if(!s->update_job) return;
    int result=launcher_update_poll(s->update_job,&s->update_manifest,NULL);
    if(!result) return;
    if(s->update_downloading) {
        if(result==1 && launcher_update_install(s->update_job)) {
            s->result=0; DestroyWindow(s->window); return;
        }
        MessageBoxW(s->window,L"The update could not be verified or installed. Please try again.",
                    L"OpenGT4",MB_OK|MB_ICONERROR);
        s->update_downloading=0;
        SetWindowTextW(s->update_button,L"Update"); EnableWindow(s->update_button,TRUE);
    } else if(result==1 && s->start->preferences.check_updates && s->page==HOME) {
        ShowWindow(s->update_button,SW_SHOW);
        InvalidateRect(s->window,NULL,FALSE);
    }
    launcher_update_release(s->update_job); s->update_job=NULL;
}
static void draw(launcher_window *s, HDC dc)
{
    compose_backdrop(s,dc);
    if (s->backdrop) {
        HDC src=CreateCompatibleDC(dc); HBITMAP old=SelectObject(src,s->backdrop);
        BitBlt(dc,0,0,900,700,src,0,0,SRCCOPY); SelectObject(src,old); DeleteDC(src);
    }
    if(!s->disc_valid && s->update_manifest.version[0] && IsWindowVisible(s->update_button)) {
        wchar_t label[100]; char text[100];
        snprintf(text,sizeof text,"Update available: %s",s->update_manifest.version);
        wide(text,label,100);
        paint_text(dc,s->normal,60,656,550,30,RGB(230,230,230),label,DT_SINGLELINE);
    }
    if (!s->disc_valid) {
        paint_text(dc,s->title,0,206,900,60,RGB(234,234,236),L"OpenGT4",DT_CENTER|DT_SINGLELINE);
        paint_text(dc,s->normal,70,294,760,30,RGB(203,204,207),
                   L"Select your Gran Turismo 4 disc image (NTSC-U, SCUS-97328)",DT_CENTER|DT_SINGLELINE);
        return;
    }
    RECT title=authored_rect(30,16,580,50);
    launcher_skin_text(&s->skin,dc,launcher_skin_widget(&s->skin,"TextFace","TextFace"),title,L"OpenGT4");
    if (s->page==HOME) {
        double origin[4],item[4],group[4];
        mp_numbers(launcher_skin_widget(&s->skin,"ScrollWindow","ScrollWindow"),"geometry",origin,4);
        mp_numbers(launcher_skin_widget(&s->skin,"Composite","Item"),"geometry",item,4);
        mp_numbers(launcher_skin_widget(&s->skin,"Composite","Group"),"geometry",group,4);
        const wchar_t *sections[]={L"Game and settings",L"Player data"};
        for (int k=0;k<2;k++) {
            RECT r=authored_rect(origin[0],origin[1]+k*(item[3]+4+group[3]),group[2],group[3]);
            launcher_skin_group(&s->skin,dc,r);
            const mp_node *style=mp_find_named(launcher_skin_widget(&s->skin,"Composite","Group"),"TextFace","label");
            double label[4]; mp_numbers(style,"geometry",label,4);
            RECT text=authored_rect(origin[0]+label[0],origin[1]+k*(item[3]+4+group[3])+label[1],label[2],label[3]);
            launcher_skin_text(&s->skin,dc,style,text,sections[k]);
        }
        /* P: details occupy three spare original slots; all framing is the
           authored Item.bg5 texture, not an invented bevel or corner. */
        RECT detail=authored_rect(origin[0]+2*item[2],origin[1]+2*group[3]+item[3]+4,
                                 3*item[2],2*item[3]);
        launcher_skin_image_draw(&s->skin,dc,1,detail);
        const mp_node *text_style=mp_find_named(launcher_skin_widget(&s->skin,"Composite","OptionItem"),"TextFace","label");
        RECT heading={detail.left+18,detail.top+22,detail.right-22,detail.top+68};
        launcher_skin_paragraph(&s->skin,dc,text_style,heading,row_names[s->selection]);
        RECT body={detail.left+18,detail.top+68,detail.right-22,detail.top+155};
        launcher_skin_paragraph(&s->skin,dc,text_style,body,descriptions[s->selection]);
        wchar_t status[LAUNCHER_PATH];
        const char *value=s->selection==1?s->start->preferences.disc:
                          s->selection==5?s->start->preferences.saves:s->status;
        wide(value,status,LAUNCHER_PATH);
        RECT info={detail.left+18,detail.top+166,detail.right-22,detail.bottom-20};
        launcher_skin_paragraph(&s->skin,dc,text_style,info,status);
    } else {
        const wchar_t *titles[]={L"",L"Screen settings",L"Sound settings",L"Controller button config"};
        const mp_node *sound_root=mp_find_named(s->skin.project,"RootWindow","SoundRoot");
        const mp_node *caption=mp_find_named(sound_root,"TextFace","title");
        launcher_skin_text(&s->skin,dc,caption,(RECT){108,122,s->page==CONTROLS?620:792,160},titles[s->page]);
        if (s->page!=CONTROLS) {
            RECT icon={390,156,510,211};
            launcher_skin_image_draw(&s->skin,dc,s->page==VIDEO?4:5,icon);
        }
        for (int i=0;i<s->count;i++) if (s->label_text[i][0]) {
            RECT r=s->label_rect[i];
            launcher_skin_control_text(&s->skin,dc,r,s->label_text[i],0);
        }
    }
    RECT foot=authored_rect(0,438,640,42);
    HBRUSH black=CreateSolidBrush(RGB(0,0,0)); FillRect(dc,&foot,black); DeleteObject(black);
    if(s->page==HOME && s->update_manifest.version[0] && IsWindowVisible(s->update_button)) {
        wchar_t label[100];char text[100];
        snprintf(text,sizeof text,"Update available: %s",s->update_manifest.version);wide(text,label,100);
        paint_text(dc,s->normal,60,656,550,30,RGB(230,230,230),label,DT_SINGLELINE);
        return;
    }
    const wchar_t *tip=s->page==HOME?descriptions[s->selection]:L"";
    launcher_skin_text(&s->skin,dc,launcher_skin_widget(&s->skin,"ToolTipFace","tip"),
                       authored_rect(0,446,640,34),tip);
}
static void draw_button_direct(launcher_window *s, DRAWITEMSTRUCT *d)
{
    if (d->CtlType == ODT_COMBOBOX) {
        wchar_t text[256] = L"";
        int index = (int)d->itemID;
        if (index < 0)
            index = (int)SendMessageW(d->hwndItem, CB_GETCURSEL, 0, 0);
        if (index >= 0)
            SendMessageW(d->hwndItem, CB_GETLBTEXT, index, (LPARAM)text);
        HBRUSH b =
            CreateSolidBrush(d->itemState & ODS_SELECTED ? RGB(184, 132, 46) : RGB(170, 170, 172));
        FillRect(d->hDC, &d->rcItem, b);
        DeleteObject(b);
        RECT text_rect=d->rcItem; text_rect.left+=8; text_rect.right-=24;
        if(s->disc_valid) {
            launcher_skin_field(&s->skin,d->hDC,d->rcItem,d->itemState&ODS_SELECTED);
            launcher_skin_control_text(&s->skin,d->hDC,text_rect,text,0);
        } else paint_text(d->hDC,s->normal,text_rect.left,text_rect.top,text_rect.right-text_rect.left,
                          text_rect.bottom-text_rect.top,RGB(0,0,0),text,DT_SINGLELINE|DT_VCENTER);
        return;
    }
    if (s->backdrop) {
        POINT origin = {0, 0};
        MapWindowPoints(d->hwndItem, s->window, &origin, 1);
        HDC background = CreateCompatibleDC(d->hDC);
        HBITMAP old = SelectObject(background, s->backdrop);
        BitBlt(d->hDC, 0, 0, d->rcItem.right, d->rcItem.bottom, background, origin.x, origin.y,
               SRCCOPY);
        SelectObject(background, old);
        DeleteDC(background);
    }
    RECT r = d->rcItem;
    int focus = (d->itemState & ODS_FOCUS) != 0, pressed = d->itemState & ODS_SELECTED;
    focus |= s->hovered_control==d->hwndItem;
    if(s->disc_valid && d->CtlID>=BACK) {
        wchar_t text[128]; GetWindowTextW(d->hwndItem,text,128);
        if(d->CtlID==UPDATE_CHECK) swprintf(text,128,L"Updates: %s",s->start->preferences.check_updates?L"On":L"Off");
        if(d->CtlID==MUSIC_MUTE) swprintf(text,128,L"Music: %s",s->start->preferences.mute_music?L"Off":L"On");
        int slot=d->CtlID==MUSIC_MUTE?14:d->CtlID==UPDATE?12:d->CtlID==UPDATE_CHECK?13:d->CtlID==TEST_AUDIO?11:8+d->CtlID-BACK;
        launcher_skin_button(&s->skin,d->hDC,r,pressed?1:s->highlight[slot]);
        RECT label=r; label.left+=14; label.right-=14;
        launcher_skin_control_text(&s->skin,d->hDC,label,text,1);
        return;
    }

    if (d->CtlID >= ROW && d->CtlID < ROW + 8) {
        int item=d->CtlID-ROW;
        wchar_t name[128]; GetWindowTextW(d->hwndItem,name,128);
        if (!s->disc_valid) { focus=1; goto capsule; }
        if (item==7) { launcher_skin_image_draw(&s->skin,d->hDC,s->hover==7?10:9,r); return; }
        const mp_node *widget=launcher_skin_widget(&s->skin,"Composite","Item");
        double dimensions[4],g[4]; mp_numbers(widget,"geometry",dimensions,4);
        const mp_node *bg=mp_find_named(widget,"ImageFace","bg5");
        mp_numbers(bg,"geometry",g,4);
        double sx=r.right/dimensions[2],sy=r.bottom/dimensions[3];
        RECT tile={(LONG)lround(g[0]*sx),(LONG)lround(g[1]*sy),
                   (LONG)lround((g[0]+g[2])*sx),(LONG)lround((g[1]+g[3])*sy)};
        launcher_skin_image_draw(&s->skin,d->hDC,1,tile);
        /* P: 140 ms hover fade; tint comes from OptionItem.bgf in the ISO. */
        launcher_skin_highlight(&s->skin,d->hDC,tile,s->highlight[item]*.3f);
        const int pictures[]={2,33,4,5,6,7,8};
        mp_numbers(mp_find_named(widget,"ImageFace","bg"),"geometry",g,4);
        RECT icon={(LONG)lround(g[0]*sx),(LONG)lround(g[1]*sy),
                   (LONG)lround((g[0]+g[2])*sx),(LONG)lround((g[1]+g[3])*sy)};
        if (item==1) {
            /* P: the disc logo fits inside the Options icon slot with breathing
               room; retain the source aspect ratio rather than stretch it. */
            const launcher_skin_image *logo=&s->skin.images[pictures[item]];
            double scale=fmin((icon.right-icon.left)*.72/logo->width,
                              (icon.bottom-icon.top)*.72/logo->height);
            LONG width=(LONG)lround(logo->width*scale),height=(LONG)lround(logo->height*scale);
            LONG x=(tile.left+tile.right-width)/2,y=(icon.top+icon.bottom-height)/2;
            icon=(RECT){x,y,x+width,y+height};
        }
        launcher_skin_image_draw(&s->skin,d->hDC,pictures[item],icon);
        mp_numbers(mp_find_named(widget,"TextFace","label"),"geometry",g,4);
        if (item==6) swprintf(name,128,L"Startup: %s",s->start->preferences.show?L"On":L"Off");
        RECT label={(LONG)lround(g[0]*sx),(LONG)lround(g[1]*sy),
                    (LONG)lround((g[0]+g[2])*sx),(LONG)lround((g[1]+g[3])*sy)};
        launcher_skin_text(&s->skin,d->hDC,mp_find_named(widget,"TextFace","label"),label,name);
        return;
    }
capsule:;
    int height = r.bottom - r.top;
    /* P: procedural capsule bevel replaces the disc button images. */
    int saved = SaveDC(d->hDC);
    HRGN clip = CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + 1, height, height);
    SelectClipRgn(d->hDC, clip);
    COLORREF top = focus ? RGB(206, 170, 104) : RGB(203, 203, 208),
             bottom = focus ? RGB(184, 132, 46) : RGB(154, 154, 161);
    if (pressed) {
        COLORREF t = top;
        top = bottom;
        bottom = t;
    }
    gradient(d->hDC, r, top, bottom);
    RECT shine = {r.left, r.top + 1, r.right, r.top + height / 3};
    gradient(d->hDC, shine, focus ? RGB(218, 190, 130) : RGB(222, 222, 225), top);
    RestoreDC(d->hDC, saved);
    DeleteObject(clip);
    HPEN pen = CreatePen(PS_SOLID, 1, focus ? RGB(119, 86, 32) : RGB(117, 117, 124)),
         old_pen = SelectObject(d->hDC, pen);
    HBRUSH old_brush = SelectObject(d->hDC, GetStockObject(NULL_BRUSH));
    RoundRect(d->hDC, r.left, r.top, r.right, r.bottom, height, height);
    SelectObject(d->hDC, old_brush);
    SelectObject(d->hDC, old_pen);
    DeleteObject(pen);
    wchar_t name[128];
    GetWindowTextW(d->hwndItem, name, 128);
    if (d->CtlID == UPDATE_CHECK) swprintf(name,128,L"Updates: %s",s->start->preferences.check_updates?L"On":L"Off");
    if (d->CtlID == ROW + 6)
        swprintf(name, 128, L"Show at startup: %s", s->start->preferences.show ? L"On" : L"Off");
    paint_text(d->hDC, s->normal, r.left + 14, r.top, r.right - r.left - 28, height, RGB(0, 0, 0),
               name, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
}
static void hover(launcher_window *s, int item);
/* the controller walks the tiles as a grid: Play, Game disc, Player data /
   Video, Audio, Controls / startup, Exit */
static void pad_navigate(launcher_window *s, int key)
{
    if (!s->disc_valid) { s->selection=1; return; }
    int at=s->hover>=0?s->hover:s->selection, next=at;
    if (key==VK_RIGHT || key==VK_LEFT) {
        int first=at<5?0:5, count=at<5?5:3;
        next=first+(at-first+(key==VK_RIGHT?1:count-1))%count;
    } else if (key==VK_DOWN) next=at<5?(at<2?at+5:7):0;
    else next=at<5?(at<2?at+5:7):(at==7?4:at-5);
    s->pad_mode = 1;
    hover(s, next);
}
/* the buttons drawn off screen and put up whole: the backdrop's photographs
   repaint them every few frames, half-drawn tiles flickered */
static void draw_button(launcher_window *s, DRAWITEMSTRUCT *d)
{
    if (d->CtlType != ODT_BUTTON) {
        draw_button_direct(s, d);
        return;
    }
    int w = d->rcItem.right - d->rcItem.left, h = d->rcItem.bottom - d->rcItem.top;
    HDC mem = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, w, h), old = SelectObject(mem, bmp);
    DRAWITEMSTRUCT off = *d;
    off.hDC = mem;
    off.rcItem = (RECT){0, 0, w, h};
    draw_button_direct(s, &off);
    BitBlt(d->hDC, d->rcItem.left, d->rcItem.top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
}
static void hover(launcher_window *s, int item)
{
    if (item == s->hover)
        return;
    int before = s->hover;
    s->hover = item;
    if (item >= 0) launcher_audio_play(s->menu_audio,"cursor");
    if (item >= 0)
        s->selection = item;
    if (before >= 0)
        InvalidateRect(s->rows[before], NULL, FALSE);
    if (item >= 0)
        InvalidateRect(s->rows[item], NULL, FALSE);
    InvalidateRect(s->window, NULL, FALSE);
}
static LRESULT CALLBACK window_proc(HWND win, UINT msg, WPARAM w, LPARAM l)
{
    launcher_window *s = (launcher_window *)GetWindowLongPtrW(win, GWLP_USERDATA);
    if (msg == WM_NCCREATE) {
        s = ((CREATESTRUCTW *)l)->lpCreateParams;
        s->window = win;
        SetWindowLongPtrW(win, GWLP_USERDATA, (LONG_PTR)s);
    }
    if (!s)
        return DefWindowProcW(win, msg, w, l);
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(win, &ps);
        HDC back = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, 900, 700), old = SelectObject(back, bmp);
        draw(s, back);
        BitBlt(dc, 0, 0, 900, 700, back, 0, 0, SRCCOPY);
        SelectObject(back, old);
        DeleteObject(bmp);
        DeleteDC(back);
        EndPaint(win, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT *)l)->itemHeight = 22;
        return TRUE;
    case WM_DRAWITEM:
        draw_button(s, (DRAWITEMSTRUCT *)l);
        return TRUE;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORLISTBOX:
        if((HWND)l==s->update_check) {
            SetTextColor((HDC)w,RGB(230,230,230));SetBkColor((HDC)w,RGB(12,12,14));
            return (LRESULT)GetStockObject(BLACK_BRUSH);
        }
        SetTextColor((HDC)w, RGB(0, 0, 0));
        SetBkMode((HDC)w, TRANSPARENT);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    case WM_COMMAND: {
        int id = LOWORD(w), event = HIWORD(w);
        if (event==BN_CLICKED && id>=BACK) launcher_audio_play(s->menu_audio,id==BACK?"cancel":id==APPLY?"ok":"select");
        if (event==CBN_SELCHANGE) {
            if(s->page==AUDIO) menu_volume(s,value(s,0),value(s,1));
            launcher_audio_play(s->menu_audio,"cursor");
        }
        if(id==MUSIC_MUTE && event==BN_CLICKED) {
            s->start->preferences.mute_music=!s->start->preferences.mute_music;
            launcher_preferences_save(&s->start->preferences);
            if(s->page==AUDIO) menu_volume(s,value(s,0),value(s,1));
            else menu_volume(s,s->current.volume[0],s->current.volume[1]);
            InvalidateRect(s->music_mute,NULL,FALSE); return 0;
        }
        if(id==UPDATE_CHECK && event==BN_CLICKED) {
            s->start->preferences.check_updates=!s->start->preferences.check_updates;
            InvalidateRect(s->update_check,NULL,FALSE);
            launcher_preferences_save(&s->start->preferences);
            if(!s->start->preferences.check_updates) ShowWindow(s->update_button,SW_HIDE);
            else if(!s->update_job) s->update_job=launcher_update_start(OPENGT4_UPDATE_URL,OPENGT4_UPDATE_TOKEN,NULL);
            InvalidateRect(win,NULL,FALSE); return 0;
        }
        if(id==UPDATE && event==BN_CLICKED && !s->update_job && s->update_manifest.version[0]) {
            s->update_job=launcher_update_start(OPENGT4_UPDATE_URL,OPENGT4_UPDATE_TOKEN,&s->update_manifest);
            if(s->update_job) {
                s->update_downloading=1;EnableWindow(s->update_button,FALSE);
                SetWindowTextW(s->update_button,L"Downloading...");
            }
            return 0;
        }
        if (id >= ROW && id < ROW + 8) {
            if (event == BN_SETFOCUS) {
                s->selection = id - ROW;
                InvalidateRect(win, NULL, FALSE);
            }
            if (event == BN_CLICKED)
                activate(s, id - ROW);
        } else if (id == BACK && event == BN_CLICKED)
            page(s, HOME);
        else if (id == APPLY && event == BN_CLICKED)
            apply(s);
        else if (id == DEFAULTS && event == BN_CLICKED) {
            launcher_options saved = s->current;
            launcher_options_defaults(&s->current);
            int p = s->page;
            page(s, p);
            s->current = saved;
        } else if (id == TEST_AUDIO && event == BN_CLICKED)
            test_audio(s);
        else if (id == PORT && event == CBN_SELCHANGE) {
            s->port = (int)SendMessageW(s->port_choice, CB_GETCURSEL, 0, 0);
            page(s, CONTROLS);
        }
        return 0;
    }
    case WM_SETCURSOR:
        if (HIWORD(l)==WM_MOUSEMOVE && s->hovered_control!=(HWND)w) {
            if(s->hovered_control) InvalidateRect(s->hovered_control,NULL,FALSE);
            s->hovered_control=(HWND)w;
            if(GetDlgCtrlID(s->hovered_control)>=BACK) launcher_audio_play(s->menu_audio,"cursor");
            InvalidateRect(s->hovered_control,NULL,FALSE);
        }
        /* P: Windows cursor follows physical mouse coordinates, including child controls. */
        if (s->page == HOME) {
            int item = -1;
            for (int k = 0; k < 8; k++)
                if ((HWND)w == s->rows[k])
                    item = k;
            if (HIWORD(l) == WM_MOUSEMOVE && s->pad_mode && item != s->hover) {
                s->pad_mode = 0;
                if (s->hover >= 0)
                    InvalidateRect(s->rows[s->hover], NULL, FALSE);
            }
            if (!s->pad_mode)
                hover(s, item);
        }
        if (LOWORD(l)==HTCLIENT && s->cursor) { SetCursor(s->cursor); return TRUE; }
        break;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) {
            launcher_audio_play(s->menu_audio,"cancel");
            if (s->page != HOME)
                page(s, HOME);
            else
                DestroyWindow(win);
        }
        if (w == VK_RETURN && s->page == HOME)
            activate(s, s->selection);
        return 0;
    case WM_TIMER: {
        update_tick(s);
        if(!IsWindow(win)) return 0;
        ULONGLONG tick = GetTickCount64();
        double elapsed = (tick - s->last_tick) / 1000.;
        s->last_tick = tick;
        for (int i=0;i<15;i++) {
            HWND target=i<8?s->rows[i]:i<12?s->footer[i-8]:i==12?s->update_button:i==13?s->update_check:s->music_mute;
            float goal=i<8?(s->hover==i?1.f:0.f):
                (target && (s->hovered_control==target || GetFocus()==target)?1.f:0.f);
            float previous=s->highlight[i];
            float step=(float)(elapsed/.14); /* P: PC hover duration, not a PS2 timing claim. */
            s->highlight[i]=goal>previous?fminf(goal,previous+step):fmaxf(goal,previous-step);
            if (target && previous!=s->highlight[i]) InvalidateRect(target,NULL,FALSE);
        }
        if (s->sdl_ready && !s->probing)
            controller_tick(s);
        launcher_photos_step(&s->photos, elapsed);
        if (s->photos.current && s->photos.phase >= 6.5)
            RedrawWindow(win, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
        if (s->photos.current && s->photos.phase < elapsed)
            RedrawWindow(win, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN);
        return 0;
    }
    case WM_CLOSE:
        DestroyWindow(win);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(win, msg, w, l);
}
static int screenshot(HWND window, const char *path)
{
    HDC dc = GetDC(window), mem = CreateCompatibleDC(dc);
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = 900;
    info.bmiHeader.biHeight = -700;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    uint8_t *pixels;
    HBITMAP bmp = CreateDIBSection(dc, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    if (!bmp) {
        DeleteDC(mem);
        ReleaseDC(window, dc);
        return 0;
    }
    HBITMAP old = SelectObject(mem, bmp);
    int ok = PrintWindow(window, mem, PW_CLIENTONLY) != 0;
    for (int i = 0; i < 900 * 700; i++) {
        uint8_t b = pixels[i * 4];
        pixels[i * 4] = pixels[i * 4 + 2];
        pixels[i * 4 + 2] = b;
        pixels[i * 4 + 3] = 255;
    }
    size_t bytes = 0;
    void *png = ok ? tdefl_write_image_to_png_file_in_memory(pixels, 900, 700, 4, &bytes) : NULL;
    wchar_t name[LAUNCHER_PATH];
    FILE *f = png && wide(path, name, LAUNCHER_PATH) ? _wfopen(name, L"wb") : NULL;
    ok = f && fwrite(png, 1, bytes, f) == bytes;
    if (f && fclose(f))
        ok = 0;
    mz_free(png);
    SelectObject(mem, old);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(window, dc);
    return ok;
}
static int run_screen(launcher_startup *start, const char *shot, const char *inputs)
{
    launcher_window s = {0};
    s.start = start;
    s.probing=shot!=NULL || inputs!=NULL;
    s.hover = -1;
    s.selection = start->preferences.disc[0] ? 0 : 1;
    launcher_options_load(&s.current, start->video_file, start->preferences.saves);
    s.status[0] = 0;
    strcpy(s.device, "No controller connected.");
    HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    s.panel = CreateSolidBrush(RGB(29, 32, 35));
    s.normal = CreateFontW(-18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                           CLEARTYPE_QUALITY, 0, L"Arial");
    s.title = CreateFontW(-46, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                          CLEARTYPE_QUALITY, 0, L"Arial");
    s.font_small = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                               CLEARTYPE_QUALITY, 0, L"Arial");
    s.section = CreateFontW(-26, 0, 0, 0, FW_HEAVY, TRUE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0,
                            CLEARTYPE_QUALITY, 0, L"Arial");
    WNDCLASSW cls = {0};
    cls.lpfnWndProc = window_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.hCursor = LoadCursor(NULL, IDC_ARROW);
    cls.hIcon = LoadIconW(cls.hInstance, MAKEINTRESOURCEW(1));
    cls.lpszClassName = L"OpenGT4Preparation";
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        s.result = -1;
        goto end;
    }
    RECT r = {0, 0, 900, 700};
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    HWND win = CreateWindowExW(WS_EX_COMPOSITED,
        cls.lpszClassName, L"OpenGT4",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN, CW_USEDEFAULT,
        CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, NULL, NULL, cls.hInstance, &s);
    if (!win) {
        s.result = -1;
        goto end;
    }
    for (int i=0;i<8;i++)
        s.rows[i]=control(&s,L"BUTTON",row_names[i],BS_OWNERDRAW|BS_NOTIFY|WS_TABSTOP,
                          ROW+i,0,0,1,1);
    refresh_disc(&s);
    page(&s, HOME);
    s.update_button=control(&s,L"BUTTON",L"Update",BS_OWNERDRAW|WS_TABSTOP,UPDATE,660,646,155,38);
    ShowWindow(s.update_button,SW_HIDE);
    s.update_check=control(&s,L"BUTTON",L"Updates",BS_OWNERDRAW|WS_TABSTOP,UPDATE_CHECK,660,18,220,34);
    s.music_mute=control(&s,L"BUTTON",L"Music",BS_OWNERDRAW|WS_TABSTOP,MUSIC_MUTE,20,18,200,34);
    ShowWindow(s.music_mute,s.disc_valid?SW_SHOW:SW_HIDE);
    if(start->preferences.check_updates && !shot && !inputs)
        s.update_job=launcher_update_start(OPENGT4_UPDATE_URL,OPENGT4_UPDATE_TOKEN,NULL);
    ShowWindow(win, SW_SHOW);
    UpdateWindow(win);
    s.sdl_ready = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) == 0;
    if (s.sdl_ready && !s.probing)
        controller_tick(&s);
    s.last_tick = GetTickCount64();
    SetTimer(win, 1, 33, NULL);
    /* Reproducible UI probes use the real window messages and fields; no ISO
       is required. Play is opt-in; probes never open path selectors. */
    char *sequence = inputs ? strdup(inputs) : NULL, *token = sequence;
    for (char *next; token && *token; token = next) {
        next = strchr(token, ',');
        if (next)
            *next++ = 0;
        if (!strncmp(token,"music-mute:",11)) {
            s.start->preferences.mute_music=!atoi(token+11);
            SendMessageW(win,WM_COMMAND,MUSIC_MUTE,0);
        } else if (!strncmp(token,"update-check:",13)) {
            s.start->preferences.check_updates=!atoi(token+13);
            SendMessageW(win,WM_COMMAND,UPDATE_CHECK,0);
        } else if (!strncmp(token,"update-manifest:",16)) {
            FILE *file=fopen(token+16,"rb"); char json[16385];
            if(file) {
                size_t count=fread(json,1,sizeof json-1,file);fclose(file);json[count]=0;
                launcher_update_manifest m;
                int available=start->preferences.check_updates && launcher_update_parse(json,&m) &&
                              launcher_update_newer(m.version,OPENGT4_VERSION);
                if(available) s.update_manifest=m;
                ShowWindow(s.update_button,available?SW_SHOW:SW_HIDE);
            }
        } else if (!strncmp(token,"disc:",5)) {
            snprintf(s.start->preferences.disc,LAUNCHER_PATH,"%s",token+5);
            refresh_disc(&s); page(&s,HOME);
        } else if (!strcmp(token, "video"))
            SendMessageW(win, WM_COMMAND, ROW + 2, 0);
        else if (!strcmp(token, "audio"))
            SendMessageW(win, WM_COMMAND, ROW + 3, 0);
        else if (!strcmp(token, "controls"))
            SendMessageW(win, WM_COMMAND, ROW + 4, 0);
        else if (!strcmp(token, "left") || !strcmp(token, "right") || !strcmp(token, "up") ||
                 !strcmp(token, "down")) {
            /* the controller's directions (the keyboard's arrows do not walk the tiles) */
            int key = !strcmp(token, "left")    ? VK_LEFT
                      : !strcmp(token, "right") ? VK_RIGHT
                      : !strcmp(token, "up")    ? VK_UP
                                                : VK_DOWN;
            if (s.page == HOME)
                pad_navigate(&s, key);
        } else if (!strcmp(token, "back"))
            SendMessageW(win, WM_COMMAND, BACK, 0);
        else if (!strcmp(token, "apply"))
            SendMessageW(win, WM_COMMAND, APPLY, 0);
        else if (!strcmp(token, "default"))
            SendMessageW(win, WM_COMMAND, DEFAULTS, 0);
        else if (!strncmp(token, "focus:", 6)) {
            int selected = atoi(token + 6);
            if (s.page == HOME && selected >= 0 && selected < 8) {
                SetFocus(s.rows[selected]);
                hover(&s, selected);
            }
        } else if (!strncmp(token,"animate:",8)) {
            double step=atof(token+8)/140.;
            for(int i=0;i<8;i++) s.highlight[i]=s.hover==i?(float)fmin(1,step):0;
            RedrawWindow(win,NULL,NULL,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
        } else if (!strncmp(token, "hover:", 6)) {
            int item = atoi(token + 6);
            if (s.page == HOME && item >= 0 && item < 8)
                SendMessageW(win, WM_SETCURSOR, (WPARAM)s.rows[item],
                             MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        } else if (!strcmp(token, "startup"))
            SendMessageW(win, WM_COMMAND, ROW + 6, 0);
        else if (!strcmp(token, "pump-play")) {
            /* Test: SDL's Windows pump dispatches Play and can consume WM_QUIT. */
            PostMessageW(win,WM_COMMAND,ROW,0);
            controller_tick(&s);
        }
        else if (!strcmp(token, "play"))
            SendMessageW(win, WM_COMMAND, ROW, 0);
        else if (!strcmp(token, "test-audio") && s.page == AUDIO)
            SendMessageW(win, WM_COMMAND, TEST_AUDIO, 0);
        else if (!strcmp(token, "2p")) {
            SendMessageW(s.port_choice, CB_SETCURSEL, 1, 0);
            SendMessageW(win, WM_COMMAND, MAKEWPARAM(PORT, CBN_SELCHANGE), 0);
        } else if (!strncmp(token, "wait:", 5))
            launcher_photos_step(&s.photos, strtod(token + 5, NULL));
        else if (!strncmp(token, "set:", 4)) {
            int field, choice;
            if (sscanf(token + 4, "%d:%d", &field, &choice) == 2 && field >= 0 && field < s.count)
                SendMessageW(s.fields[field], CB_SETCURSEL, choice, 0);
        }
    }
    free(sequence);
    UpdateWindow(win);
    int visible=0;
    for (int i=0;i<8;i++) if (IsWindowVisible(s.rows[i])) visible++;
    printf("LAUNCHER STAGE %s CACHE %d BUTTONS %d\n",s.disc_valid?"options":"disc",s.skin.cache_hit,visible);
    printf("LAUNCHER UPDATE %s CHECK %d VERSION %s\n",IsWindowVisible(s.update_button)?s.update_manifest.version:"none",
           start->preferences.check_updates,OPENGT4_VERSION);
    printf("LAUNCHER FILTER %s\n",s.skin.filter?"FSR1":"none");
    printf("LAUNCHER MUSIC MUTE %d\n",s.start->preferences.mute_music);
    printf("LAUNCHER PRESENTATION cursor=%d audio=%d hover=%.2f\n",s.cursor!=NULL,s.menu_audio!=NULL,s.highlight[s.selection]);
    if (inputs)
        printf("LAUNCHER FOCUS %d\n", s.selection);
    if (shot) {
        s.result = screenshot(win, shot) ? 0 : -1;
        DestroyWindow(win);
    }
    MSG msg;
    /* SDL WIN_PumpEvents uses PeekMessage(PM_REMOVE), including WM_QUIT.
       Window lifetime is the authoritative exit condition after Play. */
    while (IsWindow(win) && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            HWND f = GetFocus();
            wchar_t name[32];
            GetClassNameW(f, name, 32);
            if (!wcscmp(name, L"ComboBox") && SendMessageW(f, CB_GETDROPPEDSTATE, 0, 0))
                SendMessageW(f, CB_SHOWDROPDOWN, FALSE, 0);
            else
                SendMessageW(win, WM_KEYDOWN, VK_ESCAPE, 0);
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
            HWND focus = GetFocus();
            wchar_t name[32];
            GetClassNameW(focus, name, 32);
            if (!wcscmp(name, L"Button")) {
                SendMessageW(focus, BM_CLICK, 0, 0);
                continue;
            }
            if (!wcscmp(name, L"ComboBox")) {
                SendMessageW(focus, CB_SHOWDROPDOWN, !SendMessageW(focus, CB_GETDROPPEDSTATE, 0, 0),
                             0);
                continue;
            }
        }
        if (msg.message == WM_KEYDOWN && s.page == HOME &&
            (msg.wParam == VK_LEFT || msg.wParam == VK_RIGHT || msg.wParam == VK_UP ||
             msg.wParam == VK_DOWN))
            continue; /* no keyboard walk through the tiles: the mouse chooses */
        if (!IsDialogMessageW(win, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
end:
    launcher_audio_close(s.menu_audio);
    if(s.cursor) { SetCursor(LoadCursor(NULL,IDC_ARROW)); DestroyCursor(s.cursor); }
    launcher_update_release(s.update_job);
    for (int p = 0; p < 2; p++)
        if (s.pads[p])
            SDL_GameControllerClose(s.pads[p]);
    if (s.audio)
        SDL_CloseAudioDevice(s.audio);
    SDL_QuitSubSystem(SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS);
    launcher_photos_close(&s.photos);
    launcher_skin_close(&s.skin);
    if (s.backdrop)
        DeleteObject(s.backdrop);
    if (s.icons)
        DeleteObject(s.icons);
    if (s.large_icons)
        DeleteObject(s.large_icons);
    DeleteObject(s.normal);
    DeleteObject(s.font_small);
    DeleteObject(s.title);
    DeleteObject(s.section);
    DeleteObject(s.panel);
    if (SUCCEEDED(com))
        CoUninitialize();
    return s.result;
}
#endif
int launcher_bootstrap(int *argc, char ***argv, launcher_startup *start)
{
    if (*argc > 1 && strcmp((*argv)[1], "--launcher"))
        return 1;
    int force = *argc > 1;
    const char *file = NULL, *disc = NULL, *shot = NULL, *inputs = NULL;
    for (int i = 2; i < *argc; i++) {
        const char *arg = (*argv)[i];
        if (!strcmp(arg, "--launcher-config") && i + 1 < *argc)
            file = (*argv)[++i];
        else if (!strcmp(arg, "--shot") && i + 1 < *argc)
            shot = (*argv)[++i];
        else if (!strcmp(arg, "--input") && i + 1 < *argc)
            inputs = (*argv)[++i];
        else if (arg[0] != '-' && !disc)
            disc = arg;
        else
            return -1;
    }
    if (file && strlen(file) >= LAUNCHER_PATH - 32)
        return -1;
    char *pref = SDL_GetPrefPath("OpenGT4", "OpenGT4");
    if (!pref)
        return -1;
    char config[LAUNCHER_PATH];
    snprintf(config, sizeof config, "%slauncher.ini", pref);
    launcher_preferences_load(&start->preferences, file ? file : config);
    snprintf(start->video_file, sizeof start->video_file, file ? "%s.video.ini" : "%svideo.ini",
             file ? file : pref);
    if (!start->preferences.saves[0])
        snprintf(start->preferences.saves, sizeof start->preferences.saves, "%ssaves", pref);
    SDL_free(pref);
    if (disc) {
        if (strlen(disc) >= LAUNCHER_PATH || strpbrk(disc, "\r\n"))
            return -1;
        strcpy(start->preferences.disc, disc);
    }
    char message[256];
    /* Validate before reading Options artwork; the first stage never opens
       game assets. Cached art cannot make an invalid disc valid. */
    if (force || start->preferences.show ||
        !launcher_validate_disc(start->preferences.disc, message, sizeof message)) {
#ifdef _WIN32
        int result = run_screen(start, shot, inputs);
        if (result != 1)
            return result;
#else
        fprintf(
            stderr,
            "Standalone preparation currently requires Windows. Pass the ISO on the command line.\n");
        return -1;
#endif
    }
    start->arguments[0] = (*argv)[0];
    start->arguments[1] = start->preferences.disc;
    start->arguments[2] = "--save-dir";
    start->arguments[3] = start->preferences.saves;
    start->arguments[4] = "--config-file";
    start->arguments[5] = start->video_file;
    start->arguments[6] = NULL;
    *argc = 6;
    *argv = start->arguments;
    return 1;
}
