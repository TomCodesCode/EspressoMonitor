#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

enum SystemState {
    WARMUP,    // Machine is cold, waiting for temp to rise
    READY,     // Temp is hit, waiting for user to pull the lever
    BREWING,   // Lever is pulled, timer is running
    DONE       // Shot finished, showing summary
};

#endif