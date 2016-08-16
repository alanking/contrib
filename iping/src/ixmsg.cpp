/*** Copyright (c), The University of North Carolina            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
/* ixmsg.c - Xmessage communicator */

#include "rodsClient.h"
#include "irods_client_api_table.hpp"
#include "irods_pack_table.hpp"
#include "irods_configuration_keywords.hpp"
#include <cctype>
rodsEnv myRodsEnv;
rErrMsg_t errMsg;
int  connectFlag = 0;

void
printIxmsgHelp( const char *cmd ) {

    printf( "Usage: %s s [-t ticketNum] [-n startingMessageNumber] [-r numOfReceivers] [-H header] [-M message] \n" , cmd );
    printf( "Usage: %s r [-n NumberOfMessages] [-t ticketNum] [-s startingSequenceNumber] [-c conditionString]\n" , cmd );
    printf( "Usage: %s t \n" , cmd );
    printf( "Usage: %s d -t ticketNum \n" , cmd );
    printf( "Usage: %s c -t ticketNum \n" , cmd );
    printf( "Usage: %s c -t ticketNum -s sequenceNum \n" , cmd );
    printf( "    s: send messages. If no ticketNum is given, 1 is used \n" );
    printf( "    r: receive messages. If no ticketNum is given, 1 is used \n" );
    printf( "    t: create new message stream and get a new ticketNum \n" );
    printf( "    d: drop message Stream \n" );
    printf( "    c: clear message Stream \n" );
    printf( "    e: erase a message \n" );
    printReleaseInfo( "ixmsg" );
}


int
sendIxmsg( rcComm_t **inconn, sendXmsgInp_t *sendXmsgInp ) {
    int status;
    int sleepSec = 1;

    rcComm_t *conn = *inconn;

    while ( connectFlag == 0 ) {
        conn = rcConnectXmsg( &myRodsEnv, &errMsg );
        if ( conn == NULL ) {
            sleep( sleepSec );
            sleepSec = 2 * sleepSec;
            if ( sleepSec > 10 ) {
                sleepSec = 10;
            }
            continue;
        }
        status = clientLogin( conn );
        if ( status != 0 ) {
            rcDisconnect( conn );
            fprintf( stderr, "clientLogin error...Will try again\n" );
            sleep( sleepSec );
            sleepSec = 2 * sleepSec;
            if ( sleepSec > 10 ) {
                sleepSec = 10;
            }
            continue;
        }
        *inconn = conn;
        connectFlag = 1;
    }
    status = rcSendXmsg( conn, sendXmsgInp );
    /*  rcDisconnect(conn); **/
    if ( status < 0 ) {
        fprintf( stderr, "rsSendXmsg error. status = %d\n", status );
        exit( 9 );
    }
    return status;
}

int
main( int argc, char **argv ) {

    signal( SIGPIPE, SIG_IGN );

    rcComm_t *conn = NULL;
    int status;
    int mNum = 0;
    int tNum = 1;
    int opt;
    int sNum = 0;
    int sleepSec = 1;
    int rNum = 1;
    char  msgBuf[4000];
    char  condStr[NAME_LEN];
    char myHostName[MAX_NAME_LEN];
    char cmd[10];

    getXmsgTicketInp_t getXmsgTicketInp;
    xmsgTicketInfo_t xmsgTicketInfo;
    xmsgTicketInfo_t *outXmsgTicketInfo;
    sendXmsgInp_t sendXmsgInp;
    rcvXmsgInp_t rcvXmsgInp;
    rcvXmsgOut_t *rcvXmsgOut = NULL;

    msgBuf[0] = '\0';
    char  msgHdr[HEADER_TYPE_LEN] = "ixmsg";
    myHostName[0] = '\0';
    condStr[0] = '\0';

    if ( argc < 2 ) {
        printIxmsgHelp( argv[0] );
        return 0;
    }

    strncpy( cmd, argv[1], 9 );
    status = getRodsEnv( &myRodsEnv );
    if ( status < 0 ) {
        fprintf( stderr, "getRodsEnv error, status = %d\n", status );
        return 1;
    }

    // DISABLE ADVANCED CLIENT-SERVER NEGOTIATION FOR XMSG CLIENT
    std::string neg_env;
    std::transform(
        std::begin(irods::CFG_IRODS_CLIENT_SERVER_NEGOTIATION_KW),
        std::end(irods::CFG_IRODS_CLIENT_SERVER_NEGOTIATION_KW),
        std::back_inserter(neg_env),
        (int (*)(int))std::toupper );

    char env_var[NAME_LEN];// = { "IRODS_CLIENT_SERVER_NEGOTIATION='NO_NEG'" };
    snprintf( env_var, sizeof( env_var ), "%s='NO_NEG'", neg_env.c_str() );
    putenv( env_var );
    // DISABLE ADVANCED CLIENT-SERVER NEGOTIATION FOR XMSG CLIENT

    while ( ( opt = getopt( argc, argv, "ht:n:r:H:M:c:s:" ) ) != EOF ) {
        switch ( opt ) {
        case 't':
            tNum = atoi( optarg );
            break;
        case 'n':
            mNum = atoi( optarg );
            break;
        case 'r':
            rNum = atoi( optarg );
            break;
        case 's':
            sNum = atoi( optarg );
            break;
        case 'H':
            strncpy( msgHdr, optarg, HEADER_TYPE_LEN - 1 );
            break;
        case 'M':
            strncpy( msgBuf, optarg, 3999 );
            break;
        case 'c':
            strncpy( condStr, optarg, NAME_LEN - 1 );
            break;
        case 'h':
            printIxmsgHelp( argv[0] );
            return 0;
            break;
        default:
            fprintf( stderr, "ixmsg: Error: Unknown Option [%d]\n", opt );
            return 1;
            break;
        }
    }

    gethostname( myHostName, MAX_NAME_LEN );
    memset( &xmsgTicketInfo, 0, sizeof( xmsgTicketInfo ) );

    if ( !strcmp( cmd, "s" ) ) {
        memset( &sendXmsgInp, 0, sizeof( sendXmsgInp ) );
        xmsgTicketInfo.sendTicket = tNum;
        xmsgTicketInfo.rcvTicket = tNum;
        xmsgTicketInfo.flag = 1;
        sendXmsgInp.ticket = xmsgTicketInfo;
        snprintf( sendXmsgInp.sendAddr, NAME_LEN, "%s:%i", myHostName, getpid() );
        sendXmsgInp.sendXmsgInfo.numRcv = rNum;
        sendXmsgInp.sendXmsgInfo.msgNumber = mNum;
        strcpy( sendXmsgInp.sendXmsgInfo.msgType, msgHdr );
        sendXmsgInp.sendXmsgInfo.msg = msgBuf;

        if ( strlen( msgBuf ) > 0 ) {
            status = sendIxmsg( &conn, &sendXmsgInp );
            if ( connectFlag == 1 ) {
                rcDisconnect( conn );
            }
            if ( status < 0 ) {
                return 8;
            }
            return 0;
        }
        printf( "Message Header : %s\n", msgHdr );
        printf( "Message Address: %s\n", sendXmsgInp.sendAddr );
        while ( fgets( msgBuf, 3999, stdin ) != NULL ) {
            if ( strstr( msgBuf, "/EOM" ) == msgBuf ) {
                if ( connectFlag == 1 ) {
                    rcDisconnect( conn );
                }
                return 0;
            }
            sendXmsgInp.sendXmsgInfo.msgNumber = mNum;
            if ( mNum != 0 ) {
                mNum++;
            }
            sendXmsgInp.sendXmsgInfo.msg = msgBuf;
            status = sendIxmsg( &conn, &sendXmsgInp );
            if ( status < 0 ) {
                if ( connectFlag == 1 ) {
                    rcDisconnect( conn );
                }
                return 8;
            }
        }
        if ( connectFlag == 1 ) {
            rcDisconnect( conn );
        }
    }
    else if ( !strcmp( cmd, "r" ) ) {
        memset( &rcvXmsgInp, 0, sizeof( rcvXmsgInp ) );
        rcvXmsgInp.rcvTicket = tNum;
        /*      rcvXmsgInp.msgNumber = mNum; */

        if ( mNum == 0 ) {
            mNum--;
        }

        while ( mNum != 0 ) {
            if ( connectFlag == 0 ) {
                conn = rcConnectXmsg( &myRodsEnv, &errMsg );
                if ( conn == NULL ) {
                    sleep( sleepSec );
                    sleepSec = 2 * sleepSec;
                    if ( sleepSec > 10 ) {
                        sleepSec = 10;
                    }
                    continue;
                }
                status = clientLogin( conn );
                if ( status != 0 ) {
                    rcDisconnect( conn );
                    sleep( sleepSec );
                    sleepSec = 2 * sleepSec;
                    if ( sleepSec > 10 ) {
                        sleepSec = 10;
                    }
                    continue;
                }
                connectFlag = 1;
            }
            if ( strlen( condStr ) > 0 ) {
                sprintf( rcvXmsgInp.msgCondition, "(*XSEQNUM  >= %d) && (%s)", sNum, condStr );
            }
            else {
                sprintf( rcvXmsgInp.msgCondition, "*XSEQNUM >= %d ", sNum );
            }

            status = rcRcvXmsg( conn, &rcvXmsgInp, &rcvXmsgOut );
            /*        rcDisconnect(conn); */
            if ( status  >= 0 ) {
                printf( "%s:%s#%i::%s: %s",
                        rcvXmsgOut->sendUserName, rcvXmsgOut->sendAddr,
                        rcvXmsgOut->seqNumber, rcvXmsgOut->msgType, rcvXmsgOut->msg );
                if ( rcvXmsgOut->msg[strlen( rcvXmsgOut->msg ) - 1] != '\n' ) {
                    printf( "\n" );
                }
                sleepSec = 1;
                mNum--;
                sNum = rcvXmsgOut->seqNumber + 1;
                free( rcvXmsgOut->msg );
                free( rcvXmsgOut );
                rcvXmsgOut = NULL;
            }
            else {
                sleep( sleepSec );
                sleepSec = 2 * sleepSec;
                if ( sleepSec > 10 ) {
                    sleepSec = 10;
                }
            }

        }
        if ( connectFlag == 1 ) {
            rcDisconnect( conn );
        }
    }
    else if ( !strcmp( cmd, "t" ) ) {
        memset( &getXmsgTicketInp, 0, sizeof( getXmsgTicketInp ) );
        getXmsgTicketInp.flag = 1;

        // =-=-=-=-=-=-=-
        // initialize pluggable api table
        irods::api_entry_table&  api_tbl = irods::get_client_api_table();
        irods::pack_entry_table& pk_tbl  = irods::get_pack_table();
        init_api_table( api_tbl, pk_tbl );

        conn = rcConnectXmsg( &myRodsEnv, &errMsg );
        if ( conn == NULL ) {
            fprintf( stderr, "rcConnect error\n" );
            return 1;
        }
        status = clientLogin( conn );
        if ( status != 0 ) {
            fprintf( stderr, "clientLogin error\n" );
            rcDisconnect( conn );
            return 7;
        }
        status = rcGetXmsgTicket( conn, &getXmsgTicketInp, &outXmsgTicketInfo );
        rcDisconnect( conn );
        if ( status != 0 ) {
            fprintf( stderr, "rcGetXmsgTicket error. status = %d\n", status );
            return 8;
        }
        printf( "Send Ticket Number= %i\n", outXmsgTicketInfo->sendTicket );
        printf( "Recv Ticket Number= %i\n", outXmsgTicketInfo->rcvTicket );
        printf( "Ticket Expiry Time= %i\n", outXmsgTicketInfo->expireTime );
        printf( "Ticket Flag       = %i\n", outXmsgTicketInfo->flag );
        free( outXmsgTicketInfo );
    }
    else if ( !strcmp( cmd, "c" ) || !strcmp( cmd, "d" ) || !strcmp( cmd, "e" ) ) {
        memset( &sendXmsgInp, 0, sizeof( sendXmsgInp ) );
        xmsgTicketInfo.sendTicket = tNum;
        xmsgTicketInfo.rcvTicket = tNum;
        xmsgTicketInfo.flag = 1;
        sendXmsgInp.ticket = xmsgTicketInfo;
        snprintf( sendXmsgInp.sendAddr, NAME_LEN, "%s:%i", myHostName, getpid() );
        sendXmsgInp.sendXmsgInfo.numRcv = rNum;
        if ( !strcmp( cmd, "e" ) ) {
            sendXmsgInp.sendXmsgInfo.msgNumber = sNum;
        }
        else {
            sendXmsgInp.sendXmsgInfo.msgNumber = mNum;
        }
        strcpy( sendXmsgInp.sendXmsgInfo.msgType, msgHdr );
        sendXmsgInp.sendXmsgInfo.msg = msgBuf;
        if ( !strcmp( cmd, "c" ) ) {
            sendXmsgInp.sendXmsgInfo.miscInfo = strdup( "CLEAR_STREAM" );
        }
        if ( !strcmp( cmd, "e" ) ) {
            sendXmsgInp.sendXmsgInfo.miscInfo = strdup( "ERASE_MESSAGE" );
        }
        else {
            sendXmsgInp.sendXmsgInfo.miscInfo = strdup( "DROP_STREAM" );
        }
        status = sendIxmsg( &conn, &sendXmsgInp );
        if ( connectFlag == 1 ) {
            rcDisconnect( conn );
        }
        if ( status < 0 ) {
            return 8;
        }
        return 0;
    }
    else {
        fprintf( stderr, "wrong option. Check with -h\n" );
        return 9;
    }

    return 0;
}
