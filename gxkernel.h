/**********************************************************************
*                                                                     *
*   MODULE:  gxkernel.h                                               *
*   DATE:    97/11/03                                                 *
*   PURPOSE: Structure typedefs, function prototypes, macros, and     *
*            symbol definitions for kernel                            *
*                                                                     *
*---------------------------------------------------------------------*
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC             *
*                             ALL RIGHTS RESERVED                     *
*                         SPDX-License-Identifier: MIT                *
**********************************************************************/
#if __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------*/
/* Don't allow this file to be included more than once.                */
/*---------------------------------------------------------------------*/

#include "types.h"

/***********************************************************************/
/* General Definitions                                                 */
/***********************************************************************/

/***********************************************************************/
/* Structure Definitions                                            */
/***********************************************************************/

/*---------------------------------------------------------------------*/
/* I/O Driver Parameter Structure                                      */
/*---------------------------------------------------------------------*/
struct ioparms
    {
    ULONG used;             /* Set by driver if this interface used */
    ULONG tid;              /* task ID of calling task */
    ULONG in_dev;           /* Input device number */
    ULONG status;           /* Processor status of caller */
    void *in_iopb;          /* Input pointer to IO parameter block */
    void *io_data_area;     /* no longer used */
    ULONG err;              /* For error return */
    ULONG out_retval;       /* For return value */
    };

/***********************************************************************/
/* errno macro                                                         */
/***********************************************************************/
#ifndef errno
#define errno (*(int *)(errno_addr()))
#endif

/***********************************************************************/
/* System Calls (defined in gxkernel.s)                                */
/***********************************************************************/
ULONG as_catch(void (*start_addr)(), ULONG mode);
ULONG as_send(ULONG tid, ULONG signals);
ULONG as_return(void);
ULONG ev_receive(ULONG events, ULONG flags, ULONG timeout, ULONG *events_r);
ULONG ev_send(ULONG tid, ULONG events);

void  k_fatal(ULONG err_code, ULONG flags);
ULONG k_terminate(ULONG node, ULONG fcode, ULONG flags);
ULONG m_ext2int(void *ext_addr, void **int_addr);
ULONG m_int2ext(void *int_addr, void **ext_addr);
ULONG pt_create(char name[4], void *paddr, void *laddr, ULONG length,
                ULONG bsize, ULONG flags, ULONG *ptid, ULONG *nbuf);
ULONG pt_delete(ULONG ptid);

ULONG pt_getbuf(ULONG ptid, void **bufaddr);
ULONG pt_ident(char name[4], ULONG node, ULONG *ptid);
ULONG pt_retbuf(ULONG ptid, void *buf_addr);
ULONG q_broadcast(ULONG qid, ULONG msg_buf[4], ULONG *count);
ULONG q_create(char name[4], ULONG count, ULONG flags, ULONG *qid);

ULONG q_delete(ULONG qid);
ULONG q_ident(char name[4], ULONG node, ULONG *qid);
ULONG q_receive(ULONG qid, ULONG flags, ULONG timeout, ULONG msg_buf[4]);
ULONG q_send(ULONG qid, ULONG msg_buf[4]);
ULONG q_urgent(ULONG qid, ULONG msg_buf[4]);

ULONG q_vcreate(char name[4], ULONG flags, ULONG maxnum, ULONG maxlen,
                ULONG *qid);
ULONG q_vdelete(ULONG qid);
ULONG q_vident(char name[4], ULONG node, ULONG *qid);
ULONG q_vreceive(ULONG qid, ULONG flags, ULONG timeout, void *msgbuf,
                 ULONG buf_len, ULONG *msg_len);
ULONG q_vsend(ULONG qid, void *msgbuf, ULONG msg_len);
ULONG q_vurgent(ULONG qid, void *msgbuf, ULONG msg_len);
ULONG q_vbroadcast(ULONG qid, void *msgbuf, ULONG msg_len, ULONG *count);

ULONG rn_create(char name[4], void *saddr, ULONG length, ULONG unit_size,
                ULONG flags, ULONG *rnid, ULONG *asiz);
ULONG rn_delete(ULONG rnid);
ULONG rn_getseg(ULONG rnid, ULONG size, ULONG flags, ULONG timeout,
                void **seg_addr);
ULONG rn_ident(char name[4], ULONG *rnid);
ULONG rn_retseg(ULONG rnid, void *seg_addr);

ULONG sm_create(char name[4], ULONG count, ULONG flags,ULONG *smid);
ULONG sm_delete(ULONG smid);
ULONG sm_ident(char name[4], ULONG node, ULONG *smid);
ULONG sm_p(ULONG smid, ULONG flags, ULONG timeout);
ULONG sm_v(ULONG smid);

ULONG t_create(char name[4], ULONG prio, ULONG sstack, ULONG ustack,
               ULONG flags, ULONG *tid);
ULONG t_delete(ULONG tid);
ULONG t_getreg(ULONG tid, ULONG regnum, ULONG *reg_value);
ULONG t_ident(char name[4], ULONG node, ULONG *tid);
ULONG t_mode(ULONG mask, ULONG new_mode, ULONG *old_mode);

ULONG t_restart(ULONG tid, ULONG targs[]);
ULONG t_resume(ULONG tid);
ULONG t_setpri(ULONG tid, ULONG newprio, ULONG *oldprio);
ULONG t_setreg(ULONG tid, ULONG regnum, ULONG reg_value);
ULONG t_start(ULONG tid, ULONG mode, void (*start_addr)(), ULONG targs[]);
ULONG *errno_addr(void);

ULONG t_suspend(ULONG tid);
ULONG tm_cancel(ULONG tmid);
ULONG tm_evafter(ULONG ticks, ULONG events, ULONG *tmid);
ULONG tm_evevery(ULONG ticks, ULONG events, ULONG *tmid);
ULONG tm_evwhen(ULONG date, ULONG time, ULONG ticks, ULONG events,
                ULONG *tmid);

ULONG tm_get(ULONG *date, ULONG *time, ULONG *ticks);
ULONG tm_set(ULONG date, ULONG time, ULONG ticks);
ULONG tm_tick(void);
ULONG tm_wkafter(ULONG ticks);
ULONG tm_wkwhen(ULONG date, ULONG time, ULONG ticks);

/*---------------------------------------------------------------------*/
/* Asynchronous Service Call Definitions                               */
/*---------------------------------------------------------------------*/
ULONG ev_asend(ULONG tid, ULONG events);
ULONG sm_av(ULONG smid);
ULONG q_asend(ULONG qid, ULONG msg_buf[4]);
ULONG q_avsend(ULONG qid, void *msgbuf, ULONG msg_len);
ULONG q_aurgent(ULONG qid, ULONG msg_buf[4]);
ULONG q_avurgent(ULONG qid, void *msgbuf, ULONG msg_len);

/*---------------------------------------------------------------------*/
/* MMU Service Call Definitions                                        */
/*---------------------------------------------------------------------*/
ULONG mm_l2p(ULONG tid, void *laddr, void **paddr, ULONG *length,
             ULONG *flags);
ULONG mm_p2l(ULONG tid, void *paddr, void **laddr, ULONG *length);
ULONG mm_pmap(ULONG tid, void *laddr, void *paddr, ULONG length,
              ULONG flags);
ULONG mm_pread(void *paddr, void *laddr, ULONG length);
ULONG mm_pwrite(void *paddr, void *laddr, ULONG length);
ULONG mm_sprotect(void *laddr, ULONG flags);
ULONG mm_unmap(ULONG tid, void *laddr);
ULONG pt_sgetbuf(ULONG ptid, void **paddr, void **laddr);

/*---------------------------------------------------------------------*/
/* I/O Service Call Definitions                                        */
/*---------------------------------------------------------------------*/
ULONG de_close(ULONG dev, void *iopb, void *retval);
ULONG de_cntrl(ULONG dev, void *iopb, void *retval);
ULONG de_init(ULONG dev, void *iopb, void *retval, void **data_area);
ULONG de_open(ULONG dev, void *iopb, void *retval);
ULONG de_read(ULONG dev, void *iopb, void *retval);
ULONG de_write(ULONG dev, void *iopb, void *retval);

/***********************************************************************/
/* Symbol Definitions                                                  */
/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Multi-processor configuration table definitions                     */
/*---------------------------------------------------------------------*/
#define KIROSTER        0x00000001   /* KI_ROSTER service provided */
#define SEQWRAP_ON      0x00000002   /* Wrap sequence number */
#define SEQWRAP_OFF     0x00000000   /* Do not wrap sequence # */

/*---------------------------------------------------------------------*/
/* Roster change Call-outs Definitions                                 */
/*---------------------------------------------------------------------*/
#define RSTR_NEW        0       /* Node itself is joining */
#define RSTR_JOIN       1       /* Another node has joined the system */
#define RSTR_EXIT       2       /* Another node has left the system */

/*---------------------------------------------------------------------*/
/* as_catch(), t_mode(), and t_start() Definitions                     */
/*---------------------------------------------------------------------*/
#define T_NOPREEMPT     0x00000001   /* Not preemptible bit */
#define T_PREEMPT       0x00000000   /* Preemptible */
#define T_TSLICE        0x00000002   /* Time-slicing enabled bit */
#define T_NOTSLICE      0x00000000   /* No Time-slicing */
#define T_NOASR         0x00000004   /* ASRs disabled bit */
#define T_ASR           0x00000000   /* ASRs enabled */
#define T_SUPV          0x00002000   /* For compatibility with 68K */
#define T_USER          0x00000000   /* For compatibility with 68K */
#define T_NOISR         0x00000100   /* Interrupts disabled */
#define T_ISR           0x00000000   /* Interrupts enabled */
#define T_LEVELMASK0    0x00000000   /* For compatibility with 68K */    
#define T_LEVELMASK1    0x00000100   /* For compatibility with 68K */    
#define T_LEVELMASK2    0x00000100   /* For compatibility with 68K */    
#define T_LEVELMASK3    0x00000100   /* For compatibility with 68K */    
#define T_LEVELMASK4    0x00000100   /* For compatibility with 68K */    
#define T_LEVELMASK5    0x00000100   /* For compatibility with 68K */    
#define T_LEVELMASK6    0x00000100   /* For compatibility with 68K */    
#define T_LEVELMASK7    0x00000100   /* For compatibility with 68K */    

/*---------------------------------------------------------------------*/
/* ev_receive() Definitions                                            */
/*---------------------------------------------------------------------*/
#define EV_NOWAIT       0x00000001  /* Don't wait for events */
#define EV_WAIT         0x00000000  /* Wait for events */
#define EV_ANY          0x00000002  /* Wait for ANY of desired bits */
#define EV_ALL          0x00000000  /* Wait for ALL of desired bits */

/*---------------------------------------------------------------------*/
/* k_fatal() Definitions                                               */
/*---------------------------------------------------------------------*/
#define K_GLOBAL        0x00000001  /* 1 = Global */
#define K_LOCAL         0x00000000  /* 0 = Local */

/*---------------------------------------------------------------------*/
/* mm_l2p(), mm_pmap(), and mm_sprotect() Definitions                  */
/*---------------------------------------------------------------------*/
#define MM_WPROTECT     0x00000100  /* Write protect */
#define MM_WRITEABLE    0x00000000  /* Writeable */
#define MM_NOCACHE      0x00000200  /* Disable data caching */
#define MM_CACHE        0x00000000  /* Cache enabled */
#define MM_NOACCESS     0x00000400  /* Deny access */
#define MM_ACCESS       0x00000000  /* Permit access */

/*---------------------------------------------------------------------*/
/* pt_create() Definitions                                             */
/*---------------------------------------------------------------------*/
#define PT_GLOBAL       0x00000001  /* 1 = Global */
#define PT_LOCAL        0x00000000  /* 0 = Local */
#define PT_DEL          0x00000004  /* 1 = Delete regardless */
#define PT_NODEL        0x00000000  /* 0 = Delete only if unused */

/*---------------------------------------------------------------------*/
/* q_create() and q_vcreate() Definitions                              */
/*---------------------------------------------------------------------*/
#define Q_GLOBAL        0x00000001  /* 1 = Global */
#define Q_LOCAL         0x00000000  /* 0 = Local */
#define Q_PRIOR         0x00000002  /* Queue by priority */
#define Q_FIFO          0x00000000  /* Queue by FIFO order */
#define Q_LIMIT         0x00000004  /* Limit message queue */
#define Q_NOLIMIT       0x00000000  /* No limit on message queue */
#define Q_PRIBUF        0x00000008  /* Use private buffers */
#define Q_SYSBUF        0x00000000  /* Use system buffers */

/*---------------------------------------------------------------------*/
/* q_receive() and q_vreceive() Definitions                            */
/*---------------------------------------------------------------------*/
#define Q_NOWAIT        0x00000001  /* Don't wait for a message */
#define Q_WAIT          0x00000000  /* Wait for a message */

/*---------------------------------------------------------------------*/
/* rn_create() Definitions                                             */
/*---------------------------------------------------------------------*/
#define RN_PRIOR        0x00000002  /* Queue by priority bit */
#define RN_FIFO         0x00000000  /* Queue by FIFO order */
#define RN_DEL          0x00000004  /* 1 = Delete regardless */
#define RN_NODEL        0x00000000  /* 0 = Delete only if unused */

/*---------------------------------------------------------------------*/
/* rn_getseg() Definitions                                             */
/*---------------------------------------------------------------------*/
#define RN_NOWAIT       0x00000001  /* Don't wait for memory */
#define RN_WAIT         0x00000000  /* Wait for a segment */

/*---------------------------------------------------------------------*/
/* sm_create() Definitions                                             */
/*---------------------------------------------------------------------*/
#define SM_GLOBAL       0x00000001  /* 1 = Global */
#define SM_LOCAL        0x00000000  /* 0 = Local */
#define SM_PRIOR        0x00000002  /* Queue by priority */
#define SM_FIFO         0x00000000  /* Queue by FIFO order */

/*---------------------------------------------------------------------*/
/* sm_p() Definitions                                                  */
/*---------------------------------------------------------------------*/
#define SM_NOWAIT       0x00000001  /* Don't wait for semaphore */
#define SM_WAIT         0x00000000  /* Wait for semaphore */

/*---------------------------------------------------------------------*/
/* t_create() Definitions                                              */
/*---------------------------------------------------------------------*/
#define T_GLOBAL        0x00000001   /* 1 = Global */
#define T_LOCAL         0x00000000   /* 0 = Local */
#define T_NOFPU         0x00000000   /* Not using FPU */
#define T_FPU           0x00000002   /* Using FPU bit */

/***********************************************************************/
/* Error Codes                                                         */
/***********************************************************************/

/*---------------------------------------------------------------------*/
/* Common Errors (across several service groups)                       */
/*---------------------------------------------------------------------*/
#define ERR_TIMEOUT  0x01     /* Timed out; returned only if timeout */
                              /* was requested */
#if 0                         /* OBSOLETE */
#define ERR_UNIMPL   0x02     /* Unimplemented system service */
#endif
#define ERR_SSFN     0x03     /* Illegal system service function number */
#define ERR_NODENO   0x04     /* Node specifier out of range */
#define ERR_OBJDEL   0x05     /* Object has been deleted */
#define ERR_OBJID    0x06     /* Object_id is incorrect; failed validity */
                              /* check */
#define ERR_OBJTYPE  0x07     /* Incorrect Object type */
#define ERR_OBJTFULL 0x08     /* Node's Object table full */
#define ERR_OBJNF    0x09     /* Named Object not found */

/*---------------------------------------------------------------------*/
/* Task Service Group Errors                                           */
/*---------------------------------------------------------------------*/
#define ERR_RSTFS    0x0D     /* Informative; files may be corrupted */
                              /* on restart */
#define ERR_NOTCB    0x0E     /* Exceeds node's maximum number of tasks */
#define ERR_NOSTK    0x0F     /* No stack space */
#define ERR_TINYSTK  0x10     /* Stack too small */
#define ERR_PRIOR    0x11     /* Priority out of range */
#define ERR_ACTIVE   0x12     /* Task already started */
#define ERR_NACTIVE  0x13     /* Cannot restart; this task never was */
                              /* started */
#define ERR_SUSP     0x14     /* Task already suspended */
#define ERR_NOTSUSP  0x15     /* The task was not suspended */
#define ERR_SETPRI   0x16     /* Cannot change priority; new priority */
                              /* out of range */
#define ERR_REGNUM   0x17     /* Register number out of range */
#define ERR_DELFS    0x18     /* pHILE+ resources in use */
#define ERR_DELLC    0x19     /* pREPC+ resources in use */
#define ERR_DELNS    0x1A     /* pNA+ resources in use */

/*---------------------------------------------------------------------*/
/* Region Service Group Errors                                         */
/*---------------------------------------------------------------------*/
#define ERR_RNADDR   0x1B     /* Starting address not on long word */
                              /* boundary */
#define ERR_UNITSIZE 0x1C     /* Illegal unit_size -- must be power of */
                              /* 2 and >= 16 */
#define ERR_TINYUNIT 0x1D     /* Length too large (for given unit_size) */
#define ERR_TINYRN   0x1E     /* Cannot create; region length too small */
                              /* to hold RNCB */
#define ERR_SEGINUSE 0x1F     /* Cannot delete; one or more segments */
                              /* still in use */
#define ERR_ZERO     0x20     /* Cannot getseg; request size of zero is */
                              /* illegal */
#define ERR_TOOBIG   0x21     /* Cannot getseg; request size is too big */
                              /* for region */
#define ERR_NOSEG    0x22     /* No free segment; only if RN_NOWAIT */
                              /* attribute used */
#define ERR_NOTINRN  0x23     /* Segment does not belong to this region */
#define ERR_SEGADDR  0x24     /* Incorrect segment starting address */
#define ERR_SEGFREE  0x25     /* Segment is already unallocated */
#define ERR_RNKILLD  0x26     /* Cannot getseg; region deleted while */
                              /* waiting */
#define ERR_TATRNDEL 0x27     /* Informative only; there were tasks */
                              /* waiting */

/*---------------------------------------------------------------------*/
/* Partition Service Group Errors                                      */
/*---------------------------------------------------------------------*/
#define ERR_PTADDR   0x28     /* Starting address not on long word */
                              /* boundary */
#define ERR_BUFSIZE  0x29     /* Buffer size not power of 2, or less */
                              /* than 4 bytes */
#define ERR_TINYPT   0x2A     /* Length too small to hold PTCB */
#define ERR_BUFINUSE 0x2B     /* Cannot delete; one or more buffers */
                              /* still in use */
#define ERR_NOBUF    0x2C     /* Cannot allocate; partition out of free */
                              /* buffers */
#define ERR_BUFADDR  0x2D     /* Incorrect buffer starting address */
#define ERR_BUFFREE  0x2F     /* Buffer is already unallocated */

/*---------------------------------------------------------------------*/
/* Queue and Message Service Group Errors                              */
/*---------------------------------------------------------------------*/
#define ERR_KISIZE   0x30     /* Message length exceeds KI maximum */
                              /* packet buffer length */
#define ERR_MSGSIZ   0x31     /* Message length exceeds maximum message */
                              /* length specified while creating the queue */
#define ERR_BUFSIZ   0x32     /* Buffer too small to receive message */
#define ERR_NOQCB    0x33     /* Cannot create QCB; exceeds node's */
                              /* maximum number of active queues */
#define ERR_NOMGB    0x34     /* Cannot allocate private buffers; too */
                              /* few available */
#define ERR_QFULL    0x35     /* Message queue at length limit */
#define ERR_QKILLD   0x36     /* Queue deleted while task waiting */
#define ERR_NOMSG    0x37     /* Queue empty; this error code is */
                              /* returned only if Q_NOWAIT was selected */
#define ERR_TATQDEL  0x38     /* Informative only; there were tasks */
                              /* waiting at the queue */
#define ERR_MATQDEL  0x39     /* Informative only; there were messages */
                              /* pending in the queue */
#define ERR_VARQ     0x3A     /* Queue is variable size */
#define ERR_NOTVARQ  0x3B     /* Queue is not variable size */

/*---------------------------------------------------------------------*/
/* Event/Asynch Signal Service Group Errors                            */
/*---------------------------------------------------------------------*/
#define ERR_NOEVS    0x3C     /* Selected events not pending; this error */
                              /* code is returned only if the EV_NOWAIT */
                              /* attribute was selected */
#define ERR_NOTINASR 0x3E     /* Illegal, not called from an ASR */
#define ERR_NOASR    0x3F     /* Task has no valid ASR */

/*---------------------------------------------------------------------*/
/* Semaphore Service Group Errors                                      */
/*---------------------------------------------------------------------*/
#define ERR_NOSCB    0x41     /* Exceeds node's maximum number of */
                              /* semaphores */
#define ERR_NOSEM    0x42     /* No semaphore; this error code is */
                              /* returned only if SM_NOWAIT was selected */
#define ERR_SKILLD   0x43     /* Semaphore deleted while task waiting */
#define ERR_TATSDEL  0x44     /* Informative only; there were tasks */
                              /* waiting */

/*---------------------------------------------------------------------*/
/* Time Service Group Errors                                           */
/*---------------------------------------------------------------------*/
#define ERR_NOTIME   0x47     /* System time and date not yet set */
#define ERR_ILLDATE  0x48     /* Date input out of range */
#define ERR_ILLTIME  0x49     /* Time of day input out of range */
#define ERR_ILLTICKS 0x4A     /* Ticks input out of range */
#define ERR_NOTIMERS 0x4B     /* Exceeds maximum number of configured */
                              /* timers */
#define ERR_BADTMID  0x4C     /* tmid invalid */
#define ERR_TMNOTSET 0x4D     /* Timer not armed or already expired */
#define ERR_TOOLATE  0x4E     /* Too late; date and time input already */
                              /* in the past */

/*---------------------------------------------------------------------*/
/* Multiprocessor Support Service Group Errors                         */
/*---------------------------------------------------------------------*/
#define ERR_ILLRSC   0x53     /* Object not created from this node */
#define ERR_NOAGNT   0x54     /* Cannot wait; the remote node is out */
                              /* of Agents */
#define ERR_AGTBLKD  0x55     /* Agent blocked.  This is not an error. */

#define ERR_STALEID  0x65     /* Object does not exist any more */
#define ERR_NDKLD    0x66     /* Remote Node no longer in service */
#define ERR_MASTER   0x67     /* Cannot terminate Master node */

/*---------------------------------------------------------------------*/
/* IO Service Group Errors                                             */
/*---------------------------------------------------------------------*/
#define ERR_IODN     0x101    /* Illegal device (major) number */
#define ERR_NODR     0x102    /* No driver provided */
#define ERR_IOOP     0x103    /* Illegal I/O function number */

/*---------------------------------------------------------------------*/
/* Fatal Errors During Startup                                         */
/*---------------------------------------------------------------------*/
#define FAT_ALIGN    0xF00    /* Region 0 must be aligned on a long */
                              /* word boundary */
#define FAT_OVSDA    0xF01    /* Region 0 overflow while making system */
                              /* data area */
#define FAT_OVOBJT   0xF02    /* Region 0 overflow while making object */
                              /* table */
#define FAT_OVDDAT   0xF03    /* Region 0 overflow while making device */
                              /* data area table */
#define FAT_OVTCB    0xF04    /* Region 0 overflow while making task */
                              /* structures */
#define FAT_OVQCB    0xF05    /* Region 0 overflow while making queue */
                              /* structures */
#define FAT_OVSMCB   0xF06    /* Region 0 overflow while making sema- */
                              /* phore structures */
#define FAT_OVTM     0xF07    /* Region 0 overflow while making timer */
                              /* structures */
#define FAT_OVPT     0xF08    /* Region 0 overflow while making parti- */
                              /* tion structures */
#define FAT_OVRSC    0xF09    /* Region 0 overflow while making RSC */
                              /* structures */
#define FAT_OVRN     0xF0A    /* Region 0 overflow while making region */
                              /* structures */
#define FAT_ROOT     0xF0C    /* Cannot create root task */
#define FAT_IDLE     0xF0D    /* Cannot create idle task */
#ifndef FAT_CHKSUM            /* (To avoid collision with pna.h) */
#define FAT_CHKSUM   0xF0E    /* Checksum error */
#endif
#define FAT_INVCPU   0xF0F    /* Wrong processor type */

/*---------------------------------------------------------------------*/
/* Fatal Errors During Multiprocessor Operations                       */
/*---------------------------------------------------------------------*/
#define FAT_ILLPKT   0xF12    /* Illegal packet type in the received */
                              /* packet */
#define FAT_MIVERIF  0xF13    /* Multiprocessor configuration mismatch */
                              /* at system verify */
#if 0                         /* OBSOLETE */
#define FAT_OBJTENT  0xF14    /* Global object received cannot be entered */
                              /* in this node */
#endif

#define FAT_NODENUM  0xF15    /* Node # = 0 in Multi-processor config table */
#define FAT_NNODES   0xF16    /* Total # of nodes = 0 or > 2 ** 14 in mpct */
#define FAT_OVMP     0xF17    /* Region 0 overflow while making roster and */
                              /* seq.# tables) */
#define FAT_KIMAXBUF 0xF18    /* KI_MAXBUF entry is smaller than required */
#define FAT_ASYNCERR 0xF19    /* Asynchronous call error - no user callout */
#define FAT_DEVINIT  0xF1B    /* Error while initializing a device */

#define FAT_JN2SOON  0xF20    /* Slave node joined too soon ! */
#define FAT_MAXSEQ   0xF21    /* Maximum sequence # reached - can't wrap */
#define FAT_JRQATSLV 0xF22    /* Join request was sent to a slave node */

/*---------------------------------------------------------------------*/
/* Autoinit flags                                                      */
/*---------------------------------------------------------------------*/
#define IO_AUTOINIT          (1<<8)
#define IO_NOAUTOINIT        0

/*---------------------------------------------------------------------*/
/* #endif for gxkernel.h include                                           */
/*---------------------------------------------------------------------*/
#endif

#if __cplusplus
}
#endif

