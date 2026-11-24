/* 0.10: the phone app's web server (companion_net.h). Own code, no libraries: Winsock in the game, POSIX sockets
   for the tests on Linux. Small on purpose: one request per connection, GET only, answers from memory. */
#include "companion_net.h"
#include "core/companion.h"
#include "core/map.h"
#include "app_html.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET Sock;
#define BAD_SOCK INVALID_SOCKET
#define close_sock closesocket
typedef CRITICAL_SECTION Mutex;
static void mx_init(Mutex *m) { InitializeCriticalSection(m); }
static void mx_lock(Mutex *m) { EnterCriticalSection(m); }
static void mx_unlock(Mutex *m) { LeaveCriticalSection(m); }
typedef DWORD(WINAPI *ThreadFn)(void *);
#define THREAD_RET DWORD WINAPI
#define THREAD_END return 0
static int spawn(ThreadFn fn, void *arg) {
    HANDLE t = CreateThread(NULL, 0, fn, arg, 0, NULL);
    if (!t) return 0;
    CloseHandle(t);
    return 1;
}
static double now_s(void) { return (double)GetTickCount64() / 1000.0; }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
typedef int Sock;
#define BAD_SOCK (-1)
#define close_sock close
typedef pthread_mutex_t Mutex;
static void mx_init(Mutex *m) { pthread_mutex_init(m, NULL); }
static void mx_lock(Mutex *m) { pthread_mutex_lock(m); }
static void mx_unlock(Mutex *m) { pthread_mutex_unlock(m); }
typedef void *(*ThreadFn)(void *);
#define THREAD_RET void *
#define THREAD_END return NULL
static int spawn(ThreadFn fn, void *arg) {
    pthread_t t;
    if (pthread_create(&t, NULL, fn, arg)) return 0;
    pthread_detach(t);
    return 1;
}
static double now_s(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}
#endif

static Mutex mx;
static int mx_ready;
static volatile int running;
static Sock listener = BAD_SOCK, finder = BAD_SOCK;
static int port_now;
static char code_now[16], version_now[16], name_now[64] = "PC";
static AppNetLog logf_;
static char *state;            /* the newest state, 0-terminated */
static size_t state_n;
static char *lines;
static size_t lines_n;
static char *history; /* 0.13: the ride book */
static size_t history_n;
static int german_now;
static double seen_at = -1.0;
static volatile int open_conns;

#define QUEUE 16
static int q_action[QUEUE], q_arg[QUEUE], q_head, q_tail;

static void say(const char *f, const char *a) {
    if (logf_) logf_(f, a);
}

/* ------------------------------------------------------------------ the glue's side */

void app_net_publish(const char *json, size_t n, int german) {
    if (!mx_ready) return;
    mx_lock(&mx);
    char *p = (char *)realloc(state, n + 1);
    if (p) {
        memcpy(p, json, n);
        p[n] = 0;
        state = p, state_n = n;
    }
    german_now = german;
    mx_unlock(&mx);
}

void app_net_lines(const char *json, size_t n) {
    if (!mx_ready) return;
    mx_lock(&mx);
    char *p = (char *)realloc(lines, n + 1);
    if (p) {
        memcpy(p, json, n);
        p[n] = 0;
        lines = p, lines_n = n;
    }
    mx_unlock(&mx);
}

void app_net_history(const char *json, size_t n) {
    if (!mx_ready) return;
    mx_lock(&mx);
    char *p = (char *)realloc(history, n + 1);
    if (p) {
        memcpy(p, json, n);
        p[n] = 0;
        history = p, history_n = n;
    }
    mx_unlock(&mx);
}

int app_net_take(int *action, int *arg) {
    if (!mx_ready) return 0;
    int got = 0;
    mx_lock(&mx);
    if (q_head != q_tail) {
        *action = q_action[q_head], *arg = q_arg[q_head];
        q_head = (q_head + 1) % QUEUE;
        got = 1;
    }
    mx_unlock(&mx);
    return got;
}

double app_net_last_seen(void) {
    if (!mx_ready) return -1.0;
    mx_lock(&mx);
    double t = seen_at < 0.0 ? -1.0 : now_s() - seen_at;
    mx_unlock(&mx);
    return t;
}

void app_net_set_name(const char *name) {
    if (name && *name) snprintf(name_now, sizeof(name_now), "%s", name);
}

int app_net_running(void) { return running; }
int app_net_port(void) { return running ? port_now : 0; }

/* the address the PC uses into the network: a UDP socket "connected" outwards (nothing is sent) */
void app_net_address(char *out, size_t cap) {
    if (cap) out[0] = 0;
    Sock s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s == BAD_SOCK) return;
    struct sockaddr_in to;
    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(53);
    to.sin_addr.s_addr = htonl(0xC0A80001u); /* 192.168.0.1: only for picking the interface, nothing goes out */
    if (connect(s, (struct sockaddr *)&to, sizeof(to)) == 0) {
        struct sockaddr_in me;
        socklen_t len = sizeof(me);
        if (getsockname(s, (struct sockaddr *)&me, &len) == 0) {
            uint32_t a = ntohl(me.sin_addr.s_addr);
            if (a) snprintf(out, cap, "%u.%u.%u.%u", a >> 24, a >> 16 & 255, a >> 8 & 255, a & 255);
        }
    }
    close_sock(s);
}

/* ------------------------------------------------------------------ answers */

static void send_all(Sock s, const char *p, size_t n) {
    while (n) {
        int k = send(s, p, n > 65536 ? 65536 : (int)n, 0);
        if (k <= 0) return;
        p += k, n -= (size_t)k;
    }
}

static void answer(Sock s, int status, const char *type, const char *extra, const void *body, size_t n) {
    char head[512];
    const char *why = status == 200 ? "OK" : status == 204 ? "No Content" : status == 403 ? "Forbidden" : status == 404 ? "Not Found"
                    : status == 503 ? "Service Unavailable" : "Bad Request";
    int k = snprintf(head, sizeof(head),
                     "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %u\r\nAccess-Control-Allow-Origin: *\r\n"
                     "Access-Control-Allow-Headers: *\r\nConnection: close\r\n%s\r\n",
                     status, why, type, (unsigned)n, extra ? extra : "");
    send_all(s, head, (size_t)k);
    if (n) send_all(s, (const char *)body, n);
}

static void answer_text(Sock s, int status, const char *json) {
    answer(s, status, "application/json; charset=utf-8", "Cache-Control: no-store\r\n", json, strlen(json));
}

/* ?a=1&b=2: the value of name, "" if missing */
static void param(const char *query, const char *name, char *out, size_t cap) {
    out[0] = 0;
    size_t nl = strlen(name);
    for (const char *p = query; p && *p;) {
        if (!strncmp(p, name, nl) && p[nl] == '=') {
            p += nl + 1;
            size_t i = 0;
            while (*p && *p != '&' && i + 1 < cap) out[i++] = *p++;
            out[i] = 0;
            return;
        }
        p = strchr(p, '&');
        if (p) ++p;
    }
}

static void hello(char *out, size_t cap) {
    mx_lock(&mx);
    int de = german_now;
    mx_unlock(&mx);
    snprintf(out, cap, "{\"app\":\"HorizonTransit\",\"version\":\"%s\",\"name\":\"%s\",\"port\":%d,\"de\":%d,\"api\":1}", version_now,
             name_now, port_now, de);
}

/* a piece of the map: 256 x 256, as a BMP. z 0: one piece shows 24576 m; the pieces start at x -8000, z +2000. */
#define TILE 256
#define WORLD_X0 (-8000.0f)
#define WORLD_Z0 (2000.0f)
#define WORLD_SIZE 24576.0f
static void tile(Sock s, int z, int tx, int ty) {
    if (z < 0 || z > 8 || tx < 0 || ty < 0 || tx >= 1 << z || ty >= 1 << z) {
        answer_text(s, 404, "{\"error\":\"tile\"}");
        return;
    }
    if (!ts_map_ok()) {
        answer_text(s, 503, "{\"error\":\"map not ready\"}");
        return;
    }
    size_t row = TILE * 3, n = 54 + row * TILE;
    unsigned char *b = (unsigned char *)malloc(n);
    if (!b) {
        answer_text(s, 503, "{\"error\":\"memory\"}");
        return;
    }
    memset(b, 0, 54);
    b[0] = 'B', b[1] = 'M';
    uint32_t v[] = {(uint32_t)n, 0, 54, 40, TILE, TILE};
    for (int i = 0; i < 6; ++i) memcpy(b + 2 + (i ? 4 + (i - 1) * 4 : 0), &v[i], 4);
    b[26] = 1, b[28] = 24;
    float size = WORLD_SIZE / (float)(1 << z), mpp = size / TILE;
    float x0 = WORLD_X0 + tx * size, z0 = WORLD_Z0 - ty * size;
    for (int py = 0; py < TILE; ++py) {
        unsigned char *d = b + 54 + (size_t)(TILE - 1 - py) * row; /* bottom row first */
        float wz = z0 - (py + 0.5f) * mpp;
        for (int px = 0; px < TILE; ++px) {
            uint32_t c = ts_map_color(x0 + (px + 0.5f) * mpp, wz, mpp);
            d[3 * px] = (unsigned char)c, d[3 * px + 1] = (unsigned char)(c >> 8), d[3 * px + 2] = (unsigned char)(c >> 16);
        }
    }
    answer(s, 200, "image/bmp", "Cache-Control: max-age=86400\r\n", b, n);
    free(b);
}

static void handle(Sock s) {
    char req[4096];
    int got = 0;
    while (got < (int)sizeof(req) - 1) {
        int k = recv(s, req + got, (int)sizeof(req) - 1 - got, 0);
        if (k <= 0) break;
        got += k;
        req[got] = 0;
        if (strstr(req, "\r\n\r\n")) break;
    }
    if (got <= 0) return;
    req[got] = 0;
    char method[8] = "", target[1024] = "";
    if (sscanf(req, "%7s %1023s", method, target) != 2) {
        answer_text(s, 400, "{\"error\":\"request\"}");
        return;
    }
    if (!strcmp(method, "OPTIONS")) {
        answer(s, 204, "text/plain", NULL, NULL, 0);
        return;
    }
    char *query = strchr(target, '?');
    if (query) *query++ = 0;
    if (!strcmp(target, "/") || !strcmp(target, "/index.html")) {
        answer(s, 200, "text/html; charset=utf-8", "Cache-Control: no-cache\r\n", ts_app_html, sizeof(ts_app_html) - 1);
        return;
    }
    if (!strcmp(target, "/api/hello")) {
        char h[256];
        hello(h, sizeof(h));
        answer_text(s, 200, h);
        return;
    }
    if (strncmp(target, "/api/", 5)) {
        answer_text(s, 404, "{\"error\":\"not found\"}");
        return;
    }
    char k[16];
    param(query, "k", k, sizeof(k));
    if (!code_now[0] || strcmp(k, code_now)) {
        answer_text(s, 403, "{\"error\":\"code\"}");
        return;
    }
    if (!strcmp(target, "/api/state")) {
        mx_lock(&mx);
        seen_at = now_s();
        char *copy = state ? (char *)malloc(state_n + 1) : NULL;
        size_t n = state_n;
        if (copy) memcpy(copy, state, n + 1);
        mx_unlock(&mx);
        if (copy) answer(s, 200, "application/json; charset=utf-8", "Cache-Control: no-store\r\n", copy, n);
        else answer_text(s, 503, "{\"error\":\"no game yet\"}");
        free(copy);
    } else if (!strcmp(target, "/api/lines")) {
        mx_lock(&mx);
        char *copy = lines ? (char *)malloc(lines_n + 1) : NULL;
        size_t n = lines_n;
        if (copy) memcpy(copy, lines, n + 1);
        mx_unlock(&mx);
        if (copy) answer(s, 200, "application/json; charset=utf-8", "Cache-Control: no-store\r\n", copy, n);
        else answer_text(s, 503, "{\"error\":\"no game yet\"}");
        free(copy);
    } else if (!strcmp(target, "/api/history")) { /* 0.13 */
        mx_lock(&mx);
        char *copy = history ? (char *)malloc(history_n + 1) : NULL;
        size_t n = history_n;
        if (copy) memcpy(copy, history, n + 1);
        mx_unlock(&mx);
        if (copy) answer(s, 200, "application/json; charset=utf-8", "Cache-Control: no-store\r\n", copy, n);
        else answer_text(s, 503, "{\"error\":\"no game yet\"}");
        free(copy);
    } else if (!strcmp(target, "/api/do")) {
        char a[16], n[16];
        param(query, "a", a, sizeof(a));
        param(query, "n", n, sizeof(n));
        int act = ts_app_action(a);
        if (!act) {
            answer_text(s, 400, "{\"error\":\"action\"}");
            return;
        }
        mx_lock(&mx);
        int next = (q_tail + 1) % QUEUE, ok = next != q_head;
        if (ok) q_action[q_tail] = act, q_arg[q_tail] = atoi(n), q_tail = next;
        mx_unlock(&mx);
        answer_text(s, ok ? 200 : 503, ok ? "{\"ok\":true}" : "{\"error\":\"busy\"}");
    } else if (!strcmp(target, "/api/tile")) {
        char z[8], x[8], y[8];
        param(query, "z", z, sizeof(z)), param(query, "x", x, sizeof(x)), param(query, "y", y, sizeof(y));
        tile(s, atoi(z), atoi(x), atoi(y));
    } else answer_text(s, 404, "{\"error\":\"not found\"}");
}

static THREAD_RET conn_thread(void *arg) {
    Sock s = (Sock)(intptr_t)arg;
    handle(s);
    close_sock(s);
    __atomic_sub_fetch(&open_conns, 1, __ATOMIC_RELAXED);
    THREAD_END;
}

static THREAD_RET accept_thread(void *unused) {
    (void)unused;
    while (running) {
        Sock c = accept(listener, NULL, NULL);
        if (c == BAD_SOCK) {
            if (!running) break;
            continue;
        }
#ifdef _WIN32
        DWORD ms = 5000;
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (const char *)&ms, sizeof(ms));
#else
        struct timeval tv = {5, 0};
        setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
        if (__atomic_add_fetch(&open_conns, 1, __ATOMIC_RELAXED) > 24 || !spawn(conn_thread, (void *)(intptr_t)c)) {
            handle(c); /* too many at once: this one right here */
            close_sock(c);
            __atomic_sub_fetch(&open_conns, 1, __ATOMIC_RELAXED);
        }
    }
    THREAD_END;
}

/* "HORIZON-TRANSIT?" from the app: the hello back to it */
static THREAD_RET find_thread(void *unused) {
    (void)unused;
    while (running) {
        char buf[64];
        struct sockaddr_in from;
        socklen_t len = sizeof(from);
        int k = recvfrom(finder, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &len);
        if (k <= 0) {
            if (!running) break;
            continue;
        }
        buf[k] = 0;
        if (strncmp(buf, "HORIZON-TRANSIT?", 16)) continue;
        char h[256];
        hello(h, sizeof(h));
        sendto(finder, h, (int)strlen(h), 0, (struct sockaddr *)&from, len);
    }
    THREAD_END;
}

int app_net_start(int port, const char *code, const char *version, AppNetLog log) {
    if (running) return 1;
    logf_ = log;
    if (!mx_ready) mx_init(&mx), mx_ready = 1;
    snprintf(code_now, sizeof(code_now), "%s", code ? code : "");
    snprintf(version_now, sizeof(version_now), "%s", version ? version : "");
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
        say("phone app: no network (%s)", "WSAStartup");
        return 0;
    }
#endif
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == BAD_SOCK) return 0;
#ifndef _WIN32
    { /* 0.10.4: a restart may take the same port again at once (Windows allows that anyway) */
        int one = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    }
#endif
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    int bound = 0;
    for (int p = port; p < port + 10 && !bound; ++p) {
        a.sin_port = htons((unsigned short)p);
        if (bind(listener, (struct sockaddr *)&a, sizeof(a)) == 0) bound = 1, port_now = p;
    }
    if (!bound || listen(listener, 16)) {
        close_sock(listener);
        listener = BAD_SOCK;
        say("phone app: %s", "no free port");
        return 0;
    }
    running = 1;
    spawn(accept_thread, NULL);
    finder = socket(AF_INET, SOCK_DGRAM, 0);
    if (finder != BAD_SOCK) {
        a.sin_port = htons(APP_NET_FIND_PORT);
        if (bind(finder, (struct sockaddr *)&a, sizeof(a)) == 0) spawn(find_thread, NULL);
        else {
            close_sock(finder);
            finder = BAD_SOCK;
            say("phone app: %s", "the finding port 8711 is taken (type the address in the app)");
        }
    }
    return 1;
}

void app_net_stop(void) {
    if (!running) return;
    running = 0;
#ifndef _WIN32
    if (listener != BAD_SOCK) shutdown(listener, SHUT_RDWR); /* on Linux closing alone doesn't wake accept */
    if (finder != BAD_SOCK) shutdown(finder, SHUT_RDWR);
#endif
    if (listener != BAD_SOCK) close_sock(listener);
    if (finder != BAD_SOCK) close_sock(finder);
    listener = finder = BAD_SOCK;
}
