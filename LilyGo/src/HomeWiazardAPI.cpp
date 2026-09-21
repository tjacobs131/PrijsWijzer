#include "HomeWizardAPI.h"

HomeWizardAPI::HomeWizardAPI(const String& ip) {
    url = "http://" + ip + "/api/v1/data";
}

HomeWizardData HomeWizardAPI::getData() {
    HomeWizardData data;
    HTTPClient http;
    
    http.setTimeout(3000); // 3 seconds timeout
    http.begin(url);
    
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
        String payload = http.getString();
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
            data.active_power_w = doc["active_power_w"];
            data.total_power_import_t1_kwh = doc["total_power_import_t1_kwh"];
            data.l1_voltage = doc["active_voltage_l1_v"];
            data.l1_current = doc["active_current_l1_a"];
            data.l2_voltage = doc["active_voltage_l2_v"];
            data.l2_current = doc["active_current_l2_a"];
            data.l3_voltage = doc["active_voltage_l3_v"];
            data.l3_current = doc["active_current_l3_a"];
            data.success = true;
        } else {
            data.errorMessage = "JSON parsing failed: " + String(error.c_str());
        }
    } else if (httpResponseCode == 403) {
        data.errorMessage = "API Disabled (403). Enable 'Local API' in HomeWizard app.";
    } else if (httpResponseCode > 0) {
        data.errorMessage = "HTTP Error: " + String(httpResponseCode);
    } else {
        data.errorMessage = "Network Error: " + http.errorToString(httpResponseCode);
    }

    http.end();
    return data;
}