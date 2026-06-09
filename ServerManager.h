#pragma once
#include <WebServer.h>
#include "BrewSession.h"
#include "SDManager.h"
#include "SystemState.h"

class ServerManager {
public:
    ServerManager();
    void begin(const char* ssid, const char* password);
    void setDataSources(TempSnapshot* temps, portMUX_TYPE* tempMux,
                        const SystemState* state, SDManager* sd);
    void handleClient();
    bool isConnected();

private:
    WebServer          _server;
    bool               _started     = false;
    const char*        _ssid        = nullptr;
    const char*        _password    = nullptr;
    TempSnapshot*      _temps       = nullptr;
    portMUX_TYPE*      _tempMux     = nullptr;
    const SystemState* _state       = nullptr;
    SDManager*         _sd          = nullptr;

    void tryStartServer();
    void handleRoot();
    void handleApiTemps();
    void handleApiLog();
    void handleApiBrewTemps();
    const char* stateToString(SystemState s);
};
