/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        gxkSys.c
* Type:        C Source
* File:        %M%
* Version:     %I%
* Description: gxkernel (GXK)
*
* Interface (public) Routines:
*
*	gxkInit - entry point for initial development
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
#include "gxkSys.h"

/******************************************************************************
*						  
* Name:				gxkInit
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
		  
ULONG gxkInit ()

{
	gxk_t_init();
	gxk_ev_init();
	gxk_sem_init();
	gxk_q_init();

	return (0);
}
