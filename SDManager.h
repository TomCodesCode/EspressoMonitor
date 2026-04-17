#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

// HSPI pins
#define SD_CS   26
#define SD_MOSI 17
#define SD_MISO 22
#define SD_SCK  21

class SDManager {
private:
    SPIClass* sdSPI;

public:
    bool isReady;

    SDManager(SPIClass* sharedSPI);
    void init();
    bool isInitialized();

    void appendLog(const char* path, const char* message);
    void readLog(const char* path);
    void testReadWrite();
};

#endif