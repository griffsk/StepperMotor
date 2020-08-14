//////////////////////////////////////////////////////////////////////////
// Stepper Code for Column Scans
//
// Author:		Sebastian Seeds
// Date:		2.24.15
// Use:			For use with Zaber stepper motors for transport and 
//				positioning of a radioactive source above static 
//				scintillating tiles. Includes sample ZABER code for manual
//				repositioning. Made for the SPHENIX HCal upgrade for use 
//				with QA testing.
// 
//////////////////////////////////////////////////////////////////////////

// Sample Code Documentation
/*------------------------------------------------------------------------
Module:        MAIN.C
Author:        ZABER
Project:
State:
Creation Date: 27 June 2001
Description:   Example program for communicating with
Teckmo devices on a serial port
Language : C
Platform : Win32
Serial   : Polled Mode Operation
------------------------------------------------------------------------*/
//FOR BASIC OPERATION
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <conio.h>
#include <time.h>
#include "pserial.c"  // Polled Serial API

//FOR SCOPE CODE
#include <sicl.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
#include <cstdlib>

//OPTION INCLUDES
#include <TROOT.h>
#include <TFile.h>
#include <TTree.h>
#include <TVirtualStreamerInfo.h>
#include <TPluginManager.h>

using namespace std;

//FOR STEPPER
static unsigned char Unit;
static unsigned char Command;
static long Data;

//------------------------------------------------------------------------
//FUNCTIONS FOR SCOPE CODE

//Read in 16 bit data from the return buffer.
//The short array named buffer is 2 bytes wide, but iread reads
//one byte at a time.  Therefore, the pointer into buffer is explicitly
//casted as a character type in the iread function.

INST oscillo;

unsigned long ReadWord (short *buffer, unsigned long BytesToRead)
{
	unsigned long BytesRead;
	int reason;

	iread(oscillo, (char *) buffer, BytesToRead, &reason, &BytesRead);

	return BytesRead;

}

unsigned long ReadByte (char *buffer, unsigned long BytesToRead)
{
	unsigned long BytesRead;
	int reason;

	iread(oscillo,buffer,BytesToRead,&reason,&BytesRead);

	return BytesRead;
}

void WriteIO(char *buffer)
{
	unsigned long actualcnt;
	unsigned long BytesToWrite;
	int send_end=1;
	char temp[50];

	BytesToWrite = strlen(buffer)+1;
	strcpy_s(temp, buffer);
	strcat_s(temp, "\n");
	iwrite(oscillo, temp, BytesToWrite, send_end, &actualcnt);

}
void ReadIO(char *buffer)
{
	unsigned long actualcnt;
	unsigned long BytesToWrite;
	int send_end;
	char temp[50];

	BytesToWrite = strlen(buffer)+1;
	strcpy_s(temp, buffer);
	strcat_s(temp, "\n");
	iread(oscillo, temp, BytesToWrite, &send_end, &actualcnt);

}

void ReadDouble(double *buffer)
{
	iscanf(oscillo, "%lf", buffer);
}

//------------------------------------------------------------------------
//FUNCTIONS FOR INPUT CONFIG FILE

//Read in configurations file and generate a map of parameter
//Additional function to check validity of configuration file
map<string, double> configParser(string inputFilePath)
{
	string line, key, value;
	ifstream stream(inputFilePath);
	stringstream splitter;
	
	map<string, double> variables;
	
	if (stream)
	{
		while (getline(stream, line))
		{
			if (line[0] != '#')
			{
				splitter << line;
				splitter >> key;
				splitter >> value;
				splitter.clear();
				variables[key] = stod(value);
			}
		}
	}
	
	else
	{
		cout << "Error unable to open file" << endl;
		exit(0);
	}

	return variables;
}

int checkVarMap(map<string, double> varMap, string varNames[], int length)
{
	int retVal = 0;
	
	for(int i = 0; i < length; i++)
	{
		if (varMap.find(varNames[i]) == varMap.end())
			retVal += 1;
	}
	
	return retVal;
}

// END SAMPLE CODE --------------------------------------------------------

// BEGIN SCAN CODE --------------------------------------------------------

// All commands follow the structure "PSERIAL_Send( X,Y,Z )" where X is the 
// target drive (for several daisy-chained), Y is the Zaber command (20 is 
// move absolute, for instance), and Z is data (microsteps to move for 
// command 20, for instance). All daisy-chained drives must be numbered 
// for coherent commands. This is accomplished with zaber command 2 (already
// present in code). Wait times are essential to prevent data collision. 
// Data value "64" denotes that the value is irrelevant for the command.

int main(int argc, char* argv[])
{

	cout << "Starting!" << endl;

	time_t t = time(NULL);
	struct tm tm = *localtime(&t);

	char timestamp[100];
	char daypart[100];
	char timepart[100];

	if(tm.tm_hour>9 && tm.tm_min>9) sprintf(timepart,"%d%d", tm.tm_hour, tm.tm_min);
	else if(tm.tm_hour>9 && tm.tm_min<=9) sprintf(timepart,"%d0%d", tm.tm_hour, tm.tm_min);
	else if(tm.tm_hour<=9 && tm.tm_min>9) sprintf(timepart,"0%d%d", tm.tm_hour, tm.tm_min);
	else if(tm.tm_hour<=9 && tm.tm_min<=9) sprintf(timepart,"0%d0%d", tm.tm_hour, tm.tm_min);

	if(tm.tm_mon>8 && tm.tm_mday>9) sprintf(daypart,"%d%d%d",tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	else if(tm.tm_mon>8 && tm.tm_mday<=9) sprintf(daypart,"%d%d0%d",tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	else if(tm.tm_mon<=8 && tm.tm_mday>9) sprintf(daypart,"%d0%d%d",tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	else if(tm.tm_mon<=8 && tm.tm_mday<=9) sprintf(daypart,"%d0%d0%d",tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);

	sprintf(timestamp,"%s-%s",daypart,timepart);


	// Creating and opening a text file for pulse averaging method
	ofstream file_1;
	ofstream file_2;
	ofstream file_3;
	ofstream file_4;
	ofstream file_5;
	ofstream file_6;
	ofstream file_7;

	char filename1[100];
	char filename2[100];
	char filename3[100];
	char filename4[100];
	char filename5[100];
	char filename6[100];
	char filename7[100];

	sprintf(filename1,"%s_TIME.txt",timestamp);
	sprintf(filename2,"%s_AREA_SIPM1.txt",timestamp);
	sprintf(filename3,"%s_AREA_SIPM2.txt",timestamp);
	sprintf(filename4,"%s_VMIN_SIPM1.txt",timestamp);
	sprintf(filename5,"%s_VMIN_SIPM2.txt",timestamp);
	sprintf(filename6,"%s_Unaveraged_VMin1.txt",timestamp);
	sprintf(filename7,"%s_Unaveraged_VMin2.txt",timestamp);

	file_1.open(filename1);
	file_2.open(filename2);
	file_3.open(filename3);
	file_4.open(filename4);
	file_5.open(filename5);
	file_6.open(filename6);
	file_7.open(filename7);

	// Reports total number of available microsteps for each drive.
	double xmicrosteptot = 8062992;
	double ymicrosteptot = 4031496;
	double stepspercm = xmicrosteptot/100;
	
	// Get parameters from input config file
	map<string, double> varMap = parser(argv[1]);

	// Check that configuration file is valid
	string configParamNames[] = {"xOriginCm", "xMaxCm", "yOriginCm", "yMaxCm", "nStepsX", "nStepsY"};
	int configParamNamesLen = 6;
	int check = checkVarMap(varMap, configParamNames, configParamNamesLen);
	if (check != 0)
	{
		cout << "Invalid parameters file" << endl;
		exit(0);
	}

	// Sets origin for scan./////////////////////////////////////////////////
	double yorigincm = varMap["yOriginCm"];
	double xorigincm = varMap["xOriginCm"];
	double xorigin = stepspercm*xorigincm;
	double yorigin = stepspercm*yorigincm;
	
	double xmaxcm = varMap["xMaxCm"];
	double ymaxcm = varMap["yMaxCm"];
	
	
	// Adjust for number of steps across x and y ////////////////////////////
    int xsteps = (int)varMap["nStepsX"];
	int ysteps = (int)varMap["nStepsY"];
	
	// Adjust for number of steps across x and y ////////////////////////////
	double xsteplengthcm = (xmaxcm - xorigincm) / xsteps;
	double ysteplengthcm = (ymaxcm - yorigincm) / ysteps;

/*
	// Adjust for step length across both x and y by cm...
	double xsteplengthcm = 0.5;
	double ysteplengthcm = 0.5;
*/
	double xgeneralstep = stepspercm*xsteplengthcm;
	double ygeneralstep = stepspercm*ysteplengthcm;

	// Variable for recording current positions of drives. Used to calibrate
	// wait times. Takes stage 860ms to go 8062992 steps.
	double xcurrentpos = 0.0;
	double ycurrentpos = 0.0;
	double sleepzero = 0.0;
	double sleeptime = 0.0;
	double sleeptime2 = 0.0;


	
	/////////////////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////


	// Checks to see if parameters are within physics ranges of the stages
	if (xsteps*xgeneralstep+xorigin > xmicrosteptot || ysteps*ygeneralstep+yorigin > ymicrosteptot)
	{
		cout << "Settings take the source beyond the end of the drive. Please readjust." << endl;
		Sleep(10000);
		return 0;
	}

	// Variables for PSERIAL_Receive command.
	unsigned char Unit;
	unsigned char Unit2;
	unsigned char Command;
	unsigned char Command2;
	long Data;
	long Data2;

	// Warns if step settings distort scan grid.
	if ( xgeneralstep > ygeneralstep + 1000 || xgeneralstep < ygeneralstep - 1000)
	{
		cout << "Warning: Check Aspect Ratio." << endl;
		cout << "Each step in X is " << xgeneralstep << "." << endl;
		cout << "Each step in Y is " << ygeneralstep << "." << endl;
		cout << "It might be desired to adjust xaxis and yaxis parameters to produce similar values" << endl;
		Sleep(10000);
	}

	// Initializes the stepper motors to interpret instructions.
	PSERIAL_Initialize();

	// Rezero drives ---------------------------------------------------------
	PSERIAL_Open("com3");
	PSERIAL_Send( 0,2,0);
	Sleep(1500);
	//PSERIAL_Send( 1,1,64 );
	//PSERIAL_Send( 2,1,64 );
	// Waiting for drives by current position
	// Returns current position
	PSERIAL_Send( 1,60,64 );
	while (1)
	{
		if (PSERIAL_Receive (&Unit,&Command,&Data) && Unit == 1 && Command == 60)
		{
			xcurrentpos = Data;
			break;
		}
	}
	Sleep(1500);
	PSERIAL_Send( 2,60,64 );
	while (1)
	{
		if (PSERIAL_Receive (&Unit,&Command,&Data2) && Unit == 2 && Command == 60)
		{
			ycurrentpos = Data2;
			break;
		}
	}
	cout << "The position of the X stepper is " << xcurrentpos << "." << endl;
	cout << "The position of the Y stepper is " << ycurrentpos << "." << endl;

	if(xcurrentpos > ycurrentpos)
	{
		//Sleeptime if x > y
		sleepzero = 100*1000*(xcurrentpos/xmicrosteptot); //(length of drive in cm)*(ms/cm)*(fractional drive position)
	}
	else
	{
		sleepzero = 50*1000*(ycurrentpos/ymicrosteptot); //(length of drive in cm)*(ms/cm)*(fractional drive position)
	}
	//cout << sleepzero << endl;

	PSERIAL_Send( 1,20,0 );
	PSERIAL_Send( 2,20,0 );
	//Sleep(20000);
	Sleep(sleepzero);


	//cout << "now starting the scan..." << endl;

	// Begin Scan
	for (int ii = 0; ii < xsteps; ii++)
	{
		PSERIAL_Send( 1,20, (xorigin + ii*xgeneralstep) );

		for (int jj = 0; jj < ysteps; jj++)
		{
			//Setting the sleeptime for y to return
			if (jj == 0 && ii != 0){
				PSERIAL_Send( 2,60,64 );
				while (1)
				{
					if (PSERIAL_Receive (&Unit2,&Command,&Data2) && Unit2 == 2 && Command == 60)
					{
						ycurrentpos = Data2;
						break;
					}
				}
				sleeptime = 50*1000*((ycurrentpos-yorigin)/ymicrosteptot); //(length of drive in cm)*(ms/cm)*(fractional drive position)
			}
			//wait time to send to the origin from zero
			if(jj==0 && ii ==0)
			{ 
				if(xorigincm > yorigincm)
				{
					sleeptime = 1000*xorigincm; //(ms/cm)*(origin)
				}
				else
				{
					sleeptime = 1000*yorigincm; //(ms/cm)*(origin)
				}
			}

			printf("\nMoving to column %i, row %i\n", ii, jj);
			PSERIAL_Send( 2,20,(yorigin + jj*ygeneralstep) );

			if (jj == 0)
			{
				Sleep(sleeptime);
			}
			else
			{
				Sleep(1000*ysteplengthcm); // (ms/cm)*(cm/step)
			}	


			//cout << "now doing scope stuff " << endl;

			///////////////////////////////////////////////////////////////////////////
			///////////////////////////////////////////////////////////////////////////

			//NEW TEST -> INTEGRAL METHOD

			oscillo = iopen("gpib1,7");	
			double area1 = 0.0; //New
			double area2 = 0.0;
			double vmin1 = 10.0;
			double vmin2 = 10.0;
			time_t rawtime;
			clock_t t;
			struct tm * timeinfo;		
			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			//double hour = 0.0;
			double minute = 0.0;
			double second = 0.0;
			itimeout(oscillo, 2000000);


			//cout << "now setting up the display for DAQ" << endl;

			// FOR SOURCE TEST, CHECK EVERY TIME
			WriteIO(":CDISPLAY");
			//cout << "view channel1?" << endl;
			WriteIO(":VIEW CHANNEL1");
			//cout << "view channel2?" << endl;
			WriteIO(":VIEW CHANNEL2");
			// Old Source
			//WriteIO(":CHANNEL1:SCALE 20E-3");
			//WriteIO(":CHANNEL2:SCALE 20E-3");
			//WriteIO(":CHANNEL1:OFFSET -80E-3");
			//WriteIO(":CHANNEL2:OFFSET -80E-3");
			//cout << "set the channel scales?" << endl;
			// New Source, cosmics, LED on large tile
			/*WriteIO(":CHANNEL1:SCALE 500E-3");
			WriteIO(":CHANNEL2:SCALE 500E-3");
			WriteIO(":CHANNEL1:OFFSET -1800E-3");
			WriteIO(":CHANNEL2:OFFSET -1800E-3");*/
			/*WriteIO(":CHANNEL1:SCALE 200E-3");
			WriteIO(":CHANNEL2:SCALE 200E-3");
			WriteIO(":CHANNEL1:OFFSET -650E-3");
			WriteIO(":CHANNEL2:OFFSET -650E-3");*/
			WriteIO(":TIMEBASE:SCALE 20E-9");
			//WriteIO(":TIMEBASE:POSITION -40E-9"); // source, cosmics
			WriteIO(":TIMEBASE:POSITION 130E-9"); // LED
			// New LED 375 nm			
			WriteIO(":CHANNEL1:SCALE 200E-3");
			WriteIO(":CHANNEL2:SCALE 200E-3");
			WriteIO(":CHANNEL1:OFFSET -650E-3");
			WriteIO(":CHANNEL2:OFFSET -650E-3");
			//LED 361 nm
			//WriteIO(":CHANNEL1:SCALE 50E-3");
			//WriteIO(":CHANNEL2:SCALE 50E-3");
			//WriteIO(":CHANNEL1:OFFSET -180E-3");
			//WriteIO(":CHANNEL2:OFFSET -180E-3");
			/*WriteIO(":CHANNEL1:SCALE 50E-3");
			WriteIO(":CHANNEL2:SCALE 50E-3");
			WriteIO(":CHANNEL1:OFFSET -100E-3");
			WriteIO(":CHANNEL2:OFFSET -100E-3");*/
			//WriteIO(":TIMEBASE:SCALE 20E-9");
			//WriteIO(":TIMEBASE:POSITION 80E-9"); // for triggering on LED (375) pulse
			//WriteIO(":TIMEBASE:POSITION 120E-9");// for triggering on LED (361) pulse
			//WriteIO(":TIMEBASE:POSITION -40E-9"); // for triggering on coincidence

			//cout << "getting the clock" << endl;
			t = clock();
			//cout << "telling the scope to start " << endl;
			WriteIO(":RUN");
			WriteIO(":MEASURE:SENDVALID ON");

			// --- it's necessary to delay between unaveraged readouts so the scope doesn't choke and give duplicates
			// --- 100 (msec) is safe for source on panel, lower may also be possible
			// --- 200 is needed for off panel
			// --- 5000 is good for cosmics...
			
			int sleep = 100;

			// --- see below, method to prevent data loss during crashes...
			// --- this isn't necessary in linux, but it seems to be necessary in windows...
			file_1.close();
			file_4.close();
			file_5.close(); 
			file_6.close();
			file_7.close(); 

			//cout << "gonna start doing stuff now " << endl << endl;
			if(xsteps == 1 && ysteps == 1)
			{
				WriteIO(":ACQUIRE:AVERAGE OFF");		
				iclose(oscillo);
				int VMin_Count = 0;
				int VMin_Stats = 5000;
				double vminp = 0;
				while(VMin_Count < VMin_Stats)
				{
					vminp=vmin1;
					oscillo = iopen("gpib1,7");
					WriteIO(":MEASURE:SOURCE CHANNEL1");
					WriteIO(":MEASURE:VMIN");
					WriteIO(":MEASURE:VMIN?");
					ReadDouble(&vmin1);
					cout <<"VMin 1 "<< vmin1 << endl;
					iclose(oscillo);
					oscillo = iopen("gpib1,7");
					WriteIO(":MEASURE:SOURCE CHANNEL2");
					WriteIO(":MEASURE:VMIN");
					WriteIO(":MEASURE:VMIN?");
					ReadDouble(&vmin2);
					cout << "VMin 2 " << vmin2 << endl;
					iclose(oscillo);
					if(vminp!=vmin1)
					{
						file_6.open(filename6,ofstream::app);
						file_6 << vmin1 << endl;
						file_6.close();
						file_7.open(filename7,ofstream::app);
						file_7 << vmin2 << endl;
						file_7.close();
						cout << "data count number " << VMin_Count << endl;
						time ( &rawtime );
						timeinfo = localtime ( &rawtime );
						cout << "Time is: " << asctime (timeinfo) << endl;
						VMin_Count++;
					}
					Sleep(sleep);
				}
				//for(VMin_Count = 0; VMin_Count < VMin_Stats; VMin_Count++)
				//{
				//	oscillo = iopen("gpib1,7");
				//	WriteIO(":MEASURE:SOURCE CHANNEL2");
				//	WriteIO(":MEASURE:VMIN");
				//	WriteIO(":MEASURE:VMIN?");
				//	ReadDouble(&vmin2);
				//	cout << "VMin 2 " << vmin2 << endl;
				//	file_7 << vmin2 << "\n";
				//	iclose(oscillo);
				//	Sleep(sleep);
				//}
			} // matches if(xsteps...

			if ( xsteps > 1 || ysteps > 1 )
			{
				//oscillo = iopen("gpib1,7");
				WriteIO(":ACQUIRE:AVERAGE:COUNT 1500");
				WriteIO(":ACQUIRE:AVERAGE ON");
			
				// --- Measure the VMin for Channel 1
				WriteIO(":MEASURE:SOURCE CHANNEL1");
				WriteIO(":MEASURE:VMIN");
				WriteIO(":MEASURE:VMIN?");
				ReadDouble(&vmin1);
				cout << "VMin 1 "<< vmin1 << endl;
				iclose(oscillo);
				oscillo = iopen("gpib1,7");

				// --- Measure the VMin for Channel 2
				WriteIO(":MEASURE:SOURCE CHANNEL2");
				WriteIO(":MEASURE:VMIN");
				WriteIO(":MEASURE:VMIN?");
				ReadDouble(&vmin2);
				cout << "VMin 2 " << vmin2 << endl;

				file_4.open(filename4,ofstream::app);
				file_4 << vmin1 << endl;
				file_4.close();
				file_5.open(filename5,ofstream::app);
				file_5 << vmin2 << endl;
				file_5.close();

			} // matches if ( xsteps > 1 ...

			WriteIO(":STOP");
			t = clock() - t;

			iclose(oscillo);
			//cout << "Ch.1 integral: " << area1 << endl; //New
			//cout << "Ch.2 integral: " << area2 << endl;
			//cout << "Ch.4 integral: " << area4 << endl;
			time ( &rawtime );
			timeinfo = localtime ( &rawtime );
			cout << "Time is: " << asctime (timeinfo) << endl;
			printf ("It took me %d clicks (%f seconds).\n",t,((float)t)/CLOCKS_PER_SEC);
			cout << (float)t/CLOCKS_PER_SEC << endl;
			//cout << "Time is: " << hour << ":" << minute << ":" << second << endl;
			file_1.open(filename1,ofstream::app);
			file_1 << (float)t/CLOCKS_PER_SEC << "\n";
			file_1.close();
			//file_1 << hour << " " << minute << " " << second << "\n";
			file_2 << area1 << "\n"; //Saved as SiPM 1
			file_3 << area2 << "\n"; //New - Possibly confusing: file_1 contains time for data aquisition data, file_4 contains ave. pulse data from channel 1
			//file_4 << vmin1 << "\n";
			//file_5 << vmin2 << "\n";
			printf("Data Collected \n");
			Sleep(2000);

		}

	}

	// Closing and rezeroing.
	file_1.close();
	file_2.close();
	file_3.close();
	file_4.close();
	file_5.close();
	file_6.close();
	file_7.close();
	printf("Returning to home positions");
	PSERIAL_Send( 1,1,64 );
	PSERIAL_Send( 2,1,64 );

	////////////////////////////////////////////////////////////////////////////
	// ENABLE IF HODOSCOPE IS ATTACHED! ----------------------------------------
	/*
	Sleep(100000);
	PSERIAL_Send( 1,36,0 );
	PSERIAL_Send( 2,36,0 );
	*/
	// -------------------------------------------------------------------------
	////////////////////////////////////////////////////////////////////////////

	PSERIAL_Close();

	// Exiting program.
	return 0;
}




