
#ifndef REGSMP_MAIN_H
#define REGSMP_MAIN_H


#include "lib/dbl.h"
#include "lib/util.h"
#include "lib/crc.h"
#include "lib/gpio.h"
#include "lib/app.h"
#include "lib/configl.h"
#include "lib/timef.h"
#include "lib/udp.h"
#include "lib/regpidonfhc.h"
#include "lib/acp/main.h"
#include "lib/acp/app.h"
#include "lib/acp/regulator.h"
#include "lib/acp/prog.h"
#include "lib/acp/regonf.h"
#include "lib/acp/regsmp.h"

#define APP_NAME regsmp
#define APP_NAME_STR TOSTRING(APP_NAME)

#ifdef MODE_FULL
#define CONF_DIR "/etc/controller/" APP_NAME_STR "/"
#endif
#ifndef MODE_FULL
#define CONF_DIR "./"
#endif
#define CONFIG_FILE "" CONF_DIR "config.tsv"

#define PROG_FIELDS "id,sensor_id,heater_em_id,cooler_em_id,em_mode,goal,heater_mode,heater_delta,heater_kp,heater_ki,heater_kd,cooler_mode,cooler_kp,cooler_ki,cooler_kd,cooler_delta,change_gap,enable,load"

#define WAIT_RESP_TIMEOUT 3

#define MODE_SIZE 3

#define FLOAT_NUM "%.2f"

#define PROG_LIST_LOOP_DF Prog *curr = prog_list.top;
#define PROG_LIST_LOOP_ST while (curr != NULL) {
#define PROG_LIST_LOOP_SP curr = curr->next; } curr = prog_list.top;
typedef struct {
    int id;
    double power;
} Actuator;
typedef struct {
    double kl;//loose factor
    int temperature;
    double energy;
    double mass;
    double ksh;
    F1List temperature_pipe;
} Matter;
struct prog_st {
    int id;
    double ambient_temperature;
    double kl;
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
    PeerList *peer_list;
    ProgList *prog_list;
} ProgData;


enum {
    ON = 1,
    OFF,
    DO,
    INIT,
    RUN,
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

