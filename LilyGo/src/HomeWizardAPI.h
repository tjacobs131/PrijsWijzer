#ifndef __HOMEWIZARD_API_H__
#define __HOMEWIZARD_API_H__

using namespace std;
#include <WiFi.h>
#include <ArduinoJson.h>   
#include <HTTPClient.h>

struct HomeWizardData {
    bool success = false;
    float active_power_w = 0.0;
    float total_power_import_t1_kwh = 0.0;
    int l1_voltage = 0;
    float l1_current = 0.0;
    int l2_voltage = 0;
    float l2_current = 0.0;
    int l3_voltage = 0;
    float l3_current = 0.0;
    String errorMessage = "";
};

class HomeWizardAPI {
private:
    String url;

public:
    HomeWizardAPI(const String& ip);

    HomeWizardData getData();
};

#endif