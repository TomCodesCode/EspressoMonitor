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
    SemaphoreHandle_t spiMutex = NULL; // guards the shared bus (set via setSpiMutex)

public:
    bool isReady;

    SDManager(SPIClass* sharedSPI);
    void init();
    void setSpiMutex(SemaphoreHandle_t m);
    bool isInitialized();

    void appendLog(const char* path, const char* message);
    void readLog(const char* path);
    void testReadWrite();
};

#endif