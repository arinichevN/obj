
#include "main.h"

int checkProg(const Prog *item, ProgList *list) {
    if (item->matter.mass <= 0) {
        fprintf(stderr, "checkProg(): expected matter_mass > 0 in prog with id = %d\n", item->id);
        return 0;
    }
    if (item->matter.ksh <= 0) {
        fprintf(stderr, "checkProg(): expected matter_ksh > 0 in prog with id = %d\n", item->id);
        return 0;
    }
    if (item->matter.kl < 0) {
        fprintf(stderr, "checkProg(): expected loss_factor >= 0 in prog with id = %d\n", item->id);
        return 0;
    }
    if (item->matter.temperature_pipe.length < 0) {
        fprintf(stderr, "checkProg(): expected temperature_pipe_length >= 0 in prog with id = %d\n", item->id);
        return 0;
    }
    if (getActuatorById(item->cooler.id, list) != NULL) {
        fprintf(stderr, "checkProg(): cooler_id already exists where prog id = %d\n", item->id);
        return 0;
    }
    if (getActuatorById(item->heater.id, list) != NULL) {
        fprintf(stderr, "checkProg(): heater_id already exists where prog id = %d\n", item->id);
        return 0;
    }
    return 1;
}


int getProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData * data = d;
    Prog *item = data->prog;
    int load = 0, enable = 0;
    for (int i = 0; i < argc; i++) {
         if (DB_COLUMN_IS("id")) {
            item->id = atoi(argv[i]);
        } else if (DB_COLUMN_IS("heater_id")) {
            item->heater.id = atoi(argv[i]);
        } else if (DB_COLUMN_IS("cooler_id")) {
            item->cooler.id = atoi(argv[i]);
        } else if (DB_COLUMN_IS("ambient_temperature")) {
            item->ambient_temperature = atof(argv[i]);
        } else if (DB_COLUMN_IS("matter_mass")) {
            item->matter.mass = atof(argv[i]);
        } else if (DB_COLUMN_IS("matter_ksh")) {
            item->matter.ksh = atof(argv[i]);
        } else if (DB_COLUMN_IS("loss_factor")) {
            item->matter.kl = atof(argv[i]);
        } else if (DB_COLUMN_IS("temperature_pipe_length")) {
            if (!initD1List(&item->matter.temperature_pipe, atoi(argv[i]))) {
                free(item);
                return EXIT_FAILURE;
            }
        } else if (DB_COLUMN_IS("enable")) {
            enable = atoi(argv[i]);
        } else if (DB_COLUMN_IS("load")) {
            load = atoi(argv[i]);
        } else {
            #ifdef MODE_DEBUG
       fputs("getProg_callback(): unknown column: %s\n",stderr);
#endif
            
        }
    }

    if (enable) {
        item->state = INIT;
    } else {
        item->state = DISABLE;
    }
    if (!load) {
        config_saveProgLoad(item->id, 1, data->db_data, NULL);
    }
    return EXIT_SUCCESS;
}

int getProgByIdFDB(int prog_id, Prog *item, sqlite3 *dbl, const char *db_path) {
    if (dbl != NULL && db_path != NULL) {
#ifdef MODE_DEBUG
        fprintf(stderr, "getProgByIdFDB(): db xor db_path expected\n");
#endif
        return 0;
    }
    sqlite3 *db;
    if (db_path != NULL) {
        if (!db_open(db_path, &db)) {
            return 0;
        }
    } else {
        db = dbl;
    }
    char q[LINE_SIZE];
    ProgData data = {.db_data = db, .prog = item};
    snprintf(q, sizeof q, "select " PROG_FIELDS " from prog where id=%d", prog_id);
    if (!db_exec(db, q, getProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "getProgByIdFDB(): query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}

int addProg(Prog *item, ProgList *list) {
    if (list->length >= INT_MAX) {
#ifdef MODE_DEBUG
        fprintf(stderr, "addProg: ERROR: can not load prog with id=%d - list length exceeded\n", item->id);
#endif
        return 0;
    }
    if (list->top == NULL) {
        lockProgList();
        list->top = item;
        unlockProgList();
    } else {
        lockMutex(&list->last->mutex);
        list->last->next = item;
        unlockMutex(&list->last->mutex);
    }
    list->last = item;
    list->length++;
#ifdef MODE_DEBUG
    printf("addProg: prog with id=%d loaded\n", item->id);
#endif
    return 1;
}

int addProgById(int prog_id, ProgList *list, sqlite3 * db, const char *db_path) {
    Prog *rprog = getProgById(prog_id, list);
    if (rprog != NULL) {//program is already running
#ifdef MODE_DEBUG
        fprintf(stderr, "addProgById(): program with id = %d is being controlled by program\n", rprog->id);
#endif
        return 0;
    }

    Prog *item = malloc(sizeof *(item));
    if (item == NULL) {
        fputs("addProgById(): failed to allocate memory\n", stderr);
        return 0;
    }
    memset(item, 0, sizeof *item);
    item->id = prog_id;
    item->next = NULL;
    item->cycle_duration = cycle_duration;
    if (!initMutex(&item->mutex)) {
        free(item);
        return 0;
    }
    if (!getProgByIdFDB(item->id, item, db, db_path)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!checkProg(item, list)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!addProg(item, list)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    if (!createMThread(&item->thread, &threadFunction, item)) {
        freeMutex(&item->mutex);
        free(item);
        return 0;
    }
    return 1;
}
int deleteProgById(int id, ProgList *list, const char* db_path) {
#ifdef MODE_DEBUG
    printf("prog to delete: %d\n", id);
#endif
    Prog *prev = NULL, *curr;
    int done = 0;
    curr = list->top;
    while (curr != NULL) {
        if (curr->id == id) {
            if (prev != NULL) {
                lockMutex(&prev->mutex);
                prev->next = curr->next;
                unlockMutex(&prev->mutex);
            } else {//curr=top
                lockProgList();
                list->top = curr->next;
                unlockProgList();
            }
            if (curr == list->last) {
                list->last = prev;
            }
            list->length--;
            stopProgThread(curr);
            config_saveProgLoad(curr->id, 0, NULL, db_path);
            freeProg(curr);
#ifdef MODE_DEBUG
            printf("prog with id: %d deleted from prog_list\n", id);
#endif
            done = 1;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    return done;
}

int loadActiveProg_callback(void *d, int argc, char **argv, char **azColName) {
    ProgData *data = d;
    for (int i = 0; i < argc; i++) {
        if (DB_COLUMN_IS("id")) {
            int id = atoi(argv[i]);
            addProgById(id, data->prog_list,data->db_data,NULL);
        } else {
            fputs("loadActiveProg_callback(): unknown column\n", stderr);
        }
    }
    return EXIT_SUCCESS;
}

int loadActiveProg(ProgList *list, char *db_path) {
    sqlite3 *db;
    if (!db_open(db_path, &db)) {
        return 0;
    }
    ProgData data={.db_data=db, .prog_list=list};
    char *q = "select id from prog where load=1";
    if (!db_exec(db, q, loadActiveProg_callback, &data)) {
#ifdef MODE_DEBUG
        fprintf(stderr, "loadActiveProg(): query failed: %s\n", q);
#endif
        sqlite3_close(db);
        return 0;
    }
    sqlite3_close(db);
    return 1;
}
