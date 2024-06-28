/*******************************BEGIN**************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
**************************************************************************
*
* Name:        gxkCfg.h
* Type:        Include File
* File:        %M%
* Version:     %I%
* Reference:   
* Description: Environment configuration header file
* 
*	
*
* File Dependencies:
*
* Modification History:
* ----------------------------------------------------------- 
* Date 	      Initials        Change Description
* -----------------------------------------------------------
* 7/15/00	  GVH			  Created
*
*******************************END****************************************/

#define MAX_TASK			64

#define MIN_TSTACK			256					/* min task stack size */
#define MAX_TSTACK			4000				/* max task stack size */
#define MAX_SSTACK			(MAX_TASK * 2000)	/* max stack available for all tasks */

#define MAX_Q				32
#define MAX_BUF				2048

#define MAX_SEM				128

