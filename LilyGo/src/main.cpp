using namespace std;

#include <Arduino.h>

#include "Display.h"

enum MainState { Standby, GetHomeWizard, GetSolarGo, GetEnergyZero, PushNotification, PrintStatus };
MainState mainState = MainState::Standby;

OLEDDriver display;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting code");

    display.begin();

    display.clear();
    display.displayText("Setup: complete",0, 10);
    display.update();

    delay(500);

    display.clear();
}

void loop() {
    // Main State Machine
    switch (mainState) {
        case Standby:
            Serial.println("State: STANDBY");

            mainState = GetHomeWizard;

            break;

        case GetHomeWizard:
            Serial.println("State: GET HOMEWIZARD");

            mainState = GetSolarGo;

            break;

        case GetSolarGo:

            Serial.println("State: GET SOLARGO");

            mainState = GetEnergyZero;

            break;

        case GetEnergyZero:

            Serial.println("State: GET ENERGYZERO");
            
            mainState = PushNotification;

            break;

        case PushNotification:

            Serial.println("State: PUSH NOTIFICATION");

            mainState = PrintStatus;

            break;

        case PrintStatus:

            Serial.println("State: PRINT STATUS");

            display.clear();
            
            display.displayText("GetHomeWizard:", 0, 0);

            display.displayText("GetSolarGo:", 0, 20);

            display.displayText("GetEnergyZero:", 0, 40);

            mainState = Standby;

            break;
    }

    display.update();
    delay(10); // Kleine vertraging om de belasting te verminderen
}
