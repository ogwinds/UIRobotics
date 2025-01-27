// File Inclusion
#include "i2c_lib.h"
#include "hardware.h"
#include "swDelay.h"

#include <plib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

///* -------------------------- Function Prototyping --------------------------- */


/* --------------------------------- I2C_Init --------------------------------
 @ Summary
    Initializes the I2C port at the desired speed,
 @ Parameters
    @ param1 : I2C_MODULE that is the port to be configured, I2C1 or I2C2
    @ param2 : Speed to open the port at
 @ Return Value
    I2C_RESULT : Flag for whether or not the initializiation errored or not
  ---------------------------------------------------------------------------- */
I2C_RESULT I2C_Init(I2C_MODULE i2c_port,  int speed) {
	BOOL i2cFlag;
	int actualClock;
	// Set Desired Operation Frequency
	actualClock = I2CSetFrequency(i2c_port, GetPeripheralClock(), speed);
	if (abs(actualClock - speed) > speed / 10) {
		printf("I2C setup error\n\r");
		printf("Clock frequency (%d) error exceeds 10\%\r\n", (unsigned int) actualClock);
		i2cFlag = I2C_ERROR;
	}
	else {
		printf("I2C%d setup complete\n\r", i2c_port+1);
		printf("I2C clock frequency = %ld Hz\n\r", (unsigned int) actualClock);
		I2CEnable(i2c_port, TRUE);
		i2cFlag = I2C_SUCCESS;
	}
    
	return i2cFlag;
}

/* -------------------------------- I2C_Write --------------------------------
 @ Summary
    Send the provided string through the provided port to the provided device
 @ Parameters
    @ param1 : Which I2C port to send the data through, I2C1 or I2C2
    @ param2 : Device address that's being address (i.e. Mag, or GPS, etc.)
    @ param3 : A String (char *) to be sent to the device
    @ param4 : Length of the message (param3) being sent
 @ Return Value
    I2C_RESULT : Flag for whether or not the transfer errored or not
  ---------------------------------------------------------------------------- */
I2C_RESULT I2C_Write(I2C_MODULE i2c_port, BYTE DeviceAddress, BYTE *str, int *len) {
	I2C_7_BIT_ADDRESS SlaveAddress;
	int dataIndex		  = 0;
	I2C_RESULT i2c_result = I2C_SUCCESS;	// Status of I2C action
	BOOL okay 			  = TRUE;

	/* ------------------ Initialize an I2C Sequence Start ------------------- */
	okay = StartTransfer(i2c_port, TRUE); // TRUE - repeated start
	if ((i2c_result == I2C_SUCCESS) && okay) {
		I2C_FORMAT_7_BIT_ADDRESS(SlaveAddress, DeviceAddress, I2C_WRITE);
		if (!TransmitOneByte(i2c_port, SlaveAddress.byte)) {
			if (!I2CByteWasAcknowledged(i2c_port)) {
				printf("Error: Sent byte was not acknowledged\n");
				i2c_result = I2C_ERROR;
			}
		}
		dataIndex = 0;  // Reset the data array index
	}
	
	/* ------------------------- Send the data bytes ------------------------- */
	while ((dataIndex < *len) && okay) {
		okay = TransmitOneByte(i2c_port, str[dataIndex++]);	// Send byte, read response
	}

	/* -------------- Send the stop bit, ending the I2C transfer ------------- */
	StopTransfer(i2c_port); // Terminate the EEPROM transfer

	i2c_result = okay ? I2C_SUCCESS : I2C_ERROR;

	*len = dataIndex;
    
	return i2c_result;
}

/* -------------------------------- I2C_Read ---------------------------------
 @ Summary
    Read from the provided I2C device over the given port
 @ Parameters
    @ param1 : Which I2C port to read the data through, I2C1 or I2C2
    @ param2 : Device address that's being address (i.e. Mag, or GPS, etc.)
    @ param3 : A String (char *) to be read from the device
    @ param4 : Length of the message (param3) being read
 @ Return Value
    I2C_RESULT : Flag for whether or not the transfer errored or not
  ---------------------------------------------------------------------------- */
I2C_RESULT I2C_Read(I2C_MODULE i2c_port, BYTE DeviceAddress, BYTE *str, int *len) {
	I2C_7_BIT_ADDRESS SlaveAddress;
	int dataIndex 		  = 0;
	I2C_RESULT i2c_result = I2C_SUCCESS;   // Status of I2C action
	BOOL okay;

	/* ------------------- Initialize a I2C Sequence Start ------------------- */
	okay = StartTransfer(i2c_port, FALSE);  // FALSE - Not repeated start
	if ((i2c_result == I2C_SUCCESS) && okay) {
		I2C_FORMAT_7_BIT_ADDRESS(SlaveAddress, DeviceAddress, I2C_READ);
		okay = TransmitOneByte(i2c_port, SlaveAddress.byte);
		if (!okay) {
			printf("I2C error - ");
			okay = I2CByteWasAcknowledged(i2c_port);
			if (!okay) {
				printf("Sent byte was not acknowledged\n\r");
			}
			else {
				printf("\n\r");
			}
			i2c_result = I2C_ERROR;
		}
		dataIndex = 0;  // Reset the data array index
	}

	/* - Read each data byte (except the last), sending an ACK bit each time - */
	while ((dataIndex < (*len)-1) && (i2c_result == I2C_SUCCESS)) {
		i2c_result |=  ReceiveOneByte(i2c_port, &str[dataIndex++] , TRUE);
	}

	/* ------------ Read the last data byte, but send to ACK bit ------------- */
	if (i2c_result == I2C_SUCCESS) {
		i2c_result |= ReceiveOneByte(i2c_port, &str[dataIndex++], FALSE );
	}

	/* -------------- Send the stop bit, ending the I2C transfer ------------- */
	StopTransfer(i2c_port);

	*len = dataIndex;
    
	return i2c_result;
}

/* ------------------------------- I2CReadRegs -------------------------------
 @ Summary
    Read the specified number of bytes from the EEPROM, starting at the 
    specified memory address
 @ Parameters
    @ param1 : Which I2C port to read, I2C1 or I2C2
    @ param2 : Device address that's being address (i.e. Mag, or GPS, etc.)
    @ param3 : Register starting address (0x0000 to 0x7FFF)
    @ param4 : Data array that is filled upon data read
    @ param5 : Number of data bytes to read
 @ Return Value
    I2C_RESULT : Flag for whether or not the transfer errored or not
  ---------------------------------------------------------------------------- */
I2C_RESULT I2CReadRegs(I2C_MODULE i2c_port, BYTE DeviceAddress, int reg_addr, BYTE *i2cData, int len) {
	BYTE header[3];
	I2C_7_BIT_ADDRESS SlaveAddress;
	int headerIndex = 0;
	int dataIndex   = 0;
	I2C_RESULT i2c_result;
	BOOL okay;

	/* ---------------- Setup EEPROM header w/ write sequence ---------------- */
	I2C_FORMAT_7_BIT_ADDRESS(SlaveAddress, DeviceAddress, I2C_WRITE);
	header[0]   = SlaveAddress.byte;
	header[1]   = (BYTE) (reg_addr >> 8);
	headerIndex = 0;

	/* - Write the starting address. Cannot use a STOP bit, instead a repeat - */
	/* ------------ START bit will initiate the memory read phase. ----------- */
    okay = StartTransfer(i2c_port, FALSE); // FALSE - no repeated start
    i2c_result = I2C_SUCCESS;

	while ((headerIndex < 2) && (i2c_result == I2C_SUCCESS) && okay) {
		if (TransmitOneByte(i2c_port, header[headerIndex++])) {
			// Verify that the byte was acknowledged
			if(!I2CByteWasAcknowledged(i2c_port)) {
				printf("Error: Sent byte was not acknowledged\n");
				i2c_result |= I2C_ERROR;
			}
		}
		else {
			i2c_result |= I2C_ERROR;
		}
	}

	/* ---------- Continue the read sequence if there was no error ----------- */
    if (i2c_result == I2C_SUCCESS) {
		okay = StartTransfer(i2c_port, TRUE); // TRUE - repeated start
		if ((i2c_result == I2C_SUCCESS) && okay) {
			I2C_FORMAT_7_BIT_ADDRESS(SlaveAddress, DeviceAddress, I2C_READ);
			if (!TransmitOneByte(i2c_port, SlaveAddress.byte)) {
			    if (!I2CByteWasAcknowledged(i2c_port)) {
			        printf("Error: Sent byte was not acknowledged\n");
			        i2c_result = I2C_ERROR;
			    }
			}
			dataIndex = 0;	// Reset the data array index
		}

		/* -------- Read each data byte (except the last) w/ ACK bit --------- */
		while ((dataIndex < len-1) && (i2c_result == I2C_SUCCESS)) {
			i2c_result |=  ReceiveOneByte(i2c_port, &i2cData[dataIndex++], TRUE);
		}
		/* ------------- Read the last data byte w/o an ACK bit -------------- */
		if (i2c_result == I2C_SUCCESS) {
			i2c_result |= ReceiveOneByte(i2c_port, &i2cData[dataIndex], FALSE );
		}
	}

	/* -------------- Send the stop bit, ending the I2C transfer ------------- */
    StopTransfer(i2c_port);
    
    return i2c_result;
}


/* ----------------------------- TransmitOneByte -----------------------------
 @ Summary
    Send one byte to the EEPROM
 @ Parameters
    @ param1 : Which I2C port to send the byte to, I2C1 or I2C2
    @ param2 : Data to transmit
 @ Return Value
    TRUE  : Data was sent successfully
    FALSE : A bus collusion / some error occurred
 @ Notes
 	This function utilizes a blocking I2C routine
  ---------------------------------------------------------------------------- */
static BOOL TransmitOneByte(I2C_MODULE i2c_port, BYTE data) {
	// Wait for the transmitter to be ready
	while(!I2CTransmitterIsReady(i2c_port));

	// Transmit the data byte
	if (I2CSendByte(i2c_port, data) == I2C_MASTER_BUS_COLLISION) {
		printf("Error: I2C Master Bus Collision\n");
		return FALSE;
	}
	// Wait for the transmission to finish
	while (!I2CTransmissionHasCompleted(i2c_port));

	return TRUE;
}

/* ----------------------------- TransmitOneByte -----------------------------
 @ Summary
    Read one byte to the EEPROM
 @ Parameters
    @ param1 : Which I2C port to read the byte from, I2C1 or I2C2
    @ param2 : Data byte to recieve
    @ param3 : Boolean of whether or not to send an ACK on each data read
 @ Return Value
    I2C_RESULT : Flag of whether or not an error occured
 @ Notes
 	This function utilizes a blocking I2C routine
  ---------------------------------------------------------------------------- */
static I2C_RESULT ReceiveOneByte(I2C_MODULE i2c_port, BYTE *data, BOOL ack) {
	I2C_RESULT i2c_result = I2C_SUCCESS;
	if (I2CReceiverEnable(i2c_port, TRUE) == I2C_RECEIVE_OVERFLOW) {
		printf("Error: I2C Receive Overflow\n");
		i2c_result =  I2C_RECEIVE_OVERFLOW;
	}
    else {
    	// Wait for the bus to become free
		while (!I2CReceivedDataIsAvailable(i2c_port));
		// The ACK paramater determines whether or not the EERPOM read was acknowledged
		I2CAcknowledgeByte(i2c_port, ack);
		while (!I2CAcknowledgeHasCompleted(i2c_port));
		// Read the received data byte
		*data = I2CGetByte(i2c_port);
	}
    
	return i2c_result;
}

/* ------------------------------ StartTransfer ------------------------------
 @ Summary
    Starts or restarts a transfer to / from the EEPROM
 @ Parameters
    @ param1 : Which I2C port to read the byte from, I2C1 or I2C2
    @ param2 : Boolean of whether to send a START or RESTART bit
 @ Return Value
    TRUE  : Successful transfer initiated
    FALSE : Unsuccessful transfer was initiated
 @ Notes
 	This function utilizes a blocking I2C routine
  ---------------------------------------------------------------------------- */
static BOOL StartTransfer(I2C_MODULE i2c_port, BOOL restart) {
	I2C_STATUS  status;

	/* ------------------- Initialize a I2C Sequence Start ------------------- */
	if (restart) {
		I2CRepeatStart(i2c_port);
	}
	else {
		// Wait for the bus to be free
		while (!I2CBusIsIdle(i2c_port));

		if (I2CStart(i2c_port) != I2C_SUCCESS) {
			printf("Error: Bus collision during transfer Start\n");
			return FALSE;
		}
	}

	// Wait for the START or REPEAT START to finish
	do {
		status = I2CGetStatus(i2c_port);
	} while (!(status & I2C_START));

	return TRUE;
}

/* ------------------------------- StopTransfer ------------------------------
 @ Summary
    Sends a stop bit to the provided I2C module
 @ Parameters
    @ param1 : Which I2C port to read the byte from, I2C1 or I2C2
 @ Return Value
    None
 @ Notes
 	This function only works if a transfer has been initialized / started.
 	This is also implemented with a blocking I2C routine
  ---------------------------------------------------------------------------- */
static void StopTransfer( I2C_MODULE i2c_port) {
	I2C_STATUS  i2c_status;

	// Send the STOP sequence
	I2CStop(i2c_port);

	// Wait for the STOP sequence to finish
	do {
		i2c_status = I2CGetStatus(i2c_port);
	} while (!(i2c_status & I2C_STOP));
}

/* -------------------------------- calc_ck_sum ------------------------------
 @ Summary
    Computes the checksum of an NMEA sentence between the "$" and "*" characters
 @ Parameters
    @ param1 : String containing the GPS sentence to be checked
 @ Return Value
    BYTE : The checksum of the sentence
  ---------------------------------------------------------------------------- */
static BYTE calc_ck_sum(char *str) {
	BYTE cksum = 0;
	BOOL start_flag = FALSE;
	BOOL stop_flag = FALSE;

	do {	// Search for "$" character
		if (start_flag == FALSE) {
			if (*str == 0x24) {
				start_flag = TRUE;
			}
		}
		else {	// Search for "*" character signalling the end of sentence 
			if (*str == '*') {
				stop_flag = TRUE;	// Mark end of GPS sentence
			}
			else {
				cksum ^= (BYTE) (*str);	// Update check sum
			}
		}
		str++;
	} while ((*str != 0x00) && (stop_flag == FALSE));	// End of string or "*"
    
	return cksum;
}

/* I2C_WrtiteDev FUNCTION DESCRIPTION *************************************
*
* SYNTAX:      I2C_RESULT I2C_WrtiteBlk(DATA_BLOCK blk, BOOL RepStart);
* KEYWORDS:	Write,  acknowledgement, write cycle complete
* DESCRIPTION:	Writes a block of data to an I2C device
*
* Parameter 1:	    typedef struct
*                  {
*                      I2C_MODULE i2c_channel;
*                      BYTE dev_id;
*                      BYTE reg_addr;
*                      BYTE block_size;
*                      BYTE *data; // optional
*                      } DATA_BLOCK;
*
* RETURN VALUE: I2C_RESULT - i2c transaction results
* NOTES:		This write algorithm writes data in 64 byte boundries that
*				is compatible with I2C EEPROM devices.
* END DESCRIPTION **********************************************************/
I2C_RESULT I2C_WriteDev( I2C_DATA_BLOCK blk)
{
	I2C_7_BIT_ADDRESS   SlaveAddress;
	int count = 0;
	I2C_RESULT i2c_ops;     // Status of I2C action 
	BOOL i2c_started;       // Flag that indicates that I2C start successful 
	BYTE *data_ptr;         // Data buffer pointer 

	i2c_ops = I2C_SUCCESS;
	data_ptr = blk.data;    // Set up pointer to data array

	I2C_FORMAT_7_BIT_ADDRESS(SlaveAddress, blk.dev_id, I2C_WRITE);

	// Send device and memory address
	i2c_started = StartTransfer(blk.i2c_channel, FALSE); /* */
	if (i2c_started)
	{
		// Send device ID 
		if (TransmitOneByte(blk.i2c_channel, SlaveAddress.byte))
		{
			// Verify that the byte was acknowledged 
			if (!I2CByteWasAcknowledged(blk.i2c_channel))
			{
				i2c_ackError(8);
				i2c_ops = I2C_ERROR;
			}
		}

		// Send Register number 
		if (i2c_ops == I2C_SUCCESS)
		{
			if (TransmitOneByte(blk.i2c_channel, blk.reg_addr))
			{
				usDelay(35);    // Delay >30us after device ID 

								// Verify that the byte was acknowledged 
				if (!I2CByteWasAcknowledged(blk.i2c_channel))
				{
					i2c_ackError(9);
					i2c_ops = I2C_ERROR;
				}
			}
		}

		while ((count < blk.block_size) && (i2c_ops == I2C_SUCCESS))
		{
			if (TransmitOneByte(blk.i2c_channel, *data_ptr++))
			{
				// Verify that the byte was acknowledged
				if (!I2CByteWasAcknowledged(blk.i2c_channel))
				{
					i2c_ackError(10);
					i2c_ops = I2C_ERROR;
				}
				else
					count++;    // increment byte counter
			}
			else i2c_ops = I2C_ERROR;   // Byte was not sent
		}
		StopTransfer(blk.i2c_channel);
	}
	else i2c_ops = I2C_ERROR;   // I2C not started 

	return i2c_ops;
}

/* I2C_ReadI2CReadDev FUNCTION DESCRIPTION ***************************************
*
* SYNTAX:      I2C_RESULT I2C_ReadModbus(MODBUS_DATA_BLOCK blk);
* KEYWORDS:    Read Device, device address, memory address
* DESCRIPTION:	Reads a I2C device using the Modbus protocol
*
* Parameter 1:	    typedef struct
*                  {
*                      I2C_MODULE i2c_channel;
*                      BYTE dev_id;
*                      BYTE reg_addr;
*                      BYTE block_size;
*                      BYTE *data;
*                      } MODBUS_DATA_BLOCK;
*
* RETURN VALUE: I2C_RESULT - i2c transaction results
* NOTES:	There is no restriction on the number of bytes that can
* 		be read in a single operation provided that the block size is
*		less than the size of an unsigned integer data type.
*
*              The read operation receives the following data after sending
*              the device ID with the READ bit set high:
*                  READ code - 0x03
*                  Number of bytes to read - N<10
*                  [N data bytes]
*                  2 CRC check bytes
*              There is a NACK sent after the last CRC byte
END DESCRIPTION **********************************************************/
I2C_RESULT I2C_ReadDev(I2C_DATA_BLOCK  blk)
{
	I2C_7_BIT_ADDRESS   SlaveAddress;
	BOOL i2c_started;   /* Flag that indicates that I2C start successful */
	BYTE * data_ptr;    /* Data buffer pointer */
	int count = 0;      /* Byte counter */
	I2C_RESULT i2c_ops = I2C_SUCCESS;     /* Status of I2C action */

										  /* Send device ID with R/W bit set high */
	I2C_FORMAT_7_BIT_ADDRESS(SlaveAddress, blk.dev_id, I2C_READ);
	i2c_started = StartTransfer(blk.i2c_channel, FALSE); /* Repeated start */
	if (i2c_started)
	{
		if (TransmitOneByte(blk.i2c_channel, SlaveAddress.byte))
		{
			if (!I2CByteWasAcknowledged(blk.i2c_channel))
			{
				i2c_ackError(7);
				i2c_ops = I2C_ERROR;
			}
		}
		else  i2c_ops = I2C_ERROR;  /* Bytes was not sent */
		usDelay(35);                /* Wait >30us after sending device ID */
	}
	else i2c_ops = I2C_ERROR;   /* I2C was not started */

								/* While I2C read is successful, get all but the last byte with ACK bit set
								* into data buffer */
	count = 0;
	data_ptr = blk.data;
	while ((count < blk.block_size) && (i2c_ops == I2C_SUCCESS))
	{
		i2c_ops = ReceiveOneByte(blk.i2c_channel, data_ptr++, TRUE);
		count++;
	}
	/* Read the last byte without ACK bit set */
	if (i2c_ops == I2C_SUCCESS)
	{
		i2c_ops |= ReceiveOneByte(blk.i2c_channel, data_ptr, FALSE);
	}
	StopTransfer(blk.i2c_channel); /* Terminate the EEPROM transfer */
	return i2c_ops;
}

// 
// i2c_ackError()
// Simple error message for failure
//
void i2c_ackError(int loc)
{
	printf("Error: Sent byte was not acknowledged at %d\n\r", loc);
	msDelay(10);
}