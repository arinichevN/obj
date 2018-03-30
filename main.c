#include "main.h"

int app_state = APP_INIT;

char db_data_path[LINE_SIZE];

int sock_port = -1;
int sock_fd = -1;
Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
struct timespec cycle_duration = {0, 0};
Mutex progl_mutex = MUTEX_INITIALIZER;
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
    n = fscanf(stream, "%d\t%ld\t%ld\t%255s\n",
            &sock_port,
            &cycle_duration.tv_sec,
            &cycle_duration.tv_nsec,
            db_data_path
            );
    if (n != 4) {
        fclose(stream);
#ifdef MODE_DEBUG
        fputs("ERROR: readSettings: bad format\n", stderr);
#endif
        return 0;
    }
    fclose(stream);
#ifdef MODE_DEBUG

    printf("readSettings: \n\tsock_port: %d, \n\tcycle_duration: %ld sec %ld nsec, \n\tdb_data_path: %s\n", sock_port, cycle_duration.tv_sec, cycle_duration.tv_nsec, db_data_path);
#endif
    return 1;
}

void initApp() {
    if (!readSettings()) {
        exit_nicely_e("initApp: failed to read settings\n");
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
        freeProgList(&prog_list);
        return 0;
    }
    return 1;
}
#define PARSE_I1LIST acp_requestDataToI1List(&request, &i1l);if (i1l.length <= 0) {return;}
#define PARSE_I2LIST acp_requestDataToI2List(&request, &i2l);if (i2l.length <= 0) {return;}

void serverRun(int *state, int init_state) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    DEF_SERVER_I1LIST
    DEF_SERVER_I2LIST
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
            addProgById(i1l.item[i], &prog_list, NULL, db_data_path);
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
            addProgById(i1l.item[i], &prog_list, NULL, db_data_path);
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_ENABLE)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    item->state = INIT;
                    db_saveTableFieldInt("prog", "enable", item->id, 1, NULL, db_data_path);
                    unlockMutex(&item->mutex);
                }
            }
        }
        return;
    } else if (ACP_CMD_IS(ACP_CMD_PROG_DISABLE)) {
        PARSE_I1LIST
        for (int i = 0; i < i1l.length; i++) {
            Prog *item = getProgById(i1l.item[i], &prog_list);
            if (item != NULL) {
                if (lockMutex(&item->mutex)) {
                    item->state = DISABLE;
                    db_saveTableFieldInt("prog", "enable", item->id, 0, NULL, db_data_path);
                    unlockMutex(&item->mutex);
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
    } else if (ACP_CMD_IS(ACP_CMD_SET_FLOAT)) {
        PARSE_I2LIST
        for (int i = 0; i < i2l.length; i++) {
            Actuator *item = getActuatorById(i2l.item[i].p0, &prog_list);
            Prog *prog = getProgByActuatorId(i2l.item[i].p0, &prog_list);
            if (prog != NULL) {
                if (lockMutex(&prog->mutex)) {
                    if (item != NULL) {
                        if (i2l.item[i].p1 >= 0) {
                            item->power = (double) i2l.item[i].p1;
                        } else {
                            item->power = 0;
                        }
                    }
                    unlockMutex(&prog->mutex);
                }
            }
        }
        return;
    }

    acp_responseSend(&response, &peer_client);
}
#define FX pow((item->kl), (0.2 * (item->temperature - ambient_temperature)))
#define FX1 item->kl * (item->temperature - ambient_temperature)

void matter_ctrl(Matter *item, double ambient_temperature, double heater_power, double cooler_power) {
    double dE = 0.0;
    //aiming to ambient
    if (item->temperature > ambient_temperature) {
        dE = -(FX);
#ifdef MODE_DEBUG
        printf("\tcooling ");
#endif
    } else if (item->temperature < ambient_temperature) {
        dE = FX;
#ifdef MODE_DEBUG
        printf("\theating ");
#endif
    } else {
#ifdef MODE_DEBUG
        printf("\tstable ");
#endif
    }
#ifdef MODE_DEBUG
    printf("adE:%f ", dE);
#endif
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
#ifdef MODE_DEBUG
    printf("tdE:%f dT:%f\n", dE, dT);
#endif
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

void cleanup_handler(void *arg) {
    Prog *item = arg;
    printf("cleaning up thread %d\n", item->id);
}

void *threadFunction(void *arg) {
    Prog *item = arg;
#ifdef MODE_DEBUG
    printf("thread for program with id=%d has been started\n", item->id);
#endif
#ifdef MODE_DEBUG
    pthread_cleanup_push(cleanup_handler, item);
#endif
    while (1) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if (threadCancelDisable(&old_state)) {
            if (lockMutex(&item->mutex)) {
                progControl(item);
                unlockMutex(&item->mutex);
            }
            threadSetCancelState(old_state);
        }
        sleepRest(item->cycle_duration, t1);
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop(1);
#endif
}

void freeData() {
    stopAllProgThreads(&prog_list);
    freeProgList(&prog_list);
}

void freeApp() {
    freeData();
    freeSocketFd(&sock_fd);
    freeMutex(&progl_mutex);
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
#ifndef MODE_DEBUG
    daemon(0, 0);
#endif
    conSig(&exit_nicely);
    if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
        perror("main: memory locking failed");
    }
    int data_initialized = 0;
    while (1) {
#ifdef MODE_DEBUG
        printf("%s(): %s %d\n",F, getAppState(app_state), data_initialized);
#endif
        switch (app_state) {
            case APP_INIT:
                initApp();
                app_state = APP_INIT_DATA;
                break;
            case APP_INIT_DATA:
                data_initialized = initData();
                app_state = APP_RUN;
                delayUsIdle(1000000);
                break;
            case APP_RUN:
                serverRun(&app_state, data_initialized);
                break;
            case APP_STOP:
                freeData();
                data_initialized = 0;
                app_state = APP_RUN;
                break;
            case APP_RESET:
                freeApp();
                delayUsIdle(1000000);
                data_initialized = 0;
                app_state = APP_INIT;
                break;
            case APP_EXIT:
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