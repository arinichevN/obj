
#include "main.h"

FUN_LLIST_GET_BY_ID(Prog)

void stopProgThread(Prog *item) {
#ifdef MODE_DEBUG
    printf("signaling thread %d to cancel...\n", item->id);
#endif
    if (pthread_cancel(item->thread) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_cancel()");
#endif
    }
    void * result;
#ifdef MODE_DEBUG
    printf("joining thread %d...\n", item->id);
#endif
    if (pthread_join(item->thread, &result) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_join()");
#endif
    }
    if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
        printf("thread %d not canceled\n", item->id);
#endif
    }
}

void stopAllProgThreads(ProgList * list) {
    PROG_LIST_LOOP_ST
#ifdef MODE_DEBUG
            printf("signaling thread %d to cancel...\n", item->id);
#endif
    if (pthread_cancel(item->thread) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_cancel()");
#endif
    }
    PROG_LIST_LOOP_SP

    PROG_LIST_LOOP_ST
            void * result;
#ifdef MODE_DEBUG
    printf("joining thread %d...\n", item->id);
#endif
    if (pthread_join(item->thread, &result) != 0) {
#ifdef MODE_DEBUG
        perror("pthread_join()");
#endif
    }
    if (result != PTHREAD_CANCELED) {
#ifdef MODE_DEBUG
        printf("thread %d not canceled\n", item->id);
#endif
    }
    PROG_LIST_LOOP_SP
}

void freeProg(Prog*item) {
    freeMutex(&item->mutex);
    free(item);
}

void freeProgList(ProgList *list) {
    Prog *item = list->top, *temp;
    while (item != NULL) {
        temp = item;
        item = item->next;
        freeProg(temp);
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

void pipe_push(D1List *list, double value) {
    list->item[0] = value;
}

double pipe_pop(D1List *list) {
    return list->item[list->length - 1];
}

void pipe_move(D1List *list) {
    for (int i = list->length - 1; i > 0; i--) {
        list->item[i] = list->item[i - 1];
    }
    list->item[0] = 0.0;
}

Actuator * getActuatorById(int id, const ProgList *list) {
    Prog *item = list->top;
    while (item != NULL) {
        if (item->cooler.id == id) {
            return &item->cooler;
        }
        if (item->heater.id == id) {
            return &item->heater;
        }
        item = item->next;
    }
    return NULL;
}

Prog *getProgByActuatorId(int id, ProgList *list){
    Prog *item = list->top;
    while (item != NULL) {
        if (item->cooler.id == id || item->heater.id == id) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

int lockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_lock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("lockProgList: error locking mutex");
#endif 
        return 0;
    }
    return 1;
}

int tryLockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_trylock(&(progl_mutex.self)) != 0) {
        return 0;
    }
    return 1;
}

int unlockProgList() {
    extern Mutex progl_mutex;
    if (pthread_mutex_unlock(&(progl_mutex.self)) != 0) {
#ifdef MODE_DEBUG
        perror("unlockProgList: error unlocking mutex (CMD_GET_ALL)");
#endif 
        return 0;
    }
    return 1;
}

char * getStateStr(char state) {
    switch (state) {
        case ON:
            return "ON";
        case OFF:
            return "OFF";
        case DO:
            return "DO";
        case INIT:
            return "INIT";
        case RUN:
            return "RUN";
        case DISABLE:
            return "DISABLE";
    }
    return "?";
}

int bufCatProgRuntime(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        char *state = getStateStr(item->state);
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_ROW_STR,
                item->id,
                state,
                item->heater.power,
                item->cooler.power,
                item->matter.temperature
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgInit(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
                item->id,
                item->heater.id,
                item->cooler.id,
                item->ambient_temperature,
                item->matter.mass,
                item->matter.ksh,
                item->matter.kl,
                item->matter.temperature_pipe.length
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

int bufCatProgFTS(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        struct timespec tm = getCurrentTime();
        int r= acp_responseFTSCat(item->id, item->matter.temperature, tm, 1, response);
        unlockMutex(&item->mutex);
        return r;
    }
    return 0;
}

int bufCatProgEnabled(Prog *item, ACPResponse *response) {
    if (lockMutex(&item->mutex)) {
        char q[LINE_SIZE];
        int enabled = 1;
        if (item->state == OFF) {
            enabled = 0;
        }
        snprintf(q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
                item->id,
                enabled
                );
        unlockMutex(&item->mutex);
        return acp_responseStrCat(response, q);
    }
    return 0;
}

void printData(ACPResponse *response) {
    ProgList *list = &prog_list;
    char q[LINE_SIZE];
    snprintf(q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE);
    SEND_STR(q)
    snprintf(q, sizeof q, "port: %d\n", sock_port);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration sec: %ld\n", cycle_duration.tv_sec);
    SEND_STR(q)
    snprintf(q, sizeof q, "cycle_duration nsec: %ld\n", cycle_duration.tv_nsec);
    SEND_STR(q)
    snprintf(q, sizeof q, "db_data_path: %s\n", db_data_path);
    SEND_STR(q)
    snprintf(q, sizeof q, "app_state: %s\n", getAppState(app_state));
    SEND_STR(q)
    snprintf(q, sizeof q, "PID: %d\n", getpid());
    SEND_STR(q)
    snprintf(q, sizeof q, "prog_list length: %d\n", list->length);
    SEND_STR(q)
    SEND_STR("+-----------------------------------------------------------------------------------------------+\n")
    SEND_STR("|                                Program initial data                                           |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|    id     |heater_id  |cooler_id  |ambient_tem|   mass    | spec heat |loss_factor|t_pipe_len |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    PROG_LIST_LOOP_ST
    snprintf(q, sizeof q, "|%11d|%11d|%11d|%11.3f|%11.3f|%11.3f|%11.3f|%11d|\n",
            item->id,
            item->heater.id,
            item->cooler.id,
            item->ambient_temperature,
            item->matter.mass,
            item->matter.ksh,
            item->matter.kl,
            item->matter.temperature_pipe.length
            );
    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n")

    SEND_STR("+-----------------------------------------------------------------------+\n")
    SEND_STR("|                           Program runtime data                        |\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    SEND_STR("|    id     |   state   |heater_pwr |cooler_pwr |  energy   |temperature|\n")
    SEND_STR("+-----------+-----------+-----------+-----------+-----------+-----------+\n")
    PROG_LIST_LOOP_ST
            char *state = getStateStr(item->state);
    snprintf(q, sizeof q, "|%11d|%11s|%11.3f|%11.3f|%11.3f|%11.3f|\n",
            item->id,
            state,
            item->heater.power,
            item->cooler.power,
            item->matter.energy,
            item->matter.temperature
            );
    SEND_STR(q)
    PROG_LIST_LOOP_SP
    SEND_STR_L("+-----------+-----------+-----------+-----------+-----------+-----------+\n")
}

void printHelp(ACPResponse *response) {
    char q[LINE_SIZE];
    SEND_STR("COMMAND LIST\n")
    snprintf(q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tput process into standby mode; all running programs will be stopped\n", ACP_CMD_APP_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tload prog into RAM and start its execution; program id expected\n", ACP_CMD_PROG_START);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM; program id expected\n", ACP_CMD_PROG_STOP);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tunload program from RAM, after that load it; program id expected\n", ACP_CMD_PROG_RESET);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tenable running program; program id expected\n", ACP_CMD_PROG_ENABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tdisable running program; program id expected\n", ACP_CMD_PROG_DISABLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog state (1-enabled, 0-disabled); program id expected\n", ACP_CMD_PROG_GET_ENABLED);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog sensor value; program id expected\n", ACP_CMD_GET_FTS);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tset heater of cooler power; heater or cooler id expected\n", ACP_CMD_SET_PWM_DUTY_CYCLE);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog runtime data; program id expected\n", ACP_CMD_PROG_GET_DATA_RUNTIME);
    SEND_STR(q)
    snprintf(q, sizeof q, "%s\tget prog initial data; program id expected\n", ACP_CMD_PROG_GET_DATA_INIT);
    SEND_STR_L(q)

}
