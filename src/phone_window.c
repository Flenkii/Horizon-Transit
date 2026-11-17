/* The phone window, see phone_window.h. */
#include <windows.h>
#include <windowsx.h>
#include <string.h>
#include <stdio.h>
#include "phone_window.h"
#include "core/phone_draw.h"

static PhoneLogFn plog;
static SRWLOCK lock = SRWLOCK_INIT;
static TsHud want_hud;
static int want_theme, want_on;
static int sign_number, sign_over, sign_on;
static char sign_unit[8];
static volatile LONG ok, visible, mouse_on;
static PhoneClickFn on_click;
static PhoneWheelFn on_wheel;
/* the window's thread only: where the drawn phone can be clicked, and whether it takes clicks now */
static TsPhoneHit hits[TS_PHONE_HITS];
static int hit_count, clickable;

void phone_window_mouse(int enabled, PhoneClickFn click, PhoneWheelFn wheel) {
    on_click = click, on_wheel = wheel;
    InterlockedExchange(&mouse_on, enabled != 0);
}

void phone_window_show(const TsHud *h, int theme, int want) {
    AcquireSRWLockExclusive(&lock);
    if (want) want_hud = *h;
    want_theme = theme;
    want_on = want;
    ReleaseSRWLockExclusive(&lock);
}

void phone_window_sign(int number, const char *unit, int over, int want) {
    AcquireSRWLockExclusive(&lock);
    sign_on = want && number > 0;
    sign_number = number, sign_over = over;
    snprintf(sign_unit, sizeof(sign_unit), "%s", unit ? unit : "");
    ReleaseSRWLockExclusive(&lock);
}

static TsHud dash_hud;
static int dash_on, dash_right = 410, dash_bottom = 110;

void phone_window_dash(const TsHud *h, int want) {
    AcquireSRWLockExclusive(&lock);
    if (want) dash_hud = *h;
    dash_on = want;
    ReleaseSRWLockExclusive(&lock);
}

void phone_window_dash_place(int right, int bottom) {
    AcquireSRWLockExclusive(&lock);
    dash_right = right, dash_bottom = bottom;
    ReleaseSRWLockExclusive(&lock);
}

static TsFeedLine feed_lines[TS_FEED_LINES];
static ULONGLONG feed_until[TS_FEED_LINES];
static int feed_count, feed_serial;

void phone_window_feed(const char *name, const char *text, int style) {
    AcquireSRWLockExclusive(&lock);
    if (feed_count == TS_FEED_LINES) {
        memmove(feed_lines, feed_lines + 1, sizeof(feed_lines[0]) * (TS_FEED_LINES - 1));
        memmove(feed_until, feed_until + 1, sizeof(feed_until[0]) * (TS_FEED_LINES - 1));
        feed_count--;
    }
    TsFeedLine *l = &feed_lines[feed_count];
    snprintf(l->name, sizeof(l->name), "%s", name ? name : "");
    snprintf(l->text, sizeof(l->text), "%s", text ? text : "");
    l->style = style;
    feed_until[feed_count] = GetTickCount64() + 8000 + (ULONGLONG)(strlen(l->text) > 58 ? 3000 : 0);
    feed_count++;
    feed_serial++;
    ReleaseSRWLockExclusive(&lock);
}

int phone_window_ok(void) { return ok != 0; }
int phone_window_visible(void) { return visible != 0; }

/* ------------------------------------------------------------------ the game's window */

static HWND own_window, sign_window, feed_window;

typedef struct {
    HWND best;
    LONG area;
} FindGame;

static BOOL CALLBACK find_game_cb(HWND w, LPARAM user) {
    FindGame *f = (FindGame *)user;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid != GetCurrentProcessId() || w == own_window || w == sign_window || w == feed_window || !IsWindowVisible(w) || GetWindow(w, GW_OWNER)) return TRUE;
    LONG_PTR ex = GetWindowLongPtrW(w, GWL_EXSTYLE);
    if (ex & (WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT)) return TRUE; /* other mods' overlays */
    RECT r;
    if (!GetClientRect(w, &r)) return TRUE;
    LONG area = (r.right - r.left) * (r.bottom - r.top);
    if (area > f->area) f->area = area, f->best = w;
    return TRUE;
}

static HWND find_game(void) {
    FindGame f = {NULL, 640 * 360};
    EnumWindows(find_game_cb, (LPARAM)&f);
    return f.best;
}

/* ------------------------------------------------------------------ texts with GDI */

typedef struct {
    HDC dc;
} Gdi;

static HFONT make_font(int size, int weight) {
    return CreateFontW(-size, 0, 0, 0, weight >= 700 ? FW_BOLD : weight >= 600 ? FW_SEMIBOLD : FW_NORMAL, 0, 0, 0,
                       DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS,
                       L"Segoe UI");
}

static int widen(const char *utf8, wchar_t *out, int cap) {
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, cap);
    if (n <= 0) {
        out[0] = 0;
        return 0;
    }
    return n - 1;
}

static void gdi_text(void *user, const TsPhoneText *t) {
    Gdi *g = (Gdi *)user;
    wchar_t w[256];
    int n = widen(t->utf8, w, 256);
    if (!n) return;
    HFONT f = make_font(t->size, t->weight);
    HGDIOBJ old = SelectObject(g->dc, f);
    SetTextColor(g->dc, RGB((t->color >> 16) & 255, (t->color >> 8) & 255, t->color & 255));
    RECT r = {t->x, t->y, t->x + t->w, t->y + t->h};
    UINT flags = DT_NOPREFIX;
    if (t->wrap) flags |= DT_WORDBREAK | DT_EDITCONTROL | DT_END_ELLIPSIS | DT_CENTER;
    else flags |= DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS |
                  (t->align == TS_TEXT_CENTER ? DT_CENTER : t->align == TS_TEXT_RIGHT ? DT_RIGHT : DT_LEFT);
    DrawTextW(g->dc, w, n, &r, flags);
    SelectObject(g->dc, old);
    DeleteObject(f);
}

static int gdi_measure(void *user, const TsPhoneText *t) {
    Gdi *g = (Gdi *)user;
    wchar_t w[256];
    int n = widen(t->utf8, w, 256);
    if (!n) return 0;
    HFONT f = make_font(t->size, t->weight);
    HGDIOBJ old = SelectObject(g->dc, f);
    SIZE sz = {0, 0};
    GetTextExtentPoint32W(g->dc, w, n, &sz);
    SelectObject(g->dc, old);
    DeleteObject(f);
    return sz.cx;
}

/* ------------------------------------------------------------------ the window */

static const TsPhoneHit *hit_at(int x, int y) {
    for (int i = 0; i < hit_count; ++i)
        if (x >= hits[i].x && y >= hits[i].y && x < hits[i].x + hits[i].w && y < hits[i].y + hits[i].h) return &hits[i];
    return NULL;
}

static LRESULT CALLBACK wnd_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_MOUSEACTIVATE) return MA_NOACTIVATE; /* the game keeps the focus */
    int mine = w == own_window && clickable;
    if (msg == WM_NCHITTEST) return mine ? HTCLIENT : HTTRANSPARENT;
    if (mine && msg == WM_SETCURSOR) {
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(w, &pt);
        SetCursor(LoadCursorW(NULL, hit_at(pt.x, pt.y) ? (LPCWSTR)IDC_HAND : (LPCWSTR)IDC_ARROW));
        return TRUE;
    }
    if (mine && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK || msg == WM_LBUTTONUP)) return 0; /* kept from the game; poll_mouse acts */
    if (mine && msg == WM_MOUSEWHEEL) {
        if (on_wheel) on_wheel(GET_WHEEL_DELTA_WPARAM(wp) > 0 ? -1 : 1);
        return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

/* The mouse, read directly (the game may keep the mouse's messages to itself or hide the pointer): where the
   pointer is over the phone, and a press of the left button. */
static int was_down, mouse_logs, over_logged;
static int poll_mouse(POINT pos, int bw, int bh, int *cx, int *cy) {
    POINT pt;
    *cx = *cy = -1;
    if (!GetCursorPos(&pt)) return 0;
    int x = pt.x - pos.x, y = pt.y - pos.y;
    int inside = x >= 0 && y >= 0 && x < bw && y < bh;
    int down = (GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) & 0x8000) != 0;
    CURSORINFO ci;
    memset(&ci, 0, sizeof(ci));
    ci.cbSize = sizeof(ci);
    int pointer_shown = GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING);
    if (inside && !over_logged) {
        over_logged = 1;
        if (plog) plog("mouse: pointer over the phone (Windows shows the pointer: %s)", pointer_shown ? "yes" : "no, the phone draws one");
    }
    if (inside && down && !was_down) {
        const TsPhoneHit *h = hit_at(x, y);
        if (plog && mouse_logs < 20) mouse_logs++, plog("mouse: click at %d,%d -> %s %d", x, y, h ? (h->tab >= 0 ? "tab" : "row") : "nothing", h ? (h->tab >= 0 ? h->tab : h->row) : -1);
        if (h && on_click) on_click(h->tab, h->row, h->row2);
    }
    was_down = down;
    if (inside && !pointer_shown) *cx = x, *cy = y;
    return inside;
}

static int in_poly(const float *xy, int n, float x, float y) {
    int in = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) in = !in;
    }
    return in;
}

/* an arrow pointer (white, black edge) into the premultiplied picture, its tip at (cx, cy) */
static void draw_pointer(uint32_t *bits, int bw, int bh, int cx, int cy, float scale) {
    static const float shape[] = {0, 0, 0, 17, 4, 13, 7, 20, 9.5f, 19, 6.5f, 12, 12, 12};
    float poly[14];
    float s = scale < 0.8f ? 0.8f : scale;
    for (int i = 0; i < 7; ++i) poly[2 * i] = shape[2 * i] * s, poly[2 * i + 1] = shape[2 * i + 1] * s;
    int w = (int)(14 * s) + 3, h = (int)(22 * s) + 3;
    for (int y = -2; y < h; ++y)
        for (int x = -2; x < w; ++x) {
            int px = cx + x, py = cy + y;
            if (px < 0 || py < 0 || px >= bw || py >= bh) continue;
            float fx = x + 0.5f, fy = y + 0.5f;
            int fill = in_poly(poly, 7, fx, fy);
            int edge = !fill && (in_poly(poly, 7, fx - 1.2f, fy) || in_poly(poly, 7, fx + 1.2f, fy) || in_poly(poly, 7, fx, fy - 1.2f) ||
                                 in_poly(poly, 7, fx, fy + 1.2f));
            if (fill) bits[py * bw + px] = 0xFFFFFFFFu;
            else if (edge) bits[py * bw + px] = 0xFF000000u;
        }
}

/* clicks on the phone (shown, mouse on) go to it, else through to the game */
static void set_clickable(int want) {
    if (want == clickable || !own_window) return;
    clickable = want;
    LONG_PTR ex = GetWindowLongPtrW(own_window, GWL_EXSTYLE);
    ex = want ? ex & ~(LONG_PTR)WS_EX_TRANSPARENT : ex | WS_EX_TRANSPARENT;
    SetWindowLongPtrW(own_window, GWL_EXSTYLE, ex);
    SetWindowPos(own_window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    if (plog) plog("phone window: mouse %s", want ? "on" : "off");
}

static DWORD WINAPI window_thread(void *unused) {
    (void)unused;
    HINSTANCE inst = GetModuleHandleW(NULL);
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"HorizonTransitPhone";
    RegisterClassExW(&wc);
    own_window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                 wc.lpszClassName, L"Horizon Transit", WS_POPUP, 0, 0, 1, 1, NULL, NULL, inst, NULL);
    if (!own_window) {
        plog("phone window NOT made (error %lu): the phone stays in the HUD list", GetLastError());
        return 0;
    }
    InterlockedExchange(&ok, 1);
    plog("phone window made");
    sign_window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                  wc.lpszClassName, L"Horizon Transit sign", WS_POPUP, 0, 0, 1, 1, NULL, NULL, inst, NULL);
    if (!sign_window) plog("speed-limit sign window NOT made (error %lu)", GetLastError());
    feed_window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
                                  wc.lpszClassName, L"Horizon Transit lines", WS_POPUP, 0, 0, 1, 1, NULL, NULL, inst, NULL);
    HDC feed_dc = CreateCompatibleDC(NULL);
    HBITMAP feed_bmp = NULL;
    HGDIOBJ feed_old = NULL;
    uint32_t *feed_bits = NULL;
    int fbw = 0, fbh = 0, feed_shown = 0, feed_drawn_serial = -1, feed_logged = 0;
    float feed_drawn_scale = 0;
    HDC sign_dc = CreateCompatibleDC(NULL);
    HBITMAP sign_bmp = NULL;
    HGDIOBJ sign_old = NULL;
    uint32_t *sign_bits = NULL;
    int sbw = 0, sbh = 0, sign_shown = 0, sign_logged = 0;
    float sign_drawn_scale = 0;
    static TsHud dash_drawn;

    HWND game = NULL;
    ULONGLONG next_find = 0;
    HDC dc = CreateCompatibleDC(NULL);
    HBITMAP bmp = NULL;
    HGDIOBJ old_bmp = NULL;
    uint32_t *bits = NULL;
    int bw = 0, bh = 0;
    TsHud drawn;
    int drawn_theme = -1, drawn_valid = 0, shown = 0, alpha = 0, logged_first = 0;
    float drawn_scale = 0;
    char drawn_clock[8] = "";
    POINT drawn_pos = {-99999, -99999};
    int drawn_cur_x = -1, drawn_cur_y = -1;

    for (;;) {
        MSG m;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(16);

        TsHud h;
        int theme, on, s_on, s_number, s_over;
        char s_unit[8];
        AcquireSRWLockExclusive(&lock); /* exclusive: the feed's old lines are dropped here */
        on = want_on;
        theme = want_theme;
        if (on) h = want_hud;
        s_on = sign_on, s_number = sign_number, s_over = sign_over;
        int d_on = dash_on, d_right = dash_right, d_bottom = dash_bottom;
        TsFeedLine fl[TS_FEED_LINES];
        int f_n = 0, f_serial;
        {
            ULONGLONG t = GetTickCount64();
            while (feed_count > 0 && feed_until[0] <= t) { /* drop the lines whose time is up (oldest first) */
                memmove(feed_lines, feed_lines + 1, sizeof(feed_lines[0]) * (TS_FEED_LINES - 1));
                memmove(feed_until, feed_until + 1, sizeof(feed_until[0]) * (TS_FEED_LINES - 1));
                feed_count--;
                feed_serial++;
            }
            f_n = feed_count;
            memcpy(fl, feed_lines, sizeof(fl));
            f_serial = feed_serial;
        }
        TsHud dh;
        if (d_on) dh = dash_hud;
        memcpy(s_unit, sign_unit, sizeof(s_unit));
        ReleaseSRWLockExclusive(&lock);

        ULONGLONG now = GetTickCount64();
        if ((!game || !IsWindow(game)) && now >= next_find) {
            game = find_game();
            next_find = now + 2000;
            if (game) plog("phone window: game window found");
        }
        RECT client;
        POINT origin = {0, 0};
        int front = game && GetForegroundWindow() == game && !IsIconic(game) && GetClientRect(game, &client) &&
                    ClientToScreen(game, &origin) && client.bottom > 100;
        float scale = front ? client.bottom / 1080.0f : 1.0f;
        if (scale < 0.45f) scale = 0.45f;
        if (scale > 3.0f) scale = 3.0f;

        /* the ride panel: bottom right, beside the speedometer, while a passenger rides */
        (void)s_on, (void)s_number, (void)s_over;
        if (sign_window) {
            int want_dash = d_on && front;
            if (!want_dash) {
                if (sign_shown) ShowWindow(sign_window, SW_HIDE), sign_shown = 0;
            } else {
                int flash = dh.sign_over && (GetTickCount64() / 400) % 2 == 0;
                dh.sign_over = flash;
                int sw, sh;
                ts_dash_size(scale, &sw, &sh);
                int dirty = !sign_bmp || scale != sign_drawn_scale || memcmp(&dh, &dash_drawn, sizeof(dh)) != 0;
                if (dirty && (sw != sbw || sh != sbh || !sign_bmp)) {
                    if (sign_bmp) SelectObject(sign_dc, sign_old), DeleteObject(sign_bmp);
                    BITMAPINFO bi;
                    memset(&bi, 0, sizeof(bi));
                    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
                    bi.bmiHeader.biWidth = sw;
                    bi.bmiHeader.biHeight = -sh;
                    bi.bmiHeader.biPlanes = 1;
                    bi.bmiHeader.biBitCount = 32;
                    bi.bmiHeader.biCompression = BI_RGB;
                    void *p = NULL;
                    sign_bmp = CreateDIBSection(sign_dc, &bi, DIB_RGB_COLORS, &p, NULL, 0);
                    if (sign_bmp) sign_old = SelectObject(sign_dc, sign_bmp), sign_bits = (uint32_t *)p, sbw = sw, sbh = sh;
                }
                if (sign_bmp) {
                    if (dirty) {
                        GdiFlush();
                        memset(sign_bits, 0, (size_t)sbw * sbh * 4);
                        SetBkMode(sign_dc, TRANSPARENT);
                        Gdi g = {sign_dc};
                        TsPhoneCanvas cv = {sign_bits, sbw, sbh, gdi_text, gdi_measure, &g, NULL, 0};
                        ts_dash_draw(&dh, scale, &cv);
                        GdiFlush();
                        ts_dash_finish(scale, &cv);
                        dash_drawn = dh, sign_drawn_scale = scale;
                    }
                    POINT spos = {origin.x + client.right - (int)(d_right * scale) - sbw, origin.y + client.bottom - (int)(d_bottom * scale) - sbh};
                    SIZE ssize = {sbw, sbh};
                    POINT src = {0, 0};
                    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
                    UpdateLayeredWindow(sign_window, NULL, &spos, &ssize, sign_dc, &src, 0, &blend, ULW_ALPHA);
                    if (!sign_shown) {
                        ShowWindow(sign_window, SW_SHOWNOACTIVATE);
                        SetWindowPos(sign_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        sign_shown = 1;
                        if (!sign_logged++) plog("ride panel shown by the speedometer (%d %s)", dh.sign_limit, dh.sign_unit);
                    }
                }
            }
        }

        /* the passengers' lines: bottom left above the minimap, only when the HUD layer is off (the mod then calls phone_window_feed) */
        if (feed_window) {
            int want_feed = f_n > 0 && front;
            if (!want_feed) {
                if (feed_shown) ShowWindow(feed_window, SW_HIDE), feed_shown = 0;
            } else {
                int fw, fh;
                ts_feed_size(scale, &fw, &fh);
                int dirty = !feed_bmp || f_serial != feed_drawn_serial || scale != feed_drawn_scale;
                if (dirty && (fw != fbw || fh != fbh || !feed_bmp)) {
                    if (feed_bmp) SelectObject(feed_dc, feed_old), DeleteObject(feed_bmp);
                    BITMAPINFO bi;
                    memset(&bi, 0, sizeof(bi));
                    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
                    bi.bmiHeader.biWidth = fw;
                    bi.bmiHeader.biHeight = -fh;
                    bi.bmiHeader.biPlanes = 1;
                    bi.bmiHeader.biBitCount = 32;
                    bi.bmiHeader.biCompression = BI_RGB;
                    void *p = NULL;
                    feed_bmp = CreateDIBSection(feed_dc, &bi, DIB_RGB_COLORS, &p, NULL, 0);
                    if (feed_bmp) feed_old = SelectObject(feed_dc, feed_bmp), feed_bits = (uint32_t *)p, fbw = fw, fbh = fh;
                }
                if (feed_bmp) {
                    if (dirty) {
                        GdiFlush();
                        memset(feed_bits, 0, (size_t)fbw * fbh * 4);
                        SetBkMode(feed_dc, TRANSPARENT);
                        Gdi g = {feed_dc};
                        TsPhoneCanvas cv = {feed_bits, fbw, fbh, gdi_text, gdi_measure, &g, NULL, 0};
                        ts_feed_draw(fl, f_n, scale, &cv);
                        GdiFlush();
                        ts_feed_finish(scale, &cv);
                        feed_drawn_serial = f_serial, feed_drawn_scale = scale;
                    }
                    POINT fpos = {origin.x + (int)(24 * scale), origin.y + client.bottom - (int)(370 * scale) - fbh};
                    SIZE fsize = {fbw, fbh};
                    POINT src = {0, 0};
                    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
                    UpdateLayeredWindow(feed_window, NULL, &fpos, &fsize, feed_dc, &src, 0, &blend, ULW_ALPHA);
                    if (!feed_shown) {
                        ShowWindow(feed_window, SW_SHOWNOACTIVATE);
                        SetWindowPos(feed_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                        feed_shown = 1;
                        if (!feed_logged++) plog("passenger lines shown in the own window (the HUD layer is off)");
                    }
                }
            }
        }

        if (!on || !front) {
            if (shown) ShowWindow(own_window, SW_HIDE), shown = 0, alpha = 0;
            set_clickable(0);
            InterlockedExchange(&visible, 0);
            continue;
        }
        SYSTEMTIME st;
        GetLocalTime(&st);
        char clock[8];
        snprintf(clock, sizeof(clock), "%02u:%02u", (unsigned)st.wHour % 100u, (unsigned)st.wMinute % 100u);

        /* the mouse: where the pointer is (drawn by the phone when Windows hides it), a click */
        int cur_x = -1, cur_y = -1;
        if (mouse_on && shown) {
            POINT at = {origin.x + (int)(10 * scale), origin.y + (int)(78 * scale)};
            poll_mouse(at, bw, bh, &cur_x, &cur_y);
        } else {
            was_down = 1; /* a press that started elsewhere doesn't count once the phone shows */
        }
        int redraw = !drawn_valid || theme != drawn_theme || scale != drawn_scale || strcmp(clock, drawn_clock) ||
                     memcmp(&h, &drawn, sizeof(h)) != 0 || cur_x != drawn_cur_x || cur_y != drawn_cur_y;
        if (redraw) {
            int w, hh;
            ts_phone_size(scale, &w, &hh);
            if (w != bw || hh != bh || !bmp) {
                if (bmp) SelectObject(dc, old_bmp), DeleteObject(bmp);
                BITMAPINFO bi;
                memset(&bi, 0, sizeof(bi));
                bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
                bi.bmiHeader.biWidth = w;
                bi.bmiHeader.biHeight = -hh; /* top row first */
                bi.bmiHeader.biPlanes = 1;
                bi.bmiHeader.biBitCount = 32;
                bi.bmiHeader.biCompression = BI_RGB;
                void *p = NULL;
                bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &p, NULL, 0);
                if (!bmp) {
                    plog("phone window: no picture (error %lu)", GetLastError());
                    InterlockedExchange(&ok, 0);
                    ShowWindow(own_window, SW_HIDE);
                    return 0;
                }
                old_bmp = SelectObject(dc, bmp);
                bits = (uint32_t *)p, bw = w, bh = hh;
            }
            GdiFlush();
            memset(bits, 0, (size_t)bw * bh * 4);
            SetBkMode(dc, TRANSPARENT);
            Gdi g = {dc};
            TsPhoneCanvas cv = {bits, bw, bh, gdi_text, gdi_measure, &g, hits, 0};
            ts_phone_draw(&h, theme, scale, clock, &cv);
            hit_count = cv.hit_count;
            GdiFlush();
            ts_phone_finish(theme, scale, &cv);
            if (cur_x >= 0) draw_pointer(bits, bw, bh, cur_x, cur_y, scale);
            drawn_cur_x = cur_x, drawn_cur_y = cur_y;
            drawn = h, drawn_theme = theme, drawn_scale = scale, drawn_valid = 1;
            snprintf(drawn_clock, sizeof(drawn_clock), "%s", clock);
        }

        /* on the left, under the chips at the top */
        POINT pos = {origin.x + (int)(10 * scale), origin.y + (int)(78 * scale)};
        int fading = alpha < 255;
        if (fading) alpha = alpha + 64 > 255 ? 255 : alpha + 64;
        if (redraw || fading || pos.x != drawn_pos.x || pos.y != drawn_pos.y || !shown) {
            SIZE size = {bw, bh};
            POINT src = {0, 0};
            BLENDFUNCTION blend = {AC_SRC_OVER, 0, (BYTE)alpha, AC_SRC_ALPHA};
            if (!UpdateLayeredWindow(own_window, NULL, &pos, &size, dc, &src, 0, &blend, ULW_ALPHA)) {
                if (!logged_first) plog("phone window: UpdateLayeredWindow failed (error %lu)", GetLastError());
                logged_first = 1;
            }
            drawn_pos = pos;
        }
        if (!shown) {
            ShowWindow(own_window, SW_SHOWNOACTIVATE);
            SetWindowPos(own_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            shown = 1;
            if (!logged_first) plog("phone window shown (%dx%d, scale %.2f)", bw, bh, (double)scale);
            logged_first = 1;
        }
        InterlockedExchange(&visible, 1);
        set_clickable(mouse_on != 0);
    }
}

void phone_window_start(PhoneLogFn log) {
    plog = log;
    HANDLE t = CreateThread(NULL, 0, window_thread, NULL, 0, NULL);
    if (t) CloseHandle(t);
    else log("phone window thread NOT started");
}
