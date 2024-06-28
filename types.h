/**********************************************************************
*                                                                     *
*   MODULE:  types.h                                                  *
*   DATE:    96/11/04                                                 *
*   PURPOSE: standard types to be used in C files                     *
*                                                                     *
*---------------------------------------------------------------------*
*                       COPYRIGHT %G% %U% BY GHWORKS, LLC             *
*                             ALL RIGHTS RESERVED                     *
*                         SPDX-License-Identifier: MIT                *
**********************************************************************/

#ifdef __cplusplus
    #if __cplusplus
    extern "C" {
    #endif
#endif
 
#ifndef _TYPES_H
#define _TYPES_H

#ifndef _ASM_

typedef unsigned char   UCHAR;
typedef unsigned short  USHORT;
typedef unsigned int    UINT;
typedef unsigned long   ULONG;
 
#endif

#define TRUE        1
#define FALSE       0
//#define NULL        0

#define USEROM      2
#define LIB         1
#define YES         1
#define NO          0

#endif  /* _TYPES_H */
 
#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif
