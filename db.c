
#include "main.h"

int checkChannel ( const Channel *item, ChannelLList *list ) {
    int success=1;
    if ( item->matter.mass <= 0 ) {
        fprintf ( stderr, "%s(): expected matter_mass > 0 where channel id = %d\n",F, item->id );
        success= 0;
    }
    if ( item->matter.ksh <= 0 ) {
        fprintf ( stderr, "%s(): expected matter_ksh > 0 where channel id = %d\n",F, item->id );
        success= 0;
    }
    if ( item->matter.kl < 0 ) {
        fprintf ( stderr, "%s(): expected loss_factor >= 0 where channel id = %d\n",F, item->id );
        success= 0;
    }
    if ( item->matter.pl < 0 ) {
        fprintf ( stderr, "%s(): expected loss_power >= 0 where channel id = %d\n",F, item->id );
        success= 0;
    }
    if ( item->matter.temperature_pipe.length < 0 ) {
        fprintf ( stderr, "%s(): expected temperature_pipe_length >= 0 where channel id = %d\n",F, item->id );
        success= 0;
    }
    if ( getActuatorById ( item->cooler.id, list ) != NULL ) {
        fprintf ( stderr, "%s(): cooler_id already exists where channel id = %d\n",F, item->id );
        success= 0;
    }
    if ( getActuatorById ( item->heater.id, list ) != NULL ) {
        fprintf ( stderr, "%s(): heater_id already exists where channel id = %d\n",F, item->id );
        success= 0;
    }
    return success;
}


int getChannel_callback ( void *data, int argc, char **argv, char **azColName ) {
    struct ds {
        void *p1;
        void *p2;
    } *d;
    d=data;
    sqlite3 *db=d->p2;
    Channel *item = d->p1;
    int load = 0, enable = 0;
    int c=0;
    DB_FOREACH_COLUMN {
        if ( DB_COLUMN_IS ( "id" ) ) {
            item->id = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "heater_id" ) ) {
            item->heater.id = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "cooler_id" ) ) {
            item->cooler.id = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "ambient_temperature" ) ) {
            item->ambient_temperature = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "matter_mass" ) ) {
            item->matter.mass = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "matter_ksh" ) ) {
            item->matter.ksh = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "loss_factor" ) ) {
            item->matter.kl = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "loss_power" ) ) {
            item->matter.pl = DB_CVF;
            c++;
        } else if ( DB_COLUMN_IS ( "temperature_pipe_length" ) ) {
            D1List *tlist=&item->matter.temperature_pipe;
            RESET_LIST ( tlist )
            int length=DB_CVI;
            if ( length>0 ) {
                ALLOC_LIST ( tlist, length );
                if ( tlist->item ==NULL ) {
                    return EXIT_FAILURE;
                }
            }
            c++;
        } else if ( DB_COLUMN_IS ( "cycle_duration_sec" ) ) {
            item->cycle_duration.tv_sec=DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "cycle_duration_nsec" ) ) {
            item->cycle_duration.tv_nsec=DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "save" ) ) {
            item->save = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "enable" ) ) {
            enable = DB_CVI;
            c++;
        } else if ( DB_COLUMN_IS ( "load" ) ) {
            load = DB_CVI;
            c++;
        } else {
            printde ( "unknown column (we will skip it): %s\n", DB_COLUMN_NAME );
        }
    }
#define N 14
    if ( c != N ) {
        printde ( "required %d columns but %d found\n", N, c );
        return EXIT_FAILURE;
    }
#undef N
    if ( enable ) {
        item->state = INIT;
    } else {
        item->state = DISABLE;
    }
    if ( !load ) {puts("not load");
        if ( item->save ) {db_saveTableFieldInt ( "channel", "load", item->id, 1,db, NULL );puts("now load");}
    }
    return EXIT_SUCCESS;
}

int getChannelByIdFDB ( int id, Channel *item, sqlite3 *dbl, const char *db_path ) {
    int close=0;
    sqlite3 *db=db_openAlt ( dbl, db_path, &close );
    if ( db==NULL ) {
        putsde ( " failed\n" );
        return 0;
    }
    char q[LINE_SIZE];
    struct ds {
        void *p1;
        void *p2;
    } data= {.p1=item, .p2=db};
    snprintf ( q, sizeof q, "select * from channel where id=%d", id );
    if ( !db_exec ( db, q, getChannel_callback, ( void * ) &data ) ) {
        putsde ( " failed\n" );
        if ( close ) db_close ( db );
        return 0;
    }
    if ( close ) db_close ( db );
    return 1;
}

int addChannel ( Channel *item, ChannelLList *list, Mutex *list_mutex ) {
    if ( list->length >= INT_MAX ) {
        printde ( "can not load channel with id=%d - list length exceeded\n", item->id );
        return 0;
    }
    if ( list->top == NULL ) {
        lockMutex ( list_mutex );
        list->top = item;
        unlockMutex ( list_mutex );
    } else {
        lockMutex ( &list->last->mutex );
        list->last->next = item;
        unlockMutex ( &list->last->mutex );
    }
    list->last = item;
    list->length++;
    printdo ( "channel with id=%d loaded\n", item->id );
    return 1;
}
//returns deleted channel
Channel * deleteChannel ( int id, ChannelLList *list, Mutex *list_mutex ) {
    Channel *prev = NULL;
    FOREACH_LLIST ( curr,list,Channel ) {
        if ( curr->id == id ) {
            if ( prev != NULL ) {
                lockMutex ( &prev->mutex );
                prev->next = curr->next;
                unlockMutex ( &prev->mutex );
            } else {//curr=top
                lockMutex ( list_mutex );
                list->top = curr->next;
                unlockMutex ( list_mutex );
            }
            if ( curr == list->last ) {
                list->last = prev;
            }
            list->length--;
            return curr;
        }
        prev = curr;
    }
    return NULL;
}
int addChannelById ( int id, ChannelLList *list, Mutex *list_mutex, sqlite3 *dbl, const char *db_path ) {
    {
        Channel *item;
        LLIST_GETBYID ( item,list,id )
        if ( item != NULL ) {
            printde ( "channel with id = %d is being controlled\n", item->id );
            return 0;
        }
    }
    Channel *item = malloc ( sizeof * ( item ) );
    if ( item == NULL ) {
        putsde ( "failed to allocate memory for channel\n" );
        return 0;
    }
    memset ( item, 0, sizeof *item );
    item->id = id;
    item->next = NULL;
    if ( !getChannelByIdFDB ( item->id, item, dbl, db_path ) ) {
        FREE_LIST ( &item->matter.temperature_pipe )
        free ( item );
        return 0;
    }
    if ( !initMutex ( &item->mutex ) ) {
        FREE_LIST ( &item->matter.temperature_pipe )
        free ( item );
        return 0;
    }

    if ( !checkChannel ( item, list ) ) {
        freeMutex ( &item->mutex );
        FREE_LIST ( &item->matter.temperature_pipe )
        free ( item );
        return 0;
    }
    if ( !addChannel ( item, list, list_mutex ) ) {
        freeMutex ( &item->mutex );
        FREE_LIST ( &item->matter.temperature_pipe )
        free ( item );
        return 0;
    }
    if ( !createMThread ( &item->thread, &threadFunction, item ) ) {
        deleteChannel ( item->id, list, list_mutex );
        freeMutex ( &item->mutex );
        FREE_LIST ( &item->matter.temperature_pipe )
        free ( item );
        return 0;
    }
    return 1;
}

int deleteChannelById ( int id, ChannelLList *list, Mutex *list_mutex, sqlite3 *dbl, const char *db_path ) {
    printdo ( "channel to delete: %d\n", id );
    Channel *del_channel= deleteChannel ( id, list, list_mutex );
    if ( del_channel==NULL ) {
        putsdo ( "channel to delete not found\n" );
        return 0;
    }
    STOP_CHANNEL_THREAD ( del_channel );
    if ( del_channel->save ) db_saveTableFieldInt ( "channel", "load", del_channel->id, 0, dbl, db_path );
    freeChannel ( del_channel );
    printdo ( "channel with id: %d has been deleted from channel_list\n", id );
    return 1;
}
int loadActiveChannel_callback ( void *data, int argc, char **argv, char **azColName ) {
    struct ds {
        void *p1;
        void *p2;
        void *p3;
    } *d;
    d=data;
    ChannelLList *list=d->p1;
    Mutex *list_mutex=d->p2;
    sqlite3 *db=d->p3;
    DB_FOREACH_COLUMN {
        if ( DB_COLUMN_IS ( "id" ) ) {
            int id = DB_CVI;
            addChannelById ( id, list, list_mutex, db,NULL );
        } else {
            printde ( "unknown column (we will skip it): %s\n", DB_COLUMN_NAME );
        }
    }
    return EXIT_SUCCESS;
}

int loadActiveChannel ( ChannelLList *list, Mutex *list_mutex,  char *db_path ) {
    sqlite3 *db;
    if ( !db_open ( db_path, &db ) ) {
        return 0;
    }
    struct ds {
        void *p1;
        void *p2;
        void *p3;
    };
    struct ds data= {.p1=list, .p2=list_mutex, .p3=db};
    char *q = "select id from channel where load=1";
    if ( !db_exec ( db, q, loadActiveChannel_callback, &data ) ) {
        putsde ( " failed\n" );
        sqlite3_close ( db );
        return 0;
    }
    sqlite3_close ( db );
    return 1;
}
