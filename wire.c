// wire.c:		1-wire related functions...
#include <s_sysinc.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "wire.h"




// Search state   /////////////////////////////////////////////////////////////////!!!!!!!!!!!!!!!!/////////
unsigned char ROM_NO[8]  = {0};
BYTE OWID[2][8]  = {0};
int LastDiscrepancy;
int LastFamilyDiscrepancy;
int LastDeviceFlag;
unsigned char crc8;

extern DS2480Config DSconfig;
/*
/////////////////////////////////////////////////////////////////////////////!!!!!!!!!!!!!!!!!!!//////////// IO0 is the default channel
//--------------------------------------------------------------------------
// Select the 1-Wire channel on a DS2482-800.
//
// Returns: true if channel selected
// false device not detected or failure to perform select
//
int DS2482_channel_select(int channel)
{
unsigned char ch, ch_read, check;
// Channel Select (Case A)
// S AD,0 [A] CHSL [A] CC [A] Sr AD,1 [A] [RR] A\ P
// [] indicates from slave
// CC channel value
// RR channel read back
I2C_start();
I2C_write(I2C_address | I2C_WRITE, EXPECT_ACK);
I2C_write(CMD_CHSL, EXPECT_ACK);
switch (channel)
{
default: case 0: ch = 0xF0; ch_read = 0xB8; break;
case 1: ch = 0xE1; ch_read = 0xB1; break;
case 2: ch = 0xD2; ch_read = 0xAA; break;
case 3: ch = 0xC3; ch_read = 0xA3; break;
case 4: ch = 0xB4; ch_read = 0x9C; break;
case 5: ch = 0xA5; ch_read = 0x95; break;
case 6: ch = 0x96; ch_read = 0x8E; break;
case 7: ch = 0x87; ch_read = 0x87; break;
};
I2C_write(ch, EXPECT_ACK);
I2C_rep_start();
I2C_write(I2C_address | I2C_READ, EXPECT_ACK);
check = I2C_read(NACK);
I2C_stop();
// check for failure due to incorrect read back of channel
return (check == ch_read);
}
*/

// owWriteBit
//
// Writes a single bit to 1-Wire using the DS2482.
//
// value - bit to be written to the 1-Wire (0 or 1)
BYTE OWWriteBit(BYTE value)
{
	BYTE buff[3];
	BYTE test;
	
	
	while (OWBusy());
	
	if(value)
		buff[0] = 0xFF;
	else
		buff[0] = 0x7F; 
	if (!I2C_Write(OWDAddress, OWBitCommand, 0, 1, buff))
		return 0;
	Delay(10);
	// checking if 1-Wire busy
	
	while (OWBusy());
	return 1;
}

// owReadBit
//
// Returns 0 or 1 for the bit read.
BYTE OWReadBit()				   // !! should it write a 1 first? -yes
{
	BYTE buff[3];
	BYTE status[1];	

	while (OWBusy());
	status[0] = StatReg;
	OWWriteBit(1);
	I2C_Write(OWDAddress, OWSetReadPtrCommand, 0, 1, status);		// set pointer to read
	I2C_Read(OWDAddress, OWReadByteCommand, 0, 1, buff);
	while (OWBusy());
	if (buff[0] & 0x20)
		return 1;	
	else
		return 0;
}




// writeOneWireByte
//
// Writes a 1-Wire byte using I2C commands sent to the DS2482.
//
// wrByte - The byte to be written to the 1-Wire.
// returns - Nothing
BYTE OWWriteByte(BYTE wrByte)
{
	BYTE buff[1];

    while (OWBusy());
	I2C_Write(OWDAddress, OWWriteByteCommand, 0, 1, &wrByte);
    do {
        I2C_Read(OWDAddress, OWOperation, 0, 1, buff);
    } while ((buff[0] & STATUS_BUSY)); // Wait for busy bit to clear, similar to OWBusy()

	return 1;
} // OWWriteByte


// readOneWireByte
//
// Reads a 1-Wire byte using I2C commands to the DS2482.
//
// returns the byte read
//
BYTE OWReadByte(BYTE readRegister)
{
	BYTE result;

	while (OWBusy());

	I2C_Write(OWDAddress, OWSetReadPtrCommand, 0, 1, &readRegister); // set pointer to read
	I2C_Read(OWDAddress, OWOperation, 0, 1, &result);

	while (OWBusy());

	return result;
} // OWReadByte


// resetOneWire			
//
// Resets the 1-Wire using I2C through the DS2482.
//
// returns result
//
BYTE OWReset(void)
{
	BYTE reset;
	BYTE status[1];	

	I2C_Read(OWDAddress, OWResetCommand, 0, 1, status);	
   
    do {
        I2C_Read(OWDAddress, OWOperation, 0, 1, status);
    } while (!(status[0] & STATUS_PPD)); // Wait for Presense Pulse to be detected
    do { 
        I2C_Read(OWDAddress, OWOperation, 0, 1, status);
    } while ((status[0] & STATUS_BUSY)); // Wait for busy bit to clear, similar to OWBusy()
   
    reset = (status[0] & STATUS_PPD);	

	return reset;
}

// DS2480 device reset			
//
// Resets the DS2482.
//
// returns result
//
BYTE OWDReset(void)
{
	unsigned char buff[1];
	BYTE status;	

	// checking if 1-Wire busy
	while (OWBusy());

	I2C_Read(OWDAddress, OWDeviceResetCommand, 0, 1, buff);
    I2C_Read(OWDAddress, OWOperation, 0, 1, &status);

	while (OWBusy());

	return status & STATUS_RST;
}

BYTE OWDWriteConfig(DS2480Config *pDSconfig) {
    BYTE config;
	char hak[10];
    // checking if 1-Wire busy
	while (OWBusy());
    config = (pDSconfig->WS)<<3 | (pDSconfig->SPU)<<2 | (pDSconfig->APU);
    config |= ~config<<4;
    sprintf(hak, "%X\n", config);
    CommSend(COMM_EXT, hak);

    I2C_Write(OWDAddress, OWDConfig, 0, 1, &config);

    while (OWBusy());
    return 1;
}


																	 
// returns 0 if 1wire is busy, needs timer so it doesn't stay here forever
BYTE OWBusy(void)
{
	BYTE status = StatReg;
	BYTE buff;

	I2C_Write(OWDAddress, OWSetReadPtrCommand, 0, 1, &status);		// set pointer to read	
	I2C_Read(OWDAddress, OWOperation, 0, 1, &buff);						// read

	return buff & (1<<0);
}

WORD DS18B20_readTemp(BYTE deviceNumber)
{
    char hak[200];
    char ret[9];
    WORD temp;

    while(!OWReset()); 
    OWWriteByte(0x55); // Match ROM
   
    for (int i=7;i>=0;i--) {
        OWWriteByte(OWID[deviceNumber][i]);
    }
    DSconfig.APU = 1;
    DSconfig.SPU = 1;
    DSconfig.WS = 0;
    OWDWriteConfig(&DSconfig);
    
    sprintf(hak, "conf=%x\n", OWReadByte(ConfigReg));

    OWWriteByte(0x44); // Convert T

    Delay(1000); // Wait for measurement
    while(!OWReset());
    OWWriteByte(0x55); // Match ROM
   
  
    for (int i=7;i>=0;i--) {
        OWWriteByte(OWID[deviceNumber][i]);
    }


    OWWriteByte(0xBE); // Read Scratchpad
    hak[0] = DataReg;
    for (int i=0;i<9;i++) {
        I2C_Write(OWDAddress, OWReadByteCommand, 0, 0, hak);
        Delay(10);	  	
        I2C_Write(OWDAddress, OWSetReadPtrCommand, 0, 1, hak);		// set pointer to read
        Delay(10);
        I2C_Read(OWDAddress, OWOperation, 0, 1, &ret[i]);
    }
    temp = (ret[1]<<8 | ret[0]);
    sprintf(hak, "celcius = %.04f\n", temp*0.0625);
	CommSend(COMM_EXT, hak);

    return 1;

}





int OWDeviceSearch() 
{
    BYTE result;

    if (OWFirst()==1)
        while(OWNext()==1);
}



//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire network
// Return 1: device found, ROM number in ROM_NO buffer
//  	  0: no device present
//

int OWFirst()
{
	// reset the search state
	LastDiscrepancy = 0;
	LastDeviceFlag = 0;
	LastFamilyDiscrepancy = 0;
   
	return OWSearch();
}

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire network
// Return 1: device found, ROM number in ROM_NO buffer
// 		  0: device not found, end of search
//
int OWNext()
{
  
// leave the search state alone
    return OWSearch();
}

//--------------------------------------------------------------------------
// The 'OWSearch' function does a general search. This function
// continues from the previous search state. The search state
// can be reset by using the 'OWFirst' function.
// This function contains one parameter 'alarm_only'.
// When 'alarm_only' is true (1) the find alarm command
// 0xEC is sent instead of the normal search command 0xF0.
// Using the find alarm command 0xEC will limit the search to only
// 1-Wire devices that are in an 'alarm' state.
//
// Returns: 1: when a 1-Wire device was found and its
// Serial Number placed in the global ROM
//    		0: when no new device was found. Either the
// last search was the last device or there
// are no devices on the 1-Wire Net.
//
int OWSearch()
	{
	char a[200];
	int id_bit_number;
	int last_zero, rom_byte_number, search_result;
	int id_bit, cmp_id_bit;
	unsigned char rom_byte_mask, search_direction, status;
    static BYTE iter = 0;
	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = false;
	crc8 = 0;
	// if the last call was not the last one
  
	if (!LastDeviceFlag)
		{ OWReset();
			/*if (!OWReset())
                {
                // reset the search
                LastDiscrepancy = 0;
                LastDeviceFlag = 0;
                LastFamilyDiscrepancy = 0;
                CommSend(COMM_EXT, "1-WIRE RESET ERROR\n");
                return false;
            }
            */
		// issue the search command
		OWWriteByte(0xF0);
		
		// loop to do the search
		do
			{ // if this discrepancy if before the Last Discrepancy
			// on a previous next then pick the same as last time
			if (id_bit_number < LastDiscrepancy)
				{
				if ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0)
					search_direction = 1;
				else
					search_direction = 0;
				}
			else
				{
				// if equal to last pick 1, if not then pick 0
				if (id_bit_number == LastDiscrepancy)
					search_direction = 1;
				else
					search_direction = 0;
				}
			// Perform a triple operation on the DS2482 which will perform
			// 2 read bits and 1 write bit
			status = DS2482_search_triplet(search_direction);
           
			// check bit results in status byte
            id_bit = ((status & STATUS_SBR) == STATUS_SBR);
            cmp_id_bit = ((status & STATUS_TSB) == STATUS_TSB);
            
            search_direction = ((status & STATUS_DIR) == STATUS_DIR) ? 1 : 0;    
                   
			// check for no devices on 1-Wire
			if ((id_bit) && (cmp_id_bit))
				break;
			else
				{
				if ((!id_bit) && (!cmp_id_bit)  && (search_direction == 0))
					{
                       // sprintf(a, "!!!!!!id_bit = %d, cmp_id_bit = %d, search_direction = %d, %d\n", id_bit, cmp_id_bit, search_direction, id_bit_number);
                       // CommSend(COMM_EXT, a);
					last_zero = id_bit_number;
					// check for Last discrepancy in family
					if (last_zero < 9)
					LastFamilyDiscrepancy = last_zero;
					}
				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1)
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				else
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;
				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;
				// if the mask is 0 then go to new SerialNum byte rom_byte_number
				// and reset mask
				if (rom_byte_mask == 0)
					{
					//calc_crc8(ROM_NO[rom_byte_number]); // accumulate the CRC
					rom_byte_number++;
					rom_byte_mask = 1;
					}
				}
               //  sprintf(a, " %d, %d, %d, %d\n", id_bit, cmp_id_bit, id_bit_number-1, search_direction);
               //   CommSend(COMM_EXT, a); 
			}
		while(rom_byte_number < 8); // loop until through all ROM bytes 0-7
		// if the search was successful then
		if (!((id_bit_number < 65)))//if (!((id_bit_number < 65) || (crc8 != 0)))
			{
			// search successful so set LastDiscrepancy,LastDeviceFlag
			// search_result
			LastDiscrepancy = last_zero;           
			// check for last device
			if (LastDiscrepancy == 0)
			    LastDeviceFlag = true;
            search_result = true;
			}
		}
	// if no device found then reset counters so next
	// 'search' will be like a first
	if (!search_result || (ROM_NO[0] == 0))
		{
		LastDiscrepancy = 0;
		LastDeviceFlag = false;
		LastFamilyDiscrepancy = 0;
		search_result = false;       
		}
        int i,j;
    if (search_result) {
        for (i=0, j=7;i<8;i++, j--) {
            OWID[iter][j] = ROM_NO[i];
        }     
        sprintf(a, "ID = %02X%02X%02X%02X%02X%02X%02X%02X\n", OWID[iter][0], OWID[iter][1], OWID[iter][2], OWID[iter][3], OWID[iter][4], OWID[iter][5], OWID[iter][6], OWID[iter][7]);
        CommSend(COMM_EXT, a);

        ++iter;
        if (LastDeviceFlag) {
            CommSend(COMM_EXT, "LAST DEVICE\n");   
            iter = 0;
            return 2;
        }
    }
    else {
        CommSend(COMM_EXT, "NO DEVICES FOUND\n");
        iter = 0;
    }
	return search_result;
}


//--------------------------------------------------------------------------
// Use the DS2482 help command '1-Wire triplet' to perform one bit of a
//1-Wire search.
//This command does two read bits and one write bit. The write bit
// is either the default direction (all device have same bit) or in case of
// a discrepancy, the 'search_direction' parameter is used.
//
// Returns � The DS2482 status byte result from the triplet command
//
unsigned char DS2482_search_triplet(int search_direction)
{
	unsigned char status;
	int poll_count = 0;
    char hak[2];
	// 1-Wire Triplet (Case B)
	// S AD,0 [A] 1WT [A] SS [A] Sr AD,1 [A] [Status] A [Status] A\ P
	// \--------/
	// Repeat until 1WB bit has changed to 0
	// [] indicates from slave
	// SS indicates byte containing search direction bit value in msbit

	// checking if 1-Wire busy
	while (OWBusy());
    hak[0] = search_direction ? 0x80 : 0x00;
	I2C_Write(OWDAddress, OWTriplet, 0, 1, hak);
	while (OWBusy());
	status = OWReadByte(StatReg);
	// return status byte
	return status;
}