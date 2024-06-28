/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        gxkSem
* Type:        C Source
* File:        %M%
* Version:     %I%
* Description: Semaphore Services Interface
*
* Interface (public) Routines:
*
*	sm_create
*	sm_delete
*	sm_ident
*	sm_p
*	sm_v
*
*	gxk_sem_init
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

/********************************
		LOCAL DECLARATIONS
********************************/

#define MAX_SEM_COUNT		8

typedef struct
{
	char name[4];
	HANDLE gxkSid;
} SEMDESC;

/********************************
		GLOBALS
********************************/

SEMDESC SemTbl[MAX_SEM];

/******************************************************************************
*						  
* Name:				sm_create
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
		  
ULONG sm_create(char name[4], ULONG count, ULONG flags, ULONG *smid)

{
	ULONG rtn;
	ULONG inx;
	SEMDESC *sem_p;

	inx = 0;
	
	while (inx < MAX_SEM)
	{
		if (SemTbl[inx].gxkSid == NULL)
			break;
		++inx;
	}

	if (inx == MAX_SEM)
	{
		rtn = ERR_NOSCB;
	}
	else
	{
		sem_p = &SemTbl[inx];

		sem_p->gxkSid = CreateSemaphore (NULL, count, MAX_SEM_COUNT, (LPCTSTR)name);
		
		sem_p->name[0] = name[0];
		sem_p->name[1] = name[1];
		sem_p->name[2] = name[2];
		sem_p->name[3] = name[3];
		
		*smid = inx;
		
		rtn = 0;
	}

	return (rtn);
}

/******************************************************************************
*						  
* Name:				sm_delete
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

ULONG sm_delete(ULONG smid)

{
	ULONG rtn;

	if (smid < MAX_SEM)
	{
		if (SemTbl[smid].gxkSid != NULL)
		{
			SemTbl[smid].gxkSid = NULL;
			SemTbl[smid].name[0] = '\0';
			rtn = 0;
		}
		else
		{
			rtn = ERR_OBJDEL;
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
* Name:				sm_ident
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

ULONG sm_ident(char name[4], ULONG node, ULONG *smid)

{
	ULONG rtn;
	SEMDESC *sem_p;
	UINT inx;

	rtn = ERR_OBJNF;

	for (inx = 0; inx < MAX_SEM; inx++)
	{
		sem_p = &SemTbl[inx];

		if (sem_p->name[0] == name[0])
			if (sem_p->name[1] == name[1])
				if (sem_p->name[2] == name[2])
					if (sem_p->name[3] == name[3])
					{
						rtn = 0;
						*smid = inx;
						break;
					}
	}

	return (rtn);
}

/******************************************************************************
*						  
* Name:				sm_p
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

ULONG sm_p(ULONG smid, ULONG flags, ULONG timeout)

{
	ULONG rtn;
	DWORD msecTout;
	DWORD gxkrtn;

	rtn = 0;

	if (smid < MAX_SEM)
	{
		if (SemTbl[smid].gxkSid != NULL)
		{
			msecTout = (flags & SM_NOWAIT) ? 0 : (timeout * 10);
			msecTout = (msecTout) ? msecTout : INFINITE;
			
			gxkrtn = WaitForSingleObject (SemTbl[smid].gxkSid, msecTout);

			if (gxkrtn == WAIT_TIMEOUT)
			{
				rtn = (flags & SM_NOWAIT) ? ERR_NOSEM : ERR_TIMEOUT;
			}
		}
		else
		{
			rtn = ERR_OBJDEL;
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
* Name:				sm_v
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

ULONG sm_v(ULONG smid)

{
	ULONG rtn;

	rtn = 0;

	if (smid < MAX_SEM)
	{
		if (SemTbl[smid].gxkSid != NULL)
		{
			ReleaseSemaphore (SemTbl[smid].gxkSid, 1, NULL); 
		}
		else
		{
			rtn = ERR_OBJDEL;
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
* Name:				gxk_sem_init
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

ULONG gxk_sem_init (void)

{
	UINT inx;
	
	for (inx = 0; inx < MAX_SEM; inx++)
	{
		SemTbl[inx].name[0] = '\0';
		SemTbl[inx].gxkSid == NULL;
	}
	
	return (0);
}
