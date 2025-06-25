/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        gxkDevice
* Type:        C Source
* File:        %M%
* Version:     %I%
* Description: Device I/O and Management Interface
*
* Interface (public) Routines:
*
*	de_close
*	de_cntrl
*	de_init
*	de_open
*	de_read
*	de_write
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
#include "gxkernel.h"

/******************************************************************************
*						  
* Name:				de_close
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
		  
ULONG de_close(ULONG dev, void *iopb, void *retval)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				de_cntrl
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

ULONG de_cntrl(ULONG dev, void *iopb, void *retval)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				de_init
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

ULONG de_init(ULONG dev, void *iopb, void *retval, void **data_area)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				de_open
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

ULONG de_open(ULONG dev, void *iopb, void *retval)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				de_read
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

ULONG de_read(ULONG dev, void *iopb, void *retval)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				de_write
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

ULONG de_write(ULONG dev, void *iopb, void *retval)

{
	return (0);
}
