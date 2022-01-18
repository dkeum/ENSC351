//============================================================================
//
//% Student Name 1: Jun Woo Keum
//% Student 1 #: 301335490
//% Student 1 userid (email): dkeum (dkeum@sfu.ca)
//
//% Student Name 2: Joshua Yeh
//% Student 2 #: 301381664
//% Student 2 userid (email): jya163 (jy163@sfu.ca)
//
// Helpers: 'None'
//
// File Name   : ReceiverY.cpp
// Version     : November, 2021
// Description : Starting point for ENSC 351 Project Part 5
// Original portions Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include "PeerY.h"

#include <string.h> // for memcpy and strcmp
#include <sys/time.h>
#include <arpa/inet.h> // for htons() -- not available with MinGW
#include "VNPE.h"
#include "Linemax.h"
#include "myIO.h"
#include "AtomicCOUT.h"

using namespace std;
using namespace smartstate;

PeerY::
PeerY(int d, char left, char right, const char *smLogN, int conInD, int conOutD)
:result("ResultNotSet"),
errCnt(0),
// KbCan(false),
// transferringFileD(-1),  // will need to be updated
mediumD(d),
logLeft(left),
logRight(right),
smLogName(smLogN),
consoleInId(conInD),
consoleOutId(conOutD),
reportInfo(false),
absoluteTimeout(0),
holdTimeout(0)
{
	struct timeval tvNow;
	PE(gettimeofday(&tvNow, NULL));
	sec_start = tvNow.tv_sec;
}

//Send a byte to the remote peer across the medium
void
PeerY::
sendByte(uint8_t byte)
{
	if (reportInfo) {
		//*** remove all but last of this block *
		char displayByte;
		if (byte == NAK)
		displayByte = 'N';
		else if (byte == ACK)
		displayByte = 'A';
		else if (byte == EOT)
		displayByte = 'E';
		else
		displayByte = byte;
		COUT << logLeft << displayByte << logRight << flush;
	}
	PE_NOT(myWrite(mediumD, &byte, sizeof(byte)), sizeof(byte));
}

void
PeerY::
transferCommon(std::shared_ptr<StateMgr> mySM, bool reportInfoParam){
	reportInfo = reportInfoParam;
	/*
	// use this code to send stateChart logging information to a file.
	ofstream smLogFile; // need '#include <fstream>' above
	smLogFile.open(smLogName, ios::binary|ios::trunc);
	if(!smLogFile.is_open()) {
		CERR << "Error opening sender state chart log file named: " << smLogName << endl;
		exit(EXIT_FAILURE);
	}
	mySM->setDebugLog(&smLogFile);
	// */

	// comment out the line below if you want to see logging information which will,
	//  by default, go to cout.
	mySM->setDebugLog(nullptr); // this will affect both peers.  Is this okay?

	mySM->start();

	/* **** You may need to add code here **** */
	fd_set set;
	FD_ZERO(&set);
	struct timeval tv;

	while(mySM->isRunning()) {
		// ********* this loop is going to need more work ********
		FD_SET(mediumD, &set);
		FD_SET(consoleInId, &set);
		tv.tv_sec=0;
		long long int now = elapsed_usecs();

		if (now >= absoluteTimeout) {
			tv.tv_usec = 0;
			int retVal = PE(select( max(mediumD,consoleInId)+1, &set, NULL, NULL, &tv ));
			if( retVal == 0 ){
				mySM->postEvent(TM);
			}
			if (retVal != 0){
				if(FD_ISSET(consoleInId, &set)){ //read console
					char byteToReceive[LINEMAX]; //intialized array
					int bytesRead = myReadcond(consoleInId, &byteToReceive, LINEMAX, 1, 0, 0);
					char tempByte[bytesRead+1]; //temp buff
					memcpy(tempByte,byteToReceive,bytesRead+1); //copy arrays
					tempByte[bytesRead] = '\0'; // null last charcter since #define CANC_C "&c\n"
					if(strcmp(CANC_C,tempByte)== 0) {
						mySM->postEvent(KB_C);
					}
					//clean up arrays?
				}
				//we need this case?
				if(FD_ISSET( mediumD, &set)){
					char byteToReceive;
					PE_NOT(myRead(mediumD, &byteToReceive, 1), 1);
					mySM->postEvent(SER, byteToReceive);
				}
			}
		} else {
			// ...
			tv.tv_usec = absoluteTimeout - now;
			int retVal = PE(select( max(mediumD,consoleInId)+1, &set, NULL, NULL, &tv ));
			if(retVal == 0){
				mySM->postEvent(TM);
			}
			if(retVal != 0) {
				if(FD_ISSET(consoleInId, &set)){ //read console
					char byteToReceive[LINEMAX]; //intialized array
					int bytesRead = myReadcond(consoleInId, &byteToReceive, LINEMAX, 1, 0, 0);
					char tempByte[bytesRead+1]; //temp buff
					memcpy(tempByte,byteToReceive,bytesRead+1); //copy arrays
					tempByte[bytesRead] = '\0'; // null last charcter since #define CANC_C "&c\n"
					if(strcmp(CANC_C,tempByte)== 0) {
						mySM->postEvent(KB_C);
					}
					//clean up arrays?
				}
				if( FD_ISSET(mediumD, &set) ) {
					/****/ {
						//read from medium
						char byte;
						unsigned timeout = (absoluteTimeout - now) / 1000 / 100; // tenths of seconds
						if (PE(myReadcond(mediumD, &byte, 1, 1, timeout, timeout))) {
							if (reportInfo) {
								char displayByte;
								if      (byte == NAK)
								displayByte = 'N';
								else if (byte == ACK)
								displayByte = 'A';
								else if (byte == SOH)
								displayByte = 'S';
								else if (byte == EOT)
								displayByte = 'E';
								else if (byte == CAN)
								displayByte = '!';
								else if (byte == 0)
								displayByte = '0';
								else
								displayByte = byte;

								COUT << logLeft << 1.0*timeout/10 << ":" << (int)(unsigned char) byte << ":" << displayByte << logRight << flush;
							}
							mySM->postEvent(SER, byte);
						}
						else { // This won't be needed later because timeout will occur after the select() function.
							if (reportInfo)
							COUT << logLeft << 1.0*timeout/10 << logRight << flush;
							mySM->postEvent(TM); // see note 3 lines above.
						}
					}

				} // medium read
			} // ret val != 0
		}
	}
	//      smLogFile.close()
}

// returns microseconds elapsed since this peer was constructed (within 1 second)
long long int
PeerY::
elapsed_usecs()
{
	struct timeval tvNow;
	PE(gettimeofday(&tvNow, NULL));
	/*_CSTD */ time_t   tv_sec = tvNow.tv_sec;
	return (tv_sec - sec_start) * (long long int) MILLION + tvNow.tv_usec; // casting needed?
}

/*
set a timeout time at an absolute time timeoutUnits into
the future. That is, determine an absolute time to be used
for the next one or more XMODEM timeouts by adding
timeoutUnits to the elapsed time.
*/
void 
PeerY::
tm(int timeoutUnits)
{
	absoluteTimeout = elapsed_usecs() + timeoutUnits * uSECS_PER_UNIT;
}

/* make the absolute timeout earlier by reductionUnits */
void 
PeerY::
tmRed(int unitsToReduce)
{
	absoluteTimeout -= (unitsToReduce * uSECS_PER_UNIT);
}

/*
Store the current absolute timeout, and create a temporary
absolute timeout timeoutUnits into the future.
*/
void 
PeerY::
tmPush(int timeoutUnits)
{
	holdTimeout = absoluteTimeout;
	absoluteTimeout = elapsed_usecs() + timeoutUnits * uSECS_PER_UNIT;
}

/*
Discard the temporary absolute timeout and revert to the
stored absolute timeout
*/
void 
PeerY::
tmPop()
{
	absoluteTimeout = holdTimeout;
}
