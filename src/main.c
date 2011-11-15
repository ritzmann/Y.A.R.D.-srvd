/***************************************************************************
 *   Copyright (C) 2009 by M. Feser                                        *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

/*** INCLUDES **************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "globaldef.h"
#include "yarddef.h"
#include "yardserial.h"
#include "helpers.h"
#include "settings.h"


/*** DEFINES ***************************************************************/
#define PID_FILE           "/var/run/yardsrvd.pid"
#define PERM_MASK_PID      0644
#define SRVCMD_IRREG       0xEE
#define SRVCMD_IRUNREG     0xDD
#define YARDRES_TIMEOUT_US 500000          // 500 ms
#define MAIN_CYCLE_US      100                     // 100 us
#define MIN_FW_VERSION     4

// Comment out for debugging only !!!
#define DAEMONIZE


/*** TYPEDEFINITIONS *******************************************************/
typedef struct
{
    INT nSockFd;
    BOOL oIrNotif;
    BOOL oDispatched;
    struct CLI_DESC *ptNext;
} CLI_DESC;


/*** PROTOTYPES ************************************************************/
static VOID main_vErrorHandler(CHAR *pcMsg);
static VOID main_vSigTermHandler(VOID);
static VOID main_vStartDaemon(VOID);
static VOID main_vExitDaemon(VOID);
static VOID main_vDispatchYardCom(VOID);
static VOID main_vBroadcastIrCode(BYTE *pbData, WORD wLen);
static VOID main_vUpdateYardTime(VOID);


/*** VARIABLES *************************************************************/
static CLI_DESC *ptTailDesc = NULL;
static INT nMainSockFd;
static YARDSRV_CONFIG *ptConfig = NULL;


/*** FUNCTIONS *************************************************************/
static VOID main_vErrorHandler(CHAR *pcMsg)
{
    CHAR acErrMsg[MAX_LOGMSG_LEN];


    // Print error
    sprintf(acErrMsg, "ERROR in yardsrv: %s", pcMsg);
    syslog(LOG_ERR, acErrMsg);

    // Call exit function
    main_vExitDaemon();

    // Close log and exit
    syslog(LOG_NOTICE, "Daemon terminated by error...");
    closelog();
    exit(1);
}

static VOID main_vSigTermHandler(VOID)
{
    // Update YARD time
    main_vUpdateYardTime();

    // Call exit function
    main_vExitDaemon();

    // Close log and exit
    syslog(LOG_NOTICE, "Daemon terminated by SIGTERM...");
    closelog();
    exit(0);
}

static VOID main_vStartDaemon(VOID)
{
    DWORD dwIdx;
    pid_t tPid;
    INT nFd, nPidFd;


    // Exit parent process
    tPid = fork();
    if (tPid != 0)
    {
        exit(1);
    }

    // Make child session leader
    if (setsid() < 0)
    {
        exit(1);
    }

    // Ignore signal SIGHUP and terminate child
    signal(SIGHUP, SIG_IGN);
    tPid = fork();
    if (tPid != 0)
    {
        exit(1);
    }

    // Set working directory and clear mask
    chdir("/");
    umask(0);

    // Close open file descriptors
    for (dwIdx = sysconf(_SC_OPEN_MAX); dwIdx > 0; dwIdx--)
    {
        close(dwIdx);
    }

    // Create PID file
    nFd = open(PID_FILE, O_RDWR | O_CREAT, PERM_MASK_PID);
    nPidFd = fdopen(nFd,"r+");
    if ( (nFd < 0) || (nPidFd == NULL) )
    {
        main_vErrorHandler("Can't open or create PID file !");
    }
    fprintf(nPidFd, "%d\n", getpid() );
    fflush(nPidFd);
    close(nPidFd);
}

static VOID main_vExitDaemon(VOID)
{
    CLI_DESC *ptCurDesc = ptTailDesc;


    // Close all open file descriptors
    do
    {
        if (ptCurDesc != NULL) {
            close(ptCurDesc->nSockFd);
            ptCurDesc = ptCurDesc->ptNext;
        }
    } while (ptCurDesc != ptTailDesc);

    // Close and unlink socket
    close(nMainSockFd);
    unlink(ptConfig->acSock);

    // Close serial port
    yardserial_vClose();

    // Unlink PID file
    unlink(PID_FILE);
}

static VOID main_vDispatchYardCom(VOID)
{
    static BOOL oCmdProcsd = TRUE;
    static pid_t tPid;
    static INT nMsgQId;
    static struct timeval tSendTime;
    YARD_RES tYardRes;
    INT nByteCnt;
    BOOL oSByteCmd;
    BYTE bSrvCmd, bSrvRes, bYardResLen;
    BYTE abSockRxBuf[sizeof(YARD_CMD)];
    CLI_DESC *ptTmpDesc = ptTailDesc->ptNext;
    CLI_DESC *ptPrevDesc = NULL;
    static CLI_DESC *ptActDesc = NULL;


    while (oCmdProcsd && (ptTmpDesc != NULL) )
    {
        // Check if client has been dispatched alredy
        if (ptTmpDesc->oDispatched)
        {
            ptTmpDesc->oDispatched = FALSE;
        }
        else
        {
            // Poll socket for server / YARD command
            nByteCnt = recv(ptTmpDesc->nSockFd, abSockRxBuf, sizeof(abSockRxBuf), MSG_DONTWAIT);
            if ( (nByteCnt > 1) || ( (nByteCnt == 1) && (abSockRxBuf[0] < 0x80) ) )
            {
                // Get YARD command type
                oSByteCmd = (nByteCnt == 1) ? TRUE : FALSE;

                // Send command to YARD
                if ( yardserial_oSendCmd( (YARD_CMD *)abSockRxBuf, oSByteCmd) )
                {
                    // Monitor response timeout
                    gettimeofday(&tSendTime, NULL);
                    oCmdProcsd = FALSE;
                    ptTmpDesc->oDispatched = TRUE;
                    ptActDesc = ptTmpDesc;
                }
                else
                {
                    // No success
                    syslog(LOG_NOTICE, "Can't send command to YARD !");
                    bSrvRes = 0xFF;

                    if (write(ptTmpDesc->nSockFd, &bSrvRes, 1) < 0)
                    {
                        main_vErrorHandler("Dispatcher can't write to socket !");
                    }
                }
            }
            else if (nByteCnt == 1)
            {
                // Server command
                bSrvCmd = abSockRxBuf[0];
                switch (bSrvCmd)
                {
                case SRVCMD_IRREG:
                {
                    // Register IR code notification
                    syslog(LOG_NOTICE, "Client registered IR code notification...");
                    ptTmpDesc->oIrNotif = TRUE;
                    bSrvRes = 0x00;
                    break;
                }
                case SRVCMD_IRUNREG:
                {
                    // Unregister IR code notification
                    syslog(LOG_NOTICE, "Client unregistered IR code notification...");
                    ptTmpDesc->oIrNotif = FALSE;
                    bSrvRes = 0x00;
                    break;
                }
                default:
                {
                    // Unknown command
                    bSrvRes = 0xFF;
                    syslog(LOG_NOTICE, "Received an unknown command !");
                    break;
                }
                }

                // Send server response
                if (write(ptTmpDesc->nSockFd, &bSrvRes, 1) < 0)
                {
                    main_vErrorHandler("Dispatcher can't write to socket !");
                }
            }
            else if (nByteCnt == 0)
            {
                // Connection closed
                syslog(LOG_NOTICE, "Client exited...");

                // Find previous descriptor in chain
                ptPrevDesc = ptTailDesc;
                do
                {
                    ptPrevDesc = ptPrevDesc->ptNext;
                } while (ptPrevDesc->ptNext != ptTmpDesc);

                // Check if only one descriptor is left
                if (ptPrevDesc == ptTmpDesc)
                {
                    ptTailDesc = NULL;
                }
                else
                {
                    ptPrevDesc->ptNext = ptTmpDesc->ptNext;

                    // Check for deletion of tail
                    if (ptTmpDesc == ptTailDesc)
                    {
                        ptTailDesc = ptPrevDesc;
                    }
                }

                // Free memory and stop processing
                free(ptTmpDesc);
                break;
            }
        }

        // Stop chain processing if necessary
        if (ptTmpDesc != ptTailDesc)
        {
            ptTmpDesc = ptTmpDesc->ptNext;
        }
        else
        {
            break;
        }
    }

    // Update circular read buffer
    if ( !yardserial_oUpdateCircBuf() )
    {
        main_vErrorHandler("Dispatcher can't update circular read buffer !");
    }

    // Process circular read buffer
    if ( yardserial_oProcessCircBuf(&tYardRes) )
    {
        bYardResLen = ( (tYardRes.tRaw.bLen & 0x7F) + 2 );

        // Check for IR code
        if ( (tYardRes.tRaw.bCode & 0x3F) == YRC_IRCODE )
        {
            main_vBroadcastIrCode( (BYTE *)&tYardRes, bYardResLen);
        }
        else
        {
            if (write(ptActDesc->nSockFd, (BYTE *)&tYardRes, bYardResLen) < 0)
            {
                main_vErrorHandler("Dispatcher can't write to socket !");
            }
            oCmdProcsd = TRUE;
        }
    }

    // Check for communication timeout
    if ( (oCmdProcsd == FALSE) && helpers_oMonitorTimeoutUs(&tSendTime, YARDRES_TIMEOUT_US) )
    {
        // Timeout occured
        syslog(LOG_NOTICE, "YARD response timeout !");
        bSrvRes = 0xFF;

        if (write(ptActDesc->nSockFd, &bSrvRes, 1) < 0)
        {
            main_vErrorHandler("Dispatcher can't write to socket !");
        }
        oCmdProcsd = TRUE;
    }
}

static VOID main_vBroadcastIrCode(BYTE *pbData, WORD wLen)
{
    CLI_DESC *ptCurDesc = ptTailDesc;


    do
    {
        // Send IR code to client if notification is enabled
        if (ptCurDesc->oIrNotif)
        {
            if (write(ptCurDesc->nSockFd, pbData, wLen) < 0)
            {
                main_vErrorHandler("Dispatcher can't write to socket !");
            }
        }

        // Jump to next descriptor
        ptCurDesc = ptCurDesc->ptNext;
    } while (ptCurDesc != ptTailDesc);
}

static VOID main_vUpdateYardTime(VOID)
{
    YARD_CMD tYardCmd;
    YARD_RES tYardRes;
    DWORD dwYardTime;


    // Get current time
    dwYardTime = helpers_dwGetCurTime();

    // Send set time command to YARD
    tYardCmd.tMB.bCode = YCMD_SETTIME;
    tYardCmd.tMB.bLen = 5;
    memcpy(tYardCmd.tMB.abData, (BYTE *)&dwYardTime, 4);
    if ( yardserial_oSendCmdBlocking(&tYardCmd, FALSE, &tYardRes, YARDRES_TIMEOUT_US) && (tYardRes.tError.bStatus == 0x00) )
    {
        syslog(LOG_NOTICE, "YARD time updated...");
    }
}


INT main(INT argc, CHAR *argv[])
{
    INT nNewSockFd, nTmpSockFd;
    WORD wSrvAddrLen, wCliAddrLen;
    struct sockaddr_un tSrvAddr, tCliAddr;
    CHAR acLogMsg[MAX_LOGMSG_LEN];
    YARD_CMD tYardCmd;
    YARD_RES tYardRes;
    BYTE bIdx, bFwVers, bReas;
    pid_t tPid;
    DWORD dwCurTime, dwYardTime;
    LONG lTimeDiff;
    CLI_DESC *ptNewDesc = NULL;


    // Open log and start daemon
    openlog("yardsrvd", LOG_PID, LOG_USER);
#ifdef DAEMONIZE
    main_vStartDaemon();
    syslog(LOG_ERR, "Daemon started...");
#endif

    // Register SIGTERM handler function
    signal(SIGTERM, main_vSigTermHandler);

    // Parse config file and create socket
    if ( !settings_oParseCfgFile() )
    {
        main_vErrorHandler("Can't parse config file !");
    }
    ptConfig = settings_ptGetCfg();
    nMainSockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (nMainSockFd < 0)
    {
        main_vErrorHandler("Can't create socket !");
    }

    // Setup server address
    bzero( (CHAR *) &tSrvAddr, sizeof(struct sockaddr_un) );
    tSrvAddr.sun_family = AF_UNIX;
    strcpy(tSrvAddr.sun_path, ptConfig->acSock);
    wSrvAddrLen = strlen(tSrvAddr.sun_path) + sizeof(tSrvAddr.sun_family);

    // Unlink socket if it already exists and bind it
    unlink(ptConfig->acSock);
    if (bind(nMainSockFd, (struct sockaddr *)&tSrvAddr, wSrvAddrLen) < 0)
    {
        main_vErrorHandler("Can't bind socket !");
    }

    // Set non blocking mode for socket
    nTmpSockFd = fcntl(nMainSockFd, F_GETFL);
    nTmpSockFd |= O_NONBLOCK;
    fcntl(nMainSockFd, F_SETFL, nTmpSockFd);

    // Open serial port and connect to YARD
    yardserial_vOpen(ptConfig->acPort, ptConfig->dwBaud, main_vExitDaemon);
    tYardCmd.tSB.bCode = YCMD_GETFWVERSION;
    if ( yardserial_oSendCmdBlocking(&tYardCmd, TRUE, &tYardRes, YARDRES_TIMEOUT_US) &&
            ( (tYardRes.tRaw.bCode & 0x3F) == YRC_GETFWVERSION) )
    {
        // Check firmware version
        bFwVers = tYardRes.tVersion.bVers;
        if (bFwVers < MIN_FW_VERSION)
        {
            sprintf(acLogMsg, "Unsupported YARD firmware version: %i (Expected version > %i)\n", bFwVers, MIN_FW_VERSION);
            main_vErrorHandler(acLogMsg);
        }
        else
        {
            sprintf(acLogMsg, "Connected to YARD!\n Firmware version: %i\n", bFwVers);
            syslog(LOG_NOTICE, acLogMsg);
        }
    }
    else
    {
        main_vErrorHandler("Can't connect to YARD !");
    }

    // Listen for clients
    wCliAddrLen = sizeof(struct sockaddr_un);
    listen(nMainSockFd, 5);

    while (1)
    {
        // Accept client connections in polled mode
        nNewSockFd = accept(nMainSockFd, (struct sockaddr *) &tCliAddr, &wCliAddrLen);
        if (nNewSockFd > 0)
        {
            syslog(LOG_NOTICE, "Accepted new client...");

            // Create new client descriptor
            ptNewDesc = malloc( sizeof(CLI_DESC) );
            if (ptNewDesc == NULL)
            {
                // Error
                main_vErrorHandler("Can't alloc memory for new client descriptor !");
            }
            ptNewDesc->nSockFd = nNewSockFd;
            ptNewDesc->oIrNotif = FALSE;
            ptNewDesc->oDispatched = FALSE;

            // Add descriptor to chain
            if (ptTailDesc == NULL)
            {
                // Create single element chain
                ptNewDesc->ptNext = ptNewDesc;
            }
            else
            {
                // Link new element into existing chain
                ptNewDesc->ptNext = ptTailDesc->ptNext;
                ptTailDesc->ptNext = ptNewDesc;
            }
            ptTailDesc = ptNewDesc;
        }

        // Call YARD communication dispatcher
        if (ptTailDesc != NULL)
        {
            main_vDispatchYardCom();
        }

        // Sleep for MAIN_CYCLE_US microseconds
        usleep(MAIN_CYCLE_US);
    }

    return 0;
}
