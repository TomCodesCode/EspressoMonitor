#ifndef SOUND_MANAGER_H
#define SOUND_MANAGER_H

#include <Arduino.h>

// Frequencies in Hz
#define REST     0
#define NOTE_E3  165
#define NOTE_G3  196
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  261 
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_C5  523
#define NOTE_D5  587

struct Note {
    int frequency;
    int duration;
};

class SoundManager {
private:
    int pin;
    unsigned long lastUpdate;
    
    // Melody State
    const Note* currentMelody; // Pointer to the song array
    int melodyLength;          // How many notes in the song
    int noteIndex;             // Which note is played
    bool isPlaying;            // Is the melody on

public:
    SoundManager(int pinNumber);
    void init();
    void update();
    
    void playMelody(const Note* melody, int length);
    
    void playDoom();
    void playHelldivers();
    void playStop();

    bool isBusy() { return isPlaying; }
};

#endif