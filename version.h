#ifndef VERSION_H
#define VERSION_H

// build stamp is filled in by the compiler (__DATE__/__TIME__) and changes automatically on every
// recompile, so the version shown in Settings always advances per build
#define FW_VERSION_BASE "1."

// e.g. "1.Jun 15 2026 14:30:22"
#define FW_VERSION  FW_VERSION_BASE "" __DATE__ "/" __TIME__

#endif
