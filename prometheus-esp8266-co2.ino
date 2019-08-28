#include <stdio.h>

#include "Arduino.h"
#include "mhz19.h"

#include "SoftwareSerial.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

#define PIN_RX  D1
#define PIN_TX  D2

#define DATA_LEN 250
#define ROOM "bedroom"

SoftwareSerial sensor(PIN_RX, PIN_TX);
ESP8266WebServer server(8080);
WiFiManager wifiManager;
WiFiClient wifiClient;

static char esp_id[16];

static bool exchange_command(uint8_t cmd, uint8_t data[], unsigned int timeout)
{
    // create command buffer
    uint8_t buf[9];
    int len = prepare_tx(cmd, data, buf, sizeof(buf));

    // send the command
    sensor.write(buf, len);

    // wait for response
    unsigned long start = millis();
    while ((millis() - start) < timeout) {
        if (sensor.available() > 0) {
            uint8_t b = sensor.read();
            if (process_rx(b, cmd, data)) {
                return true;
            }
        }
        yield();
    }

    return false;
}

static bool read_temp_co2(int *co2, int *temp)
{
    uint8_t data[] = { 0, 0, 0, 0, 0, 0 };
    bool result = exchange_command(0x86, data, 3000);
    if (result) {
        *co2 = (data[0] << 8) + data[1];
        *temp = data[2] - 40;
#if 0
        char raw[32];
        sprintf(raw, "RAW: %02X %02X %02X %02X %02X %02X", data[0], data[1], data[2], data[3],
                data[4], data[5]);
        Serial.println(raw);
#endif
    }
    return result;
}

void handleRoot()
{
	int co2, temp;

	Serial.println("incoming request");

        if (read_temp_co2(&co2, &temp)) {
            Serial.print("CO2:");
            Serial.println(co2, DEC);
            Serial.print("TEMP:");
            Serial.println(temp, DEC);

	    char data[DATA_LEN];
	    snprintf(data, DATA_LEN, "# HELP mhz19_co2 (ppm)\n# TYPE mhz19_co2 gauge\nmhz19_co2{room=\"%s\"} %d \n# HELP mhz19_temp (celcius)\n# TYPE mhz19_temp gauge\nmhz19_temp{room=\"%s\"} %d \n", ROOM, co2, ROOM, temp);
			    
	    server.send(200, "text/plain", data);
        } else {
            // how to handle this in prometheus
	    server.send(200, "text/plain", "error");
	}
}

void handleNotFound()
{
	server.send(404, "text/plain", "not found");
}

void setup()
{
    Serial.begin(115200);
    Serial.println("MHZ19 ESP reader\n");

    sprintf(esp_id, "%08X", ESP.getChipId());
    Serial.print("ESP ID: ");
    Serial.println(esp_id);

    sensor.begin(9600);

    Serial.println("Starting WIFI manager ...");
    wifiManager.setConfigPortalTimeout(120);
    wifiManager.autoConnect("ESP-MHZ19");

    Serial.println("Starting webserver");

    server.on("/", handleRoot);
    server.on("/metrics", handleRoot);

    server.onNotFound(handleNotFound);

    server.begin();
}

void loop()
{
    server.handleClient();
}
