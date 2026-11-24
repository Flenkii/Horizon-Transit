/* Horizon Transit 0.5.1: talking back. Now and then a passenger asks you something; for a few seconds the ride
   panel shows "[B] Reply". Press it (B on the keyboard, 0.8 also LB+RB+down on the controller) and your driver answers, in the feed and,
   with "Voices" on, out loud in the driver's own voice; the passenger is pleased (+3 % satisfaction). Don't press
   it and nothing happens: staying quiet is fine too. At most two questions a ride; never from a quiet passenger. */
#include "jobs_internal.h"
#include "companion.h"
#include <stdio.h>

#define ASK_SECONDS 12.0 /* 0.8: was 9, a little more time for the controller chord */
#define ASK_REWARD 3.0f
#define ASKS_PER_RIDE 2

typedef struct {
    const char *q_de, *q_en; /* the passenger's question */
    const char *a_de, *a_en; /* your answer */
    const char *r_de, *r_en; /* their reaction */
} TsAsk;

static const TsAsk asks[] = {
    {"Machst du den Job schon lange?", "Have you been doing this long?",
     "Eine Weile. Ich fahr einfach gern.", "A while. I just love driving.",
     "Merkt man. Du fährst echt entspannt.", "It shows. You drive really relaxed."},
    {"Ist das eigentlich dein eigenes Auto?", "Is this actually your own car?",
     "Ja, mein ganzer Stolz.", "Yep, my pride and joy.",
     "Zu Recht. Schönes Teil!", "Rightly so. Lovely thing!"},
    {"Kennst du hier einen guten Ort zum Essen?", "Know a good place to eat around here?",
     "Unten am Hafen, die kleine Trattoria. Unbedingt probieren!", "Down by the harbour, the little trattoria. You have to try it!",
     "Super, danke für den Tipp!", "Great, thanks for the tip!"},
    {"Wie lange brauchen wir ungefähr noch?", "How much longer, roughly?",
     "Nicht mehr lange. Ich bring dich hin.", "Not long now. I'll get you there.",
     "Gut, dann lehn ich mich zurück.", "Good, then I'll sit back."},
    {"Warst du schon mal auf dem Festival?", "Have you been to the festival yet?",
     "Klar, jeden Abend. Die Stimmung ist irre.", "Sure, every night. The vibe is crazy.",
     "Dann muss ich da heute Abend hin!", "Then I have to go tonight!"},
    {"Darf ich das Fenster ein bisschen aufmachen?", "Mind if I open the window a bit?",
     "Klar, mach ruhig.", "Sure, go ahead.",
     "Ah, die Luft hier ist herrlich.", "Ah, the air here is wonderful."},
    {"Wie ist das Wetter hier sonst so?", "What's the weather usually like here?",
     "Meistens Sonne. Und wenn's regnet, glänzen die Straßen.", "Mostly sunny. And when it rains, the roads shine.",
     "Das klingt nach Urlaub.", "Sounds like a holiday."},
    {"Hörst du beim Fahren eigentlich Musik?", "Do you listen to music while driving?",
     "Immer. Ohne Radio fahr ich nicht los.", "Always. I never go without the radio.",
     "Ein Mann mit Geschmack.", "A man of taste."},
    {"Was war dein verrücktester Fahrgast?", "Who was your craziest passenger?",
     "Ein Brautpaar, das unterwegs noch umziehen wollte. Lange Geschichte.", "A wedding couple who wanted to move house on the way. Long story.",
     "Haha, das will ich irgendwann hören!", "Haha, I want to hear that one day!"},
    {"Fährst du auch nachts?", "Do you drive at night too?",
     "Klar. Nachts sind die Straßen am schönsten.", "Sure. The roads are at their best at night.",
     "Stimmt, die Lichter hier sind schon was.", "True, the lights here are something."},
    {"Kommst du eigentlich von hier?", "Are you from around here?",
     "Nicht ganz, aber ich kenne jede Kurve.", "Not quite, but I know every bend.",
     "Das beruhigt mich.", "That's reassuring."},
    {"Ist das hier eine gute Gegend zum Wohnen?", "Is this a nice area to live?",
     "Ruhig, schöne Aussicht, nette Leute. Ja, eigentlich schon.", "Quiet, great views, nice people. Yes, pretty much.",
     "Vielleicht zieh ich ja her.", "Maybe I'll move here."},
    {"Wie viele PS hat der eigentlich?", "How much power does this thing have?",
     "Genug, um dich pünktlich hinzubringen.", "Enough to get you there on time.",
     "Haha, gute Antwort.", "Haha, good answer."},
    {"Hast du heute schon viele Fahrten gehabt?", "Had many rides today?",
     "Ein paar. Mit dir ist es bisher die netteste Fahrt.", "A few. You're the nicest so far.",
     "Ach, du Schmeichler!", "Oh, you charmer!"},
};
#define ASK_COUNT (int)(sizeof(asks) / sizeof(asks[0]))

static const char *pick(const TsCore *c, const char *de, const char *en) { return c->german ? de : en; }

/* from the ride's talk: maybe a question instead of small talk; 1 if one was asked */
int talk_maybe_ask(TsCore *c) {
    if (!c->replies_on || c->thinking || c->mode != TS_UBER || c->asks_done >= ASKS_PER_RIDE || c->ask >= 0) return 0;
    if (random_unit(c) >= 0.4f) return 0;
    int k = random_below(c, ASK_COUNT);
    for (int tries = 0; tries < ASK_COUNT && (c->asked_mask >> k & 1u); ++tries) k = (k + 1) % ASK_COUNT; /* not twice a game */
    if (c->asked_mask >> k & 1u) c->asked_mask = 0;
    c->asked_mask |= 1u << k;
    c->ask = k;
    c->ask_until = c->t + ASK_SECONDS;
    c->asks_done++;
    chat(c, 0, pick(c, asks[k].q_de, asks[k].q_en), 1);
    return 1;
}

/* the reply key: your answer, and a little later their reaction */
void ts_reply(TsCore *c) {
    if (c->ask < 0 || c->mode != TS_UBER) return;
    const TsAsk *a = &asks[c->ask];
    event(c, TS_EV_FEED, 0, 0, T(c, "Du", "You"), pick(c, a->a_de, a->a_en));
    if (c->event_count) {
        TsEvent *e = &c->events[c->event_count - 1];
        e->style = 1, e->speaker = TS_SPEAKER_DRIVER;
    }
    c->sat = clampf(c->sat + ASK_REWARD, 0.0f, 100.0f);
    c->replied = c->ask;
    c->react_at = c->t + 3.0;
    c->ask = -1;
    c->chat_cooldown = c->t + 6.0; /* nobody talks over the answer */
}

void talk_step(TsCore *c) {
    if (c->mode != TS_UBER) {
        c->ask = c->replied = -1;
        return;
    }
    if (c->ask >= 0 && c->t >= c->ask_until) c->ask = -1; /* no answer: that's fine */
    if (c->replied >= 0 && c->t >= c->react_at) {
        const TsAsk *a = &asks[c->replied];
        c->replied = -1;
        chat(c, 1, pick(c, a->r_de, a->r_en), 1);
    }
}

void talk_ride_start(TsCore *c) {
    c->ask = c->replied = -1;
    c->asks_done = 0;
}

/* "[B] Reply (7)" while a question waits */
void talk_chip(const TsCore *c, char *out, size_t size) {
    out[0] = 0;
    if (c->ask < 0) return;
    int left = (int)(c->ask_until - c->t + 0.99);
    snprintf(out, size, "[%s] %s (%d)", c->reply_key[0] ? c->reply_key : "B", T(c, "Antworten", "Reply"), left < 0 ? 0 : left);
}

/* 0.10: the phone app's reply button: the question waiting and the answer it gives (NULL = none) */
const char *ts_talk_question(const TsCore *c) {
    if (c->ask < 0 || c->ask >= ASK_COUNT || c->mode != TS_UBER) return NULL;
    return pick(c, asks[c->ask].q_de, asks[c->ask].q_en);
}

const char *ts_talk_answer(const TsCore *c) {
    if (c->ask < 0 || c->ask >= ASK_COUNT || c->mode != TS_UBER) return NULL;
    return pick(c, asks[c->ask].a_de, asks[c->ask].a_en);
}
