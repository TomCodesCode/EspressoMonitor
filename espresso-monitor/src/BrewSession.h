#ifndef BREW_SESSION_H
#define BREW_SESSION_H

// One brew's worth of recorded data, shared across cores.
// The display layer (Core 1) writes into it during a shot; the SD logger and
// the future web server (Core 0) read from it. All access should be guarded by
// the accompanying portMUX (see brewMux in the .ino).

// 30 seconds of brew data at 10 samples/sec (one point per 0.1s).
// Single source of truth for the chart resolution (replaces MAX_BREW_TIME).
static const int BREW_MAX_POINTS = 300;

// Coherent boiler+grouphead pair published by Core 0 under tempMux.
struct TempSnapshot {
    float boiler;
    float grouphead;
};

struct BrewSession {
    float temperatures[BREW_MAX_POINTS];
    int   pointCount = 0;        // how many valid points are in temperatures[]
    float durationSeconds = 0.0; // final shot length, set at DONE
    bool  isComplete = false;    // true once the shot has finished
};

#endif
