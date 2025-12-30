#include "TimeManager.h"

TimerManager::TimerManager() {
    reset();
}

void TimerManager::start() {
    if (!running) {
        startTime = millis();
        running = true;
    }
}

void TimerManager::stop() {
    if (running) {
        stopTime = millis();
        running = false;
    }
}

void TimerManager::reset() {
    running = false;
    startTime = 0;
    stopTime = 0;
}

bool TimerManager::isRunning() {
    return running;
}

float TimerManager::getSeconds() {
    unsigned long current = running ? millis() : stopTime;
    
    // Handle the edge case where we haven't started yet
    if (startTime == 0 && !running) return 0.0;
    
    long elapsed = current - startTime;
    return elapsed / 1000.0;
}

char* TimerManager::getFormattedTime() {
    // Static buffer (no malloc, reduce the chance of fragmentation over long run periods)
    static char buffer[10];
    
    float rawTime = getSeconds();
    // int minutes = (int)rawTime / 60;
    int seconds = (int)rawTime % 60;
    int milliseconds = (int)((rawTime - (int)rawTime) * 100);

    // Format as "MM:SS:mm" (e.g., "00:05:78")
    // sprintf(buffer, "%02d:%02d:%02d", minutes, seconds, milliseconds);
    sprintf(buffer, "%02d.%02d", seconds, milliseconds);
    
    return buffer;
}