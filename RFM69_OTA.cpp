// **********************************************************************************
// Library for OTA wireless programming of Moteinos using an RFM69 transceiver
// **********************************************************************************
// Hardware requirements:
//   - DualOptiboot bootloader - ships with all Moteinos
//   - SPI "Flash MEM" chip on Moteino (optional)
// Library requirements:
//   - RFM69      - get library at: https://github.com/LowPowerLab/RFM69
//   - SPIFLash.h - get it here: http://github.com/LowPowerLab/SPIFlash
// **********************************************************************************
// Copyright LowPowerLab LLC 2018, https://www.LowPowerLab.com/contact
// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************
#include "RFM69_OTA.h"
#include "RFM69registers.h"

#ifdef __AVR__
  #include <avr/wdt.h>
#endif

//===================================================================================================================
// CheckForWirelessHEX() - Checks whether the last message received was a wireless programming request handshake
// If so it will start the handshake protocol, receive the new HEX image and 
// store it on the external flash chip, then reboot
// Assumes radio has been initialized and has just received a message (is not in SLEEP mode, and you called CRCPass())
// Assumes flash is an external SPI flash memory chip that has been initialized
//===================================================================================================================
void CheckForWirelessHEX(RFM69& radio, SPIFlash& flash, uint8_t DEBUG, uint8_t LEDpin)
{
  //special FLASH command, enter a FLASH image exchange sequence
  if (radio.DATALEN >= 4 && radio.DATA[0]=='F' && radio.DATA[1]=='L' && radio.DATA[2]=='X' && radio.DATA[3]=='?')
  {
    uint16_t remoteID = radio.SENDERID;
    if (radio.DATALEN == 7 && radio.DATA[4]=='E' && radio.DATA[5]=='O' && radio.DATA[6]=='F')
    { //sender must have not received EOF ACK so just resend
      radio.send(remoteID, "FLX?OK",6);
    }
#ifdef SHIFTCHANNEL
    else if (HandleWirelessHEXDataWrapper(radio, remoteID, flash, DEBUG, LEDpin))
#else
    else if (HandleWirelessHEXData(radio, remoteID, flash, DEBUG, LEDpin))
#endif
    {
      if (DEBUG) Serial.print(F("FLASH IMG TRANSMISSION SUCCESS!\n"));
      resetUsingWatchdog(DEBUG);
    }
    else
    {
      if (DEBUG) Serial.print("Timeout/Error, erasing written data ... ");
      //flash.blockErase32K(0); //clear any written data in first 32K block
      if (DEBUG) Serial.println(F("DONE"));
    }
  }
}


//===================================================================================================================
// HandleHandshakeACK() - checks there is a FLASH chip and sends an ACK for the OTA request handshake
//===================================================================================================================
uint8_t HandleHandshakeACK(RFM69& radio, SPIFlash& flash, uint8_t flashCheck) {
  if (flashCheck)
  {
    uint16_t deviceID=0;
    flash.wakeup(); //if flash.sleep() was previously called, flash.wakeup() is required or it's non responsive
    for (uint8_t i=0;i<10;i++) {
      uint16_t idNow = flash.readDeviceId();
      if (idNow==0 || idNow==0xffff || (i>0 && idNow != deviceID)) {
        deviceID=0;
        break;
      }
      deviceID=idNow;
    }
    if (deviceID==0) {
      radio.sendACK("FLX?NOK:NOFLASH",15); //NO FLASH CHIP FOUND, ABORTING
      Serial.println(F("FAIL:NO FLASH MEM"));
      return false;
    }
  }
  radio.sendACK("FLX?OK",6); //ACK the HANDSHAKE
  return true;
}


//===================================================================================================================
// HandleWirelessHEXDataWrapper() - wrapper function for HandleWirelessHEXData()
// that also shifts channel when SHIFTCHANNEL is defined
//===================================================================================================================
#ifdef SHIFTCHANNEL
uint8_t HandleWirelessHEXDataWrapper(RFM69& radio, uint16_t remoteID, SPIFlash& flash, uint8_t DEBUG, uint8_t LEDpin) {
  if (!HandleHandshakeACK(radio, flash)) return false;
  if (DEBUG) { Serial.println(F("FLX?OK (ACK sent)")); Serial.print(F("Shifting channel to ")); Serial.println(radio.getFrequency() + SHIFTCHANNEL);}
  radio.setFrequency(radio.getFrequency() + SHIFTCHANNEL); //shift center freq by SHIFTCHANNEL amount
  uint8_t result = HandleWirelessHEXData(radio, remoteID, flash, DEBUG, LEDpin);
  if (DEBUG) { Serial.print(F("UNShifting channel to ")); Serial.println(radio.getFrequency() - SHIFTCHANNEL);}
  radio.setFrequency(radio.getFrequency() - SHIFTCHANNEL); //restore center freq
  return result;
}
#endif


//===================================================================================================================
// HandleWirelessHEXData() - ACKs the wireless programming handshake and handles
// the complete transmission of the HEX image at the OTA programmed node side
//===================================================================================================================
#ifdef STM32IDE
uint8_t HandleWirelessHEXData(RFM69& radio, uint16_t remoteID, SPIFlash& flash, uint8_t DEBUG, struct gpio_pin LEDpin) {
#else
	uint8_t HandleWirelessHEXData(RFM69& radio, uint16_t remoteID, SPIFlash& flash, uint8_t DEBUG, uint8_t LEDpin) {
#endif
  uint32_t now=0;
  uint16_t tmp,seq=0;
  char buffer[16];
  uint16_t timeout = 3000; //3s for flash data
  
#ifndef SHIFTCHANNEL
  HandleHandshakeACK(radio, flash);
  if (DEBUG) Serial.println(F("FLX?OK (ACK sent)"));
#endif

  //first clear the fist 32k block (dedicated to a new FLASH image)
  flash.blockErase32K(0);
  flash.writeBytes(0,"FLXIMG:", 7);
#if defined (MOTEINO_M0)
  flash.writeByte(10,':');
  uint32_t bytesFlashed=11;
#else
  flash.writeByte(9,':');
  uint32_t bytesFlashed=10;
#endif
  now=millis();
  pinMode(LEDpin,OUTPUT);
    
  while(1)
  {
    if (radio.receiveDone() && radio.SENDERID == remoteID)
    {
      uint8_t dataLen = radio.DATALEN;

      digitalWrite(LEDpin,HIGH);
      if (dataLen >= 4 && radio.DATA[0]=='F' && radio.DATA[1]=='L' && radio.DATA[2]=='X')
      {
        if (radio.DATA[3]==':' && dataLen >= 7) //FLX:_:_
        {
          uint8_t index=3;
          tmp = 0;
          
          //read packet SEQ
          for (uint8_t i = 4; i<8; i++) //up to 4 characters for seq number
          {
            if (radio.DATA[i] >=48 && radio.DATA[i]<=57)
              tmp = tmp*10+radio.DATA[i]-48;
            else if (radio.DATA[i]==':')
            {
              if (i==4)
                return false;
              else break;
            }
            index++;
          }

          if (DEBUG) {
            Serial.print(F("radio ["));
            Serial.print(dataLen);
            Serial.print(F("] > "));
            PrintHex83((uint8_t*)radio.DATA, dataLen);
          }

          if (radio.DATA[++index] != ':') return false;
          now = millis(); //got "good" packet
          index++;
          if (tmp==seq || tmp==seq-1) // if {temp==seq : new packet}, {temp==seq-1 : ACK was lost, host resending previously saved packet so must only resend the ACK}
          {
            if (tmp==seq)
            {
              seq++;
              for(uint8_t i=index;i<dataLen;i++)
              {
                flash.writeByte(bytesFlashed++, radio.DATA[i]);
                if (bytesFlashed%32768==0) flash.blockErase32K(bytesFlashed);//erase subsequent 32K blocks (possible in case of atmega1284p)
              }
              //faster alternative using writeBytes - the gain is only up to 2 seconds
              //if (bytesFlashed<=32768 && (bytesFlashed+dataLen-index)>32768) flash.blockErase32K(32768);//erase subsequent 32K blocks (possible in case of atmega1284p)
              //flash.writeBytes(bytesFlashed, (void*)(radio.DATA+index), dataLen-index);
              //bytesFlashed += dataLen-index;
            }

            //send ACK
            tmp = sprintf(buffer, "FLX:%u:OK", tmp);
            if (DEBUG) Serial.println((char*)buffer);
            radio.sendACK(buffer, tmp);
          }
        }

        if (radio.DATA[3]=='?')
        {
          if (dataLen==4) //ACK for handshake was lost, resend
          {
            HandleHandshakeACK(radio, flash);
            if (DEBUG) Serial.println(F("FLX?OK resend"));
          }
          if (dataLen==7 && radio.DATA[4]=='E' && radio.DATA[5]=='O' && radio.DATA[6]=='F') //Expected EOF
          {
          
#if defined (MOTEINO_M0)
            if ((bytesFlashed-10)>253952) { //max 253952 - 10 bytes (signature)
              if (DEBUG) Serial.println(F("IMG > 253952, too big"));
              radio.sendACK("FLX?NOK:HEX>248k",16);
              return false; //just return, let MAIN timeout
            }
#elif defined(__AVR_ATmega1284P__)
            if ((bytesFlashed-10)>65526) { //max 65536 - 10 bytes (signature)
              if (DEBUG) Serial.println(F("IMG > 64k, too big"));
              radio.sendACK("FLX?NOK:HEX>64k",15);
              return false; //just return, let MAIN timeout
            }
#else //assuming atmega328p
            if ((bytesFlashed-10)>31744) {
              if (DEBUG) Serial.println(F("IMG > 31k, too big"));
              radio.sendACK("FLX?NOK:HEX>31k",15);
              return false; //just return, let MAIN timeout
            }
#endif
            HandleHandshakeACK(radio, flash, false);
            if (DEBUG) Serial.println(F("FLX?OK"));
            //save # of bytes written
#ifdef MOTEINO_M0
            flash.writeByte(7,(bytesFlashed-11)>>16);
            flash.writeByte(8,(bytesFlashed-11)>>8);
            flash.writeByte(9,(bytesFlashed-11));
            //flash.writeByte(10,':'); //already done
#else
            flash.writeByte(7,(bytesFlashed-10)>>8);
            flash.writeByte(8,(bytesFlashed-10));
            //flash.writeByte(9,':'); //already done
#endif
            return true;
          }
        }
      }
      digitalWrite(LEDpin,LOW);
    }
    
    //abort FLASH sequence if no valid packet received for a long time
    if (millis()-now > timeout)
    {
      return false;
    }
  }
}


//===================================================================================================================
// readSerialLine() - reads a line feed (\n) terminated line from the serial stream
// returns # of bytes read, up to 254
// timeout in ms, will timeout and return after so long
// this is called at the OTA programmer side
//===================================================================================================================
uint8_t readSerialLine(char* input, char endOfLineChar, uint8_t maxLength, uint16_t timeout)
{
  uint8_t inputLen = 0;
  Serial.setTimeout(timeout);
  inputLen = Serial.readBytesUntil(endOfLineChar, input, maxLength);
  input[inputLen]=0;//null-terminate it
  Serial.setTimeout(0);
  return inputLen;
}


//===================================================================================================================
// CheckForSerialHEX() - returns TRUE if a HEX file transmission was detected and it was actually transmitted successfully
// this is called at the OTA programmer side
//===================================================================================================================
uint8_t CheckForSerialHEX(uint8_t* input, uint8_t inputLen, RFM69& radio, uint16_t targetID, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, uint8_t DEBUG)
{
  if (inputLen == 4 && input[0]=='F' && input[1]=='L' && input[2]=='X' && input[3]=='?') {
    if (HandleSerialHandshake(radio, targetID, false, TIMEOUT, ACKTIMEOUT, DEBUG))
    {
      if (radio.DATALEN >= 7 && radio.DATA[4] == 'N')
      {
        Serial.println((char*)radio.DATA); //signal serial handshake fail/error and return
        return false;
      }
      
      Serial.println(F("\nFLX?OK")); //signal serial handshake back to host script
#ifdef SHIFTCHANNEL
      if (HandleSerialHEXDataWrapper(radio, targetID, TIMEOUT, ACKTIMEOUT, DEBUG))
#else
      if (HandleSerialHEXData(radio, targetID, TIMEOUT, ACKTIMEOUT, DEBUG))
#endif
      {
        Serial.println(F("FLX?OK")); //signal EOF serial handshake back to host script
        if (DEBUG) Serial.println(F("FLASH IMG TRANSMISSION SUCCESS"));
        return true;
      }
      if (DEBUG) Serial.println(F("FLASH IMG TRANSMISSION FAIL"));
      return false;
    }
    else Serial.println(F("FLX?NOK"));
  }
  return false;
}


//===================================================================================================================
// HandleSerialHandshake() - handles the handshake with the serial port
//===================================================================================================================
uint8_t HandleSerialHandshake(RFM69& radio, uint16_t targetID, uint8_t isEOF, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, uint8_t DEBUG)
{
  long now = millis();

  while (millis()-now<TIMEOUT)
  {
    if (radio.sendWithRetry(targetID, isEOF ? "FLX?EOF" : "FLX?", isEOF?7:4, 2,ACKTIMEOUT))
      if (radio.DATALEN >= 6 && radio.DATA[0]=='F' && radio.DATA[1]=='L' && radio.DATA[2]=='X' && radio.DATA[3]=='?')
        return true;
  }

  if (DEBUG) Serial.println(F("Handshake fail"));
  return false;
}


//===================================================================================================================
// HandleSerialHEXDataWrapper() - wrapper for HandleSerialHEXData(), also shifts the channel if SHIFTCHANNEL is defined
//===================================================================================================================
#ifdef SHIFTCHANNEL
uint8_t HandleSerialHEXDataWrapper(RFM69& radio, uint16_t targetID, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, uint8_t DEBUG) {
  radio.setFrequency(radio.getFrequency() + SHIFTCHANNEL); //shift center freq by SHIFTCHANNEL amount
  uint8_t result = HandleSerialHEXData(radio, targetID, TIMEOUT, ACKTIMEOUT, DEBUG);
  radio.setFrequency(radio.getFrequency() - SHIFTCHANNEL); //shift center freq by SHIFTCHANNEL amount
  return result;
}
#endif


//===================================================================================================================
// HandleSerialHEXData() - handles the transmission of the HEX image from the serial port to the node being OTA programmed
// this is called at the OTA programmer side
//===================================================================================================================
uint8_t HandleSerialHEXData(RFM69& radio, uint16_t targetID, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, uint8_t DEBUG) {
  long now=millis();
  uint16_t seq=0, tmp=0, inputLen;
  uint16_t remoteID = radio.SENDERID; //save the remoteID as soon as possible
  uint8_t sendBuf[57];
  char input[115];
  //a FLASH record should not be more than 64 bytes: FLX:9999:10042000FF4FA591B4912FB7F894662321F48C91D6 

  while(1) {
    inputLen = readSerialLine(input);
    if (inputLen == 0) goto timeoutcheck;
    tmp = 0;
    
    if (inputLen >= 6) { //FLX:9:
      if (input[0]=='F' && input[1]=='L' && input[2]=='X')
      {
        if (input[3]==':')
        {
          uint8_t index = 3;
          for (uint8_t i = 4; i<8; i++) //up to 4 characters for seq number
          {
            if (input[i] >=48 && input[i]<=57)
              tmp = tmp*10+input[i]-48;
            else if (input[i]==':')
            {
              if (i==4)
                return false;
              else break;
            }
            index++;
          }
          //Serial.print(F("input[index] = "));Serial.print(F("["));Serial.print(index);Serial.print(F("]="));Serial.println(input[index]);
          if (input[++index] != ':') return false;
          now = millis(); //got good packet
          index++;
          uint8_t hexDataLen = validateHEXData(input+index, inputLen-index);

          if (hexDataLen>0 && hexDataLen<253)
          {
            if (tmp==seq) //only read data when packet number is the next expected SEQ number
            {
              uint8_t sendBufLen = prepareSendBuffer(input+index+8, sendBuf, hexDataLen, seq); //extract HEX data from input to BYTE data into sendBuf (go from 2 HEX bytes to 1 byte), +8 jumps over the header to the HEX raw data
              //Serial.print(F("PREP "));Serial.print(sendBufLen); Serial.print(F(" > ")); PrintHex83(sendBuf, sendBufLen);
              
              //SEND RADIO DATA
              if (sendHEXPacket(radio, remoteID, sendBuf, sendBufLen, seq, TIMEOUT, ACKTIMEOUT, DEBUG))
              {
                sprintf((char*)sendBuf, "FLX:%u:OK",seq);
                Serial.println((char*)sendBuf); //response to host
                seq++;
              }
              else return false;
            }
          }
          //else Serial.print(F("FLX:INV"));
          else { Serial.print(F("FLX:INV:"));Serial.println(hexDataLen); }
        }
        if (inputLen==7 && input[3]=='?' && input[4]=='E' && input[5]=='O' && input[6]=='F')
        {
          //SEND RADIO EOF
          return HandleSerialHandshake(radio, targetID, true, TIMEOUT, ACKTIMEOUT, DEBUG);
        }
      }
    }
    
    //abort FLASH sequence if no valid packet received for a long time
timeoutcheck:
    if (millis()-now > TIMEOUT)
    {
      Serial.print(F("Timeout getting FLASH image from SERIAL, aborting.."));
      //send abort msg or just let node timeout as well?
      return false;
    }
  }
  return true;
}


//===================================================================================================================
// validateHEXData() - returns length of HEX data bytes if everything is valid
//returns 0 if any validation failed
//===================================================================================================================
uint8_t validateHEXData(void* data, uint8_t length)
{
  //assuming 1 byte record length, 2 bytes address, 1 byte record type, N data bytes, 1 CRC byte
  char* input = (char*)data;
  if (length <12 || length%2!=0) return 0; //shortest possible intel data HEX record is 12 bytes
  //Serial.print(F("VAL > ")); Serial.println((char*)input);

  uint8_t checksum=0;
  //check valid HEX data and CRC
  for (uint8_t i=0; i<length;i++)
  {
    if (!((input[i] >=48 && input[i]<=57) || (input[i] >=65 && input[i]<=70))) //0-9,A-F
      return 255;
    if (i%2 && i<length-2) checksum+=BYTEfromHEX(input[i-1], input[i]);
  }
  checksum=(checksum^0xFF)+1;
  
  //TODO : CHECK for address continuity (intel HEX addresses are big endian)

  //Serial.print(F("final CRC:"));Serial.println((uint8_t)checksum, HEX);
  //Serial.print(F("CRC byte:"));Serial.println(BYTEfromHEX(input[length-2], input[length-1]), HEX);

  //check CHECKSUM byte
  if (((uint8_t)checksum) != BYTEfromHEX(input[length-2], input[length-1]))
    return 254;

  uint8_t dataLength = BYTEfromHEX(input[0], input[1]); //length of actual HEX flash data (usually 16bytes)
  //calculate record length
  if (length != dataLength*2 + 10) //add headers and checksum bytes (a total of 10 combined)
    return 253;

  return dataLength; //all validation OK!
}


//===================================================================================================================
// prepareSendBuffer() - returns the final size of the buf
//===================================================================================================================
uint8_t prepareSendBuffer(char* hexdata, uint8_t*buf, uint8_t length, uint16_t seq)
{
  uint8_t seqLen = sprintf(((char*)buf), "FLX:%u:", seq);
  for (uint8_t i=0; i<length;i++)
    buf[seqLen+i] = BYTEfromHEX(hexdata[i*2], hexdata[i*2+1]);
  return seqLen+length;
}


//===================================================================================================================
// BYTEfromHEX() - converts from ASCII HEX to byte, assume A and B are valid HEX chars [0-9A-F]
//===================================================================================================================
uint8_t BYTEfromHEX(char MSB, char LSB)
{
  return (MSB>=65?MSB-55:MSB-48)*16 + (LSB>=65?LSB-55:LSB-48);
}


//===================================================================================================================
// sendHEXPacket() - return the SEQ of the ACK received, or -1 if invalid
//===================================================================================================================
uint8_t sendHEXPacket(RFM69& radio, uint16_t targetID, uint8_t* sendBuf, uint8_t hexDataLen, uint16_t seq, uint16_t TIMEOUT, uint16_t ACKTIMEOUT, uint8_t DEBUG)
{
  long now = millis();
  
  while(1) {
    if (DEBUG) { Serial.print(F("RFTX > ")); PrintHex83(sendBuf, hexDataLen); Serial.println(); }
    if (radio.sendWithRetry(targetID, sendBuf, hexDataLen, 2, ACKTIMEOUT))
    {
      uint8_t ackLen = radio.DATALEN;
      
      if (DEBUG) { Serial.print(F("RFACK > ")); Serial.print(ackLen); Serial.print(F(" > ")); PrintHex83((uint8_t*)radio.DATA, ackLen); Serial.println(); }
      
      if (ackLen >= 8 && radio.DATA[0]=='F' && radio.DATA[1]=='L' && radio.DATA[2]=='X' && 
          radio.DATA[3]==':' && radio.DATA[ackLen-3]==':' &&
          radio.DATA[ackLen-2]=='O' && radio.DATA[ackLen-1]=='K')
      {
        uint16_t tmp=0;
#if defined(__arm__)
        // On the ARM platform, uint16_t = short unsigned int, so %hu formatting is needed:
        sscanf((const char*)radio.DATA, "FLX:%hu:OK", &tmp);
#else
        // On the AVR platform, uint16_t = unsigned int, so %u formatting is needed:
        sscanf((const char*)radio.DATA, "FLX:%u:OK", &tmp);
#endif
        return tmp == seq;
      }
    }

    if (millis()-now > TIMEOUT)
    {
      Serial.println(F("Timeout waiting for packet ACK, aborting FLASH operation ..."));
      break; //abort FLASH sequence if no valid ACK was received for a long time
    }
  }
  return false;
}


//===================================================================================================================
// PrintHex83() - prints 8-bit data in HEX format
//===================================================================================================================
void PrintHex83(uint8_t* data, uint8_t length) 
{
  uint8_t tmp;
  for (uint8_t i=0; i<length; i++) 
  {
    tmp = (data[i] >> 4) | 48;
    Serial.print((char)((tmp > 57) ? tmp+7 : tmp));
    tmp = (data[i] & 0x0F) | 48;
    Serial.print((char)((tmp > 57) ? tmp+7 : tmp));
  }
  Serial.println();
}

//===================================================================================================================
// resetUsingWatchdog() - Use watchdog to reset the MCU
//===================================================================================================================
void resetUsingWatchdog(uint8_t DEBUG)
{
#ifdef __AVR__
  //wdt_disable();
  if (DEBUG) Serial.print(F("REBOOTING"));
  wdt_enable(WDTO_15MS);
  while(1) if (DEBUG) Serial.print(F("."));
#elif defined(MOTEINO_M0)
  //WDT->CTRL.reg = 0; // disable watchdog
  //while (WDT->STATUS.bit.SYNCBUSY == 1); // sync is required
  //WDT->CONFIG.reg = 0; // see Table 18.8.2 Timeout Period (valid values 0-11)
  //WDT->CTRL.reg = WDT_CTRL_ENABLE; //enable WDT
  //while (WDT->STATUS.bit.SYNCBUSY == 1);
  //WDT->CLEAR.reg= 0x00; // system reset via WDT
  //while (WDT->STATUS.bit.SYNCBUSY == 1);
  *((volatile uint32_t *)(HMCRAMC0_ADDR + HMCRAMC0_SIZE - 4)) = 0xF1A507AF;
  NVIC_SystemReset();
#endif
}
