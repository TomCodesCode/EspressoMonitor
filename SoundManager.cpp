#include "SoundManager.h"

const Note DOOM_THEME[] = {
    // === BAR 1 (0:00 - 0:02.5) ===
    {NOTE_E3, 120}, {NOTE_E3, 120}, {REST, 120},     // Duh-Duh
    {NOTE_E3, 120}, {NOTE_E3, 120}, {REST, 120},     // Duh-Duh
    {NOTE_G3, 250}, {NOTE_E3, 120}, {NOTE_E3, 120},  // Weee-Duh-Duh
    {REST, 120},    {NOTE_A3, 300}, {NOTE_G3, 300},  // WAAAA-WAAAA

    // === BAR 2 (0:02.5 - 0:05) ===
    {NOTE_E3, 120}, {NOTE_E3, 120}, {REST, 120},     
    {NOTE_E3, 120}, {NOTE_E3, 120}, {REST, 120},     
    {NOTE_G3, 250}, {NOTE_E3, 120}, {NOTE_E3, 120},
    {REST, 120},    {NOTE_A3, 300}, {NOTE_G3, 300},  // Identical Repeat

    // === BAR 3 (0:05 - 0:07.5) ===
    {NOTE_E3, 120}, {NOTE_E3, 120}, {REST, 120},     
    {NOTE_E3, 120}, {NOTE_E3, 120}, {REST, 120},     
    {NOTE_G3, 250}, {NOTE_E3, 120}, {NOTE_E3, 120},
    {REST, 120},    {NOTE_A3, 300}, {NOTE_AS3, 300}, // Pitch shifts UP to Bb (Evil sound)

    // === BAR 4 (0:07.5 - 0:10.5) ===
    {NOTE_E3, 120}, {NOTE_E3, 120}, {REST, 120},     
    {NOTE_E3, 120}, {NOTE_E3, 120}, {REST, 120},     
    {NOTE_G3, 250}, {NOTE_E3, 120}, {NOTE_E3, 120},
    {REST, 120},    {NOTE_C4, 300}, {NOTE_B3, 150}, {NOTE_G3, 150} // The Turnaround
};

SoundManager::SoundManager(int pinNumber) : pin(pinNumber) {
    isPlaying = false;
    noteIndex = 0;
}

void SoundManager::init() {
    pinMode(pin, OUTPUT); // tone also  works without pinMode
}

void SoundManager::playMelody(const Note* melody, int length) {
    currentMelody = melody;
    melodyLength = length;
    noteIndex = 0;
    isPlaying = true;
    lastUpdate = millis(); // Reset clock
    
    // Play first note immediately
    if (currentMelody[0].frequency > 0) {
        tone(pin, currentMelody[0].frequency);
    } else {
        noTone(pin);
    }
}

void SoundManager::playDoom() {
    playMelody(DOOM_THEME, sizeof(DOOM_THEME) / sizeof(Note));
}

void SoundManager::playStop() {
    tone(4, 400, 300);
}

void SoundManager::update() {
    if (!isPlaying) return;

    unsigned long currentMillis = millis();
    int noteDuration = currentMelody[noteIndex].duration * 1.2;
    
    // STACCATO CHECK (a term in music where notes are played short)
    // If 90% of the note was played, cut the sound to create a gap.
    if (currentMillis - lastUpdate > (noteDuration * 0.9)) {
        noTone(pin);
    }

    // NEXT NOTE
    // If the full duration has passed, move to the next index.
    if (currentMillis - lastUpdate >= noteDuration) {
        lastUpdate = currentMillis; // Reset timer
        
        noteIndex++; // Next note

        if (noteIndex >= melodyLength) {
            isPlaying = false;
            noTone(pin); // ensure silence at the end
        } else {
            // Play the next note
            int nextFreq = currentMelody[noteIndex].frequency;
            if (nextFreq > 0) {
                tone(pin, nextFreq);
            } else {
                noTone(pin);
            }
        }
    }
}