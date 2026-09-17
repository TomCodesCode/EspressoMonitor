// ============================================================================
// esp_now_protocol.h  —  shared ESP-NOW wire format for the coffee system.
//
// EVERY node (espresso-monitor, scale x2, grinder-control) MUST compile against
// THIS single file so the byte layout of messages is identical across the mesh.
// Never copy/duplicate this struct into a node — always include it from here.
//
// Bump ESPNOW_PROTOCOL_VERSION whenever the struct layout changes, and have
// receivers reject mismatched versions.
// ============================================================================
#ifndef ESP_NOW_PROTOCOL_H
#define ESP_NOW_PROTOCOL_H

#include <stdint.h>

static const uint8_t ESPNOW_PROTOCOL_VERSION = 1;

// Logical identity of each node in the system.
enum NodeId : uint8_t {
    NODE_ESPRESSO_MONITOR = 0,  // ESP32-WROOM-32 (V1 hub, receives data)
    NODE_SCALE_MACHINE    = 1,  // C3 scale under the espresso machine
    NODE_SCALE_GRINDER    = 2,  // C3 scale under the grinder
    NODE_GRINDER_CONTROL  = 3,  // ESP32-S3 grinder controller
};

// Message kinds carried in EspNowMessage.msgType.
enum MsgType : uint8_t {
    MSG_WEIGHT = 1,  // live weight / flow sample
    MSG_STATUS = 2,  // heartbeat / battery / online
};

// Fixed, tightly-packed layout so all chips (Xtensa + RISC-V) agree byte-for-byte.
#pragma pack(push, 1)
struct EspNowMessage {
    uint8_t  version;      // == ESPNOW_PROTOCOL_VERSION; receiver rejects mismatches
    uint8_t  srcNode;      // NodeId of the sender
    uint8_t  msgType;      // MsgType
    uint32_t timestampMs;  // millis() on the sender when sampled
    float    grams;        // MSG_WEIGHT: current weight
    float    gramsPerSec;  // MSG_WEIGHT: flow rate
    int16_t  batteryMv;    // MSG_STATUS: battery millivolts (-1 = unknown/USB)
};
#pragma pack(pop)

// A single fixed-size payload keeps ESP-NOW send/recv trivial.
static const uint8_t ESPNOW_MSG_SIZE = sizeof(EspNowMessage);

#endif  // ESP_NOW_PROTOCOL_H
