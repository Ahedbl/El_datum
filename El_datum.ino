#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include "Free_Fonts.h"
#include "time.h"

TFT_eSPI tft = TFT_eSPI();
#define GRAPH_X 30      // Startpunkt för grafen (vänster)
#define GRAPH_Y 20      // Startpunkt för grafen (uppe)
#define GRAPH_W 440     // Grafens bredd
#define GRAPH_H 200     // Grafens höjd
#define TEXT_X 20
#define TEXT_Y 250

const char* ssid = "Hedblom";
const char* password = "Deco1962";

const char* weather = "https://api.open-meteo.com/v1/forecast?latitude=59.19&longitude=13.41&daily=temperature_2m_min,sunrise,sunset,rain_sum,wind_speed_10m_max,wind_gusts_10m_max,temperature_2m_max&hourly=temperature_2m&timezone=Europe%2FBerlin&forecast_days=1";
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 0;

void setup() {
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  Serial.begin(115200);
  delay(1000);

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void loop() {
  renderWeather();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  int minute = timeinfo.tm_min;
  int hour = (timeinfo.tm_hour + 1)%24;
  int month = timeinfo.tm_mon + 1;
  int year = timeinfo.tm_year + 1900;

  String stringDay = timePadding(timeinfo.tm_mday);
  String stringMonth = timePadding(timeinfo.tm_mon + 1);

  String call = "https://www.elprisetjustnu.se/api/v1/prices/"+ String(year) +"/"+ stringMonth +"-"+ stringDay +"_SE3.json";

  tft.setFreeFont(FSB18);
  tft.drawString(String(year) +"/"+ stringMonth +"-"+ stringDay , GRAPH_X, GRAPH_Y, GFXFF);

  Serial.println("\nConnected\n");

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.begin(
    client,
    call
  );

  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
    https.end();
    return;
  }

  String payload = https.getString();
  https.end();


  Serial.println("Payload length: " + String(payload.length()));

  StaticJsonDocument<8192> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  float sum = 0;
  float average = 0;
  int counter = 0;
  float prices[100];
  for (JsonObject obj : doc.as<JsonArray>()) {
    const char* time_start = obj["time_start"];
    float price = obj["SEK_per_kWh"];
    prices[counter] = price;
    counter = counter + 1;
    sum = sum + price;
  }
  average = sum/counter;
  render(prices, counter, average, year, stringMonth, stringDay);
  currentLine(hour, minute);
  delay(15*60000);
}

String timePadding(int time){
  if(time < 10) return "0"+String(time);
  return String(time);
}

void currentLine(int hour, int minute){
  int minutesNow = hour * 60 + minute + 60; ////////////////////////////////// Adjusted for summer time with + 60
  float lineX = GRAPH_X + (GRAPH_W * minutesNow) / 1440.0;

  Serial.println("hour:minute" + String(hour) + ":" + String(minute) +" line = " + String(lineX));
  tft.drawLine(lineX, GRAPH_H + GRAPH_Y, lineX, GRAPH_Y, TFT_GREEN);
}
void render(const float price[], int count, float average, int year, String month, String day) {
  tft.fillScreen(TFT_BLACK);
  tft.drawString(String(year) +"/"+ month +"-"+ day , GRAPH_X, GRAPH_Y, GFXFF);
  float maxPrice = 3.0; 
  float high = 0;
  float low = 10;
  int lastX = GRAPH_X;
  int lastY = GRAPH_Y + GRAPH_H - (price[0] / maxPrice) * GRAPH_H;
  int avgY = GRAPH_Y + GRAPH_H - (average / maxPrice) * GRAPH_H;

  for(int i = 1; i < count; i++) {
    if(price[i] > high) high = price[i];
    if(price[i] < low) low = price[i];
    int x = GRAPH_X + i * GRAPH_W / count;
    int y = GRAPH_Y + GRAPH_H - (price[i] / maxPrice) * GRAPH_H;
    tft.drawLine(lastX, lastY, x, y, TFT_CYAN);
    tft.drawLine(lastX, avgY, x, avgY, TFT_RED);
    lastX = x;
    lastY = y;
  }
  //X-axel
  tft.setFreeFont(FF6);
  for(int i = 0; i < 12; i++){     
    if(i*2<10) tft.drawString(String(i*2), GRAPH_X + i*(GRAPH_W/12)-6, GRAPH_Y + GRAPH_H, GFXFF);
    else tft.drawString(String(i*2), GRAPH_X + i*(GRAPH_W/12)-13, GRAPH_Y + GRAPH_H, GFXFF);
    tft.drawLine(GRAPH_X + i*(GRAPH_W/12), GRAPH_Y + GRAPH_H, GRAPH_X + i*(GRAPH_W/12), GRAPH_Y+ GRAPH_H-10, TFT_WHITE);
  }
  //TEXT-------------------
  tft.drawRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, TFT_WHITE);
  tft.setFreeFont(FF21);
  tft.drawString("Highest: " + String(high) + "kr", TEXT_X, TEXT_Y, GFXFF);
  tft.drawString("Average: " + String(average) + "kr", TEXT_X, TEXT_Y+20, GFXFF);
  tft.drawString("Lowest:  " + String(low) + "kr", TEXT_X, TEXT_Y+40, GFXFF);

  //Y-axel
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_H - (1.0 / maxPrice) * GRAPH_H, GRAPH_X + 10, GRAPH_Y + GRAPH_H - (1.0 / maxPrice) * GRAPH_H, TFT_WHITE);
  tft.drawString("1.0", 0, GRAPH_Y + GRAPH_H - (1.0 / maxPrice) * GRAPH_H - 8, GFXFF);
  
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_H - (1.5 / maxPrice) * GRAPH_H, GRAPH_X + 10, GRAPH_Y + GRAPH_H - (1.5 / maxPrice) * GRAPH_H, TFT_WHITE);
  tft.drawString("1.5", 0, GRAPH_Y + GRAPH_H - (1.5 / maxPrice) * GRAPH_H - 8, GFXFF);
  
  tft.drawLine(GRAPH_X, GRAPH_Y + GRAPH_H - (2.0 / maxPrice) * GRAPH_H, GRAPH_X + 10, GRAPH_Y + GRAPH_H - (2.0 / maxPrice) * GRAPH_H, TFT_WHITE);
  tft.drawString("2.0", 0, GRAPH_Y + GRAPH_H - (2.0 / maxPrice) * GRAPH_H - 8, GFXFF);
}

void renderWeather(){
  Serial.print("Sahar langt iaf");
  String call = "https://api.open-meteo.com/v1/forecast?latitude=59.19&longitude=13.41&daily=temperature_2m_min,sunrise,sunset,rain_sum,wind_speed_10m_max,wind_gusts_10m_max,temperature_2m_max&hourly=temperature_2m&timezone=Europe%2FBerlin&forecast_days=1";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.begin(
    client,
    call
  );

  int httpCode = https.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
    https.end();
    return;
  }

  String payload = https.getString();
  https.end();


  //Serial.println("Payload length: " + String(payload.length()));

  StaticJsonDocument<8192> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  int counter = 0;
  float temps[100];
  float maxTemp = doc["daily"]["temperature_2m_max"][0];
  float minTemp = doc["daily"]["temperature_2m_min"][0];
  JsonArray tempsArray = doc["hourly"]["temperature_2m"];

  for (float temp : tempsArray) {
    Serial.println(temp);
  }
  Serial.print("lowest");
  Serial.println(minTemp);
}

