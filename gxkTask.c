/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        gxkTask
* Type:        C Source
* File:        %M%
* Version:     %I%
* Description: Task Management Interface
*
* Interface (public) Routines:
*
*	t_create
*	t_delete
*	t_getreg
*	t_ident
*	t_mode
*	t_restart
*	t_resume
*	t_setpri
*	t_setreg
*	t_start
*	t_suspend
*
*	gxk_t_getHandle
*	gxk_t_getTid
*	gxk_t_init
*
* Private Functions:
*
*	clear_gxktcb
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

/********************************
		LOCAL DECLARATIONS
********************************/

#define MIN_PRIO			1
#define MAX_PRIO			256

#define REG_CNT				7

#define TS_DEAD				0					/* task states */
#define TS_CREATED			1
#define TS_RUNNING			2
#define TS_SUSPEND			3

typedef struct
{
	char name[4];
	ULONG prio;
	ULONG sstacksize;
	ULONG ustacksize;
	ULONG flags;
	ULONG reg[REG_CNT];
	ULONG mode;
	void *start_addr;
	
	HANDLE w32id;
	unsigned threadid;

	UINT state;
} GXKTCB;

/********************************
		GLOBALS
********************************/

ULONG TotalTaskCount;
ULONG TotalStackUsed;
GXKTCB TaskList[MAX_TASK];
ULONG CurrentTask;

/******************************************************************************
*						  
* Name:				clear_gxktcb
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

ULONG clear_gxktcb(ULONG tid)

{
	UINT inx;
	GXKTCB *tcb_p;
	
	if (tid < MAX_TASK)
	{
		tcb_p = &TaskList[tid];

		tcb_p->name[0] = '\0';
		tcb_p->prio = 0;
		tcb_p->sstacksize = 0;
		tcb_p->ustacksize = 0;
		tcb_p->flags = 0;

		for (inx = 0; inx < REG_CNT; inx++)
		{
			tcb_p->reg[inx] = 0;
		}
		
		tcb_p->mode = 0;
		tcb_p->start_addr = NULL;
		tcb_p->w32id = 0;
		tcb_p->threadid = 0;

		tcb_p->state = TS_DEAD;
	}

	return (0);
}

/******************************************************************************
*						  
* Name:				t_create
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
		  
ULONG t_create(char name[4], ULONG prio, ULONG sstack, ULONG ustack, ULONG flags, ULONG *tid)

{
	ULONG rtn;
	UINT inx;
	GXKTCB *tcb_p;
	
	if (TotalTaskCount < MAX_TASK)
	{
		if ((sstack < MIN_TSTACK) && (ustack < MIN_TSTACK))
		{
			rtn = ERR_TINYSTK;
		}
		else if ((sstack + ustack + TotalStackUsed) > MAX_SSTACK)
		{
			rtn = ERR_NOSTK;
		}
		else if ((prio < MIN_PRIO) || (prio > MAX_PRIO))
		{
			rtn = ERR_PRIOR;
		}
		else
		{
			/*
			 * all parameters check OK - setup global data
			 */

			TotalStackUsed += sstack + ustack;

			for (inx = 0; inx < MAX_TASK; inx++)
			{
				tcb_p = &TaskList[inx];

				if (tcb_p->state == TS_DEAD)
				{
					/*
					 * initialize the task control block
					 */
					
					tcb_p->name[0] = name[0];
					tcb_p->name[1] = name[1];
					tcb_p->name[2] = name[2];
					tcb_p->name[3] = name[3];
					tcb_p->prio = prio;
					tcb_p->sstacksize = sstack;
					tcb_p->ustacksize = ustack;
					tcb_p->flags = flags;

					tcb_p->state = TS_CREATED;

					/*
					 * return the runtime task id
					 */
					
					*tid = inx;

					++TotalTaskCount;

					rtn = 0;
					break;
				}
			}
		}
	}
	else
	{
		rtn = ERR_NOTCB;
	}

	return (rtn);
}

/******************************************************************************
*						  
* Name:				t_delete
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

ULONG t_delete(ULONG tid)

{
	ULONG rtn;

	rtn = 0;

	if (tid < MAX_TASK)
	{
		if (TaskList[tid].state == TS_DEAD)
		{
			rtn = ERR_OBJDEL;
		}
		else
		{
			/*
			 * kill it
			 */

			if (CloseHandle(TaskList[tid].w32id) == 0)

			{
				rtn = ERR_OBJDEL;
			}

			/*
			 * clear local data for task and reset state
			 */
			
			clear_gxktcb (tid);
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
* Name:				t_getreg
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

ULONG t_getreg(ULONG tid, ULONG regnum, ULONG *reg_value)

{
	ULONG rtn;
	GXKTCB *pTcb;
	unsigned threadid;
	ULONG Index;

	rtn = 0;

	if (tid < MAX_TASK)
	{
		if (tid == 0)
		{
			/*
			 * determine who the calling task is
			 */
			
			threadid = GetCurrentThreadId ();

			for (Index = 0, pTcb = TaskList; Index < MAX_TASK; Index++, pTcb++)
			{
				if (pTcb->threadid == threadid) break;
			}
		}
		else
		{
			pTcb = &TaskList[tid];
		}

		if ((pTcb->state == TS_DEAD) || (Index == MAX_TASK))
		{
			rtn = ERR_OBJDEL;
		}
		else if (regnum > REG_CNT)
		{
			rtn = ERR_REGNUM;
		}
		else
		{
			*reg_value = pTcb->reg[regnum];
			rtn = 0;
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
* Name:				t_ident
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

ULONG t_ident(char name[4], ULONG node, ULONG *tid)

{
	ULONG rtn;
	UINT inx;
	GXKTCB *tcb_p;
	unsigned threadid;

	rtn = ERR_OBJNF;

	/*
	 * if name parameter is NULL, get ID of current thread
	 */
	
	if (name == 0)
	{
		threadid = GetCurrentThreadId ();

		for (inx = 0, tcb_p = TaskList; inx < MAX_TASK; inx++, tcb_p++)
		{
			if (tcb_p->threadid == threadid)
			{
				rtn = 0;
				*tid = inx;
				break;
			}
		}
	}
	else
	{
		/*
		 * otherwise, get ID of specified thread
		 */
		
		for (inx = 0; inx < MAX_TASK; inx++)
		{
			tcb_p = &TaskList[inx];

			if ((tcb_p->name[0] == name[0]) &&
				 (tcb_p->name[1] == name[1]) &&
					(tcb_p->name[2] == name[2]) &&
						 (tcb_p->name[3] == name[3]))
			{
				rtn = 0;
				*tid = inx;
				break;
			}
		}
	}
	
	return (rtn);
}

/******************************************************************************
*						  
* Name:				t_mode
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

ULONG t_mode(ULONG mask, ULONG new_mode, ULONG *old_mode)

{
	*old_mode = TaskList[CurrentTask].mode;

	if (mask & T_NOPREEMPT)
	{
		TaskList[CurrentTask].mode |= (new_mode & T_NOPREEMPT);
	}
	if (mask & T_TSLICE)
	{
		TaskList[CurrentTask].mode |= (new_mode & T_TSLICE);
	}
	if (mask & T_NOASR)
	{
		TaskList[CurrentTask].mode |= (new_mode & T_NOASR);
	}
	if (mask & T_NOISR)
	{
		TaskList[CurrentTask].mode |= (new_mode & T_NOISR);
	}

	return (0);  /* always returns 0 */
}

/******************************************************************************
*						  
* Name:				t_restart
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

ULONG t_restart(ULONG tid, ULONG targs[])

{
	ULONG rtn;
	ULONG mode;
	void *start_addr;

	rtn = 0;

	if (tid < MAX_TASK)
	{
		if (TaskList[tid].state == TS_DEAD)
		{
			rtn = ERR_OBJDEL;
		}
		else if (TaskList[tid].state == TS_CREATED)
		{
			rtn = ERR_NACTIVE;
		}
		else
		{
			/*
			 * save parameters to be reused, then init tcb
			 */

			mode = TaskList[tid].mode;
			start_addr = TaskList[tid].start_addr;
			clear_gxktcb (tid);

			/*
			 * kill task
			 */

			t_delete(tid);

			/*
			 * start task
			 */
			
			rtn = t_start(tid, mode, start_addr, targs);
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
* Name:				t_resume
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

ULONG t_resume(ULONG tid)

{
	ULONG rtn;

	rtn = 0;

	if (tid < MAX_TASK)
	{
		if (TaskList[tid].state == TS_DEAD)
		{
			rtn = ERR_OBJDEL;
		}
		else if (TaskList[tid].state != TS_SUSPEND)
		{
			rtn = ERR_NOTSUSP;
		}
		else
		{
			/*
			 * resume thread and update state
			 */

			ResumeThread (TaskList[tid].w32id); 
			TaskList[tid].state = TS_RUNNING;

			rtn = 0;
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
* Name:				t_setpri
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

ULONG t_setpri(ULONG tid, ULONG newprio, ULONG *oldprio)

{
	ULONG rtn;

	rtn = 0;

	/*
	 * return current priority
	 */

	*oldprio = TaskList[tid].prio;

	if (tid < MAX_TASK)
	{
		if (TaskList[tid].state == TS_DEAD)
		{
			rtn = ERR_OBJDEL;
		}
		else if ((newprio < MIN_PRIO) || (newprio > MAX_PRIO))
		{
			rtn = ERR_SETPRI;
		}
		else
		{
			/*
			 * priority is meaningless in win32 environment
			 */

			TaskList[tid].prio = newprio;

			rtn = 0;
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
* Name:				t_setreg
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

ULONG t_setreg(ULONG tid, ULONG regnum, ULONG reg_value)

{
	ULONG rtn;
	GXKTCB *pTcb;

	rtn = 0;

	if (tid < MAX_TASK)
	{
		if (tid == 0)
		{
			/*
			 * only used to associate w32 tid with our tid
			 * determine who the calling task is;
			 * reg_value was our TCB index when thread was created
			 */

			pTcb = &TaskList[reg_value];

			pTcb->threadid = GetCurrentThreadId ();
			rtn = 0;
		}
		else
		{
			pTcb = &TaskList[tid];

			if (pTcb->state == TS_DEAD)
			{
				rtn = ERR_OBJDEL;
			}
			else if (regnum > REG_CNT)
			{
				rtn = ERR_REGNUM;
			}
			else
			{
				pTcb->reg[regnum] = reg_value;
				rtn = 0;
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
* Name:				t_start
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

ULONG t_start(ULONG tid, ULONG mode, void (*start_addr)(), ULONG targs[])

{
	ULONG rtn;
	GXKTCB *tcb_p;

	rtn = 0;

	if (tid < MAX_TASK)
	{
		tcb_p = &TaskList[tid];

		if (tcb_p->state == TS_DEAD)
		{
			rtn = ERR_OBJDEL;
		}
		else if (tcb_p->state != TS_CREATED)
		{
			rtn = ERR_ACTIVE;
		}
		else
		{
			/*
			 * init startup data
			 */

			tcb_p->mode = mode;
			tcb_p->start_addr = start_addr;

			tcb_p->state = TS_RUNNING;
			
			/*
			 * start the task
			 */
			
			if ((tcb_p->w32id = _beginthreadex(NULL,
				                              tcb_p->sstacksize,
									          (unsigned (__stdcall *)(void *))start_addr,
									          targs,
									          0,
									          &tcb_p->threadid)) == 0)
			{
				rtn = ERR_OBJDEL;
			}
			else
			{
				rtn = 0;
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
* Name:				t_suspend
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

ULONG t_suspend(ULONG tid)

{
	ULONG rtn;

	rtn = 0;

	if (tid < MAX_TASK)
	{
		if (TaskList[tid].state == TS_DEAD)
		{
			rtn = ERR_OBJDEL;
		}
		else if (TaskList[tid].state == TS_SUSPEND)
		{
			rtn = ERR_SUSP;
		}
		else
		{
			/*
			 * suspend thread and update state
			 */

			SuspendThread (TaskList[tid].w32id); 
			TaskList[tid].state = TS_SUSPEND;

			rtn = 0;
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
* Name:				gxk_t_getTid
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

ULONG gxk_t_getTid(unsigned threadid, ULONG *tid)

{
	ULONG rtn;
	UINT inx;

	rtn = 1;

	for (inx = 0; inx < MAX_TASK; inx++)
	{
		if (TaskList[inx].threadid == threadid)
		{
			*tid = inx;
			rtn = 0;
			break;
		}
	}

	return (rtn);
}

/******************************************************************************
*						  
* Name:				gxk_t_init
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

ULONG gxk_t_init()

{
	UINT inx;

	TotalTaskCount = 0;
	TotalStackUsed = 0;

	CurrentTask = MAX_TASK;

	for (inx = 0; inx < MAX_TASK; inx++)
	{
		clear_gxktcb (inx);
	}

	return (0);
}
