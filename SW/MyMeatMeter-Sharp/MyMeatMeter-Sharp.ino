/* Smokehouse temp measurement for PT100/PT1000 sensor and inner temp measurement for SHT4x sensor
 * ESP32 S3 devkit
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 */
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>	 // https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SharpMem.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_MAX31865.h>
#include <Adafruit_MAX31856.h>
#include "page.h"

#define TOMTHUMB_USE_EXTENDED 1 // Set to 1 to use the extended 128 ASCII character set - Adds degree sign and other useful characters.
#include "TomThumb.h"

#define PT1000 // select sensor type: PT100 or PT1000

#if defined(PT1000)
#define RREF 4300.0
#define RNOMINAL 1000.0
#elif defined(PT100)
#define RREF 430.0
#define RNOMINAL 100.0
#endif

const char *host = "smokehouse"; // Connect to http://smokehouse.local

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define SCK   12          //LaskaKit ESP32-S3-DEVKit
#define MOSI  11          //LaskaKit ESP32-S3-DEVKit
#define MISO  13          //LaskaKit ESP32-S3-DEVKit
#define SHARP_CS    17
#define MAX31865_CS 18
#define MAX31856_CS 10
#define USUP_POWER  47 //uSUP connector power output pin
#define BLACK 0
#define WHITE 1

#define DELAYTIME 1   //seconds
#define DEGREE_SIGN 0x8E

Adafruit_SharpMem display(SCK, MOSI, SHARP_CS, 400, 240);                       // Nastaví display
Adafruit_MAX31865 thermoPT = Adafruit_MAX31865(MAX31865_CS, MOSI, MISO, SCK);   // Nastaví senzor PT1000
Adafruit_MAX31856 thermoK = Adafruit_MAX31856(MAX31856_CS, MOSI, MISO, SCK);  // Nastaví senzor K

// Set web server port number to 80
WebServer server(80);

/* -----------------WiFi network ---------------- */
char ssid[] = "laskalab";
char pass[] = "laskaLAB754125";

void sht4x_init(void)
{
	if (!sht4.begin(&Wire))
	{
		Serial.println("Couldn't find SHT4x");
	}
	sht4.setPrecision(SHT4X_HIGH_PRECISION); // Set sensor precision (HIGH, MED, LOW), different modes affect measurement duration
}

void thermoK_init()
{
  if (!thermoK.begin()) {
    Serial.println("Could not initialize thermocouple.");
    while (1) delay(10);
  }
  thermoK.setThermocoupleType(MAX31856_TCTYPE_K);
  thermoK.setConversionMode(MAX31856_ONESHOT_NOWAIT);

}

int8_t sht4x_errorCheck(void)
{
	sensors_event_t humidity, temp;
	if (!sht4.getEvent(&humidity, &temp)) // populate temp and humidity objects with fresh data
	{
		if (!sht4.begin(&Wire))
		{
			Serial.println("Couldn't find SHT4x");
			return -1;
		}
	}
	return 0;
}

float sht4x_measureTemp(void)
{
	sensors_event_t humidity, temp;
	float temp_value;
	sht4.getEvent(&humidity, &temp); // populate temp and humidity objects with fresh data
	temp_value = temp.temperature;	 // read temp value
	Serial.print("Temperature SHT40: ");
	Serial.print(temp_value);
	Serial.println(" degrees C");
	//   Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");
	return temp_value;
}

float tempPT_measure(void)
{
	float temp = thermoPT.temperature(RNOMINAL, RREF);

	// Read temperature
	Serial.print("Meat temperature = ");
	Serial.println(temp);
	Serial.println();
	return temp;
}

int8_t tempPT_errorCheck(void)
{
	// Check and print any faults
	uint8_t fault = thermoPT.readFault();
	if (fault)
	{
		Serial.println("Mas spravne zapojeny senzor? Mas spravne zvoleny typ senzoru - PT100/PT100? Nebo spatne nastavene propojky?");
		Serial.print("Fault 0x");
		Serial.println(fault, HEX);
		if (fault & MAX31865_FAULT_HIGHTHRESH)
		{
			Serial.println("RTD High Threshold");
		}
		if (fault & MAX31865_FAULT_LOWTHRESH)
		{
			Serial.println("RTD Low Threshold");
		}
		if (fault & MAX31865_FAULT_REFINLOW)
		{
			Serial.println("REFIN- > 0.85 x Bias");
		}
		if (fault & MAX31865_FAULT_REFINHIGH)
		{
			Serial.println("REFIN- < 0.85 x Bias - FORCE- open");
		}
		if (fault & MAX31865_FAULT_RTDINLOW)
		{
			Serial.println("RTDIN- < 0.85 x Bias - FORCE- open - ");
		}
		if (fault & MAX31865_FAULT_OVUV)
		{
			Serial.println("Under/Over voltage");
		}
		Serial.println();
		thermoPT.clearFault();
		return -1;
	}
	return 0;
}

float thermoK_measure(void)
{
  float temp = 0;
  // trigger a conversion, returns immediately
  thermoK.triggerOneShot();

  delay(500); // replace this with whatever

  // check for conversion complete and read temperature
  if (thermoK.conversionComplete()) {
    temp = thermoK.readThermocoupleTemperature();
    Serial.println(temp);
  } else {
    Serial.println("Conversion not complete!");
  }

	// Read temperature
	Serial.print("Inside temperature = ");
	Serial.println(temp);
	Serial.println();
	return temp;
}

void displayInit(void)
{
  display.begin();  // initialization
  display.setRotation(0); // rotation
  display.clearDisplay(); // clear display
  display.setTextColor(BLACK, WHITE); // set color
	display.setFont(&TomThumb);
}

void printDisplay(void)
{
	uint16_t w, h, h_offset;
	int16_t x1, y1;
	char buffer[10];
	display.setTextSize(2);
	display.getTextBounds("Meat", 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((DISPLAY_WIDTH / 4) - (w / 2)), h);
	display.print("Meat");

	display.getTextBounds("Inner", 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((DISPLAY_WIDTH * 0.75) - (w / 2)), h);
	display.print("Inner");

  display.getTextBounds("Inside", 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((DISPLAY_WIDTH * 0.75) - (w / 2)), h);
	display.print("Inside");

	display.drawLine((DISPLAY_WIDTH / 2), 0, (DISPLAY_WIDTH / 2), DISPLAY_HEIGHT, WHITE);
	display.drawLine(0, h + 2, DISPLAY_WIDTH, h + 2, WHITE);

	if (tempPT_errorCheck() == -1)
	{
		sprintf(buffer, "E");
	}
	else
	{
		sprintf(buffer, "%.0f%c", tempPT_measure(), DEGREE_SIGN);
	}
	display.setTextSize(4);
	h_offset = h + 2;
	display.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((DISPLAY_WIDTH / 4) - (w / 2)), ((DISPLAY_HEIGHT + h_offset + h) / 2));
	display.print(buffer);

	if (sht4x_errorCheck() == -1)
	{
		sprintf(buffer, "E");
	}
	else
	{
		sprintf(buffer, "%.0f%c", sht4x_measureTemp(), DEGREE_SIGN);
	}
	display.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((DISPLAY_WIDTH * 0.75) - (w / 2)), ((DISPLAY_HEIGHT + h_offset + h) / 2));
	display.print(buffer);
	display.refresh();
	display.clearDisplay();
}

void handleRoot()
{
	server.send_P(200, "text/html", index_html); // Send web page
}

void handleNotFound()
{
	String message = "Error\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";
	for (uint8_t i = 0; i < server.args(); i++)
	{
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}
	server.send(404, "text/plain", message);
}

void handleMeatTemp()
{
	if (tempPT_errorCheck() == -1)
	{
		server.send(200, "text/plain", "ERROR");
	}
	else
	{
		char buff[11] = {0};
		sprintf(buff, "%0.2f ˚C", tempPT_measure());
		server.send(200, "text/plain", buff);
	}
}

void handleInnerTemp()
{
	if (sht4x_errorCheck() == -1)
	{
		server.send(200, "text/plain", "ERROR");
	}
	else
	{
		char buff[11] = {0};
		sprintf(buff, "%0.2f ˚C", sht4x_measureTemp());
		server.send(200, "text/plain", buff);
	}
}

void WiFiInit()
{
	// Connecting to WiFi
	Serial.println();
	Serial.print("Connecting to...");
	Serial.println(ssid);

	WiFi.begin(ssid, pass);

	int i = 0;
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(1000);
		i++;
		if (i == 10)
		{
			i = 0;
			Serial.println("");
			Serial.println("Not able to connect");
		}
		else
		{
			Serial.print(".");
		}
	}
	Serial.println("");
	Serial.println("Wi-Fi connected successfully");
}

void DNS_setup()
{
	if (MDNS.begin(host))
	{
		MDNS.addService("http", "tcp", 80);
		Serial.println("MDNS responder started");
		Serial.print("You can now connect to http://");
		Serial.print(host);
		Serial.println(".local");
	}
}

void setup(void)
{
  pinMode(PIN_ON, OUTPUT);
  digitalWrite(PIN_ON, HIGH);
  Serial.begin(115200);

	WiFiInit();
	DNS_setup();

	server.on("/", handleRoot);		   // Main page
	server.onNotFound(handleNotFound); // Function done when hangle is not found
	server.on("/readMeatTemp", handleMeatTemp);
	server.on("/readInnerTemp", handleInnerTemp);
	server.begin();
	Serial.println("HTTP server started");

	thermoPT.begin(MAX31865_2WIRE); // set to 2WIRE, 3WIRE or 4WIRE as necessary
	thermoK_init();

  displayInit();
}

void loop(void)
{
	server.handleClient();
	printDisplay();
  delay(DELAYTIME*1000);
}