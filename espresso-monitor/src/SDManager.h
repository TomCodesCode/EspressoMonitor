#ifndef SD_MANAGER_H
#define SD_MANAGER_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

class WebServer;  // fwd decl — full include only needed in the .cpp

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
    // Stream a file to the web client in bounded chunks (no whole-file String).
    // Returns false if the file is missing (before any response is sent), so the caller can issue a 404. Sends a 200 + body itself on success.
    bool streamFileChunked(const char* path, WebServer& server, const char* contentType = "text/plain");
    void clearLogs();
    uint32_t getFreeSpaceMB();
    int getBrewCount();
    void saveBrewTemps(unsigned long id, float* temps, int count);
    void testReadWrite();
};

#endif