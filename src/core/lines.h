#ifndef TS_LINES_H
#define TS_LINES_H
/* What passengers say, in German and English. {car} is the driven car ("Lamborghini Miura P400"),
   {max} the passenger's speed limit in km/h, {n} the ride's number with this passenger, {lastcar} the car they
   rode in last time, {group} the kind of car ("Sportwagen"), {limit} a wished speed, {place} a wished stop. */

typedef struct {
    const char *de, *en;
} TsLineText;

enum {
    L_GREET_NORMAL = 0, L_GREET_RELAXED, L_GREET_HURRY, L_GREET_FAN, L_CAR_COMPLIMENT, L_CAR_VAN,
    L_FAN_FASTER, L_FAN_HAPPY, L_HURRY_NAG, L_RELAXED_TALK, L_SMALL_TALK,
    L_BRAKE, L_CORNER, L_ACCEL, L_SPEEDING, L_CRASH, L_SMOOTH,
    L_WAITING, L_NOT_IN_YET, L_WANT_OUT, L_LATE,
    L_BYE_GOOD, L_BYE_OK, L_BYE_BAD,
    L_BUS_CRASH, L_BUS_BRAKE, L_BUS_CORNER, L_BUS_SPEEDING, L_BUS_LATE, L_BUS_NOT_DONE, L_BUS_SMOOTH, L_BUS_TALK,
    L_GREET_REGULAR, /* a passenger who rode with you before; {n} = this ride's number with them */
    /* 0.4: memory ({lastcar}), car likes ({group}), special passengers, wishes ({limit}, {place}), the timetable */
    L_REG_SAME_CAR, L_REG_OTHER_CAR, L_REG_LAST_GOOD, L_REG_LAST_BAD, L_LIKES_CAR, L_DISLIKES_CAR, L_OPEN_TOP, L_GREET_VIP, L_VIP_TALK, L_GREET_BUSINESS, L_BUSINESS_TALK, L_GREET_WEDDING, L_WEDDING_TALK, L_GREET_TOURIST, L_TOURIST_TALK, L_WISH_SLOWER, L_WISH_SMOOTH, L_WISH_FASTER, L_WISH_STOP, L_WISH_THANKS, L_WISH_FAILED, L_STOP_DONE, L_BUS_ON_TIME,
    /* 0.4.1: talk by passenger kind and by where you are */
    L_HURRY_TALK, L_FAN_TALK, L_TOWN_TALK, L_FESTIVAL_TALK, L_COUNTRY_TALK,
    L_THOUGHT, /* 0.4.2: a quiet passenger's thought about the driving (shown as "Name (thinks)", not spoken) */
    L_KINDS
};

typedef struct {
    const TsLineText *lines;
    unsigned count;
} TsLineSet;

extern const TsLineSet ts_line_sets[L_KINDS];

#endif
