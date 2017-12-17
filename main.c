#include "main.h"

char pid_path[LINE_SIZE];
int app_state = APP_INIT;

char db_data_path[LINE_SIZE];

int pid_file = -1;
int proc_id;
int sock_port = -1;
int sock_fd = -1;
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
DEF_THREAD
Mutex progl_mutex = {.created = 0, .attr_initialized = 0};
I1List i1l = {NULL, 0};
I2List i2l = {NULL, 0};
ProgList prog_list = {NULL, NULL, 0};

#include "util.c"
#include "db.c"

int readSettings() {
#ifdef MODE_DEBUG
    printf("readSettings: configuration file to read: %s\n", CONFIG_FILE);
#endif
    FILE* stream = fopen(CONFIG_FILE, "r");
    if (stream == NULL) {
#ifdef MODE_DEBUG
        perror("readSettings()");
#endif
        return 0;
    }
    skipLine(stream);
    int n;
    n = fscanf(stream, "%d\t%255s\t%ld\t%ld\t%255s\n",
            &sock_port,
            pid_path,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            db_data_path
            );
    if (n != 5) {
        fclose(stream);
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: bad format\n", stderr);
#endif
        return 0;
    }
    fclose(stream);
#ifdef MODE_DEBUG

    printf("readSettings: \n\tsock_port: %d, \n\tpid_path: %s, \n\tcycle_duration: %ld sec %ld nsec, \n\tdb_data_path: %s\n", sock_port, pid_path, cycle_duration.tv_sec, cycle_duration.tv_nsec, db_data_path);
#endif
    return 1;
}

void initApp() {
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
    }
    if (!initPid(&pid_file, &proc_id, pid_path)) {
        exit_nicely_e("initApp: failed to initialize pid\n");
    }
    if (!initMutex(&progl_mutex)) {
        exit_nicely_e("initApp: failed to initialize prog mutex\n");
    }

    if (!initServer(&sock_fd, sock_port)) {

        exit_nicely_e("initApp: failed to initialize udp server\n");
    }
}

int initData() {
    if (!loadActiveProg(&prog_list, db_data_path)) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to load active programs\n", stderr);
#endif
        freeProg(&prog_list);
        return 0;
    }
    if (!initI1List(&i1l, ACP_BUFFER_MAX_SIZE)) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to allocate memory for i1l\n", stderr);
#endif
        freeProg(&prog_list);
        return 0;
    }
    if (!initI2List(&i2l, ACP_BUFFER_MAX_SIZE)) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to allocate memory for i2l\n", stderr);
#endif
        FREE_LIST(&i1l);
        freeProg(&prog_list);
        return 0;
    }
    if (!THREAD_CREATE) {
#ifdef MODE_DEBUG
        fputs("initData: ERROR: failed to create thread\n", stderr);
#endif
        FREE_LIST(&i2l);
        FREE_LIST(&i1l);
        freeProg(&prog_list);

        return 0;
    }
    return 1;
}
#define PARSE_I1LIST acp_requestDataToI1List(&request, &i1l, prog_list.length);if (i1l.length <= 0) {return;}
#define PARSE_I2LIST acp_requestDataToI2List(&request, &i2l, prog_list.length);if (i2l.length <= 0) {return;}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS

    if (ACP_CMD_IS(ACP_CMD_PROG_STOP)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                deleteProgById(i1l.item[i], &prog_list, db_data_path);
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_START)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_RESET)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                deleteProgById(i1l.item[i], &prog_list, db_data_path);
            }
        }
        for (int i = 0; i < i1l.length; i++) {
            addProgById(i1l.item[i], &prog_list, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_ENABLE)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockProg(item)) {
                    item->state = INIT;
                    saveProgEnable(item->id, 1, db_data_path);
                    unlockProg(item);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_DISABLE)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockProg(item)) {
                    item->state = DISABLE;
                    saveProgEnable(item->id, 0, db_data_path);
                    unlockProg(item);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_RUNTIME)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgRuntime(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_DATA_INIT)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgInit(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_PROG_GET_ENABLED)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgEnabled(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_GET_FTS)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (!bufCatProgFTS(item, &response)) {
                    return;
                }
            }
        }
    } else if (ACP_CMD_IS(ACP_CMD_SET_PWM_DUTY_CYCLE)) {
        PARSE_I2LIST
        for (int i = 0; i < i2l.length; i++) {
            Actuator *item = getActuatorById(i2l.item[i].p0, &prog_list);
            if (item != NULL) {
                if (i2l.item[i].p1 >= 0) {
                    item->power = (double) i2l.item[i].p1;
                }else{
                    item->power=0;
                }
            }
        }
        return;
    }

    acp_responseSend(&response, &peer_client);
}
#define FX pow((1 + item->kl), (0.2 * (item->temperature - ambient_temperature))) - 1
#define FX1 item->kl * (item->temperature - ambient_temperature)

void matter_ctrl(Matter *item, double ambient_temperature, double heater_power, double cooler_power) {
    double dE = 0.0;
    //aiming to ambient
    if (item->temperature > ambient_temperature) {
        dE = -(FX);
        printf("\tcooling ");
    } else if (item->temperature < ambient_temperature) {
        dE = FX;
        printf("\theating ");
    } else {
        printf("\tstable ");
    }
    printf("adE:%f ", dE);
    //actuator affect
    dE += heater_power;
    dE -= cooler_power;
    //total affect
    item->energy += dE;
    if (item->energy < 0.0) {
        item->energy = 0.0;
        dE = 0.0;
    }
    //temperature computation
    double dT = dE / (item->ksh * item->mass);
    printf("tdE:%f dT:%f\n", dE, dT);
    if (item->temperature_pipe.length > 0) {//delay for temperature
        pipe_move(&item->temperature_pipe);
        pipe_push(&item->temperature_pipe, dT);
        item->temperature += pipe_pop(&item->temperature_pipe);
    } else {
        item->temperature += dT;
    }
}

void progControl(Prog * item) {
#ifdef MODE_DEBUG
    char *state = getStateStr(item->state);
    printf("prog: id:%d state:%s temp:%.2f energy:%.2f heater_pwr:%.2f cooler_pwr:%.2f \n",
            item->id,
            state,
            item->matter.temperature,
            item->matter.energy,
            item->heater.power,
            item->cooler.power
            );
#endif
    switch (item->state) {
        case INIT:
            item->matter.temperature = item->ambient_temperature;
            item->matter.energy = item->matter.temperature * item->matter.ksh * item->matter.mass;
            item->state = RUN;
            break;
        case RUN:
            matter_ctrl(&item->matter, item->ambient_temperature, item->heater.power, item->cooler.power);
            break;
        case DISABLE:
            item->state = OFF;
            break;
        case OFF:
            break;
        default:
            break;
    }

}

void *threadFunction(void *arg) {
    THREAD_DEF_CMD
#ifdef MODE_DEBUG
            puts("threadFunction: running...");
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();

        lockProgList();
        Prog *item = prog_list.top;
        unlockProgList();
        while (1) {
            if (item == NULL) {
                break;
            }
            if (tryLockProg(item)) {

                progControl(item);
                Prog *temp = item;
                item = item->next;
                unlockProg(temp);
            }
            THREAD_EXIT_ON_CMD
        }
        THREAD_EXIT_ON_CMD
        sleepRest(cycle_duration, t1);
    }
}

void freeProg(ProgList * list) {
    Prog *item = list->top, *temp;
    while (item != NULL) {

        temp = item;
        item = item->next;
        free(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

void freeData() {

    THREAD_STOP
    freeProg(&prog_list);
    FREE_LIST(&i2l);
    FREE_LIST(&i1l);
#ifdef MODE_DEBUG
    puts("freeData: done");
#endif
}

void freeApp() {

    freeData();
    freeSocketFd(&sock_fd);
    freeMutex(&progl_mutex);
    freePid(&pid_file, &proc_id, pid_path);
}

void exit_nicely() {

    freeApp();
#ifdef MODE_DEBUG
    puts("\nBye...");
#endif
    exit(EXIT_SUCCESS);
}

void exit_nicely_e(char *s) {
#ifdef MODE_DEBUG

    fprintf(stderr, "%s", s);
#endif
    freeApp();
    exit(EXIT_FAILURE);
}

int main(int argc, char** argv) {
    if (geteuid() != 0) {
#ifdef MODE_DEBUG
        fprintf(stderr, "%s: root user expected\n", APP_NAME_STR);
#endif
        return (EXIT_FAILURE);
    }
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
    int data_initialized = 0;
    while (1) {
        switch (app_state) {
            case APP_INIT:
#ifdef MODE_DEBUG
                puts("MAIN: init");
#endif
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
#ifdef MODE_DEBUG
                puts("MAIN: init data");
#endif
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000000);
                break;
            case APP_RUN:
#ifdef MODE_DEBUG
                puts("MAIN: run");
#endif
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
#ifdef MODE_DEBUG
                puts("MAIN: stop");
#endif
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
#ifdef MODE_DEBUG
                puts("MAIN: reset");
#endif
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
#ifdef MODE_DEBUG
                puts("MAIN: exit");
#endif
                exit_nicely();
                break;
            default:
                exit_nicely_e("main: unknown application state");
                break;
        }
    }
    freeApp();
    return (EXIT_SUCCESS);
}