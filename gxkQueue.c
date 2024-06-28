/************************************BEGIN*****************************************
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC 
*                             ALL RIGHTS RESERVED
*                         SPDX-License-Identifier: MIT
* ********************************************************************************
* Name:        gxkQueue
* Type:        C Source
* File:        %M%
* Version:     %I%
* Description: Message Queue Services Interface
*
* Interface (public) Routines:
*
*	q_broadcast
*	q_create
*	q_delete
*	q_ident
*	q_receive
*	q_send
*	q_urgent
*	q_vcreate
*	q_vdelete
*	q_vident
*	q_vreceive
*	q_vsend
*
* Private Functions:
*
*	gxk_q_init
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

typedef struct
{
	ULONG msg[4];
} MSGBUF;

typedef struct
{
	ULONG start;
	ULONG end;
	ULONG nextin;
	ULONG nextout;
} QBUFDESC;

typedef struct
{
	char name[4];
	ULONG count;
	ULONG flags;
	char semname[4];
	ULONG semid;
	QBUFDESC buf;
} QDESC;

/********************************
		GLOBALS
********************************/

QDESC QTbl[MAX_Q];
MSGBUF Buf[MAX_BUF];
ULONG NextAvailBuf;

/******************************************************************************
*						  
* Name:				q_broadcast
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
		  
ULONG q_broadcast(ULONG qid, ULONG msg_buf[4], ULONG *count)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				q_create
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

ULONG q_create(char name[4], ULONG count, ULONG flags, ULONG *qid)

{
	ULONG rtn;
	ULONG inx;
	QDESC *q;

	rtn = 0;

	for (inx = 0, *qid = MAX_Q; inx < MAX_Q; inx++)
	{
		if (QTbl[inx].name[0] == '\0') break;
	}

	if ((inx < MAX_Q) && (NextAvailBuf < MAX_BUF))
	{
		if (NextAvailBuf >= MAX_BUF)
		{
			rtn = ERR_NOMGB;
		}
		else
		{
			q = &QTbl[inx];

			sprintf (q->semname, "qs%d",inx);

			if (sm_create(q->semname, 0, (SM_LOCAL | SM_FIFO),&q->semid) == 0)
			{
				*qid = inx;

				q->name[0] = name[0];
				q->name[1] = name[1];
				q->name[2] = name[2];
				q->name[3] = name[3];

				q->count = count;
				q->flags = flags;

				q->buf.start = q->buf.nextin = q->buf.nextout = NextAvailBuf;
				q->buf.end = q->buf.start + count - 1;

				NextAvailBuf += count;
			}
			else
			{
				rtn = ERR_NOQCB;
			}
		}
	}
	else
	{
		rtn = ERR_NOQCB;
	}

	return (rtn);
}

/******************************************************************************
*						  
* Name:				q_delete
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

ULONG q_delete(ULONG qid)

{
	ULONG rtn;

	rtn = 0;

	if (qid < MAX_Q)
	{
		if (QTbl[qid].name[0] != '\0')
		{
			QTbl[qid].name[0] = '\0';

			if (sm_delete(QTbl[qid].semid) != 0)
			{
				printf ("\nUnable to delete queue semaphore");
			}

			/* free queues buffers */
		}
		else
		{
			rtn = ERR_OBJID;
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
* Name:				q_ident
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

ULONG q_ident(char name[4], ULONG node, ULONG *qid)

{
	ULONG rtn;
	ULONG inx;

	rtn = ERR_OBJNF;

	for (inx = 0; inx < MAX_Q; inx++)
	{
		if ((QTbl[inx].name[0] == name[0]) &&
			(QTbl[inx].name[1] == name[1]) &&
			(QTbl[inx].name[2] == name[2]) &&
			(QTbl[inx].name[3] == name[3]))
		{
			*qid = inx;
			rtn = 0;
			break;
		}
	}

	return (rtn);
}

/******************************************************************************
*						  
* Name:				q_receive
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

ULONG q_receive(ULONG qid, ULONG flags, ULONG timeout, ULONG msg_buf[4])

{
	ULONG rtn;
	ULONG rtn1;
	QDESC *q;
	ULONG lflags;

	rtn = 0;

	if ((qid < MAX_Q) && (QTbl[qid].name[0] != '\0'))
	{
		q = &QTbl[qid];

		if (q->buf.nextout == q->buf.nextin)
		{
			/* no message available, wait for sender */
			lflags = (flags & Q_NOWAIT) ? SM_NOWAIT : SM_WAIT;

			rtn1 = sm_p (q->semid, lflags, timeout);
			switch (rtn1)
			{
			case 0:
				if (flags & Q_NOWAIT)
				{
					rtn = ERR_NOMSG;
				}
				else
				{
					/* message pending */
					msg_buf[0] = Buf[q->buf.nextout].msg[0];
					msg_buf[1] = Buf[q->buf.nextout].msg[1];
					msg_buf[2] = Buf[q->buf.nextout].msg[2];
					msg_buf[3] = Buf[q->buf.nextout].msg[3];

					q->buf.nextout = 
						(q->buf.nextout == q->buf.end) ? q->buf.start : (q->buf.nextout + 1);
				}
				break;

			case ERR_TIMEOUT:
				rtn = ERR_TIMEOUT;
				break;

			default:
				printf("\nWait on semaphore call failed");
				break;
			}
		}
		else
		{
			/* message pending */
			msg_buf[0] = Buf[q->buf.nextout].msg[0];
			msg_buf[1] = Buf[q->buf.nextout].msg[1];
			msg_buf[2] = Buf[q->buf.nextout].msg[2];
			msg_buf[3] = Buf[q->buf.nextout].msg[3];

			q->buf.nextout = 
				(q->buf.nextout == q->buf.end) ? q->buf.start : (q->buf.nextout + 1);
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
* Name:				q_send
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

ULONG q_send(ULONG qid, ULONG msg_buf[4])

{
	ULONG rtn;
	QDESC *q;

	if ((qid < MAX_Q) && (QTbl[qid].name[0] != '\0'))
	{
		q = &QTbl[qid];

		if (((q->buf.nextin + 1) == q->buf.nextout) || 
			((q->buf.nextin == q->buf.end) && (q->buf.nextout == q->buf.start)))
		{
			rtn = ERR_QFULL;
		}
		else
		{
			/* queue message */
			Buf[q->buf.nextin].msg[0] = msg_buf[0];
			Buf[q->buf.nextin].msg[1] = msg_buf[1];
			Buf[q->buf.nextin].msg[2] = msg_buf[2];
			Buf[q->buf.nextin].msg[3] = msg_buf[3];

			if (++q->buf.nextin > q->buf.end) q->buf.nextin = q->buf.start;

			/* signal waiting task */
			if (sm_v (q->semid) == 0)
			{
				rtn = 0;
			}
			else
			{
				printf("\nSend to queue failed");
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
* Name:				q_urgent
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

ULONG q_urgent (ULONG qid, ULONG msg_buf[4])

{
	/* in the future, put message to head of queue */
	return (q_send (qid, msg_buf));
}

/******************************************************************************
*						  
* Name:				q_vcreate
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

ULONG q_vcreate(char name[4], ULONG flags, ULONG maxnum, ULONG maxlen, ULONG *qid)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				q_vdelete
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

ULONG q_vdelete(ULONG qid)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				q_vident
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

ULONG q_vident(char name[4], ULONG node, ULONG *qid)

{
	return (0);
}

/******************************************************************************
*						  
* Name:				q_vreceive
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

ULONG q_vreceive(ULONG qid, ULONG flags, ULONG timeout, void *msgbuf, ULONG buf_len, ULONG *msg_len)

{
	q_receive (qid, flags, timeout, msgbuf);

	return (0);
}

/******************************************************************************
*						  
* Name:				q_vsend
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

ULONG q_vsend(ULONG qid, void *msgbuf, ULONG msg_len)

{
	q_send(qid, msgbuf);

	return (0);
}

/******************************************************************************
*						  
* Name:				gxk_q_init
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

ULONG gxk_q_init(void)

{
	ULONG inx;
	QDESC *q;

	NextAvailBuf = 0;

	for (inx = 0; inx < MAX_Q; inx++)
	{
		q = &QTbl[inx];

		q->name[0] = '\0';
		q->count = 0;
		q->flags = 0;
		q->semname[0] = '\0';
		q->semid = 0;
		q->buf.start = q->buf.end = q->buf.nextin = q->buf.nextout = 0;
	}

	return (0);
}

