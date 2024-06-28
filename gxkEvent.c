/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        gxkEvent
* Type:        C Source
* File:        %M%
* Version:     %I%
* Description: Event Services Interface
*
* Interface (public) Routines:
*
*	ev_receive
*	ev_send
*
*	gxk_ev_init
*
* Private Functions:
*
*	gxkTmp
*
* Modification History:
* ----------------------------------------------------------- 
* Date		Initials		Change Description
* -----------------------------------------------------------
* 7/15/00	GVH				Created
*
**************************************END***************************************/

#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <process.h>
#include "gxkernel.h"
#include "gxkCfg.h"

extern ULONG gxk_t_getTid(unsigned threadid, ULONG *tid);

/********************************
		LOCAL DECLARATIONS
********************************/

typedef struct
{
	ULONG evWait;
	ULONG evPend;
	UINT condition;
	HANDLE gxkEvent;
} EVDESC;

/********************************
		GLOBALS
********************************/

EVDESC EvTable[MAX_TASK];

/******************************************************************************
*						  
* Name:				ev_receive
*
* Type:				Function
*
* Description:		
* 
* Formal Inputs:	
*
* Global Inputs:	
*
* Formal Outputs:	
*
* Return Value:		
*
* Side Effects:		
* 
* Author:			GVH
*
******************************************************************************/
		  
ULONG ev_receive(ULONG events, ULONG flags, ULONG timeout, ULONG *events_r)

{
	ULONG rtn;
	unsigned threadid;
	ULONG tid;
	DWORD msecTout;

	threadid =  GetCurrentThreadId ();
	
	if (gxk_t_getTid (threadid, &tid) == 0)
	{
		EvTable[tid].evWait = events;

		/*
		 * check for requested events already pending
		 */

		if ((((EvTable[tid].evPend & EvTable[tid].evWait) == EvTable[tid].evWait) &&
			                                                (EvTable[tid].condition == EV_ALL)) ||
			((EvTable[tid].evPend & EvTable[tid].evWait) && (EvTable[tid].condition == EV_ANY)))
		{
			*events_r = EvTable[tid].evPend;
			EvTable[tid].evPend = EvTable[tid].evWait = 0;
			rtn = 0;
		}
		else
		{
			/*
			 * requested event(s) not pending
			 * request win32 wait according to conditions
			 */

			if (flags & EV_NOWAIT)
			{
				*events_r = 0;
				rtn = ERR_NOEVS;
			}
			else
			{
				/*
				 * set wait condtions
				 */
				
				EvTable[tid].evWait = events;
				EvTable[tid].condition = (flags & EV_ANY);

				/*
				 * convert timeout to win32 time value (milliseconds)
				 */

				msecTout = (timeout == 0) ? INFINITE : (DWORD)(timeout / 10);

				/*
				 * wait
				 */

				if (WaitForSingleObject (EvTable[tid].gxkEvent, msecTout) == WAIT_TIMEOUT)
				{
					rtn = ERR_TIMEOUT;
				}
			}
		}
	}
	else
	{
		rtn = ERR_OBJID;
	}

	return (rtn);
}

/******************************************************************************
*						  
* Name:				ev_send
*
* Type:				Function
*
* Description:		
* 
* Formal Inputs:	
*
* Global Inputs:	
*
* Formal Outputs:	
*
* Return Value:		
*
* Side Effects:		
* 
* Author:			GVH
*
******************************************************************************/

ULONG ev_send(ULONG tid, ULONG events)

{
	ULONG rtn;

	rtn = 0;
	
	if (tid < MAX_TASK)
	{
		/*
		 * add new events to those currently pending
		 */

		EvTable[tid].evPend |= events;

		/*
		 * signal waiting task, depending on wait condition
		 */

		if (EvTable[tid].condition == EV_ALL)
		{
			/*
			 * wait for ALL
			 */

			if ((EvTable[tid].evPend & EvTable[tid].evWait) == EvTable[tid].evWait)
			{
				SetEvent (EvTable[tid].gxkEvent);
			}
		}
		else
		{
			/*
			 * wait for ANY
			 */

			if (EvTable[tid].evPend & EvTable[tid].evWait)
			{
				SetEvent (EvTable[tid].gxkEvent);
			}
		}
	}
	else
	{
		rtn = ERR_OBJID;
	}
	
	return (rtn);
}

/******************************************************************************
*						  
* Name:				gxk_ev_init
*
* Type:				Function
*
* Description:		
* 
* Formal Inputs:	
*
* Global Inputs:	
*
* Formal Outputs:	
*
* Return Value:		
*
* Side Effects:		
* 
* Author:			GVH
*
******************************************************************************/

ULONG gxk_ev_init()

{
	UINT inx;

	/*
	 * create the gxk event for each task
	 */

	for (inx = 0; inx < MAX_TASK; inx++)
	{
		EvTable[inx].evWait = 0;
		EvTable[inx].evPend = 0;
		EvTable[inx].condition = 0;
		
		EvTable[inx].gxkEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	}

	return (0);
}
