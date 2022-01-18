//============================================================================
//
//% Student Name 1: dkeum
//% Student 1 #: 301335490
//% Student 1 dkeum (email): dkeum@sfu.ca
//
//% Student Name 2: jya163
//% Student 2 #: 301381664
//% Student 2 jya163 (email): jya163@sfu.ca
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P2_<userid1>_<userid2>" (eg. P2_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : ReceiverY.cpp
// Version     : September 24th, 2021
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2021 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================
#include "ReceiverY.h"
#include <string.h> // for memset()
#include <fcntl.h>
#include <stdint.h>
#include "myIO.h"
#include "VNPE.h"
//using namespace std;
ReceiverY::
ReceiverY(int d)
	:PeerY(d),
	closedOk(1),
	anotherFile(0xFF),
	NCGbyte('C'),
	goodBlk(false),
	goodBlk1st(false),
	syncLoss(false), // transfer will end when syncLoss becomes true
	errCnt(0),
	numLastGoodBlk(0)
{
	for (int i = 0; i < 133; i++) {
		rcvBlk[i] = 0;
	}
}
/* Only called after an SOH character has been received.
The function receives the remaining characters to form a complete
block.
The function will set or reset a Boolean variable,
goodBlk. This variable will be set (made true) only if the
calculated checksum or CRC agrees with the
one received and the received block number and received complement
are consistent with each other.
Boolean member variable syncLoss will only be set to
true when goodBlk is set to true AND there is a
fatal loss of syncronization as described in the XMODEM
specification.
The member variable goodBlk1st will be made true only if this is the first
time that the block was received in "good" condition. Otherwise
goodBlk1st will be made false.
*/
void ReceiverY::getRestBlk()
{
	if (rcvBlk[0] == CAN) {
		PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 1, 1), 1);
		return;
	}
	// ********* this function must be improved ***********
	PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 0, 0), REST_BLK_SZ_CRC);
	goodBlk1st = goodBlk = true;
	// My code below *******************
	//first check if the blk number and its complement has not been modified
	if ((rcvBlk[1] + rcvBlk[2]) != 255) {
		goodBlk = false;
		goodBlk1st = false;
		syncLoss = false;
		return;
	}
	else { //the blk # and its complement is good
		// have a counter to keep track
		//if our received blk doesnt match our counter
		if (rcvBlk[1] != numLastGoodBlk) {
			if (rcvBlk[1] != (numLastGoodBlk + 1)) {
				if (rcvBlk[1] == 0 && numLastGoodBlk == 255) {
				}
				else {
					syncLoss = true;
					goodBlk = false;
					goodBlk1st = false;
				}
			}
		}
		//numLastGoodBlk++;
		//recalculate the crc and see if it matches received blk
		/* calculate and add CRC in network byte order */
		uint16_t myCrc16ns;
		crc16ns(&myCrc16ns, &rcvBlk[3]);
		uint8_t temp_crc1 = myCrc16ns & 0xFF;   //take last 8 bits
		uint8_t  temp_crc2 = myCrc16ns >> 8; // take first 8 bits
		if (rcvBlk[131] != temp_crc1 || rcvBlk[132] != temp_crc2) {
			goodBlk = false;
			goodBlk1st = false;
		}
		if (goodBlk1st) {
			numLastGoodBlk = rcvBlk[1];
		}
	}
}
//Write chunk (data) in a received block to disk
void ReceiverY::writeChunk()
{
	PE_NOT(myWrite(transferringFileD, &rcvBlk[DATA_POS], CHUNK_SZ), CHUNK_SZ);
}
int
ReceiverY::
openFileForTransfer()
{
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	transferringFileD = myCreat((const char*)&rcvBlk[DATA_POS], mode);
	return transferringFileD;
}
int
ReceiverY::
closeTransferredFile()
{
	//return myClose(transferringFileD);
	this->closedOk = myClose(transferringFileD);
	return this->closedOk;
}
uint8_t
ReceiverY::
checkForAnotherFile()
{
	return (anotherFile = rcvBlk[DATA_POS]);
}
//Send CAN_LEN CAN characters in a row to the XMODEM sender, to inform it of
//  the cancelling of a file transfer
void ReceiverY::cans()
{
	// no need to space in time CAN chars coming from receiver
	char buffer[CAN_LEN];
	memset(buffer, CAN, CAN_LEN);
	PE_NOT(myWrite(mediumD, buffer, CAN_LEN), CAN_LEN);
}
void ReceiverY::receiveFiles()
{
	int state = 0;
	ReceiverY& ctx = *this; // needed to work with SmartState-generated code
	// ***** improve this member function *****
	// below is just an example template.  You can follow a
	//  different structure if you want.
	ctx.errCnt = 0;
	while (state != 2) {
		switch (state) {
		case 0:
			while (
				sendByte(ctx.NCGbyte),
				PE_NOT(myRead(mediumD, &rcvBlk[0], 1), 1), // Should be SOH
				ctx.getRestBlk(), // get block 0 with fileName and filesize
				checkForAnotherFile()) {
				if (ctx.openFileForTransfer() == -1) {
					cans();
					result = "CreatError"; // include errno or so
				   // return;
				}
				else {
					//    sendByte(ACK); // acknowledge block 0 with fileName.
					if (!ctx.syncLoss && (ctx.errCnt < errB) && !ctx.goodBlk) {
						ctx.sendByte(NAK);
						ctx.errCnt++;
						// continue;
					}
					if (!ctx.syncLoss && (ctx.errCnt < errB) && ctx.goodBlk) {
						ctx.checkForAnotherFile();
						if (ctx.anotherFile) {
							ctx.openFileForTransfer();
							if (transferringFileD == -1) {
								cans();
								result = "CreatError"; // include errno or so
								return;
							}
							else {
								ctx.sendByte(ACK);
								sendByte(ctx.NCGbyte);
							}
						} // anotherFile if statement
						else { //!anotherfile
							ctx.sendByte(ACK);
							ctx.result = "Done";
							return;
						}
					}
					else if (ctx.syncLoss || ctx.errCnt >= errB) { // CondTransientStat
						//yes syncloss or err > errb
						ctx.cans();
						if (ctx.syncLoss)
							ctx.result = "LossOfSync at Stat Blk";
						else
							ctx.result = "ExcessiveErrors at Stat";
						return;
					}
					// inform sender that the receiver is ready and that the
					//      sender can send the first block
					while (PE_NOT(myRead(mediumD, rcvBlk, 1), 1), (rcvBlk[0] == SOH))
					{
						ctx.getRestBlk();
						if (ctx.goodBlk1st) {
							ctx.errCnt = 0;
							ctx.anotherFile = 0;
						}
						else {
							ctx.errCnt++;
						}
						// check if syncloss or too many errors
						if (ctx.syncLoss || ctx.errCnt >= errB) {
							ctx.cans();
							ctx.closeTransferredFile();
							if (ctx.syncLoss) {
								ctx.result = "LossOfSyncronization";
							}
							else {
								ctx.result = "ExcessiveErrors";
							}
							return;
						}
						if (!ctx.syncLoss && (ctx.errCnt < errB)) {
							if (ctx.goodBlk) {
								ctx.sendByte(ACK);
								// ctx.sendByte(ACK);
								if (ctx.anotherFile) {
									ctx.sendByte(ctx.NCGbyte);
								}
							}
							else {
								ctx.sendByte(NAK);
								if (ctx.goodBlk1st) {
									ctx.writeChunk();
								}
							}
						}
					};
					// assume EOT was just read in the condition for the while loop
					ctx.sendByte(NAK); // NAK the first EOT
					PE_NOT(myRead(mediumD, rcvBlk, 1), 1); // presumably read in another EOT
					// Check if the file closed properly.  If not, result should be "CloseError".
					if (ctx.closeTransferredFile()) {
						// ***** fill this in *****
						if (!ctx.closedOk) {
							ctx.cans();
							ctx.result = "CloseError";
						}
						ctx.sendByte(ACK);  // ACK the second EOT
						ctx.sendByte(ctx.NCGbyte);
						ctx.errCnt = 0;
						// ctx.result = "Done";
					}
					else {
						sendByte(ACK); // acknowledge empty block 0.
						ctx.result = "Done";
					}
				}
			}
			state = 1;
		case 1:
			if (rcvBlk[0] == CAN) {
				if (transferringFileD != -1) {
					ctx.closeTransferredFile();
					ctx.result = "SndCancelled";
				}
				ctx.result = "Got CANS";
				if (checkForAnotherFile() == 0) {
					//                    sendByte(ctx.NCGbyte);
					//                    PE_NOT(myRead(mediumD, &rcvBlk[0], 1), 1);
					state = 0;
				}
				else {
					state = 2;
				}
			}
			else {
				state = 2;
			}
			break;
		case 2:
			break;
		}
	}
	sendByte(NAK); // acknowledge empty block 0.
 //   ctx.result = "Done";
}
