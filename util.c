
#include "main.h"


void freeChannel ( Channel*item ) {
    freeMutex ( &item->mutex );
    FREE_LIST ( &item->matter.temperature_pipe )
    free ( item );
}

void freeChannelList ( ChannelLList *list ) {
    Channel *item = list->top, *temp;
    while ( item != NULL ) {
        temp = item;
        item = item->next;
        freeChannel ( temp );
    }
    list->top = NULL;
    list->last = NULL;
    list->length = 0;
}

void pipe_push ( D1List *list, double value ) {
    list->item[0] = value;
}

double pipe_pop ( D1List *list ) {
    return list->item[list->max_length - 1];
}

void pipe_move ( D1List *list ) {
    for ( int i = list->max_length - 1; i > 0; i-- ) {
        list->item[i] = list->item[i - 1];
    }
    list->item[0] = 0.0;
}
void pipe_print( D1List *list){
    for(int i=0;i<list->max_length;i++){
     printf("%.1f ", list->item[i]);   
    }
    puts("");
}

void pipe_fill( D1List *list, double value){
    for(int i=0;i<list->max_length;i++){
     list->item[i]=value;
    }
}

Actuator * getActuatorById ( int id, const ChannelLList *list ) {
    Channel *item = list->top;
    while ( item != NULL ) {
        if ( item->cooler.id == id ) {
            return &item->cooler;
        }
        if ( item->heater.id == id ) {
            return &item->heater;
        }
        item = item->next;
    }
    return NULL;
}

Channel *getChannelByActuatorId ( int id, ChannelLList *list ) {
    Channel *item = list->top;
    while ( item != NULL ) {
        if ( item->cooler.id == id || item->heater.id == id ) {
            return item;
        }
        item = item->next;
    }
    return NULL;
}

char * getStateStr ( char state ) {
    switch ( state ) {
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

int bufCatChannelRuntime ( Channel *item, ACPResponse *response ) {
    if ( lockMutex ( &item->mutex ) ) {
        char q[LINE_SIZE];
        char *state = getStateStr ( item->state );
        snprintf ( q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%s" ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_ROW_STR,
                   item->id,
                   state,
                   item->heater.power,
                   item->cooler.power,
                   item->matter.temperature
                 );
        unlockMutex ( &item->mutex );
        return acp_responseStrCat ( response, q );
    }
    return 0;
}

int bufCatChannelInit ( Channel *item, ACPResponse *response ) {
    if ( lockMutex ( &item->mutex ) ) {
        char q[LINE_SIZE];
        snprintf ( q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR FSTR ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
                   item->id,
                   item->heater.id,
                   item->cooler.id,
                   item->ambient_temperature,
                   item->matter.mass,
                   item->matter.ksh,
                   item->matter.kl,
                   item->matter.temperature_pipe.max_length
                 );
        unlockMutex ( &item->mutex );
        return acp_responseStrCat ( response, q );
    }
    return 0;
}

int bufCatChannelFTS ( Channel *item, ACPResponse *response ) {
    if ( lockMutex ( &item->mutex ) ) {
        struct timespec tm = getCurrentTime();
        int state=1;
        if ( item->state==OFF || item->state==DISABLE ) {
            state=0;
        }
        int r= acp_responseFTSCat ( item->id, item->matter.temperature, tm, state, response );
        unlockMutex ( &item->mutex );
        return r;
    }
    return 0;
}

int bufCatChannelEnabled ( Channel *item, ACPResponse *response ) {
    if ( lockMutex ( &item->mutex ) ) {
        char q[LINE_SIZE];
        int enabled = 1;
        if ( item->state == OFF ) {
            enabled = 0;
        }
        snprintf ( q, sizeof q, "%d" ACP_DELIMITER_COLUMN_STR "%d" ACP_DELIMITER_ROW_STR,
                   item->id,
                   enabled
                 );
        unlockMutex ( &item->mutex );
        return acp_responseStrCat ( response, q );
    }
    return 0;
}

void printData ( ACPResponse *response ) {
    char q[LINE_SIZE];
    snprintf ( q, sizeof q, "CONFIG_FILE: %s\n", CONFIG_FILE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "port: %d\n", sock_port );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "db_path: %s\n", db_path );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "app_state: %s\n", getAppState ( app_state ) );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "PID: %d\n", getpid() );
    SEND_STR ( q )
    SEND_STR ( "+-----------------------------------------------------------------------------------------------------------+\n" )
    SEND_STR ( "|                                     Channel initial data                                                  |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n" )
    SEND_STR ( "|    id     | heater_id | cooler_id |ambient_tem|   mass    | spec heat |loss_factor|loss_power |t_pipe_len |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n" )
    FOREACH_CHANNEL {
        snprintf ( q, sizeof q, "|%11d|%11d|%11d|%11.3f|%11.3f|%11.3f|%11.3f|%11.3f|%11d|\n",
        item->id,
        item->heater.id,
        item->cooler.id,
        item->ambient_temperature,
        item->matter.mass,
        item->matter.ksh,
        item->matter.kl,
        item->matter.pl,
        item->matter.temperature_pipe.max_length
                 );
        SEND_STR ( q )
    }
    SEND_STR ( "+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+-----------+\n" )

    SEND_STR ( "+---------------------------------------------------------------------------+\n" )
    SEND_STR ( "|                             Channel runtime data                          |\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+---------------+-----------+\n" )
    SEND_STR ( "|     id    |   state   |heater_pwr |cooler_pwr |    energy     |temperature|\n" )
    SEND_STR ( "+-----------+-----------+-----------+-----------+---------------+-----------+\n" )
    FOREACH_CHANNEL {
        char *state = getStateStr ( item->state );
        snprintf ( q, sizeof q, "|%11d|%11s|%11.3f|%11.3f|%15.3f|%11.3f|\n",
        item->id,
        state,
        item->heater.power,
        item->cooler.power,
        item->matter.energy,
        item->matter.temperature
                 );
        SEND_STR ( q )
    }
    SEND_STR_L ( "+-----------+-----------+-----------+-----------+---------------+-----------+\n" )
}

void printHelp ( ACPResponse *response ) {
    char q[LINE_SIZE];
    SEND_STR ( "COMMAND LIST\n" )
    snprintf ( q, sizeof q, "%s\tput process into active mode; process will read configuration\n", ACP_CMD_APP_START );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tput process into standby mode; all running channels will be stopped\n", ACP_CMD_APP_STOP );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tfirst stop and then start process\n", ACP_CMD_APP_RESET );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tterminate process\n", ACP_CMD_APP_EXIT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget state of process; response: B - process is in active mode, I - process is in standby mode\n", ACP_CMD_APP_PING );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget some variable's values; response will be packed into multiple packets\n", ACP_CMD_APP_PRINT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget this help; response will be packed into multiple packets\n", ACP_CMD_APP_HELP );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tload channel into RAM and start its execution; channel id expected\n", ACP_CMD_CHANNEL_START );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tunload channel from RAM; channel id expected\n", ACP_CMD_CHANNEL_STOP );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tunload channel from RAM, after that load it; channel id expected\n", ACP_CMD_CHANNEL_RESET );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tenable running channel; channel id expected\n", ACP_CMD_CHANNEL_ENABLE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tdisable running channel; channel id expected\n", ACP_CMD_CHANNEL_DISABLE );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget channel state (1-enabled, 0-disabled); channel id expected\n", ACP_CMD_CHANNEL_GET_ENABLED );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget channel sensor value; channel id expected\n", ACP_CMD_GET_FTS );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tset heater of cooler power; heater or cooler id expected\n", ACP_CMD_SET_FLOAT );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget channel runtime data; channel id expected\n", ACP_CMD_CHANNEL_GET_DATA_RUNTIME );
    SEND_STR ( q )
    snprintf ( q, sizeof q, "%s\tget channel initial data; channel id expected\n", ACP_CMD_CHANNEL_GET_DATA_INIT );
    SEND_STR_L ( q )

}
