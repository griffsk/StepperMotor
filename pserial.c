/*------------------------------------------------------------------------
 Module:        PSERIAL.C
 Author:        ZABER
 Project:
 State:
 Creation Date: 27 June 2001
 Description:   Serial port API for Zaber Techmo units.
                Language : C
                Platform : Win32
                Serial   : Polled mode operation
------------------------------------------------------------------------*/


#include <windows.h>  // This provides all the file handling routines
#include <mmsystem.h> // This include file is necessary to use the timer function
                      // MUST INCLUDE "winmm.lib" AT LINK TIME
#include "pserial.h"  // Header file for this API

#define RXTIMEOUT 500 //milliseconds

static HANDLE PortHandle; // Handle to the serial port
static DCB dcb;           // Device control block of the serial port (structure)
static COMMTIMEOUTS ctmo; // Timeout values for the serial port (structure)

static unsigned long BytesWritten; // used by the WriteFile command to return bytes written
static unsigned long BytesRead;    // used by the ReadFile command to return bytes read

static unsigned char TxBuffer[PSERIAL_PACKETSIZE]; // Transmit buffer for data packets
static unsigned char RxBuffer[PSERIAL_PACKETSIZE]; // Receive buffer for data packets
static int RxCount;                           // counter to keep track of receive status
static unsigned long RxTimeStamp;             // Timestamp used to expire incomplete packets


/*------------------------------------------------------------------------
 Procedure:     PSERIAL_Initialize ID:1
 Purpose:       Initializes the serial communication API Must be
                called before using any other functions in the API
 Input:         None
 Output:        None
 Errors:        None
------------------------------------------------------------------------*/
void PSERIAL_Initialize ( void )
{
  // Initialize the handle to an invalid value
  // So that PSERIAL_GetPortHandle can be called to see if port is open
  // at all times.
  PortHandle = INVALID_HANDLE_VALUE;

  // Miscelaneous initialization
  BytesWritten = 0;
  BytesRead = 0;

  RxCount = 0;
  RxTimeStamp = timeGetTime();
}


/*------------------------------------------------------------------------
 Procedure:     PSERIAL_Open ID:1
 Purpose:       Attempts to open a serial port and set up the port
                for communication with Teckmo chains
 Input:         PortName
 Output:        Error Code
 Errors:        If the function succeeded, returns TRUE
                If the function failed, returns FALSE.
                Call GetLastError() to get extended error info.
------------------------------------------------------------------------*/
int PSERIAL_Open ( const char *PortName )
{
  // Check that a port is not already opened for this application
	if ( INVALID_HANDLE_VALUE != PortHandle )
	{
	  return FALSE;
	}
	// Open the port
  PortHandle = CreateFile( PortName,
                           GENERIC_READ | GENERIC_WRITE,
                           0,
                           0,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, //not FILE_FLAG_OVERLAPPED,
                           0 );
  if ( PortHandle == INVALID_HANDLE_VALUE )
  {
    // CreateFile failed -- the port could already be in use.
    return FALSE;
  }
  // Set the Device Control Block
  ZeroMemory(&dcb, sizeof(dcb));
  dcb.DCBlength = sizeof(dcb);
  if (!BuildCommDCB("9600,n,8,1", &dcb))
  {
    // Error building DCB
    CloseHandle( PortHandle );
    PortHandle = INVALID_HANDLE_VALUE;
    return FALSE;
  }
  if (!SetCommState( PortHandle, &dcb ))
  {
    // Error setting DCB parameters
    CloseHandle( PortHandle );
    PortHandle = INVALID_HANDLE_VALUE;
    return FALSE;
  }

  // Set Timeouts
  ctmo.ReadIntervalTimeout = MAXDWORD;
  ctmo.ReadTotalTimeoutMultiplier = 0;
  ctmo.ReadTotalTimeoutConstant = 0;
  ctmo.WriteTotalTimeoutMultiplier = 0;
  ctmo.WriteTotalTimeoutConstant = 0;
  if (!SetCommTimeouts( PortHandle, &ctmo ))
  {
    // Error setting timeout parameters
    CloseHandle( PortHandle );
    PortHandle = INVALID_HANDLE_VALUE;
    return FALSE;
  }

  return TRUE;
}


/*------------------------------------------------------------------------
 Procedure:     PSERIAL_Close ID:1
 Purpose:       Closes the serial communication port
 Input:         None
 Output:        None
 Errors:        None
------------------------------------------------------------------------*/
void PSERIAL_Close ( void )
{
  if ( INVALID_HANDLE_VALUE != PortHandle )
	{
	  CloseHandle(PortHandle);
    PortHandle = INVALID_HANDLE_VALUE;
	}
}


/*------------------------------------------------------------------------
 Procedure:     PSERIAL_Receive ID:1
 Purpose:       Polls the receive buffer to see if any byte has
                arrived.  When 6 bytes are read, it returns TRUE and
                the user can read the Unit, Command and Data.
                Should be called frequently (i.e. polled mode)
 Input:         None
 Output:        Returns TRUE if a 6-byte packet is ready
                Returns FALSE if a 6-byte packet is not ready
                Also returns Unit, Command, Data
 Errors:        None
------------------------------------------------------------------------*/
int PSERIAL_Receive( unsigned char *Unit,
                     unsigned char *Command,
                     long *Data )
{
  unsigned char TempByte;
  if ( timeGetTime() - RxTimeStamp > RXTIMEOUT )
  {
    RxCount = 0;
    RxTimeStamp = timeGetTime();
  }
  ReadFile( PortHandle,
            &TempByte,     // Where to put the byte read
            1,             // read one byte at a time
            &BytesRead,    // Bytes actually read
            NULL );
  if ( BytesRead > 0 ) // A byte is read
  {
    RxBuffer[RxCount] = TempByte; // Store the byte
    RxCount++;                    // Increment counter
    RxTimeStamp = timeGetTime();  // reload timestamp
  }
  if ( PSERIAL_PACKETSIZE == RxCount ) // A full buffer
  {
    // A packet is ready to be used
    *Unit    = RxBuffer[0];
    *Command = RxBuffer[1];
    // Position 2 is LSB; Position 5 is MSB
    *Data  = ((RxBuffer[2]      ) & 0x000000FF)
	         + ((RxBuffer[3] <<  8) & 0x0000FF00)
           + ((RxBuffer[4] << 16) & 0x00FF0000)
           + ((RxBuffer[5] << 24) & 0xFF000000);
    // reset the byte counter to receive new packet
    RxCount = 0;
    return TRUE;
  }
  else
  {
    // Did not receive a full packet yet.
    return FALSE;
  }
}


/*------------------------------------------------------------------------
 Procedure:     PSERIAL_Send ID:1
 Purpose:       Write a packet to the serial port
 Input:         Unit
                Command
                Data
 Output:        None
 Errors:        None
------------------------------------------------------------------------*/
void PSERIAL_Send( unsigned char Unit,
                   unsigned char Command,
                   long Data )
{
  TxBuffer[0] = Unit;
  TxBuffer[1] = Command;
  // Position 2 is LSB; Position 5 is MSB
  TxBuffer[2] = (Data & 0x000000FF);
  TxBuffer[3] = ((Data >> 8) & 0x000000FF);
  TxBuffer[4] = ((Data >> 16) & 0x000000FF);
  TxBuffer[5] = ((Data >> 24) & 0x000000FF);

  WriteFile( PortHandle,
             TxBuffer,
             PSERIAL_PACKETSIZE,
             &BytesWritten,
             NULL );
}


