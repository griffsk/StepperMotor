#include <algorithm>
#include <conio.h>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <math.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <vector>
#include <windows.h>

#include "phidget21.h"
#include "pserial.c"
#include "sicl.h"

using namespace std;

//!!When compiling link phidget21.lib, sicl32.lib, -lwinmm!!//
//!!Use C++17 and C17!!//
//!!Static link for all!!//

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
//OTHER FUNCTIONS
//Read in configurations file and generate a map of parameters
//Additional function to check validity of configuration file
//Generate timestamp for naming output files

string timestamp()
{
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	string year, month, day, hour, minute, tstamp;

	year = to_string(1900 + tm.tm_year);

	if(tm.tm_mon <= 9) {month = "0" + to_string(tm.tm_mon);}
	else {month = to_string(tm.tm_mon);}

	if(tm.tm_mday <= 9) {day = "0" + to_string(tm.tm_mday);}
	else {day = to_string(tm.tm_mday);}

	if(tm.tm_hour <= 9) {hour = "0" + to_string(tm.tm_hour);}
	else {hour = to_string(tm.tm_hour);}

	if(tm.tm_min <= 9) {minute = "0" + to_string(tm.tm_min);}
	else {minute = to_string(tm.tm_min);}

	tstamp = year + month + day + "-" + hour + minute;

	return tstamp;
}

void CreateFolder(const char * path)
{
    if(!CreateDirectory(path ,NULL))
    {
        return;
    }
}

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

int checkVarMap(map<string, double> varMap)
{
	string configParamNames[] = {"xOriginCm", "xMaxCm", "yOriginCm", "yMaxCm", "nStepsX", "nStepsY"};
	int configParamNamesLen = 6;
	int retVal = 0;

	for(int i = 0; i < configParamNamesLen; i++)
	{
		if (varMap.find(configParamNames[i]) == varMap.end())
			retVal += 1;
	}

	return retVal;
}

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
	clock_t progStart, progEnd;
	progStart = clock();
	cout << "Starting!" << endl;

	string paramFile = string(argv[1]);
	cout << "Reading scan parameters from: " << paramFile << endl;

	// Get tile name for metadata
	string tileName;
	cout << "Enter tile name: ";
	getline(cin, tileName);

	// Timestamp needed for file names
	string timeStamp = timestamp();

	// Create output files
	CreateFolder("output");
	string outputDir = "output\\" + timeStamp + "\\";
	CreateFolder(outputDir.c_str());
	string filename1 = outputDir + timeStamp + "_Metadata.txt";
	string filename2 = outputDir + timeStamp + "_VMIN_SIPM1.txt";
	string filename3 = outputDir + timeStamp + "_VAVG_SIPM1.txt";
	string filename4 = outputDir + timeStamp + "_TIME.txt";
	ofstream file_1;
	ofstream file_2;
	ofstream file_3;
	ofstream file_4;
	// This is weirdly necessary to prevent data loss on windows
	file_1.open(filename1);
	file_2.open(filename2);
	file_3.open(filename3);
	file_4.open(filename4);
	file_1.close();
	file_2.close();
	file_3.close();
	file_4.close();

	// Total number of available microsteps for each drive.
	double xmicrosteptot = 8062992;
	double ymicrosteptot = 4031496;
	double stepspercm = xmicrosteptot/100;

	// Get parameters from input config file
	map<string, double> varMap = configParser(paramFile);

	// Check that configuration file is valid, returns 0 if param names are valid
	int check = checkVarMap(varMap);
	if (check != 0)
	{
		cout << "Invalid parameters file" << endl;
		exit(0);
	}

	// Determine step lengths from input parameters
	double xsteplengthcm = (varMap["xMaxCm"] - varMap["xOriginCm"]) / ((int)varMap["nStepsX"] - 1);
	double ysteplengthcm = (varMap["yMaxCm"] - varMap["yOriginCm"]) / ((int)varMap["nStepsY"] - 1);

	// Make vectors of all x,y values converted from cm to steps
	vector<double> xvals;
	for (int i = 0; i < (int)varMap["nStepsX"]; i++)
    {
        double xvalcm = varMap["xOriginCm"] + i*xsteplengthcm;
        double xval = xvalcm * stepspercm;
        xvals.push_back(xval);
    }
	vector<double> yvals;
	for (int i = 0; i < (int)varMap["nStepsY"]; i++)
    {
        double yvalcm = varMap["yOriginCm"] + i*ysteplengthcm;
        double yval = yvalcm * stepspercm;
        yvals.push_back(yval);
    }

	//////////////////////////////////////////////////////////////////////
	////////////////// Check validity of parameters /////////////////////
	// Checks to see if parameters are within physics ranges of the stages
	double xmin = *min_element(xvals.begin(), xvals.end());
	double xmax = *max_element(xvals.begin(), xvals.end());
	double ymin = *min_element(yvals.begin(), yvals.end());
	double ymax = *max_element(yvals.begin(), yvals.end());
	if (xmin <= 0 || xmax >= xmicrosteptot || ymin <= 0 || ymax >= ymicrosteptot)
	{
		cout << "Settings take the source beyond the end of the drive. Please readjust." << endl;
		Sleep(10000);
		return 0;
	}

	// Warns if step settings distort scan grid.
	if (fabs(xsteplengthcm - ysteplengthcm) > 5)
	{
		cout << "Warning: Check Aspect Ratio." << endl;
		cout << "Each step in X is " << xsteplengthcm << "cm" << endl;
		cout << "Each step in Y is " << ysteplengthcm << "cm" << endl;
		cout << "It might be desired to adjust x-axis and y-axis parameters to produce similar values" << endl;
		Sleep(10000);
	}

	if ((int)varMap["nStepsX"] <= 1 || (int)varMap["nStepsY"] <= 1)
    {
        cout << "Must have more than 1 step in both x and y" << endl;
        return 0;
    }
    //////////////////////////////////////////////////////////////////////

	// Initialize stepper motors and rezero drives
	PSERIAL_Initialize();
	PSERIAL_Open("com3");
	PSERIAL_Send(0, 2, 0);
	Sleep(1500);

	// Variables for recording current positions of drives. Used to calibrate
	// wait times. Takes stage 860ms to go 8062992 steps, use 1000(ms/cm) for margin.
	double xcurrentpos = 0.0;
	double ycurrentpos = 0.0;
	double sleeptime = 0.0;

	// Variables for PSERIAL_Receive command.
	unsigned char Unit;
	unsigned char Unit2;
	unsigned char Command;
	unsigned char Command2;
	long Data;
	long Data2;

	// Get initial position
	PSERIAL_Send(1, 60, 64);
	while (1)
	{
		if (PSERIAL_Receive (&Unit,&Command,&Data) && Unit == 1 && Command == 60)
		{
			xcurrentpos = Data;
			break;
		}
	}
	Sleep(1500);
	PSERIAL_Send(2, 60, 64);
	while (1)
	{
		if (PSERIAL_Receive (&Unit2,&Command2,&Data2) && Unit2 == 2 && Command2 == 60)
		{
			ycurrentpos = Data2;
			break;
		}
	}
	cout << "The position of the X stepper is " << xcurrentpos << "." << endl;
	cout << "The position of the Y stepper is " << ycurrentpos << "." << endl;

	// Scan, looping over all x,y steps
	cout << "Now starting the scan..." << endl;
	for (int i = 0; i < xvals.size(); i++)
	{
		for (int j = 0; j < yvals.size(); j++)
		{
			// Get current position
			PSERIAL_Send(1, 60, 64);
			while (1)
			{
				if (PSERIAL_Receive (&Unit,&Command,&Data) && Unit == 1 && Command == 60)
				{
					xcurrentpos = Data;
					break;
				}
			}
			PSERIAL_Send(2, 60, 64);
			while (1)
			{
				if (PSERIAL_Receive (&Unit2,&Command2,&Data2) && Unit2 == 2 && Command2 == 60)
				{
					ycurrentpos = Data2;
					break;
				}
			}

			// Determine sleeptime from largest travel in x or y for next step
			if(fabs(xcurrentpos - xvals[i]) > fabs(ycurrentpos - yvals[j]))
			{
				sleeptime = 100*1000*(fabs(xcurrentpos - xvals[i])/xmicrosteptot); //(length of drive in cm)*(ms/cm)*(fraction of drive)
			}
			else
			{
				sleeptime = 50*1000*(fabs(ycurrentpos - yvals[j])/ymicrosteptot); //(length of drive in cm)*(ms/cm)*(fraction of drive)
			}

			// Move to next scan position
			cout << endl << "Moving to column " << i << ", row " << j << endl;
			PSERIAL_Send(1, 20, xvals[i]);
			PSERIAL_Send(2, 20, yvals[j]);
			Sleep(sleeptime);

			//Take scope readings
			cout << "Taking scope readings" << endl;
			oscillo = iopen("gpib1,7");
			double vmin1 = 10.0;
			double vavg1 = 10.0;
			clock_t t;
			itimeout(oscillo, 2000000);

			// FOR SOURCE TEST, CHECK EVERY TIME
			WriteIO(":CDISPLAY");
			WriteIO(":VIEW CHANNEL1");
			WriteIO(":TIMEBASE:SCALE 20E-9");
			WriteIO(":TIMEBASE:POSITION 130E-9"); // LED
			// New LED 375 nm
			WriteIO(":CHANNEL1:SCALE 500E-3");
			WriteIO(":CHANNEL1:OFFSET -1300E-3");

			// Get clock for time output
			t = clock();

			// Start scope
			WriteIO(":RUN");
			WriteIO(":MEASURE:SENDVALID ON");

			// Only needed if using 1 step, not used currently
			// --- it's necessary to delay between unaveraged readouts so the scope doesn't choke and give duplicates
			// --- 100 (msec) is safe for source on panel, lower may also be possible
			// --- 200 is needed for off panel
			// --- 5000 is good for cosmics...
			//int sleep = 100;

			if ((int)varMap["nStepsX"] > 1 || (int)varMap["nStepsY"] > 1)
			{
				//oscillo = iopen("gpib1,7");
				WriteIO(":ACQUIRE:AVERAGE:COUNT 1500");
				WriteIO(":ACQUIRE:AVERAGE ON");

				// --- Measure the VMin for Channel 1
				//oscillo = iopen("gpib1,7");
				WriteIO(":MEASURE:SOURCE CHANNEL1");
				WriteIO(":MEASURE:VMIN");
				WriteIO(":MEASURE:VMIN?");
				ReadDouble(&vmin1);
				cout << "VMin 1 "<< vmin1 << endl;
				iclose(oscillo);

				// --- Measure the VAvg for Channel 1
				oscillo = iopen("gpib1,7");
				WriteIO(":MEASURE:SOURCE CHANNEL1");
				WriteIO(":MEASURE:VAVERAGE");
				WriteIO(":MEASURE:VAVERAGE?");
				ReadDouble(&vavg1);
				cout << "VAvg 1 " << vavg1 << endl;
				iclose(oscillo);

				file_2.open(filename2,ofstream::app);
				file_2 << vmin1 << endl;
				file_2.close();
				file_3.open(filename3,ofstream::app);
				file_3 << vavg1 << endl;
				file_3.close();
			}

			WriteIO(":STOP");
			t = clock() - t;

			// Process data for time output file
			cout << "It took me " << t << " clicks(" << (float)t/CLOCKS_PER_SEC << " seconds)" << endl;
			file_4.open(filename4,ofstream::app);
			file_4 << (float)t/CLOCKS_PER_SEC << endl;
			file_4.close();

			cout << "Data Collected" << endl;
			Sleep(500);
		}
	}

	// Write scan parameters to metadata file
	file_1.open(filename1,ofstream::app);
	file_1 << "TILENAME," << tileName << endl;
	file_1 << "XORIGINCM," <<varMap["xOriginCm"] << endl;
	file_1 << "XMAXCM," << varMap["xMaxCm"] << endl;
	file_1 << "XSTEPS," << (int)varMap["nStepsX"] << endl;
	file_1 << "YORIGINCM," << varMap["yOriginCm"] << endl;
	file_1 << "YMAXCM," << varMap["yMaxCm"] << endl;
	file_1 << "YSTEPS," << (int)varMap["nStepsY"] << endl;

	// Close file and return to scan origin
	file_1.close();
	file_2.close();
	file_3.close();
	file_4.close();
	cout << "Returning to scan origin position" << endl;
	PSERIAL_Send(1, 20, xvals[0]);
	PSERIAL_Send(2, 20, yvals[0]);

	PSERIAL_Close();

	progEnd = clock();
	double timeElapsed = double(progEnd - progStart) / double(CLOCKS_PER_SEC);
	cout << "Time elapsed: " << fixed << timeElapsed << setprecision(3);
	cout << "sec" << endl;

	return 0;
}
