#ifndef SENDER_H
#define SENDER_H

#include <vector>

#include <unistd.h>
#include <stdint.h> // uint8_t

#include "PeerY.h"

class SenderY : public PeerY
{

public:
	SenderY(std::vector<const char*> iFileNames, int d);
	void statBlk(const char* fileName);
	void prep1stBlk(); // tries to prepare the first block.

	void sendBlkPrepNext(); // While sending the now current block for the first time, prepare the next block if possible.
    void resendBlk(); // Resends the block that had been sent previously to the xmodem receiver.

    int
    //SenderX::
    openFileToTransfer(const char* fileName)
    ;

    int
    //SenderX::
    closeTransferredFile()
    ;

    void cans(); // Send CAN_LEN copies of CAN characters in a row.
    void sendFiles();

    ssize_t bytesRd;  // The number of bytes last read from the input file.

    /* A variable which counts the number of problem responses received. The reception
     *  of an ACK resets the count. */
  unsigned errCnt;    // found in PeerX.h

private:
	std::vector<const char*> fileNames;
//	unsigned fileNameIndex;
	uint8_t blkBuf[BLK_SZ_CRC];     // a  block
	blkT blkBufs[2];	// Array of two blocks

	uint8_t blkNum;		// number of the current block to be acknowledged

    void dumpGlitches(); // get rid of any characters that may have arrived from the medium.

	// Send the block, less the block's last byte, to the receiver
	uint8_t sendMostBlk(blkT blkBuf);
//	uint8_t sendMostBlk(uint8_t blkBuf[BLK_SZ_CRC])
//	;

	// Send the last byte of a block to the receiver
	void
	//SenderX::
	sendLastByte(uint8_t lastByte)
	;

    void genBlk(blkT blkBuf); // tries to generate a block.
//  void genBlk(uint8_t blkBuf[BLK_SZ_CRC])
//  ;
	void genStatBlk(blkT blkBuf, const char* fileName);
	//void genStatBlk(uint8_t blkBuf[BLK_SZ_CRC], const char* fileName)
	//;
};

#endif
