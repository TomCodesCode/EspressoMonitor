#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <Arduino.h>
#include <utility>

class TimerManager {
private:
    unsigned long startTime;
    unsigned long stopTime;
    bool running;

public:
    TimerManager();

    enum TimeFormat{SECONDS, MINUTES};

    void start();
    void stop();
    void reset();
    
    bool isRunning();   // Returns true if the timer is currently counting
    
    float getSeconds();

    std::pair<const char*, const char*> getFormattedTime(TimeFormat time_format);
};

#endif