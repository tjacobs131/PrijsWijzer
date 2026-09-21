#ifndef __GOODWE_API_H__
#define __GOODWE_API_H__

#include <Arduino.h>
#include <WiFiUdp.h>

struct GoodWeData {
    bool success = false;
    float active_power_w = 0.0;
    float daily_generation_kwh = 0.0;
    String errorMessage = "";
};

class GoodWeAPI {
private:
    String ip;
    uint16_t port;
    WiFiUDP udp;

public:
    void init(const String& inverterIp, uint16_t inverterPort = 8899);

    GoodWeData getData();

    static String searchInverters();
};

#endif