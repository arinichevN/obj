
#ifndef OBJ_MAIN_H
#define OBJ_MAIN_H

#include "lib/dbl.h"
#include "lib/util.h"
#include "lib/crc.h"
#include "lib/app.h"
#include "lib/configl.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/tsv.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/regulator.h"
#include "lib/acp/channel.h"
#include "lib/acp/regonf.h"
#include "lib/acp/regsmp.h"
#include <math.h>

#define APP_NAME obj
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif
#define CONFIG_FILE "" CONF_DIR "config.tsv"

#define FSTR "%.3f"

typedef struct {
    int id;
    double power;
} Actuator;

typedef struct {
    double temperature;
    double energy;
    double mass;
    double ksh;
    double kl; //loose factor
    double pl;//loose power
    D1List temperature_pipe;
    struct timespec t1; 
} Matter;

struct channel_st {
    int id;
    double ambient_temperature;
    Matter matter;
    Actuator heater;
    Actuator cooler;
    int state;

    int save;
    struct timespec cycle_duration;
    pthread_t thread;
    Mutex mutex;
    struct channel_st *next;
};

typedef struct channel_st Channel;

DEC_LLIST(Channel)

enum {
    ON = 1,
    OFF,
    DO,
    INIT,
    RUN,
    DISABLE,
    UNKNOWN
} StateProg;

extern int readSettings ( TSVresult* r,char *config_path, char **peer_id, char **db_path );

extern int initApp();

extern int initData();

extern void serverRun(int *state, int init_state);

extern void channelControl(Channel *item);

extern void *threadFunction(void *arg);

extern void freeData();

extern void freeApp();

extern void exit_nicely();

#endif 

