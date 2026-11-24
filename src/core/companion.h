#ifndef TS_COMPANION_H
#define TS_COMPANION_H
/* 0.10: the phone app (Android app or a browser in the home network). The mod's own little web server
   (src/companion_net.c) answers it; this part says what it gets, as JSON. Plain C, no Windows, no sockets: tested
   in tests/test_core.c. Everything here runs under the glue's lock (the same one as the TsCore). */
#include <stddef.h>
#include "jobs.h"

#define TS_APP_FEED 24     /* the passengers' lines the app gets (the newest) */
#define TS_APP_ROUTE 400   /* points of the way to the next target */

/* a passenger's line (TS_EV_FEED) for the app's chat */
void ts_app_feed(const char *name, const char *text, int style);
/* forget the chat (a new game) */
void ts_app_feed_clear(void);

/* the state: the car, the money, the job, the offers, the chat, the way. Returns the length written (out is always
   0-terminated), or 0 if cap was too small. */
size_t ts_app_state_json(const TsCore *c, const TsHud *h, char *out, size_t cap);
/* the bus lines and their stops (doesn't change while playing) */
size_t ts_app_lines_json(const TsCore *c, char *out, size_t cap);

/* 0.13: the ride book (earnings and history; the app keeps it for without the game) */
size_t ts_app_history_json(const TsCore *c, char *out, size_t cap);

/* what the app asks for (src/companion_net.c parses it, the glue does it under the lock) */
/* 0.10.1: SETTING presses a row of the phone's Options (the glue changes it), BUY buys from the shop */
enum { TS_APP_NONE = 0, TS_APP_ACCEPT, TS_APP_LINE, TS_APP_CANCEL, TS_APP_REPLY, TS_APP_REFRESH, TS_APP_SHIFT, TS_APP_SETTING, TS_APP_BUY, TS_APP_SET,
       TS_APP_TARGET };
/* 0.13: TARGET: a place from the app's map for the GPS: arg = x2 * 32768 + z2 with x2 = (x + 30000) / 2, z2 the same; -1 = none */
/* 0.10.4: SET sets a row of the Options to a value: arg = row * 16 + value (what the app changed while offline) */
/* does it: 1 done, 0 not possible now */
int ts_app_do(TsCore *c, int action, int arg);
/* the action's name in a request ("accept") -> TS_APP_, 0 = unknown */
int ts_app_action(const char *name);

/* the answer the reply button gives now, NULL = no question waiting (talk.c) */
const char *ts_talk_answer(const TsCore *c);
const char *ts_talk_question(const TsCore *c);

#endif
