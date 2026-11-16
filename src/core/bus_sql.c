/* The bus in the Autoshow by rewriting the game's queries, see bus_sql.h. */
#include "bus_sql.h"
#include <ctype.h>
#include <string.h>

#define DRIVABLE "(SELECT * FROM Data_Car WHERE Id NOT IN (SELECT CarId FROM TrafficCars WHERE CarId<>414))"
#define BUCKETS                                                                                                        \
    "(SELECT CarId, CarBucket, BucketHero FROM Data_Car_Buckets UNION ALL SELECT 414, 21, 0 WHERE NOT EXISTS "        \
    "(SELECT 1 FROM Data_Car_Buckets WHERE CarId=414))"
#define IS_TRAFFIC "FROM TrafficCars WHERE CarId="
#define NOT_BUS_TRAFFIC "FROM TrafficCars WHERE CarId<>414 AND CarId="

typedef struct {
    char *out;
    size_t cap, n;
    int full;
} Out;

static void put(Out *o, const char *s, size_t len) {
    if (o->n + len + 1 > o->cap) {
        o->full = 1;
        return;
    }
    memcpy(o->out + o->n, s, len);
    o->n += len;
}

static int ncase_eq(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return 0;
    return 1;
}
#define strncasecmp(a, b, n) (!ncase_eq(a, b, n))

static int word_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* the word at p equals w (case doesn't matter), as a whole word */
static int word_is(const char *p, const char *w) {
    size_t n = strlen(w);
    return strncasecmp(p, w, n) == 0 && !word_char(p[n]);
}

/* after a table name: does an alias follow ("AS x" or a plain name that isn't a keyword)? */
static int has_alias(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (!word_char(*p)) return 0; /* ) , ; end */
    static const char *const keywords[] = {"WHERE", "JOIN", "LEFT", "RIGHT", "INNER", "OUTER", "CROSS", "NATURAL", "ON",
                                           "USING", "ORDER", "GROUP", "HAVING", "LIMIT", "UNION", "EXCEPT", "INTERSECT"};
    for (unsigned i = 0; i < sizeof(keywords) / sizeof(keywords[0]); ++i)
        if (word_is(p, keywords[i])) return 0;
    return 1; /* AS, or a name */
}

int ts_bus_sql(const char *sql, char *out, size_t cap) {
    if (!sql || !out || !cap) return 0;
    if (!strstr(sql, "Drivable_Data_Car") && !strstr(sql, "Data_Car_Buckets") && !strstr(sql, IS_TRAFFIC)) return 0;
    const char *p = sql;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    if (strncasecmp(p, "CREATE", 6) == 0 || strncasecmp(p, "INSERT", 6) == 0 || strncasecmp(p, "UPDATE", 6) == 0 ||
        strncasecmp(p, "DELETE", 6) == 0 || strncasecmp(p, "DROP", 4) == 0)
        return 0; /* only questions are changed, never the database */
    Out o = {out, cap, 0, 0};
    int changed = 0, after_from = 0; /* the previous word was FROM or JOIN */
    char quote = 0;
    for (p = sql; *p;) {
        if (quote) { /* inside '...' or "..." or [...]: copied as it is */
            put(&o, p, 1);
            if (*p == quote) quote = 0;
            ++p;
            continue;
        }
        if (*p == '\'' || *p == '"' || *p == '[') {
            quote = *p == '[' ? ']' : *p;
            put(&o, p, 1);
            ++p;
            after_from = 0;
            continue;
        }
        if (word_char(*p) && (p == sql || !word_char(p[-1]))) {
            const char *w = p;
            while (word_char(*p)) ++p;
            size_t len = (size_t)(p - w);
            if (after_from && word_is(w, "Drivable_Data_Car") && w[len] != '.') {
                put(&o, DRIVABLE, strlen(DRIVABLE));
                if (!has_alias(p)) put(&o, " AS Drivable_Data_Car", 21);
                changed = 1;
            } else if (after_from && word_is(w, "Data_Car_Buckets") && w[len] != '.') {
                put(&o, BUCKETS, strlen(BUCKETS));
                if (!has_alias(p)) put(&o, " AS Data_Car_Buckets", 20);
                changed = 1;
            } else if (word_is(w, "FROM") && strncmp(w, IS_TRAFFIC, strlen(IS_TRAFFIC)) == 0) {
                put(&o, NOT_BUS_TRAFFIC, strlen(NOT_BUS_TRAFFIC));
                p = w + strlen(IS_TRAFFIC);
                changed = 1;
                after_from = 0;
                continue;
            } else {
                put(&o, w, len);
            }
            after_from = word_is(w, "FROM") || word_is(w, "JOIN");
            continue;
        }
        if (!(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) after_from = 0;
        put(&o, p, 1);
        ++p;
    }
    if (o.full) return -1;
    out[o.n] = 0;
    return changed;
}
