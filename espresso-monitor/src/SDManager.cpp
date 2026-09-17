#include "SDManager.h"
#include "esp_task_wdt.h"
#include <WebServer.h>
// testing.
// TODO: CSV files for server.
SDManager::SDManager(SPIClass* sharedSPI) {
    sdSPI = sharedSPI;
    isReady = false;
}

void SDManager::init() {
    Serial.println("Initializing SD Card...");

    // sdSPI->begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    // Hand bus to SD library
    if (!SD.begin(SD_CS, *sdSPI)) {
        Serial.println("ERROR: SD Card Mount Failed!");
        return;
    }

    // Verify card in slot
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("ERROR: No SD card attached!");
        return;
    }

    Serial.println("SUCCESS: SD Card isolated on custom pins and ready!");
    isReady = true;
}

void SDManager::setSpiMutex(SemaphoreHandle_t m) {
    spiMutex = m;
}

bool SDManager::isInitialized() {
    return isReady;
}

void SDManager::appendLog(const char* path, const char* message) {
    if (!isReady) return; // Don't crash if the card isn't mounted!

    // own the shared bus for the whole open/write/close transaction.
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);

    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        Serial.print("ERROR: Failed to open ");
        Serial.println(path);
        if (spiMutex) xSemaphoreGive(spiMutex);
        return;
    }

    file.println(message);
    file.close();

    if (spiMutex) xSemaphoreGive(spiMutex);
}

void SDManager::readLog(const char* path) {
    if (!isReady) return;

    // Fix 6: own the shared bus for the whole read transaction.
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);

    File file = SD.open(path);
    if (!file) {
        Serial.print("ERROR: Failed to open ");
        Serial.println(path);
        if (spiMutex) xSemaphoreGive(spiMutex);
        return;
    }

    Serial.println("START OF FILE");
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println("END OF FILE");
    file.close();

    if (spiMutex) xSemaphoreGive(spiMutex);
}

bool SDManager::streamFileChunked(const char* path, WebServer& server, const char* contentType) {
    if (!isReady) return false;

    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    File file = SD.open(path);
    if (!file) {
        if (spiMutex) xSemaphoreGive(spiMutex);
        return false;   // missing — caller sends 404, nothing written yet
    }
    size_t total = file.size();
    if (spiMutex) xSemaphoreGive(spiMutex);

    // Bounded-memory transfer. The old readString() built the whole file in one String: realloc churn fragmented the heap and the contiguous allocation
    // eventually failed, hanging the request. Here peak memory is just the buffer. We drop the SPI mutex between reads so the MAX31865 keeps
    // getting the shared bus, and pet the WDT so a large file can't trip it.
    server.setContentLength(total);
    server.sendHeader("Connection", "close");
    server.send(200, contentType, "");

    uint8_t buf[512];
    while (true) {
        if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
        int n = file.read(buf, sizeof(buf));
        if (spiMutex) xSemaphoreGive(spiMutex);
        if (n <= 0) break;
        server.sendContent((const char*)buf, n);
        esp_task_wdt_reset();
    }

    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    file.close();
    if (spiMutex) xSemaphoreGive(spiMutex);
    return true;
}

void SDManager::clearLogs() {
    if (!isReady) return;
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    SD.remove("/brew_log.csv");
    SD.remove("/brew_log.txt");
    // Delete individual brew temp files (brew_*.csv)
    File root = SD.open("/");
    File entry = root.openNextFile();
    while (entry) {
        esp_task_wdt_reset();
        String name = String(entry.name());
        entry.close();
        if (name.startsWith("/brew_") && name.endsWith(".csv") && name != "/brew_log.csv") {
            SD.remove(name.c_str());
        }
        esp_task_wdt_reset();
        entry = root.openNextFile();
    }
    root.close();
    if (spiMutex) xSemaphoreGive(spiMutex);
    Serial.println("Logs cleared.");
}

void SDManager::saveBrewTemps(unsigned long id, float* temps, int count) {
    if (!isReady || count <= 0) return;
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    char path[32];
    snprintf(path, sizeof(path), "/brew_%lu.csv", id);
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        if (spiMutex) xSemaphoreGive(spiMutex);
        return;
    }
    for (int i = 0; i < count; i++) {
        file.println(temps[i], 1);
    }
    file.close();
    if (spiMutex) xSemaphoreGive(spiMutex);
}

uint32_t SDManager::getFreeSpaceMB() {
    if (!isReady) return 0;
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    uint32_t free = (uint32_t)((SD.totalBytes() - SD.usedBytes()) >> 20);
    if (spiMutex) xSemaphoreGive(spiMutex);
    return free;
}

int SDManager::getBrewCount() {
    if (!isReady) return 0;
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    File file = SD.open("/brew_log.csv");
    if (!file) {
        if (spiMutex) xSemaphoreGive(spiMutex);
        return 0;
    }
    int count = 0;
    while (file.available()) {
        if (file.read() == '\n') count++;
    }
    file.close();
    if (spiMutex) xSemaphoreGive(spiMutex);
    return count;
}

// A quick diagnostic test to run during setup
void SDManager::testReadWrite() {
    if (!isReady) {
        Serial.println("Skipping SD Test: Card not ready.");
        return;
    }
    
    Serial.println("Running SD Card Read/Write Test...");
    
    appendLog("/brew_log.txt", "TEST: System Booted Successfully.");
    
    readLog("/brew_log.txt");
}