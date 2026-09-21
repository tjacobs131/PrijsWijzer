#include <Arduino.h>
#include <WiFi.h>           // ESP32 Wi-Fi library
#include <sstream>
#include <iomanip>
#include <cstdlib>
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

// Advice Icons storage 
const uint8_t* active_smiley = icon_smiley_neutral;
const uint8_t* active_reason = 0; 

void setup() {
    Serial.begin(115200);
    Serial.println("Starting code");

    Serial.println("Wifi SSID: " + String(ssid));
    Serial.println("Wifi Password: " + String(password));

    display.begin();

    display.clear();
    display.displayText("Connecting WiFi...", 0, 10);
    display.update();
    
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi Connected.");

    display.clear();
    display.displayText("Syncing Time...", 0, 10);
    display.update();

    configTime(0, 0, "pool.ntp.org");
    time_t now = time(nullptr);
    while (now < 24 * 3600) {
        delay(200);
        now = time(nullptr);
    }
    Serial.println("Time synced.");

    display.clear();
    display.displayText("Inverter search...", 0, 10);
    display.update();

    String inverterList = GoodWeAPI::searchInverters();
    Serial.println("Inverter Search Result: " + inverterList);
    String inverterIP = inverterList.substring(0, inverterList.indexOf(','));
    Serial.println("First Inverter IP: " + inverterIP);

    String inverterDisplay = "Inverter IP: ";
    inverterDisplay += inverterIP;
    display.displayText(inverterDisplay.c_str(), 0, 20);

    goodwe.init(inverterIP, 8899); 

    delay(1000);

    display.clear();
    display.displayText("Setup: complete", 0, 10);
    display.update();

    delay(100);
    display.clear();

    mainState = Init;
}

unsigned long lastGoodWeUpdate = 0;
unsigned long lastHomeWizardUpdate = 0;
unsigned long lastEnergyZeroUpdate = 0;
unsigned long lastStatusChange = 0;

bool goodweError = false;

void loop() {
    time_t now = time(nullptr);

    if (millis() + 500 - lastGoodWeUpdate >= 2000) {
        lastGoodWeUpdate = millis();

        gwData = goodwe.getData();

        if (gwData.success) {
            Serial.print("Current Inverter Power: ");
            Serial.print(gwData.active_power_w);
            Serial.println(" W");

            goodweError = false;
        } else {
            Serial.println(gwData.errorMessage);
            goodweError = true;
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
            // Route from Standby -> Decide -> PrintStatus
            mainState = Decide;
            break;

        case Decide:
            // Determine the advice / emojis based on the current data            
            if (gwData.success && gwData.active_power_w > 1400) {
                // High Solar Generation -> Good! 
                active_smiley = icon_smiley_good;
                active_reason = advice_icon_solar_panel;
            } else if (hwData.active_power_w > 1500) {
                // High HomeWizard Import -> Bad!
                active_smiley = icon_smiley_wait;
                active_reason = advice_icon_charging;
            } else if (hwData.active_power_w < 200) {
                // High HomeWizard Export -> Good!
                active_smiley = icon_smiley_good;
                active_reason = advice_icon_solar_panel;
            } else if (currentPrice + 0.13 <= 0.17) {
                // Very Cheap Energy -> Good!
                active_smiley = icon_smiley_good;
                active_reason = advice_icon_euro_symbol;
            } else if (currentPrice + 0.13 >= 0.35) {
                // Expensive Energy -> Wait!
                active_smiley = icon_smiley_wait;
                active_reason = advice_icon_euro_symbol;
            } else {
                // Normal usage / No specific advice -> Neutral.
                active_smiley = icon_smiley_neutral;
                active_reason = 0; 
            }

            // Decide on push notification
            bool doPushNotification;
            doPushNotification = false;
            
            if (doPushNotification) {
                mainState = PushNotification;
            } else {
                mainState = PrintStatus;
            }
            break;

        case PushNotification:
            Serial.println("State: PUSH NOTIFICATION");
            mainState = PrintStatus;
            break;

        case PrintStatus:
        {
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
                            stream << "No Solar";
                        }
                        display.displayText(stream.str().c_str(), 26, 5, 1);
                        
                        stream.str(""); // Clear the stream
                        stream << fixed << std::setprecision(0) << gwData.active_power_w + hwData.active_power_w << " W";
                        display.displayText(stream.str().c_str(), 26, 15, 1);
                    }
                    break;

                case Status_Price:
                    display.displayImage(0, 0, icon_import_price, 24, 24);
                    { 
                        stringstream stream;
                        stream << fixed << std::setprecision(2) << currentPrice + 0.13 << "/kWh"; // 0.13 is the base price 
                        display.displayText(stream.str().c_str(), 24, 10, 1);
                    }
                    break;
            }

            if (active_smiley != nullptr) {
                display.displayImage(78, 0, active_smiley, 24, 24); 
            }
            if (active_reason != nullptr) {
                display.displayImage(104, 0, active_reason, 24, 24); 
            }
            // --------------------------------------------------

            // Scope block for string conversion of HomeWizard Power (Bottom)
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