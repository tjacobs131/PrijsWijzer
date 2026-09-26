#include "EnergyZeroAPI.h"

float EnergyZeroAPI::GetCurrentPrice()
{
    time_t now;
    time(&now);

    if (lastUpdateTimestamp != 0) {
        double diff = difftime(now, lastUpdateTimestamp);
        secondsSinceLastUpdate = (diff > 32767) ? 32767 : (int16_t)diff; 
    }

    if (currentPrice != -1.0 && lastUpdateTimestamp != 0)
    {
        // Use 1-hour cache logic
        if ((now / 3600) == (lastUpdateTimestamp / 3600)) {
            return currentPrice;
        }
    }

    string url = getURL();

    if (!createConnection(url)) {
        return currentPrice != -1.0 ? currentPrice : -1.0; 
    }

    // Capture the raw HTTP string before parsing it
    String payload = http.getString();
    http.end(); 

    JsonDocument doc; 
    DeserializationError error = deserializeJson(doc, payload);

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return currentPrice != -1.0 ? currentPrice : -1.0; 
    }

    if (!doc["Prices"].isNull() && doc["Prices"].size() > 0)
    {
        currentPrice = doc["Prices"][0]["price"].as<float>();
        lastUpdateTimestamp = now;  
        secondsSinceLastUpdate = 0; 
        cachedPricesJSON = payload; // Save 24h data to serve to the app

        return currentPrice;
    }
    else
    {
        Serial.println("No price data found");
        return currentPrice != -1.0 ? currentPrice : -1.0; 
    }
}

int16_t EnergyZeroAPI::GetSecondsSinceLastUpdate() const { 
    return secondsSinceLastUpdate; 
}

String EnergyZeroAPI::GetCachedPricesJSON() const {
    return cachedPricesJSON;
}

string EnergyZeroAPI::getURL()
{
    time_t now;
    time(&now);
    
    // fromDate is right now (UTC)
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    char fromDateStr[30];
    strftime(fromDateStr, sizeof(fromDateStr), "%Y-%m-%dT%H:00:00.000Z", &timeinfo);
    
    // tillDate is +24 hours into the future
    time_t tillTime = now + (24 * 3600);
    struct tm tillTimeinfo;
    gmtime_r(&tillTime, &tillTimeinfo);
    tillTimeinfo.tm_min = 59;
    tillTimeinfo.tm_sec = 59;
    
    char tillDateStr[30];
    strftime(tillDateStr, sizeof(tillDateStr), "%Y-%m-%dT%H:%M:%S.999Z", &tillTimeinfo);

    string url = "https://api.energyzero.nl/v1/energyprices?";
    url += "fromDate=" + string(fromDateStr) + "&tillDate=" + string(tillDateStr);
    url += "&interval=4&usageType=1&inclBtw=true";
    
    return url;
}

bool EnergyZeroAPI::createConnection(const string& url) {
    http.begin(url.c_str());
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
        if (httpResponseCode == HTTP_CODE_OK)
        {
            return true;
        }
        else
        {
            Serial.print("HTTP Error: ");
            Serial.println(httpResponseCode);
            http.end();
            return false;
        }
    }
    else
    {
        Serial.print("Error during HTTP GET: ");
        Serial.println(httpResponseCode);
        http.end();
        return false;
    }
}