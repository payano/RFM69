#pragma once

class SPIFlash {
public:
  static uint8_t UNIQUEID[8];
  SPIFlash(uint8_t slaveSelectPin, uint16_t jedecID=0);
  bool initialize();
  void command(uint8_t cmd, bool isWrite=false);
  uint8_t readStatus();
  uint8_t readByte(uint32_t addr);
  void readBytes(uint32_t addr, void* buf, uint16_t len);
  void writeByte(uint32_t addr, uint8_t byt);
  void writeBytes(uint32_t addr, const void* buf, uint16_t len);
  bool busy();
  void chipErase();
  void blockErase4K(uint32_t address);
  void blockErase32K(uint32_t address);
  void blockErase64K(uint32_t addr);
  uint16_t readDeviceId();
  uint8_t* readUniqueId();

  void sleep();
  void wakeup();
  void end();
protected:
  void select();
  void unselect();
  uint8_t _slaveSelectPin;
  uint16_t _jedecID;
  uint8_t _SPCR;
  uint8_t _SPSR;
#ifdef SPI_HAS_TRANSACTION
  SPISettings _settings;
#endif
};
