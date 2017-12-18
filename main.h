
#ifndef OBJ_MAIN_H
#define OBJ_MAIN_H

#include "lib/dbl.h"
#include "lib/util.h"
#include "lib/crc.h"
#include "lib/app.h"
#include "lib/configl.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/regulator.h"
#include "lib/acp/prog.h"
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

#define PROG_FIELDS "id,heater_id,cooler_id,ambient_temperature,matter_mass,matter_ksh,loss_factor,temperature_pipe_length,enable,load"

#define PROG_LIST_LOOP_DF {Prog *item = prog_list.top;
#define PROG_LIST_LOOP_ST while (item != NULL) {
#define PROG_LIST_LOOP_SP item = item->next; } item = prog_list.top;}

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
        double kl;//loose factor
    D1List temperature_pipe;
} Matter;

struct prog_st {
    int id;
    double ambient_temperature;
    Matter matter;
    Actuator heater;
    Actuator cooler;
    int state;
    Mutex mutex;
    struct prog_st *next;
};

typedef struct prog_st Prog;

DEC_LLIST(Prog)

typedef struct {
    sqlite3 *db;
    ProgList *prog_list;
} ProgData;


enum {
    ON = 1,
    OFF,
    DO,
    INIT,
    RUN,
    DISABLE,
    UNKNOWN
} StateProg;

extern int readSettings() ;

extern void initApp() ;

extern int initData() ;

extern void serverRun(int *state, int init_state) ;

extern void progControl(Prog *item) ;

extern void *threadFunction(void *arg) ;

extern int createThread_ctl() ;

extern void freeProg(ProgList * list) ;

extern void freeData() ;

extern void freeApp() ;

extern void exit_nicely() ;

extern void exit_nicely_e(char *s) ;

#endif 

