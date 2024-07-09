/* Smokehouse temp measurement for PT100/PT1000 sensor and inner temp measurement for K thermocoupler
 * ESPink Shelf 2.9
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 */
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_MAX31865.h>
#include <Adafruit_MAX31856.h>
#include "page.h"

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>

#define PT1000 // select sensor type: PT100 or PT1000

#if defined(PT1000)
#define RREF 4300.0
#define RNOMINAL 1000.0
#elif defined(PT100)
#define RREF 430.0
#define RNOMINAL 100.0
#endif

const char *host = "smokehouse"; // Connect to http://smokehouse.local

#define SCK   14          //LaskaKit ESP32-S3-DEVKit
#define MOSI  13          //LaskaKit ESP32-S3-DEVKit
#define MISO  12          //LaskaKit ESP32-S3-DEVKit
#define MAX31865_CS 27
#define MAX31856_CS 22

#define DELAYTIME 1   //seconds
#define DEGREE_SIGN 247

#define PIN_ON 2

GxEPD2_BW<GxEPD2_290_GDEY029T94, GxEPD2_290_GDEY029T94::HEIGHT> display(GxEPD2_290_GDEY029T94(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));     // ESPink-Shelf-213 GDEM0213B74 -> 2.13" 122x250, SSD1680 

Adafruit_MAX31865 thermoPT = Adafruit_MAX31865(MAX31865_CS, MOSI, MISO, SCK);   // Nastaví senzor PT1000
Adafruit_MAX31856 thermoK = Adafruit_MAX31856(MAX31856_CS, MOSI, MISO, SCK);  // Nastaví senzor K

// Set web server port number to 80
WebServer server(80);

/* -----------------WiFi network ---------------- */
char ssid[] = "WiFi_SSID"; // Replace with your SSID
char pass[] = "WiFi_password"; // Replace with your password

void thermoK_init(void)
{
  if (!thermoK.begin()) {
    Serial.println("Could not initialize thermocouple.");
    while (1) delay(10);
  }
  thermoK.setThermocoupleType(MAX31856_TCTYPE_K);
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
  float temp;
  temp = thermoK.readThermocoupleTemperature();

	// Read temperature
	Serial.print("Inside temperature = ");
	Serial.println(temp);
	Serial.println();
	return temp;
}

int8_t thermoK_errorCheck(void)
{
  uint8_t fault = thermoK.readFault(); 
  if (fault)
  {
    if (fault & MAX31856_FAULT_CJRANGE) Serial.println("Cold Junction Range Fault");
    if (fault & MAX31856_FAULT_TCRANGE) Serial.println("Thermocouple Range Fault");
    if (fault & MAX31856_FAULT_CJHIGH)  Serial.println("Cold Junction High Fault");
    if (fault & MAX31856_FAULT_CJLOW)   Serial.println("Cold Junction Low Fault");
    if (fault & MAX31856_FAULT_TCHIGH)  Serial.println("Thermocouple High Fault");
    if (fault & MAX31856_FAULT_TCLOW)   Serial.println("Thermocouple Low Fault");
    if (fault & MAX31856_FAULT_OVUV)    Serial.println("Over/Under Voltage Fault");
    if (fault & MAX31856_FAULT_OPEN)    Serial.println("Thermocouple Open Fault");
    return -1;
  }
  return 0;
}

void display_init(void)
{
  // turn on power to display
  char text[10];
  Serial.println("Display power ON");
  delay(500); 
  display.init(); // inicializace

  display.setRotation(1);
  display.setPartialWindow(0, 0, display.width(), display.height()); // Set display window for fast update

  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
}

void display_print(void)
{
	uint16_t w, h, h_offset;
	int16_t x1, y1;
	char buffer[10];

  display.fillScreen(GxEPD_WHITE);

	display.setTextSize(4);
	display.getTextBounds("Meat", 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((display.width() / 4) - (w / 2)), 0);
	display.print("Meat");

	display.getTextBounds("Inside", 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((display.width() * 0.75) - (w / 2)), 0);
	display.print("Inside");

	display.drawLine((display.width() / 2), 0, (display.width() / 2), display.height(), GxEPD_BLACK);
	display.drawLine(0, h + 2, display.width(), h + 2, GxEPD_BLACK);

	if (tempPT_errorCheck() == -1)
	{
		sprintf(buffer, "E");
	}
	else
	{
		sprintf(buffer, "%.0f%c", tempPT_measure(), DEGREE_SIGN);
	}
	display.setTextSize(5);
	h_offset = h + 2;
	display.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((display.width() / 4) - (w / 2)), (h_offset + display.height() - h) / 2);
	display.print(buffer);

	if (thermoK_errorCheck() == -1)
	{
		sprintf(buffer, "E");
	}
	else
	{
		sprintf(buffer, "%.0f%c", thermoK_measure(), DEGREE_SIGN    );
	}
	display.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((display.width() * 0.75) - (w / 2)), (h_offset + display.height() - h) / 2);
	display.print(buffer);
 	display.display(true);
}

void display_wifi_connecting()
{
  uint16_t w, h, h_offset;
	int16_t x1, y1;

  display.fillScreen(GxEPD_WHITE);

	display.setTextSize(2);
	display.getTextBounds("Connecting to WiFi...", 0, 0, &x1, &y1, &w, &h);
	display.setCursor((display.width() - w) / 2, (display.height() - h) / 2);
	display.print("Connecting to WiFi...");
  display.display();
}

void handleRoot(void)
{
	server.send_P(200, "text/html", index_html); // Send web page
}

void handleNotFound(void)
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

void handleMeatTemp(void)
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

void handleInnerTemp(void)
{
	if (thermoK_errorCheck() == -1)
	{
		server.send(200, "text/plain", "ERROR");
	}
	else
	{
		char buff[11] = {0};
		sprintf(buff, "%0.2f ˚C", thermoK_measure());
		server.send(200, "text/plain", buff);
	}
}

void wifi_init(void)
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

void DNS_setup(void)
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

// Non blocking delay, instance can be used only once because of static variable
uint8_t delay_nonBlocking(uint16_t seconds)
{
  static uint32_t last_time = 0;
  if (millis() >= (last_time + seconds))
  {
    last_time = millis();
    return 1;
  }
  return 0;
}

void setup(void)
{
  // Turn on the second stabilisator
  pinMode(PIN_ON, OUTPUT);
  digitalWrite(PIN_ON, HIGH);
  Serial.begin(115200);

  display_init();
  display_wifi_connecting();

	wifi_init();
	DNS_setup();

	server.on("/", handleRoot);		   // Main page
	server.onNotFound(handleNotFound); // Function done when hangle is not found
	server.on("/readMeatTemp", handleMeatTemp);
	server.on("/readInnerTemp", handleInnerTemp);
	server.begin();
	Serial.println("HTTP server started");

	thermoPT.begin(MAX31865_2WIRE); // set to 2WIRE, 3WIRE or 4WIRE as necessary
	thermoK_init();

  display_print();
}

void loop(void)
{
	server.handleClient();
  if (delay_nonBlocking(DELAYTIME))
  {
    display_print();
  }
}