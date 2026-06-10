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

const Note HELLDIVERS_THEME[] = {
    // === BAR 1 (The iconic intro: "Da-da-da DAAAA") ===
    {NOTE_DS4, 150}, {REST, 1}, {NOTE_D4, 150}, {REST, 1}, {NOTE_DS4, 150}, {REST, 1},
    {NOTE_E3, 1500}, {REST, 200},

    // === BAR 2 (The echo: "Da-da DAAAA") ===
    {NOTE_C4, 500}, {REST, 100},
    {NOTE_D4, 2500}, {REST, 200},

    // === BAR 3 (The climb: "Da-da-da DAAA... DAAA...") ===
    {NOTE_DS4, 200}, {NOTE_D4, 200}, {NOTE_E3, 200},
    {NOTE_DS4, 1500},

    {NOTE_G3, 200}, {NOTE_C4, 200}, {NOTE_D4, 200}, {NOTE_F4, 200},
    {REST, 150}, {NOTE_C4, 150}, {NOTE_G4, 2000}

    // === BAR 4 (The soaring climax: "...DAAA... DAAA... DAAAAAAA") ===
    // {NOTE_G4, 500}, {NOTE_C5, 500},
    // {NOTE_D5, 1600}, {REST, 500}
};

SoundManager::SoundManager(int pinNumber) : pin(pinNumber) {
    isPlaying = false;
    noteIndex = 0;
}

void SoundManager::init() {
    pinMode(pin, OUTPUT); // tone also works without pinMode
}

void SoundManager::playMelody(const Note* melody, int length) {
    currentMelody = melody;
    melodyLength = length;
    noteIndex = 0;
    isPlaying = true;
    lastUpdate = millis();

    if (currentMelody[0].frequency > 0) {
        tone(pin, currentMelody[0].frequency);
    } else {
        noTone(pin);
    }
}

void SoundManager::playDoom() {
    playMelody(DOOM_THEME, sizeof(DOOM_THEME) / sizeof(Note));
}

void SoundManager::playHelldivers() {
    playMelody(HELLDIVERS_THEME, sizeof(HELLDIVERS_THEME) / sizeof(Note));
}

void SoundManager::playStop() {
    tone(pin, 400, 300);
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
