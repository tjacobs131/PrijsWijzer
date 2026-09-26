#include <Arduino.h>
#include <WiFi.h>           // ESP32 Wi-Fi library
#include <WebServer.h>      // ESP32 Web Server
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <vector>
#include "Display.h"
#include "EnergyZeroAPI.h"
#include "HomeWizardAPI.h"
#include "GoodWeAPI.h"
#include "icons.h"

using namespace std;

#ifndef WIFI_SSID
#error "WIFI_SSID is not defined"
#endif

#ifndef WIFI_PASSWORD
#error "WIFI_PASSWORD is not defined"
#endif

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

enum MainState { Init, Standby, GetHomeWizard, GetSolarGo, GetEnergyZero, Decide, PushNotification, PrintStatus };
MainState mainState = MainState::Standby;

enum StatusState { Status_Import, Status_Solar, Status_Price };
StatusState currentStatus = StatusState::Status_Import;

float currentPrice = -1.0;

// Variables to hold HomeWizard data
float currentPowerUsage = 0.0;
float currentTotalImport = 0.0;

OLEDDriver display;

HomeWizardAPI hwAPI("192.168.2.27");
EnergyZeroAPI energyAPI;
GoodWeAPI goodwe;

GoodWeData gwData;
HomeWizardData hwData;

// Advice and reason icons
const uint8_t* active_smiley = icon_smiley_neutral;
const uint8_t* active_reason = nullptr; 

// Dynamic cycle tracking logic
const uint8_t* active_reasons_array[5] = {0}; 
int active_reasons_count = 0; 

// Textual representation for the Android App
String currentAdviceStr = "NEUTRAL";
std::vector<String> currentReasonsStr;

WebServer server(8080);

void setup() {
    Serial.begin(115200);
    Serial.println("Starting code");



    Serial.println("Wifi SSID: " + String(ssid));
    Serial.println("Wifi Password: " + String(password));

    display.begin();

    display.clear();
    display.displayText("Connecting WiFi...", 0, 10);
    String ssidDisplay = "SSID: ";
    ssidDisplay += String(ssid);
    display.displayText(ssidDisplay.c_str(), 0, 20);
    display.update();
    
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");
    Serial.println("Own IP Address: " + WiFi.localIP().toString());

    // Setup Local Server
    server.on("/api/notification", HTTP_GET, []() {
        JsonDocument responseDoc;
        responseDoc["advice"] = currentAdviceStr;
        
        JsonArray reasons = responseDoc["reasons"].to<JsonArray>();
        for (const String& r : currentReasonsStr) {
            reasons.add(r);
        }

        JsonDocument pricesDoc;
        String rawJson = energyAPI.GetCachedPricesJSON();
        if (rawJson.length() > 0) {
            deserializeJson(pricesDoc, rawJson);
            responseDoc["prices"] = pricesDoc["Prices"]; // Add 24h price array to payload
        } else {
            responseDoc["prices"] = JsonArray(); // Fallback empty
        }

        String responseString;
        serializeJson(responseDoc, responseString);
        server.send(200, "application/json", responseString);
    });
    server.begin();
    Serial.println("Local JSON server running on port 8080");

    display.clear();
    display.displayText("Syncing Time...", 0, 10);
    display.update();

    configTime(0, 0, "pool.ntp.org");
    time_t now = time(nullptr);
    while (now < 24 * 3600) {
        delay(2000);
        now = time(nullptr);
    }
    Serial.println("Time synced.");

    display.clear();
    display.displayText("Inverter search...", 0, 10);
    display.update();

    String inverterList = GoodWeAPI::searchInverters();
    Serial.println("Inverter Search Result: " + inverterList);
    if (inverterList.length() == 0) {
        display.clear();
        display.displayText("No inverter found", 0, 10);
        display.update();
        delay(2000);
        return;
    }
    else
    {
        String inverterIP = inverterList.substring(0, inverterList.indexOf(','));
        goodwe.init(inverterIP, 8899); 
        Serial.println("Inverter IP: " + inverterIP);

        display.displayText("Inverter IP: ", 0, 20);
        display.displayText(inverterIP.c_str(), 0, 30);
        display.update();
    }

    delay(2000);

    display.clear();
    display.displayText("Setup: complete", 0, 10);
    display.update();

    delay(2000);
    display.clear();

    mainState = Init;
}

unsigned long lastGoodWeUpdate = 0;
unsigned long lastGoodweSearch = 0;
unsigned long lastHomeWizardUpdate = 0;
unsigned long lastEnergyZeroUpdate = 0;
unsigned long lastStatusChange = 0;

bool goodweError = false;

void loop() {
    // Process incoming HTTP requests for the Android notification payload
    server.handleClient();

    time_t now = time(nullptr);

    if (millis() - lastGoodweSearch >= 60000) {
        lastGoodweSearch = millis();
        String inverterList = GoodWeAPI::searchInverters();
        
        String inverterIP = inverterList.substring(0, inverterList.indexOf(','));
        goodwe.init(inverterIP, 8899); 
    }

    if (millis() + 500 - lastGoodWeUpdate >= 2000) {
        lastGoodWeUpdate = millis();
        if (goodwe.isInitialized()) {
            gwData = goodwe.getData();
            if (gwData.success) {
                goodweError = false;
            } else {
                goodweError = true;
            }
        }
    }

    if (millis() - lastHomeWizardUpdate >= 2000) {
        lastHomeWizardUpdate = millis();
        hwData = hwAPI.getData();
    }

    if (millis() - lastEnergyZeroUpdate >= 60000) {
        lastEnergyZeroUpdate = millis();
        currentPrice = energyAPI.GetCurrentPrice();
    }

    if (millis() - lastStatusChange >= 5000) {
        lastStatusChange = millis();
        currentStatus = static_cast<StatusState>((currentStatus + 1) % 3);
    }

    // Main State Machine
    switch (mainState) {
        case Init:
            display.displayText("Fetching data...", 0, 0);
            display.update();

            gwData = goodwe.getData();
            hwData = hwAPI.getData();
            currentPrice = energyAPI.GetCurrentPrice();

            mainState = Decide;
            break;

        case Standby:
            mainState = Decide;
            break;

        case Decide: {
            bool gw_high = gwData.active_power_w > 1200;
            bool gw_low  = gwData.active_power_w < 300;
            bool gw_medium = !gw_high && !gw_low;

            bool hw_high_import = hwData.active_power_w > 2000;
            bool hw_high_export = hwData.active_power_w < 400;
            
            float price      = currentPrice + 0.13;
            bool price_low   = price < 0.18; 
            bool price_high  = price > 0.33;
            bool price_medium = !price_low && !price_high;
            bool price_negative = price < 0.0;

            int good_votes = 0;
            const uint8_t* reasons_good[5] = {nullptr};
            int wait_votes = 0;
            const uint8_t* reasons_wait[5] = {nullptr};
            int neutral_votes = 0;
            const uint8_t* reasons_neutral[5] = {nullptr};

            auto addVote = [](const uint8_t** arr, int& count, const uint8_t* icon) {
                for(int i = 0; i < count; i++) { if (arr[i] == icon) return; }
                if (count < 5) arr[count++] = icon;
            };

            if (gw_high) addVote(reasons_good, good_votes, advice_icon_solar_panel); 
            if (gw_medium) addVote(reasons_neutral, neutral_votes, advice_icon_solar_panel); 
            if (hw_high_import && price_high) addVote(reasons_wait, wait_votes, advice_icon_charging); 
            if (hw_high_export && price_low) addVote(reasons_good, good_votes, advice_icon_charging); 

            if (price_negative) addVote(reasons_good, good_votes, advice_icon_euro_symbol); 
            if (price_low) addVote(reasons_good, good_votes, advice_icon_euro_symbol); 
            if (price_high) addVote(reasons_wait, wait_votes, advice_icon_euro_symbol); 
            if (price_medium) addVote(reasons_neutral, neutral_votes, advice_icon_euro_symbol); 

            // Update icon states and text-equivalent states
            if (good_votes > wait_votes) {
                active_smiley = icon_smiley_good;
                currentAdviceStr = "GOOD";
                active_reasons_count = good_votes;
                for (int i=0; i < good_votes; i++) active_reasons_array[i] = reasons_good[i];
                
            } else if (wait_votes > good_votes) {
                active_smiley = icon_smiley_wait;
                currentAdviceStr = "WAIT";
                active_reasons_count = wait_votes;
                for (int i=0; i < wait_votes; i++) active_reasons_array[i] = reasons_wait[i];

            } else {
                active_smiley = icon_smiley_neutral;
                currentAdviceStr = "NEUTRAL";
                active_reasons_count = 0;
                
                for (int i=0; i < good_votes; i++) { if (active_reasons_count < 5) active_reasons_array[active_reasons_count++] = reasons_good[i]; }
                for (int i=0; i < wait_votes; i++) { if (active_reasons_count < 5) active_reasons_array[active_reasons_count++] = reasons_wait[i]; }
                for (int i=0; i < neutral_votes; i++) { if (active_reasons_count < 5) active_reasons_array[active_reasons_count++] = reasons_neutral[i]; }
            }

            // Sync the textual representation for the Android app
            currentReasonsStr.clear();
            for (int i = 0; i < active_reasons_count; i++) {
                if (active_reasons_array[i] == advice_icon_solar_panel) currentReasonsStr.push_back("SOLAR");
                else if (active_reasons_array[i] == advice_icon_charging) currentReasonsStr.push_back("CHARGING");
                else if (active_reasons_array[i] == advice_icon_euro_symbol) currentReasonsStr.push_back("PRICE");
            }

            mainState = PrintStatus;
            break;
        }

        case PushNotification:
            mainState = PrintStatus;
            break;

        case PrintStatus: {
            display.clear();
            switch (currentStatus) {
                case Status_Import:
                    display.displayImage(0, 0, icon_import, 24, 24);
                    {
                        stringstream stream;
                        stream << fixed << std::setprecision(0) << hwData.active_power_w << " W";
                        display.displayText(stream.str().c_str(), 24, 10, 1);
                    }
                    break;
                case Status_Solar:
                    display.displayImage(0, 0, icon_power, 24, 24);
                    {
                        stringstream stream;
                        if (gwData.success) {
                            stream << fixed << std::setprecision(0) << gwData.active_power_w << " W";
                        } else {
                            Serial.println("GoodWe API error: " + gwData.errorMessage);
                            stream << "No Solar";
                        }
                        display.displayText(stream.str().c_str(), 26, 5, 1);
                        stream.str(""); 
                        stream << fixed << std::setprecision(0) << gwData.active_power_w + hwData.active_power_w << " W";
                        display.displayText(stream.str().c_str(), 26, 15, 1);
                    }
                    break;
                case Status_Price:
                    display.displayImage(0, 0, icon_import_price, 24, 24);
                    { 
                        stringstream stream;
                        stream << fixed << std::setprecision(2) << currentPrice + 0.13 << "/kWh"; 
                        display.displayText(stream.str().c_str(), 24, 10, 1);
                    }
                    break;
            }

            if (active_reasons_count > 0) {
                int timeTicker_index = (millis() / 2000) % active_reasons_count;
                active_reason = active_reasons_array[timeTicker_index];
            } else {
                active_reason = nullptr; 
            }

            if (active_smiley != nullptr) {
                display.displayImage(78, 0, active_smiley, 24, 24); 
            }
            if (active_reason != nullptr) {
                display.displayImage(104, 0, active_reason, 24, 24); 
            }

            {
                stringstream stream;
                stream.str("");
                stream << fixed << "Fase 1: " << hwData.l1_voltage << "V, " << fixed << std::setprecision(3) <<  hwData.l1_current << "A";
                display.displayText(stream.str().c_str(), 0, 30);
                
                stream.str("");
                stream << fixed << "Fase 2: " << hwData.l2_voltage << "V, " << fixed << std::setprecision(3) <<  hwData.l2_current << "A";
                display.displayText(stream.str().c_str(), 0, 40);
                
                stream.str("");
                stream << fixed << "Fase 3: " << hwData.l3_voltage << "V, " << fixed << std::setprecision(3) << hwData.l3_current << "A";
                display.displayText(stream.str().c_str(), 0, 50);
            }   

            mainState = Standby;
            break;
        }
    }

    display.update();
    delay(20);
}