#ifndef __ENERGYZEROAPI_H__ 
#define __ENERGYZEROAPI_H__ 

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <string>

using namespace std;

class EnergyZeroAPI {
public:
    EnergyZeroAPI() = default;

    float GetCurrentPrice();
    int16_t GetSecondsSinceLastUpdate() const;
    String GetCachedPricesJSON() const;

private:
    String url;
    HTTPClient http;

    float currentPrice = -1.0;               
    int16_t secondsSinceLastUpdate = -1;     
    time_t lastUpdateTimestamp = 0;          
    String cachedPricesJSON = "{}";
    
    string getURL();
    bool createConnection(const string& url);
};

#endif