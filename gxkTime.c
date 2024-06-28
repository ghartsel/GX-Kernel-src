/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        gxkTime
* Type:        C Source
* File:        %M%
* Version:     %I%
* Description: Timer Services Interface
*
* Interface (public) Routines:
*
*	tm_cancel
*	tm_evafter
*	tm_evevery
*	tm_evwhen
*	tm_get
*	tm_set
*	tm_wkafter
*	tm_wkwhen
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
#include <stdlib.h>
#include <windows.h>
#include <process.h>
#include "gxkernel.h"
#include "gxkCfg.h"

/******************************************************************************
*						  
* Name:				tm_cancel
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
		  
ULONG tm_cancel(ULONG tmid)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				tm_evafter
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

ULONG tm_evafter(ULONG ticks, ULONG events, ULONG *tmid)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				tm_evevery
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

ULONG tm_evevery(ULONG ticks, ULONG events, ULONG *tmid)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				tm_evwhen
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

ULONG tm_evwhen(ULONG date, ULONG time, ULONG ticks, ULONG events, ULONG *tmid)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				tm_get
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

ULONG tm_get(ULONG *date, ULONG *time, ULONG *ticks)

{
	*date = 0x07d1090b;
	*time = 0x00083600;
	*ticks = 0;
	
	return (0);
}

/******************************************************************************
*						  
* Name:				tm_set
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

ULONG tm_set(ULONG date, ULONG time, ULONG ticks)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				tm_wkafter
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

ULONG tm_wkafter(ULONG ticks)

{
	Sleep (ticks * 10);

	return (0);
}

/******************************************************************************
*						  
* Name:				tm_wkwhen
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

ULONG tm_wkwhen(ULONG date, ULONG time, ULONG ticks)

{
	/*
	 * not keeping clock, so make it in 10 seconds
	 */
	
	Sleep (10000);

	return (0);
}
