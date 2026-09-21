#include "GoodWeAPI.h"

void GoodWeAPI::init(const String& inverterIp, uint16_t inverterPort) {
    ip = inverterIp;
    port = inverterPort;
}

GoodWeData GoodWeAPI::getData() {
    GoodWeData data;
    data.success = false;
    
    // GoodWe NS payload
    // Slave ID: 0x7F, Function: 0x03, Start Reg: 0x7594, Count: 0x0049 (73 regs), CRC: D5 C2
    uint8_t request[] = { 0x7F, 0x03, 0x75, 0x94, 0x00, 0x49, 0xD5, 0xC2 };
    
    Serial.printf("Sending GoodWe NS-Series UDP request to %s:%d\n", ip.c_str(), port);

    udp.begin(8890); 
    
    IPAddress targetIP;
    if (!targetIP.fromString(ip)) {
        data.errorMessage = "Error: Invalid IP Address format";
        return data;
    }

    udp.beginPacket(targetIP, port);
    udp.write(request, sizeof(request));
    
    if (udp.endPacket() == 0) {
        data.errorMessage = "Network Error: Failed to send UDP packet";
        udp.stop();
        return data;
    }

    // Wait up to 2 seconds for a response from the inverter
    unsigned long start = millis();
    int packetSize = 0;
    while (millis() - start < 2000) {
        packetSize = udp.parsePacket();
        if (packetSize > 0) break;
        delay(10);
    }

    if (packetSize > 0) {
        uint8_t buffer[256];
        int len = udp.read(buffer, sizeof(buffer));
        
        Serial.printf("Received %d bytes from inverter.\n", len);

        int data_offset = -1;

        // Check if response is wrapped in the older "AA 55" Wi-Fi module prefix
        if (len > 70 && buffer[0] == 0xAA && buffer[1] == 0x55 && buffer[2] == 0x7F && buffer[3] == 0x03) {
            data_offset = 5; // The actual Modbus data bytes start at index 5
        } 
        // Check if it's a pure Modbus RTU response in case it uses the new protocol
        else if (len > 68 && buffer[0] == 0x7F && buffer[1] == 0x03) {
            data_offset = 3; // The actual Modbus data bytes start at index 3
        }

        if (data_offset != -1) {
            // Total Active Power is at Register 30127 (4 bytes long)
            int active_power_index = data_offset + 54;
            int32_t active_power = ((int32_t)buffer[active_power_index] << 24) | 
                                   ((int32_t)buffer[active_power_index + 1] << 16) | 
                                   ((int32_t)buffer[active_power_index + 2] << 8) | 
                                   buffer[active_power_index + 3]; 
            
            // Daily Generation (kWh) is at Register 30144 (2 bytes) scaled by 10 (26 = 2.6 kWh)
            int daily_kwh_index = data_offset + 88;
            uint16_t daily_kwh_raw = (buffer[daily_kwh_index] << 8) | buffer[daily_kwh_index + 1];
            
            data.active_power_w = (float)active_power;
            data.daily_generation_kwh = daily_kwh_raw / 10.0;
            data.success = true;
            
            Serial.println("SUCCESS: Parsed NS-Series running data!");
        } else {
            data.errorMessage = "Protocol Error: Unrecognized NS response header";
            
            // Print out the first 20 bytes to the serial monitor so you can debug what it sent back
            Serial.print("Raw Bytes: ");
            for(int i=0; i< (len < 20 ? len : 20); i++) {
                Serial.printf("%02X ", buffer[i]);
            }
            Serial.println();
        }
    } else {
        data.errorMessage = "Network Error: Timeout waiting for GoodWe NS response";
    }

    udp.stop();
    return data;
}


String GoodWeAPI::searchInverters() {
    WiFiUDP searchUdp;
    const uint16_t broadcastPort = 48899;
    const char* command = "WIFIKIT-214028-READ";
    
    // Start UDP on the broadcast port
    searchUdp.begin(broadcastPort);
    
    searchUdp.beginPacket(IPAddress(255, 255, 255, 255), broadcastPort);
    searchUdp.write((const uint8_t*)command, strlen(command));
    
    if (searchUdp.endPacket() == 0) {
        Serial.println("Error: Failed to send broadcast discovery packet");
        searchUdp.stop();
        return "";
    }

    unsigned long start = millis();
    int packetSize = 0;
    
    // Wait up to 2 seconds for a broadcast response
    while (millis() - start < 2000) {
        packetSize = searchUdp.parsePacket();
        if (packetSize > 0) break;
        delay(10);
    }

    String response = "";
    if (packetSize > 0) {
        char buffer[256];
        // Read the packet into the buffer, leaving room for the null terminator
        int len = searchUdp.read(buffer, sizeof(buffer) - 1);
        if (len > 0) {
            buffer[len] = '\0'; // Null terminate for safe string conversion
            response = String(buffer);
            Serial.print("Discovered inverter response: ");
            Serial.println(response);
        }
    } else {
        Serial.println("Discovery Error: No response received to broadcast request.");
    }

    searchUdp.stop();
    return response;
}