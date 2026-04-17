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

bool SDManager::isInitialized() {
    return isReady;
}

void SDManager::appendLog(const char* path, const char* message) {
    if (!isReady) return; // Don't crash if the card isn't mounted!

    File file = SD.open(path, FILE_APPEND);
    if (!file) {
        Serial.print("ERROR: Failed to open ");
        Serial.println(path);
        return;
    }
    
    file.println(message);
    file.close();
}

void SDManager::readLog(const char* path) {
    if (!isReady) return;

    File file = SD.open(path);
    if (!file) {
        Serial.print("ERROR: Failed to open ");
        Serial.println(path);
        return;
    }

    Serial.println("START OF FILE");
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println("END OF FILE");
    file.close();
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