/*------------------------------------------------------------------------
 Module:        PSERIAL.H
 Author:        ZABER
 Project:
 State:
 Creation Date: 27 June 2001
 Description:   Serial port API Header for Zaber Techmo units.
                Language : C
								Platform : Win32
                Serial   : Polled mode operation
------------------------------------------------------------------------*/

#ifndef _PSERIAL_H_
#define _PSERIAL_H_

#define PSERIAL_PACKETSIZE 6

extern void PSERIAL_Initialize ( void );

extern int PSERIAL_Open  ( const char *PortName );

extern void PSERIAL_Close ( void );

extern int PSERIAL_Receive( unsigned char *Unit,
                           unsigned char *Command,
                           long *Data );

extern void PSERIAL_Send( unsigned char Unit,
                             unsigned char Command,
                             long Data );

#endif
