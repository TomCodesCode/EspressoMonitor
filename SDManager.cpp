#include "SDManager.h"
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

String SDManager::readLogString(const char* path) {
    if (!isReady) return "";
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    File file = SD.open(path);
    if (!file) {
        if (spiMutex) xSemaphoreGive(spiMutex);
        return "";
    }
    String result = file.readString();
    file.close();
    if (spiMutex) xSemaphoreGive(spiMutex);
    return result;
}

void SDManager::clearLogs() {
    if (!isReady) return;
    if (spiMutex) xSemaphoreTake(spiMutex, portMAX_DELAY);
    SD.remove("/brew_log.csv");
    SD.remove("/brew_log.txt");
    if (spiMutex) xSemaphoreGive(spiMutex);
    Serial.println("Logs cleared.");
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