#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <Arduino.h>

class TimerManager {
private:
    unsigned long startTime;
    unsigned long stopTime;
    bool running;

public:
    TimerManager();

    void start();
    void stop();
    void reset();
    
    bool isRunning();   // Returns true if the timer is currently counting
    
    float getSeconds();

    char* getFormattedTime();
};

#endif