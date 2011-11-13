/***************************************************************************
 *   Copyright (C) 2009 by M. Feser   *
 *      *
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
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <syslog.h>
#include <termio.h>
#include <termios.h>
#include <string.h>

#include "globaldef.h"
#include "yarddef.h"
#include "yardserial.h"


/*** DEFINES ***************************************************************/
#define CIRCBUF_SIZE_BYTE	100
#define RECV_TIMEOUT_US		100000		// 100 ms
#define MAX_ERR_COUNT		3


/*** PROTOTYPES ************************************************************/
static VOID yardserial_vErrorHandler(CHAR *pcErrMsg);
static BOOL yardserial_oIsReady(VOID);
static BOOL yardserial_oSetupSingleByteCmd(YARD_CMD *ptYardCmd);
static BOOL yardserial_oSetupMultiByteCmd(YARD_CMD *ptYardCmd);
static BOOL yardserial_oCheckResponse(YARD_RES *ptYardRes);
	

/*** VARIABLES *************************************************************/
static struct
{
	BYTE abData[CIRCBUF_SIZE_BYTE];
	BYTE *pbCurRead;
	BYTE *pbCurWrite;
	DWORD dwFillCnt;
} tCircBuf; 

static INT nSerFd = -1;
static FUNC_PTR pvExitHandler = NULL;


/*** FUNCTIONS *************************************************************/
static VOID yardserial_vErrorHandler(CHAR *pcMsg)
{
	CHAR acErrMsg[MAX_LOGMSG_LEN];
	
	
	// Print error
	sprintf(acErrMsg, "ERROR in yardserial: %s", pcMsg);
	syslog(LOG_ERR, acErrMsg);
	
	// Call exit handler
	pvExitHandler();
	
	// Close log and exit
	syslog(LOG_NOTICE, "Daemon terminated by error...");
	closelog();
	exit(2);
}

VOID yardserial_vOpen(CHAR *pcDevPath, DWORD dwYardBd, FUNC_PTR pvExitCallback)
{
	struct termios tSerAttr;
	YARD_CMD tYardCmd;
	YARD_RES tYardRes;
	BYTE bCmd;
	BOOL oRetVal;
	CHAR acLogMsg[MAX_LOGMSG_LEN];
	
	
	// Init exit handler function
	pvExitHandler = pvExitCallback;
	
	// Init buffer processing state machine
	yardserial_oProcessCircBuf(&tYardRes);
	
	// Open serial port
	if (pcDevPath == NULL)
	{
		yardserial_vErrorHandler("Device path not valid !");
	}
	nSerFd = open(pcDevPath, O_RDWR | O_NOCTTY | O_NDELAY);
	if (nSerFd < 0)
	{
		yardserial_vErrorHandler("Can't open serial port !");
	}

	// Set 8N1 mode
	if (tcgetattr(nSerFd, &tSerAttr) < 0) 
	{
		yardserial_vErrorHandler("Can't get serial port attributes !");
	}
	tSerAttr.c_cflag = (CS8 | CREAD | CLOCAL);
	tSerAttr.c_iflag = 0;
	tSerAttr.c_oflag = 0;
	tSerAttr.c_lflag = 0; 
	
	// Set baudrate
	switch (dwYardBd)
	{
		case 9600:
			cfsetspeed(&tSerAttr, B9600);
			break;
		
		case 19200:
			cfsetspeed(&tSerAttr, B19200);
			break;
		
		case 57600:
			cfsetspeed(&tSerAttr, B57600);
			break;
			
		default:
			yardserial_vErrorHandler("YARD baudrate not supported !");
			break;
	}
	
	// Set attributes
	if (tcsetattr(nSerFd, TCSANOW, &tSerAttr) < 0) 
	{
		yardserial_vErrorHandler("Can't set serial attributes !");
	}
	
	// Flush pending data in RX and TX buffer
	tcflush(nSerFd, TCIOFLUSH);
}

VOID yardserial_vClose(VOID)
{
	// Close serial device
	if (nSerFd > 0)
	{
		close(nSerFd);
		nSerFd = -1;
	}
}

static BOOL yardserial_oIsReady(VOID)
{
	INT nFlags;
	
	
	// Error check
	if (nSerFd < 0)
	{
		return FALSE;
	}

	// Check if CTS is set
	nFlags = 0;
	ioctl(nSerFd, TIOCMGET, &nFlags);
	if (nFlags & TIOCM_CTS)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

BOOL yardserial_oUpdateCircBuf(VOID)
{
	DWORD dwByteCntAvl;
	WORD wBufSpace, wBufSpaceEnd;
	INT nByteCntRd;
	
	
	// Error check
	if (nSerFd < 0)
	{
		return FALSE;
	}
	
	// Get space left in circular buffer
	wBufSpaceEnd = (WORD)( &( tCircBuf.abData[CIRCBUF_SIZE_BYTE-1] ) - tCircBuf.pbCurWrite + 1);
	wBufSpace = CIRCBUF_SIZE_BYTE - tCircBuf.dwFillCnt;
	
	// Get number of bytes waiting in RX buffer
	ioctl(nSerFd, FIONREAD, &dwByteCntAvl);
	if (dwByteCntAvl > 0)
	{
		// Prevent overwriting of unread data
		if (dwByteCntAvl > wBufSpace)
		{
			dwByteCntAvl = wBufSpace;
		}
		tCircBuf.dwFillCnt += dwByteCntAvl;
		
		
		// Wrap around in circular buffer if necessary
		if (dwByteCntAvl > wBufSpaceEnd)
		{
			nByteCntRd = read(nSerFd, tCircBuf.pbCurWrite, wBufSpaceEnd);
			if ( (nByteCntRd < 0) || (nByteCntRd != wBufSpaceEnd) )
			{
				yardserial_vErrorHandler("Can't read from serial port !");
			}
			tCircBuf.pbCurWrite = tCircBuf.abData;
			dwByteCntAvl -= wBufSpaceEnd;
		}
		
		// Read bytes into circular buffer and update write address
		nByteCntRd = read(nSerFd, tCircBuf.pbCurWrite, dwByteCntAvl);
		if ( (nByteCntRd < 0) || (nByteCntRd != dwByteCntAvl) )
		{
			yardserial_vErrorHandler("Can't read from serial port !");
		}
		tCircBuf.pbCurWrite += dwByteCntAvl;
	}
	
	return TRUE;
}

BOOL yardserial_oProcessCircBuf(YARD_RES *ptYardRes)
{
	static enum
	{
		BUFPROCSTATE_INITWAIT,
		BUFPROCSTATE_LOCKED,
  		BUFPROCSTATE_WAIT,
  		BUFPROCSTATE_RESET
	} eBufprocState = BUFPROCSTATE_RESET;
	
	static BYTE bYardResLen = 0;
	static BYTE bErrCnt = 0;
	static struct timeval tStartTime;
	WORD wBufSpaceEnd, wFragCnt;
	YARD_RES *ptTmpYardRes;
	YARD_RES tFragRes;
	BOOL oRetVal = FALSE;
	
	
	switch (eBufprocState)
	{
		case BUFPROCSTATE_INITWAIT:
		{
			if (tCircBuf.dwFillCnt > 2)
			{
				// Switch to locked state
				eBufprocState = BUFPROCSTATE_LOCKED;
			}
			else
			{
				break;
			}
		}
		
		case BUFPROCSTATE_LOCKED:
		{
			// Check for empty buffer
			if (tCircBuf.dwFillCnt > 0)
			{
				wBufSpaceEnd = (WORD)( &( tCircBuf.abData[CIRCBUF_SIZE_BYTE-1] ) - tCircBuf.pbCurRead + 1);
	
				// Set up YARD response pointer
				if (wBufSpaceEnd < MAX_YARDMSG_LEN)
				{
					// Concatenate fragmented YARD response data
					wFragCnt = (MAX_YARDMSG_LEN - wBufSpaceEnd);
					memcpy( (BYTE *)&tFragRes, tCircBuf.pbCurRead, wBufSpaceEnd);
					memcpy( ( (BYTE *)&tFragRes + wBufSpaceEnd), tCircBuf.abData, wFragCnt);
					ptTmpYardRes = &tFragRes;
				}
				else
				{
					ptTmpYardRes = (YARD_RES *)(tCircBuf.pbCurRead);
				}
				
				// Check if YARD response is complete
				bYardResLen = ( (ptTmpYardRes->tRaw.bLen & 0x7F) + 2 );
				if (bYardResLen <= tCircBuf.dwFillCnt)
				{
					// Check YARD response
					if ( (bYardResLen <= MAX_YARDMSG_LEN) && yardserial_oCheckResponse(ptTmpYardRes) )
					{
						// Response valid, copy data and stop buffer processing
						memcpy( (BYTE *)ptYardRes, (BYTE *)ptTmpYardRes, bYardResLen);
						oRetVal = TRUE;
					}
					else
					{
						// Response invalid, increment error counter and switch to reset state if necessary
						bErrCnt++;
						if (bErrCnt > MAX_ERR_COUNT)
						{
							eBufprocState = BUFPROCSTATE_RESET;
						}
					}
					
					// Update read address and update fill counter
					if (wBufSpaceEnd <= bYardResLen)
					{
						// Wrap around in circular buffer
						tCircBuf.pbCurRead = (tCircBuf.abData + bYardResLen - wBufSpaceEnd);
					}
					else
					{
						tCircBuf.pbCurRead += bYardResLen;
					}	
					tCircBuf.dwFillCnt -= bYardResLen;
				}
				else
				{
					// Response incomplete, switch to wait state
					gettimeofday(&tStartTime, NULL);
					eBufprocState = BUFPROCSTATE_WAIT;
				}
			}
			else
			{
				eBufprocState = BUFPROCSTATE_INITWAIT;	
			}
			
			break;
		}
		
		case BUFPROCSTATE_WAIT:
		{
			// Check for timeout
			if ( helpers_oMonitorTimeoutUs(&tStartTime, RECV_TIMEOUT_US) )
			{
				// Timeout, switch to reset state
				eBufprocState = BUFPROCSTATE_RESET;
			}
			else
			{
				if (tCircBuf.dwFillCnt >= bYardResLen)
				{
					// Response complete, switch to locked state
					eBufprocState = BUFPROCSTATE_LOCKED;
				}
			}
	
			break;
		}
		
		case BUFPROCSTATE_RESET:
		default:
		{
			// Clear buffer and reset buffer addresses
			memset(tCircBuf.abData, 0x00, CIRCBUF_SIZE_BYTE);
			tCircBuf.pbCurWrite = tCircBuf.abData;
			tCircBuf.pbCurRead = tCircBuf.abData;
			tCircBuf.dwFillCnt = 0;
			
			// Reset static variables
			bYardResLen = 0;
			bErrCnt = 0;
			
			// Switch to locked state
			eBufprocState = BUFPROCSTATE_LOCKED;
			
			break;
		}
	}
		
	return oRetVal;
}			

static BOOL yardserial_oSetupSingleByteCmd(YARD_CMD *ptYardCmd)
{
	BYTE i, bBitCnt;
	BYTE bCode;
	BYTE bTxCount;
	
	
	// TBD: Error check
	
	// Set checksum field
	bCode = ptYardCmd->tSB.bCode;
	bCode |= 0x38;
	bCode &= 0x3F;
	ptYardCmd->tSB.bChSum = ptYardCmd->tSB.bCode;
  
	// Set parity
	bBitCnt = 0;
	for (i=0; i<6; i++)
	{
		if ( (bCode >> i) & 0x01 )
		{
			bBitCnt++;
		}
	}
	if (bBitCnt & 0x01)
	{
		ptYardCmd->tSB.bCode |= 0xC0;
	}
	else
	{
		ptYardCmd->tSB.bCode |= 0x80;
	}
	
	return TRUE;
}

static BOOL yardserial_oSetupMultiByteCmd(YARD_CMD *ptYardCmd)
{
	BYTE i, bBitCnt;
	BYTE bTxCount;
	BYTE bChsum = 0;
	
	
	// Error check
	if ( (ptYardCmd->tMB.bLen > 30) || (ptYardCmd->tMB.bLen < 2) )
	{
		return FALSE;
	}

	// Calc checksum
	ptYardCmd->tMB.bCode &= 0x3F;
	for (i=0; i<(ptYardCmd->tMB.bLen-1); i++)
	{
		bChsum += ptYardCmd->tMB.abData[i];
	}
	bChsum += ptYardCmd->tMB.bCode + (ptYardCmd->tMB.bLen & 0x7F);
	ptYardCmd->tMB.abData[ptYardCmd->tMB.bLen-1] = bChsum;
	
	// Set parity of cmd field
	bBitCnt = 0;
	for (i=0; i<6; i++)
	{
		if ( (ptYardCmd->tMB.bCode >> i) & 0x01 )
		{
			bBitCnt++;
		}
	}
	if (bBitCnt & 0x01)
	{
		ptYardCmd->tMB.bCode |= 0xC0;
	}
	else
	{
		ptYardCmd->tMB.bCode |= 0x80;
	}
  
	// Set parity of length field
	bBitCnt = 0;
	for (i=0; i<7; i++)
	{
		if ( (ptYardCmd->tMB.bLen >> i) & 0x01 )
		{
			bBitCnt++;
		}
	}
	if (bBitCnt & 1)
	{
		ptYardCmd->tMB.bLen |= 0x80;
	}
	
	return TRUE;
}

static BOOL yardserial_oCheckResponse(YARD_RES *ptYardRes)
{
	BYTE i, bBitCnt;
	BYTE bTmp, bPar, bLen;
	BYTE bChsum;

	
	// Check command field
	if ( !(ptYardRes->tRaw.bCode & 0x80) )
	{
		return FALSE;
	}
	bTmp = (ptYardRes->tRaw.bCode & 0x3F);
	bBitCnt = 0;
	while (bTmp) 
	{
		if (bTmp & 0x01)
		{
			bBitCnt++;
		}
		bTmp >>= 1;
	}
	bPar = ( (bBitCnt & 1) << 6 );
	bTmp = (ptYardRes->tRaw.bCode & 0x40);
	if (bTmp ^ bPar)
	{
		return FALSE;
	}
	
	// Check length field
	bTmp = (ptYardRes->tRaw.bLen & 0x7F);
	if ( (bTmp > (MAX_YARDMSG_LEN-2) ) || (bTmp < (MIN_YARDMSG_LEN-2) ) )
	{
		return FALSE;
	}
	bLen = bTmp;
	
	bBitCnt = 0;
	while (bTmp)
	{
		if (bTmp & 0x01)
		{
			bBitCnt++;
		}
		bTmp >>= 1;
	}
	bPar = ( (bBitCnt & 1) << 7 );
	bTmp = (ptYardRes->tRaw.bLen & 0x80);
	if (bTmp ^ bPar)
	{
		return FALSE;
	}
	
	// Check data
	bChsum = 0;
	for (i=0; i<(bLen-1); i++)
	{
		bChsum += ptYardRes->tRaw.abData[i];
	}
	bChsum += (ptYardRes->tRaw.bCode & 0x3F) + bLen;
	if ( bChsum != ptYardRes->tRaw.abData[bLen-1] )
	{
		return FALSE;
	}

	return TRUE;
}

BOOL yardserial_oSendCmd(YARD_CMD *ptYardCmd, BOOL oSByte)
{
	BYTE bCmdLen, bResLen;
	BYTE bByteCnt;
	BOOL oRetVal = FALSE;
	
	
	// Error check
	if ( (nSerFd < 0) || !yardserial_oIsReady() )
	{
		return FALSE;
	}
	
	// Check for command type
	if (oSByte)
	{
		// Single byte command
		bCmdLen = 2;
		oRetVal = yardserial_oSetupSingleByteCmd(ptYardCmd);
	}
	else
	{
		// Multi byte commmand
		bCmdLen = ptYardCmd->tMB.bLen + 2;
		oRetVal = yardserial_oSetupMultiByteCmd(ptYardCmd);
	}
	
	if (oRetVal)
	{		
		// Send command to YARD
		if (write(nSerFd, ptYardCmd, bCmdLen) < 0)
		{
			yardserial_vErrorHandler("Can't write to serial port !");
		}
		tcdrain(nSerFd);
	}
	
	return oRetVal;
}

BOOL yardserial_oSendCmdBlocking(YARD_CMD *ptYardCmd, BOOL oSByte, YARD_RES *ptYardRes, DWORD dwTimeout)
{
	struct timeval tSendTime;
	BOOL oTimeout, oRes;
	BYTE bYardResLen;
	
	// Send command to YARD
	oRes = yardserial_oSendCmd(ptYardCmd, oSByte);
	if (oRes)
	{
		// Monitor response timeout
		gettimeofday(&tSendTime, NULL);
		oTimeout = FALSE;
		
		// Wait for response
		while (1)
		{
			if ( yardserial_oUpdateCircBuf() )
			{
				if ( yardserial_oProcessCircBuf(ptYardRes) && ( (ptYardRes->tRaw.bCode & 0x3F) != YRC_IRCODE ) )
				{
					oRes = TRUE;
					break;
				}
			}
			
			if ( helpers_oMonitorTimeoutUs(&tSendTime, dwTimeout) )
			{
				oRes = FALSE;
				break;
			}
		}
	}
	
	return oRes;
}