/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        gxkKernel
* Type:        C Source
* File:        %M%
* Version:     %I%
* Description: Kernel Services Interface
*
* Interface (public) Routines:
*
*	k_fatal
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

/******************************************************************************
*						  
* Name:				k_fatal
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
		  
void  k_fatal(ULONG err_code, ULONG flags)

{
	printf ("FATAL FAULT: %x", err_code);
}
