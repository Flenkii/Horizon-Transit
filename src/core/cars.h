#ifndef TS_CARS_H
#define TS_CARS_H
/* Every car of the game (generated from its database by tools/gen_cars.py), to tell what the player drives. */

enum { TS_CAR_UNKNOWN = 0, TS_CAR_NORMAL, TS_CAR_SPORTY, TS_CAR_DREAM, TS_CAR_VAN, TS_CAR_BUS };

/* what kind of car (the Autoshow's groups, from the game's database) */
enum {
    TS_GROUP_SMALL = 0, /* small cars and everyday hatchbacks */
    TS_GROUP_HATCH,     /* hot hatches, rally saloons */
    TS_GROUP_SALOON,    /* super saloons: BMW M5, Mercedes E63, Bentley, Tesla Model S */
    TS_GROUP_SPORTS,    /* sports cars and muscle */
    TS_GROUP_SUPER,     /* supercars, hypercars, track toys */
    TS_GROUP_CLASSIC,   /* classics and old racers */
    TS_GROUP_OFFROAD,   /* off-road, SUVs, vans */
    TS_GROUP_BUS,
    TS_GROUPS
};

typedef struct {
    const char *media; /* the car's media name, as its zip: "LAM_MiuraP400_67" */
    const char *make;  /* "Lamborghini" ("" for the game's generic traffic cars) */
    const char *model; /* "Miura P400" */
    int year;
    int cost;          /* BaseCost, CR */
    int kind;          /* TS_CAR_ */
    int group;         /* TS_GROUP_ */
    int open_top;      /* a roadster or open car */
} TsCarInfo;

extern const TsCarInfo ts_cars[];
extern const unsigned ts_car_count;

/* the car with this media name (case doesn't matter), or NULL */
const TsCarInfo *ts_car_find(const char *media);

#endif
