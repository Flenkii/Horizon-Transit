#ifndef TS_PEOPLE_H
#define TS_PEOPLE_H
/* The passengers (people.c): each one a person with a job and things to tell. */
#include "lines.h"

#define TS_PERSON_LINES 6
#define TS_PEOPLE_MAX 64 /* room in TsCore.met */

typedef struct {
    const char *name;
    int age;
    const char *job_de, *job_en;
    TsLineText story[TS_PERSON_LINES]; /* in the order they tell it */
} TsPerson;

extern const TsPerson ts_people[];
extern const unsigned ts_people_count;
/* [person][0] = the TS_GROUP_ they like, [1] = the one they don't */
extern const signed char ts_people_taste[][2];
/* [person]: 1 = a female voice (the "Voices" switch) */
extern const unsigned char ts_people_female[];
extern const unsigned ts_people_female_count;

#endif
