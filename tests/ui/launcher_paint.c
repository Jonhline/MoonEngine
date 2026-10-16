/* Exercise real Win32 controls between erasure and complete custom painting. */
#include "../../src/app/launcher.c"
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "paint line %d: %s\n", __LINE__, #x);                                  \
            return 1;                                                                              \
        }                                                                                          \
    } while (0)
int main(int argc, char **argv)
{
    CHECK(argc >= 3);
    launcher_window state = {0};
    CHECK(launcher_skin_open(&state.skin, argv[1], argv[2]));
    state.disc_valid = 1;
    state.panel = CreateSolidBrush(RGB(65, 66, 67));
    state.normal = GetStockObject(DEFAULT_GUI_FONT);
    WNDCLASSW cls = {0};
    cls.lpfnWndProc = window_proc;
    cls.hInstance = GetModuleHandleW(NULL);
    cls.lpszClassName = L"MoonLauncherPaintTest";
    CHECK(RegisterClassW(&cls));
    HWND window = CreateWindowExW(0, cls.lpszClassName, L"", WS_OVERLAPPED, 0, 0, LAUNCHER_WIDTH,
                                  LAUNCHER_HEIGHT, NULL, NULL, cls.hInstance, &state);
    CHECK(window);
    HWND button = control(&state, L"BUTTON", L"Play", BS_OWNERDRAW, ROW, 20, 500, 128, 140);
    HWND combo =
        control(&state, L"COMBOBOX", L"", CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS,
                300, 40, 90, 280, 200);
    CHECK(button && combo);
    SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)L"Widescreen 16:9");
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
    HDC screen = GetDC(window), dc = CreateCompatibleDC(screen);
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof info.bmiHeader;
    info.bmiHeader.biWidth = LAUNCHER_WIDTH;
    info.bmiHeader.biHeight = -LAUNCHER_HEIGHT;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    uint32_t *pixels;
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, (void **)&pixels, NULL, 0);
    CHECK(bitmap);
    HBITMAP previous = SelectObject(dc, bitmap);
    HDC backdrop = CreateCompatibleDC(screen);
    state.backdrop =
        CreateDIBSection(screen, &info, DIB_RGB_COLORS, (void **)&state.backdrop_pixels, NULL, 0);
    CHECK(state.backdrop);
    HBITMAP previous_backdrop = SelectObject(backdrop, state.backdrop);
    for (int i = 0; i < LAUNCHER_WIDTH * LAUNCHER_HEIGHT; i++)
        state.backdrop_pixels[i] = 0x00506070;
    HWND controls[] = {button, combo};
    for (unsigned step = 0; step < 100; step++) {
        for (unsigned item = 0; item < 2; item++) {
            GdiFlush();
            for (int i = 0; i < LAUNCHER_WIDTH * LAUNCHER_HEIGHT; i++)
                pixels[i] = 0x00123456;
            if (argc > 3 && !item) {
                WNDPROC original = (WNDPROC)GetPropW(button, L"OpenGT4ControlProc");
                CallWindowProcW(original, button, WM_ERASEBKGND, (WPARAM)dc, 0);
            } else
                CHECK(SendMessageW(controls[item], WM_ERASEBKGND, (WPARAM)dc, 0) == 1);
            GdiFlush();
            for (int i = 0; i < LAUNCHER_WIDTH * LAUNCHER_HEIGHT; i++)
                CHECK(pixels[i] == 0x00123456);
        }
    }
    RECT rect;
    GetClientRect(combo, &rect);
    SelectObject(backdrop, previous_backdrop);
    SendMessageW(combo, WM_PRINTCLIENT, (WPARAM)dc, PRF_CLIENT);
    GdiFlush();
    size_t bytes = (size_t)LAUNCHER_WIDTH * LAUNCHER_HEIGHT * 4;
    uint32_t *actual = malloc(bytes);
    CHECK(actual);
    memcpy(actual, pixels, bytes);
    POINT origin = {0, 0};
    MapWindowPoints(combo, window, &origin, 1);
    SelectObject(backdrop, state.backdrop);
    BitBlt(dc, 0, 0, rect.right, rect.bottom, backdrop, origin.x, origin.y, SRCCOPY);
    launcher_skin_field(&state.skin, dc, rect, 0);
    rect.left += 8;
    rect.right -= 30;
    launcher_skin_control_text(&state.skin, dc, rect, L"Widescreen 16:9", 0);
    GdiFlush();
    for (size_t i = 0; i < bytes / 4; i++)
        if ((actual[i] & 0xffffff) != (pixels[i] & 0xffffff)) {
            fprintf(stderr, "composition pixel %zu,%zu: %08x/%08x focus=%d rect=%ldx%ld\n",
                    i % LAUNCHER_WIDTH, i / LAUNCHER_WIDTH, actual[i], pixels[i],
                    GetFocus() == combo, rect.right + 30, rect.bottom);
            CHECK(0);
        }
    free(actual);
    SelectObject(dc, previous);
    SelectObject(backdrop, previous_backdrop);
    DeleteObject(bitmap);
    DeleteObject(state.backdrop);
    state.backdrop = NULL;
    DeleteDC(dc);
    DeleteDC(backdrop);
    ReleaseDC(window, screen);
    DestroyWindow(window);
    launcher_skin_close(&state.skin);
    DeleteObject(state.panel);
    puts("200 native erase messages leave pixels intact; complete dropdown matches composition");
    return 0;
}
