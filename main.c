#include "main.h"

int app_state = APP_INIT;

TSVresult config_tsv = TSVRESULT_INITIALIZER;
char * db_path;
char * peer_id;

int sock_port = -1;
int sock_fd = -1;

Peer peer_client = {.fd = &sock_fd, .addr_size = sizeof peer_client.addr};
Mutex channel_list_mutex = MUTEX_INITIALIZER;
ChannelLList channel_list = LLIST_INITIALIZER;

#include "util.c"
#include "db.c"

int readSettings ( TSVresult* r,char *config_path, char **peer_id, char **db_path ) {
    if ( !TSVinit ( r, config_path ) ) {
        return 0;
    }
    char *_peer_id = TSVgetvalues ( r, 0, "peer_id" );
    char *_db_path = TSVgetvalues ( r, 0, "db_path" );
    if ( TSVnullreturned ( r ) ) {
        return 0;
    }
    *peer_id = _peer_id;
    *db_path = _db_path;
    return 1;
}

int initApp() {
    if ( !readSettings ( &config_tsv, CONFIG_FILE, &peer_id, &db_path ) ) {
        putsde ( "failed to read settings\n" );
        return 0;
    }
    if ( !initMutex ( &channel_list_mutex ) ) {
        TSVclear ( &config_tsv );
        putsde ( "failed to initialize channel mutex\n" );
        return 0;
    }
    if ( !config_getPort ( &sock_port, peer_id, NULL, db_path ) ) {
        freeMutex ( &channel_list_mutex );
        TSVclear ( &config_tsv );
        putsde ( "failed to read port\n" );
        return 0;
    }
    if ( !initServer ( &sock_fd, sock_port ) ) {
        freeMutex ( &channel_list_mutex );
        TSVclear ( &config_tsv );
        putsde ( "failed to initialize udp server\n" );
        return 0;
    }
    printdo ( "initApp():\n\tsock_port=%d\n\tdb_path=%s\n",sock_port, db_path );
    return 1;
}

int initData() {
    if ( !loadActiveChannel ( &channel_list, &channel_list_mutex,  db_path ) ) {
        freeChannelList ( &channel_list );
        return 0;
    }
    return 1;
}

void serverRun ( int *state, int init_state ) {
    SERVER_HEADER
    SERVER_APP_ACTIONS
    if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_STOP ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            deleteChannelById ( i1l.item[i], &channel_list , &channel_list_mutex,NULL, db_path );
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_START ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            addChannelById ( i1l.item[i], &channel_list, &channel_list_mutex ,  NULL, db_path );
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_RESET ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            deleteChannelById ( i1l.item[i], &channel_list , &channel_list_mutex, NULL,db_path );
        }
        FORLISTN ( i1l, i ) {
            addChannelById ( i1l.item[i], &channel_list, &channel_list_mutex ,  NULL, db_path );
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_ENABLE ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] );
            if ( item == NULL ) continue;
            if ( lockMutex ( &item->mutex ) ) {
                item->state = INIT;
                db_saveTableFieldInt ( "channel", "enable", item->id, 1, NULL, db_path );
                unlockMutex ( &item->mutex );
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_DISABLE ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] );
            if ( item == NULL ) continue;
            if ( lockMutex ( &item->mutex ) ) {
                item->state = DISABLE;
                db_saveTableFieldInt ( "channel", "enable", item->id, 0, NULL, db_path );
                unlockMutex ( &item->mutex );
            }
        }
        return;
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_GET_DATA_RUNTIME ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] );
            if ( item == NULL ) continue;
            if ( !bufCatChannelRuntime ( item, &response ) ) return;
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_GET_DATA_INIT ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] );
            if ( item == NULL ) continue;
            if ( !bufCatChannelInit ( item, &response ) ) return;
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_CHANNEL_GET_ENABLED ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] );
            if ( item == NULL ) continue;
            if ( !bufCatChannelEnabled ( item, &response ) ) return;
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_GET_FTS ) ) {
        SERVER_GET_I1LIST_FROM_REQUEST
        FORLISTN ( i1l, i ) {
            Channel *item;
            LLIST_GETBYID ( item, &channel_list, i1l.item[i] );
            if ( item == NULL ) continue;
            if ( !bufCatChannelFTS ( item, &response ) ) return;
        }
    } else if ( ACP_CMD_IS ( ACP_CMD_SET_FLOAT ) ) {
        SERVER_GET_I1F1LIST_FROM_REQUEST
        FORLISTN ( i1f1l, i ) {
            Actuator *item = getActuatorById ( i1f1l.item[i].p0, &channel_list );
            Channel *channel = getChannelByActuatorId ( i1f1l.item[i].p0, &channel_list );
            if ( item==NULL || channel==NULL ) continue;
            if ( lockMutex ( &channel->mutex ) ) {
                if ( i1f1l.item[i].p1 >= 0.0 ) {
                    item->power = i1f1l.item[i].p1;
                } else {
                    item->power = 0.0;
                }
                unlockMutex ( &channel->mutex );
            }

        }
        return;
    }

    acp_responseSend ( &response, &peer_client );
}

#define PER_TIME(V,DTM) (V*DTM.tv_sec*1000.0 + V*DTM.tv_nsec/1000000.0)
void matter_ctrl ( Matter *item, double ambient_temperature, double heater_power, double cooler_power ) {
    struct timespec dTM=getTimePassed_ts ( item->t1 );
    double temperature_k=item->energy / ( item->ksh * item->mass );
    double ambient_temperature_k=273+ambient_temperature;
    double dE = 0.0;
    //aiming to ambient temperature
    double dTemp= ( temperature_k > ambient_temperature_k ) ? ( temperature_k - ambient_temperature_k ) : ( ambient_temperature_k - temperature_k );
    //double aE=pow((dTemp*(item->kl)), (0.2 * dTemp));
    double aE=item->kl * pow ( dTemp, item->pl );
    aE=PER_TIME ( aE, dTM );
    if ( temperature_k > ambient_temperature_k ) {
        dE -= aE;
    } else if ( temperature_k < ambient_temperature_k ) {
        dE += aE;
    }
    //actuator affect
    double actuator_power=heater_power-cooler_power;
    dE += PER_TIME ( actuator_power, dTM );
    //total affect
    item->energy += dE;
    if ( item->energy < 0.0 ) {
        item->energy = 0.0;
        dE = 0.0;
    }
    //delay for temperature
    double temperature_c=temperature_k-273;
    if ( item->temperature_pipe.max_length > 0 ) {
        pipe_move ( &item->temperature_pipe );
        pipe_push ( &item->temperature_pipe, temperature_c );
        item->temperature = pipe_pop ( &item->temperature_pipe );
    } else {
        item->temperature = temperature_c;
    }
#ifdef MODE_DEBUG
   // pipe_print ( &item->temperature_pipe );
#endif
    item->t1=getCurrentTime();
}

void channelControl ( Channel * item ) {
#ifdef MODE_DEBUG
    char *state = getStateStr ( item->state );
    printf ( "channel: id:%d state:%s temp:%.2f energy:%.2f power(h c):%.2f %.2f \n",
             item->id,
             state,
             item->matter.temperature,
             item->matter.energy,
             item->heater.power,
             item->cooler.power
           );
#endif
    switch ( item->state ) {
    case INIT:
        item->matter.temperature = item->ambient_temperature;
        item->matter.energy = ( item->matter.temperature+273.0 ) * item->matter.ksh * item->matter.mass;
        pipe_fill ( &item->matter.temperature_pipe, item->matter.temperature );
        item->state = RUN;
        break;
    case RUN:
        matter_ctrl ( &item->matter, item->ambient_temperature, item->heater.power, item->cooler.power );
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

void cleanup_handler ( void *arg ) {
    Channel *item = arg;
    printf ( "cleaning up thread %d\n", item->id );
}

void *threadFunction ( void *arg ) {
    Channel *item = arg;
    printdo ( "thread for channel with id=%d has been started\n", item->id );
#ifdef MODE_DEBUG
    pthread_cleanup_push ( cleanup_handler, item );
#endif
    while ( 1 ) {
        struct timespec t1 = getCurrentTime();
        int old_state;
        if ( threadCancelDisable ( &old_state ) ) {
            if ( lockMutex ( &item->mutex ) ) {
                channelControl ( item );
                unlockMutex ( &item->mutex );
            }
            threadSetCancelState ( old_state );
        }
        delayTsIdleRest ( item->cycle_duration, t1 );
    }
#ifdef MODE_DEBUG
    pthread_cleanup_pop ( 1 );
#endif
}

void freeData() {
    STOP_ALL_CHANNEL_THREADS ( &channel_list );
    freeChannelList ( &channel_list );
}

void freeApp() {
    freeData();
    freeSocketFd ( &sock_fd );
    freeMutex ( &channel_list_mutex );
    TSVclear ( &config_tsv );
}

void exit_nicely ( ) {
    freeApp();
    putsdo ( "\nexiting now...\n" );
    exit ( EXIT_SUCCESS );
}

int main ( int argc, char** argv ) {
#ifndef MODE_DEBUG
    daemon ( 0, 0 );
#endif
    conSig ( &exit_nicely );
    if ( mlockall ( MCL_CURRENT | MCL_FUTURE ) == -1 ) {
        perrorl ( "mlockall()" );
    }
    int data_initialized = 0;
    while ( 1 ) {
#ifdef MODE_DEBUG
        printf ( "%s(): %s %d\n",F, getAppState ( app_state ), data_initialized );
#endif
        switch ( app_state ) {
        case APP_INIT:
            if ( !initApp() ) {
                return ( EXIT_FAILURE );
            }
            app_state = APP_INIT_DATA;
            break;
        case APP_INIT_DATA:
            data_initialized = initData();
            app_state = APP_RUN;
            delayUsIdle ( 1000000 );
            break;
        case APP_RUN:
            serverRun ( &app_state, data_initialized );
            break;
        case APP_STOP:
            freeData();
            data_initialized = 0;
            app_state = APP_RUN;
            break;
        case APP_RESET:
            freeApp();
            delayUsIdle ( 1000000 );
            data_initialized = 0;
            app_state = APP_INIT;
            break;
        case APP_EXIT:
            exit_nicely();
            break;
        default:
            freeApp();
            putsde ( "unknown application state\n" );
            return ( EXIT_FAILURE );
        }
    }
    freeApp();
    putsde ( "unexpected while break\n" );
    return ( EXIT_FAILURE );
}
