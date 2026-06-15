# ☕ Espresso Monitor (v1)

![EspressoMonitor](https://img.shields.io/badge/Platform-ESP32-blue) ![C++](https://img.shields.io/badge/Language-C%2B%2B-00599C) ![RTOS](https://img.shields.io/badge/Architecture-FreeRTOS-FF0000) ![LVGL](https://img.shields.io/badge/UI-LVGL_v9-green)

*(Insert a cool picture of the screen running on your machine here)*

***Add demo images***

***Make a demo video (YouTube)***

## 🗂️ Table of Contents
- [What is it?](#-what-is-it)
- [What does it do?](#-what-does-it-do)
- [Why bother? (The Origin Story)](#-why-bother-the-origin-story)
- [What do we use? (BOM)](#-what-do-we-use-bom)
- [The Process, Rationalities, and Trouble Along the Way](#-the-process-rationalities-and-trouble-along-the-way)
- [Future Roadmap](#-future-roadmap)

---

## 💡 What is it?
This project's goal is to make every espresso lover's brewing much smarter with even the most "legacy" mechanical machines (specifically tested on my VBM Domobar Junior). It utilizes sensors and the full power and might of an ESP32-WROOM-32. We're talking a state-machine, dual-bus, and dual-core RTOS architecture with a high-FPS touch display.

## ⚙️ What does it do?
Using sensors, Wi-Fi, math, and some thermodynamics, we can:

* 🌡️ **Know exactly when the boiler is ready** using a high-precision PT100 temperature sensor, so we can steam milk already if we don't want the coffee and prefer just a hot chocolate (why?).
* 🔮 **Predict with surprising accuracy the machine's ready time** (after a per-machine calibration), so we know when the grouphead is actually ready to brew. 
* 🎵 **Audio Cues:** When ready, a melody plays via a passive buzzer, with 3 available options: the *Doom* theme, the *Helldivers 2* theme, or mute (for democracy).
* 📱 **Push Notifications:** Also when ready, an HTTP notification is sent via `ntfy.sh` to your phone, so you can work in another room and still know when your machine is ready to pull.
* 📈 **Live Telemetry:** Started brewing your shot? You get a live shot timer and boiler temps on a rolling 30-second chart! The brew is detected automatically with a non-invasive SCT current sensor clamped around the machine's pump wire.
* 🌐 **Interactive Web Dashboard:** A baked-in HTML/JS interface served locally (`espresso.local`). Watch live temps, view historical CSV logs, and remotely delete old files directly from your phone.
* ⭐ **Rate Your Shot:** A post-brew UI slider allows you to score the shot from 0-10, which is immediately appended to the SD card CSV log alongside the 1Hz temperature arrays.

## 📖 Why bother? (The Origin Story)
The legendary E61 espresso grouphead is a marvel of 1960s thermodynamics. It uses a massive 4kg block of solid brass to guarantee temperature stability during a shot. But that massive thermal mass comes with a catch: while the internal boiler might reach 117°C in 3 minutes, the grouphead itself takes another 15+ minutes to absorb that heat and reach the perfect 90°C brewing temperature. 

I wanted to know *exactly* when my machine was truly ready, track my shot parameters in real-time, and log the data to dial in my espresso. I also wanted to do it safely, without permanently modifying the machine or splicing into 220V mains power and risking a fiery death.

Thus, Espresso Monitor was born. 

## 🛠️ What do we use? (BOM)
* 🧠 **ESP32-WROOM-32:** The brains. Specifically, a DOIT DevKit V1 (though we use a custom partition scheme).
* 🌡️ **Adafruit MAX31865 RTD Amplifier & PT100 Probe:** For extreme temperature accuracy.
* ⚡ **SCT-013 Non-invasive AC Current Sensor:** To detect the water pump.
* 🔌 **10uF capacitor + two 10K ohms resistors:** Needed for the SCT to work (and not kill the ESP32).
* 🎧 **3.5mm female audio jack:** Instead of stripping the SCT's wire and soldering its tips to the circuit, it's much easier to just use a female jack.
* 📺 **ILI9341 TFT SPI touch display 240x320:** The screen.
* 💾 **MicroSD card module:** Any SD card you trust enough not to die tomorrow.
* 🔊 **Passive Buzzer:** For the audio cues.
* 🔀 **Wago splitters (Optional but recommended):** Less of a mess in your box and easy to use.
* 📏 **Extension wires for the PT100 (Optional):** Only use insulated cables! The PT100 is super sensitive. I used spare backup camera wires.

---

## 🚧 The Process, Rationalities, and Trouble Along the Way

So... There I am, sitting there in front of an empty IDE project, and a desk full of electronic parts I ordered. Where do I start?

> 📝 *Side note: I originally started with a simple non-touch I2C 0.98-inch display and had no plans for a server yet. The tiny screen was cute, but it was holding me back. I wanted live rolling charts. I wanted touch controls. I wanted data. So, I upgraded to the 2.4" TFT and dove headfirst into the abyss of UI design and dual-core microcontrollers.*

Here is a chronological list of the walls I hit at 100mph, and how I engineered my way through them.

### 🔧 Phase 1: The Hardware Trenches

#### 💥 1. You Can't Drill a Hole in a $1,500 Machine (Newton's Law)
I could measure the boiler, but how do you measure the temperature of a 4kg chrome brass E61 grouphead without physically putting a probe inside it? 

> **✅ The Fix:** Thermodynamics. I treated the boiler as the heat source and wrote a C++ algorithm using an inverted **Newton’s Law of Heating**. While the machine is in the `WARMUP` state, a stopwatch tracks the boiler and uses an exponential asymptote curve to estimate how much heat the brass has absorbed. Once it hits `READY`, the algorithm freezes the estimate to prevent the math from violently snapping the temperature backwards when internal timers reset. I even built a calibration screen. You stick a physical thermometer on your grouphead, enter that number into the UI spinbox, and the ESP32 dynamically reverse-engineers the thermal "sluggishness" ($\tau$) of your specific brass block. 

> 💡 **Pro-tip:** *If you rapidly click a "+" button 15 times to change a calibration setting, and your code writes that to NVS flash memory on every click, you will fry your flash chip's write-cycles in a month. I tied the NVS flash command to the RTOS state machine, so it only physically burns the new calibration to memory exactly once when you exit the Settings screen.*

#### 💥 2. The 220V EMI Nightmare (Pump Detection)
I needed to start the shot timer the millisecond the 220V water pump turned on. Clamping an SCT-013 analog current sensor over the wire was safe, but the inside of an espresso machine is a nightmare of Electromagnetic Interference (EMI) from the heating elements. 

> **✅ The Fix:** I built a dynamic `CurrentManager`. Instead of hardcoding a trigger value, the ESP32 samples the ambient electrical noise floor on boot, calculates the RMS voltage, sets a dynamic threshold, and applies a strict time-based debounce filter (ON for 250ms, OFF for 500ms) to guarantee the timer only triggers on true pump activation. *Crucially, I had to pin this DSP (Digital Signal Processing) loop specifically to Core 1. When it ran on Core 0, the ESP32's Wi-Fi radio would constantly interrupt the microsecond ADC sampling, creating "phantom" current readings!*

---

### 🧠 Phase 2: The RTOS Ceiling

#### 💥 3. The 3.7-Second Death (Or: Never Trust the UI Builder)
I designed a beautiful interface using SquareLine Studio, exported the code, and flashed it. I pulled the virtual lever. 1 second... 2 seconds... 3.7 seconds... and the ESP32 violently panicked and rebooted. When it didn't crash, the chart drew wild, jagged lines of red "junk" data.

> **✅ The Fix:** SquareLine had hardcoded a tiny 10-point C-array into the background. When my code shoved 300 points of temperature data into that chart, it violently overwrote 290 adjacent memory addresses in RAM—corrupting FreeRTOS task pointers. I severed SquareLine's control over the chart entirely. By re-initializing the series natively in C++, LVGL's memory manager took over, allowing the chart to dynamically resize itself every tick safely without nuking the heap.

#### 💥 4. The Missing Espresso (The Beat Frequency Bug)
I ran a mock 25-second shot. When I looked at the summary chart, it only showed about 18 seconds of data. Two-thirds of the graph was just... gone. Where did my espresso go?

> **✅ The Fix:** A classic RTOS timing collision. My main loop had a `delay(33)` to feed the FreeRTOS watchdog. I was also saving a data point every `100ms` via `millis()`. But 33 + 33 + 33 = 99. The 3rd loop was *just barely* too fast, so the ESP32 waited for the 4th loop (132ms) to log the data. I thought I was logging perfectly, but I was secretly dropping frames. I abandoned `millis()` entirely and synced the array writes directly to the visual seconds timer string changing on the UI. Flawless, perfectly synced 1Hz logging.

#### 💥 5. The 1.79MB Brick Wall
I wanted an Async web server so I didn't have to squint at the machine from the living room. I crammed a beautiful HTML dashboard into the code, hit compile... and the IDE basically laughed at me. `Compilation error: text section exceeds available space.` My binary was 1.79MB. The default ESP32 partition scheme only gives you 1.2MB for the app.

> **✅ The Fix:** A good engineer knows when to kill his darlings. I switched to the `Huge APP (3MB No OTA)` partition scheme. Goodbye Over-The-Air Wi-Fi updates. Goodbye to the mini-game I wanted to add. I sacrificed them to the memory gods, unlocking 3MB of pure, unadulterated headroom.

#### 💥 6. The Shared SPI Bus of Death
The SD Card? Works perfectly. The PT100 temperature amplifier? Works perfectly. Both of them running at the same time? Absolute chaos. They share the exact same HSPI hardware pins. Because Core 0 (the web server) and Core 1 (the UI) were running asynchronously, if they both tried to use the pins at the same microsecond, the machine locked up.

> **✅ The Fix:** I had to become a traffic cop. I implemented a strict FreeRTOS `SemaphoreHandle_t` (the `spiMutex`). Every SD card write and temperature read is wrapped in a mutex lock. The cores have to politely wait their turn to talk over the physical copper wires. Zero collisions.

#### 💥 7. The Ticking "RAM Bomb"
I added a button to the dashboard to download the CSV log. I used `file.readString()` to load the file from the SD card into a string to send over Wi-Fi. It worked great... for about 5 shots. By my 50th shot, that file would be larger than the ESP32's fragile heap memory, triggering an Out-Of-Memory panic.

> **✅ The Fix:** I bypassed the RAM entirely, switching to the `streamFile` method to chunk the CSV data bit-by-bit directly to the Wi-Fi chip. Oh, and I added an `/api/clear` endpoint with a bright red "Clear" button, because the absolute last thing I ever want to do is unscrew this enclosure to pop out the SD card.

---

## 🚀 Future Roadmap
* Adding AI (API) based log analysis and recommendations.
* Adding a Bluetooth scale to stop the brewing automatically at a desired shot weight (using a relay).
* ~~Adding a mini-game to play on the touch screen while waiting for the heat soak.~~ *(Killed to protect RTOS stability. A good engineer knows when to say no).*

***

> ☕ *Built with C++, FreeRTOS, and far too much caffeine.*