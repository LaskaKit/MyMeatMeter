/* Smokehouse temp measurement for PT100/PT1000 sensor and inner temp measurement for K thermocoupler

 * List off parts: 
 *  1x LaskaKit ESPink-Shelf-2.9 ESP32 e-Paper                  https://www.laskakit.cz/laskakit-espink-shelf-2-9-esp32-e-paper/?variantId=14193
 *  1x LaskaKit MAX31865 převodník pro termočlánek PT100/1000   https://www.laskakit.cz/laskakit-max31865-prevodnik-pro-termoclanek-pt100-1000/
 *  1x LaskaKit MAX31856 Univerzální převodník pro termočlánek  https://www.laskakit.cz/univerzalni-modul-prevodniku-pro-termoclanky--max31856--spi/
 *  1x LaskaKit μŠup-SPI 4x na DIP adaptér                      https://www.laskakit.cz/laskakit-sup-spi-4x-na-dip-adapter/
 *  1x Potravinářský teplotní senzor PT1000, 1.25m              https://www.laskakit.cz/potravinarsky-teplotni-senzor-pt1000--1-25m/
 *  1x Termočlánek typu K 600C M6 1m                            https://www.laskakit.cz/termoclanek-typu-k-600c-m6-1m/
 *  3x μŠup-SPI JST-SH 6-pin kabel - 10cm                       https://www.laskakit.cz/--sup-spi-jst-sh-6-pin-kabel-10cm/
 *  Optional for better wire management: 
 *  1x μŠup, STEMMA QT, Qwiic JST-SH 4-pin kabel - dupont samice https://www.laskakit.cz/--sup--stemma-qt--qwiic-jst-sh-4-pin-kabel-dupont-samice/
 * 
 * Email:podpora@laskakit.cz
 * Web:laskakit.cz
 * 
 * The Pit Barrel Cooker typically maintains an internal temperature between 275°F and 310°F (135 - 155°C)
 * Ribs cook to 90-95˚C
 */


#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Adafruit_MAX31865.h>  // PT100/1000
#include <Adafruit_MAX31856.h>
#include "page.h"

#include <GxEPD2_BW.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

#define PT1000 // select sensor type: PT100 or PT1000

#if defined(PT1000)
#define RREF 4300.0
#define RNOMINAL 1000.0
#elif defined(PT100)
#define RREF 430.0
#define RNOMINAL 100.0
#endif

const char *host = "mymeatmeter"; // Connect to http://mymeatmeter.local
#define AP_NAME "MyMeatMeter"    // WiFiManager config portal access point

#define SCK   14
#define MOSI  13
#define MISO  12
#define MAX31865_CS 22    //  21 for SCK I2C µŠup connector (You will can not use I2C if you do so)
#define MAX31856_CS 27
#define PIN_ON 2

#define DELAYTIME 1   //seconds
#define HEADER_HEIGHT 30  // px, header row on the e-ink display

/* ---------------- Temperature history (served as JSON on /history) ---------------- */
#define HISTORY_INTERVAL 60   // seconds between samples
#define HISTORY_LENGTH 1440   // 24 h of samples
float historyMeat[HISTORY_LENGTH];
float historyInner[HISTORY_LENGTH];
uint16_t historyHead = 0;   // next write position (ring buffer)
uint16_t historyCount = 0;  // number of valid samples



GxEPD2_BW<GxEPD2_290_GDEY029T94, GxEPD2_290_GDEY029T94::HEIGHT> display(GxEPD2_290_GDEY029T94(/*CS=5*/ SS, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4));     // ESPink-Shelf-213 GDEM0213B74 -> 2.13" 122x250, SSD1680 

Adafruit_MAX31865 thermoPT = Adafruit_MAX31865(MAX31865_CS, MOSI, MISO, SCK);   // Nastaví senzor PT1000
Adafruit_MAX31856 thermoK = Adafruit_MAX31856(MAX31856_CS, MOSI, MISO, SCK);  // Nastaví senzor K

// Set web server port number to 80
WebServer server(80);

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

// Print one temperature centered in a column, with a drawn degree mark
// (FreeSans fonts have no ° glyph)
void display_temperature(uint16_t col_center, float temp, bool error)
{
	uint16_t w, h;
	int16_t x1, y1;
	char buffer[8];
	const uint16_t degree_r = 4;
	const uint16_t degree_gap = 7;

	if (error)
	{
		strcpy(buffer, "ERR");
	}
	else
	{
		sprintf(buffer, "%.0f", temp);
	}

	display.setFont(&FreeSansBold24pt7b);
	display.getTextBounds(buffer, 0, 0, &x1, &y1, &w, &h);

	uint16_t degree_w = error ? 0 : (degree_gap + 2 * degree_r);
	uint16_t x = col_center - ((w + degree_w) / 2);
	uint16_t y = HEADER_HEIGHT + ((display.height() - HEADER_HEIGHT - h) / 2); // top edge of text

	display.setCursor(x - x1, y - y1);
	display.print(buffer);

	if (!error)
	{
		display.drawCircle(x + w + degree_gap + degree_r, y + degree_r, degree_r, GxEPD_BLACK);
		display.drawCircle(x + w + degree_gap + degree_r, y + degree_r, degree_r - 1, GxEPD_BLACK);
	}
}

void display_header(uint16_t col_center, const char *label)
{
	uint16_t w, h;
	int16_t x1, y1;

	display.setFont(&FreeSansBold12pt7b);
	display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
	display.setCursor(col_center - (w / 2) - x1, ((HEADER_HEIGHT - h) / 2) - y1);
	display.print(label);
}

void display_print(void)
{
	display.fillScreen(GxEPD_WHITE);

	display_header(display.width() / 4, "Meat");
	display_header((display.width() * 3) / 4, "Inside");

	display.drawLine(display.width() / 2, 0, display.width() / 2, display.height(), GxEPD_BLACK);
	display.drawLine(0, HEADER_HEIGHT, display.width(), HEADER_HEIGHT, GxEPD_BLACK);

	bool errorPT = (tempPT_errorCheck() == -1);
	display_temperature(display.width() / 4, errorPT ? 0 : tempPT_measure(), errorPT);

	bool errorK = (thermoK_errorCheck() == -1);
	display_temperature((display.width() * 3) / 4, errorK ? 0 : thermoK_measure(), errorK);

	display.display(true);
}

// Print one line of text horizontally centered, y_top = top edge of the line
void display_centered(const char *text, uint16_t y_top)
{
	uint16_t w, h;
	int16_t x1, y1;

	display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
	display.setCursor(((display.width() - w) / 2) - x1, y_top - y1);
	display.print(text);
}

void display_wifi_connecting()
{
	display.fillScreen(GxEPD_WHITE);
	display.setFont(&FreeSansBold12pt7b);
	display_centered("Connecting to WiFi...", (display.height() / 2) - 9);
	display.display();
}

// Shown when WiFiManager starts the config portal (no saved WiFi / connection failed)
void display_wifi_config(WiFiManager *wm)
{
	display.fillScreen(GxEPD_WHITE);

	display.setFont(&FreeSansBold12pt7b);
	display_centered("WiFi setup", 10);

	display.setFont(&FreeSansBold9pt7b);
	display_centered("Connect to WiFi \"" AP_NAME "\"", 58);
	display_centered("and open 192.168.4.1", 88);

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
	WiFiManager wm;

	wm.setAPCallback(display_wifi_config); // show instructions on the e-ink display
	wm.setConfigPortalTimeout(180);        // seconds, then retry saved credentials

	// Connects with saved credentials; on failure starts the config portal
	if (!wm.autoConnect(AP_NAME))
	{
		Serial.println("Config portal timed out, restarting...");
		ESP.restart();
	}

	Serial.println("Wi-Fi connected successfully");
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());
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

// Non blocking timer, caller keeps its own last_time so it can be used for
// several independent intervals (also survives millis() rollover)
uint8_t timer_elapsed(uint32_t *last_time, uint32_t interval_ms)
{
  if ((millis() - *last_time) >= interval_ms)
  {
    *last_time = millis();
    return 1;
  }
  return 0;
}

void history_sample(void)
{
	historyMeat[historyHead] = (tempPT_errorCheck() == -1) ? NAN : tempPT_measure();
	historyInner[historyHead] = (thermoK_errorCheck() == -1) ? NAN : thermoK_measure();
	historyHead = (historyHead + 1) % HISTORY_LENGTH;
	if (historyCount < HISTORY_LENGTH)
	{
		historyCount++;
	}
}

void history_appendSeries(String &json, const float *series)
{
	char buf[12];
	for (uint16_t i = 0; i < historyCount; i++)
	{
		uint16_t idx = (historyHead + HISTORY_LENGTH - historyCount + i) % HISTORY_LENGTH;
		if (isnan(series[idx]))
		{
			strcpy(buf, "null");
		}
		else
		{
			sprintf(buf, "%.1f", series[idx]);
		}
		if (i)
		{
			json += ',';
		}
		json += buf;
	}
}

void handleHistory(void)
{
	String json;
	json.reserve((historyCount * 14) + 64);
	json = "{\"interval\":";
	json += HISTORY_INTERVAL;
	json += ",\"meat\":[";
	history_appendSeries(json, historyMeat);
	json += "],\"inner\":[";
	history_appendSeries(json, historyInner);
	json += "]}";
	server.send(200, "application/json", json);
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
	server.on("/history", handleHistory);
	server.begin();
	Serial.println("HTTP server started");

	thermoPT.begin(MAX31865_2WIRE); // set to 2WIRE, 3WIRE or 4WIRE as necessary
	thermoK_init();

	history_sample(); // first sample right away so the chart is not empty
  display_print();
}

void loop(void)
{
	static uint32_t display_timer = 0;
	static uint32_t history_timer = 0;

	server.handleClient();

	if (timer_elapsed(&display_timer, DELAYTIME * 1000UL))
	{
		display_print();
	}
	if (timer_elapsed(&history_timer, HISTORY_INTERVAL * 1000UL))
	{
		history_sample();
	}
}