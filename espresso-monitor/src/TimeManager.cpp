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

std::pair<const char*, const char*> TimerManager::getFormattedTime(TimeFormat time_format) {
    // Static buffer (no malloc, reduce the chance of fragmentation over long run periods)
    // static char buffer[10];
    static char first[10];
    static char second[10];
    
    float rawTime = getSeconds();
    int minutes = (int)rawTime / 60;
    int seconds = (int)rawTime % 60;
    int tenths = (int)((rawTime - seconds) * 10);
    //int milliseconds = (int)((rawTime - (int)rawTime) * 100);

    // sprintf(buffer, "%02d:%02d:%02d", minutes, seconds, milliseconds);
    if (time_format == SECONDS){
        // if (seconds > 60) seconds = 0;
        // sprintf(buffer, "%02d.%d", seconds, tenths);
        sprintf(first, "%02d", seconds);
        sprintf(second, "%d", tenths);
    } else if (time_format == MINUTES){
        // sprintf(buffer, "%01d:%02d", minutes, seconds);
        sprintf(first, "%02d", minutes);
        sprintf(second, "%02d", seconds);
    }
    return {first, second};
    
    // return buffer;
}