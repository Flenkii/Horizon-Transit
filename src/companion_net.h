#ifndef TS_COMPANION_NET_H
#define TS_COMPANION_NET_H
/* 0.10: the mod's little web server for the phone app, in the home network only.
   TCP (port 8710, or the next free one): the app's page (GET /), and with the pairing code (?k=1234):
     /api/state  what core/companion.c says (the glue publishes it every 100 ms)
     /api/lines  the bus lines
     /api/history the ride book (0.13)
     /api/do?a=accept&n=0   what the app asks for (the glue takes it with app_net_take)
     /api/tile?z=&x=&y=     a 256 px piece of the map (BMP), drawn from the map the mod built from the game's files
   /api/hello answers without the code (the app shows which PC it found).
   UDP 8711: the app calls "HORIZON-TRANSIT?" into the network, the mod answers with /api/hello's JSON, so the app
   finds the PC by itself.
   Windows (Winsock) in the game; POSIX for the tests on Linux. */
#include <stddef.h>

typedef void (*AppNetLog)(const char *format, ...);

#define APP_NET_PORT 8710
#define APP_NET_FIND_PORT 8711

/* starts the server's threads; 1 = it listens (app_net_port says where) */
int app_net_start(int port, const char *code, const char *version, AppNetLog log);
void app_net_stop(void);
int app_net_running(void);
int app_net_port(void);
/* the PC's address in the home network ("192.168.0.12"), "" if none */
void app_net_address(char *out, size_t cap);
/* the newest state (JSON) and the game's language; any thread */
void app_net_publish(const char *json, size_t n, int german);
void app_net_lines(const char *json, size_t n);
void app_net_history(const char *json, size_t n); /* 0.13: the ride book */
/* the next thing the app asked for: 1 = one (TS_APP_ and its number) */
int app_net_take(int *action, int *arg);
/* seconds since the app last asked for the state, -1 = never */
double app_net_last_seen(void);
/* the PC's name (shown in the app) */
void app_net_set_name(const char *name);

#endif
