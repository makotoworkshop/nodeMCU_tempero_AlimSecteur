/*
Un capteur DHT22 à l'intéréreur
Un capteur DHT22 à l'extéréreur
Envoient leurs valeurs sur un afficheur LCD
Envoient leurs valeurs sur un serveur influxDB + Grafana
*/

#include "ESP8266WiFi.h"
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include "DHT.h"
#include "ESPinfluxdb.h"    // lib ESP InfluxDB

//################
//# DÉCLARATIONS #
//################

//——— DHT Sensors ———//
#define DHTinPIN D5     // what digital pin the DHT22 is conected to
#define DHTexPIN D6     // what digital pin the DHT22 is conected to
#define DHTTYPE DHT22   // there are multiple kinds of DHT sensors
DHT dht_in(DHTinPIN, DHTTYPE);
DHT dht_ex(DHTexPIN, DHTTYPE);

//——— LCD 1602 ———//
LiquidCrystal_I2C lcd(0x27, 16, 2);

//——— Variables ———//
float temperature_in;
float temperature_ex;
float humidite_in;
float humidite_ex;
unsigned long previousMillis = 0;   // Stores last time temperature was published
unsigned long previousMillisDB = 0;   // Stores last time temperature was published
const long interval = 2000;        // Interval at which to publish sensor readings
const long intervalDB = 6000;

//——— InfluxDB ———//
const char *INFLUXDB_HOST = "machin.org";
const uint16_t INFLUXDB_PORT = 8086;
const char *DATABASE = "BDDmeteo";
const char *DB_USER = "meteo";
const char *DB_PASSWORD = "xxxx";
Influxdb influxdb(INFLUXDB_HOST, INFLUXDB_PORT);  //https://github.com/projetsdiy/grafana-dashboard-influxdb-exp8266-ina219-solar-panel-monitoring/tree/master/solar_panel_monitoring


//——— WiFi ———//
char ssid[] = "xxxx";
char password[] = "xxxx";
void connectToWifi() {
  Serial.println("Connecting to WiFi...");
  lcd.setCursor(0,0);
  lcd.print("Connecting WiFi");
  WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.print(".");
     lcd.setCursor(0,1);
     lcd.print("Please wait...");
  }
  Serial.println ("WiFi Connecté"); 
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("WiFi Connected !");
  Serial.println(WiFi.localIP());
    lcd.setCursor(2,1);
    lcd.print(WiFi.localIP());  
    delay(5000);
}

void setup() {
Serial.begin(115200);

//——— DHT Sensors ———//
dht_in.begin();
dht_ex.begin();

//——— LCD 1602 ———//
Wire.begin(D2, D1);
lcd.begin(0x27, 16, 2);
lcd.backlight();
lcd.home();
lcd.print("Hello, NodeMCU");

connectToWifi();

////——— WiFi ———//
//  WiFiMulti.addAP(ssid, password);  // Connection au réseau WiFi
//  while (WiFiMulti.run() != WL_CONNECTED) {
//    delay(100);
//  }
//  Serial.println ("WiFi Connecté"); 
  
//——— InfluxDB ———//
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Opening database");
  while (influxdb.opendb(DATABASE, DB_USER, DB_PASSWORD)!=DB_SUCCESS) { // Connexion à la base InfluxDB
    Serial.println("Open database failed");
    lcd.setCursor(0,1);
    lcd.print("database failed");
    delay(30000);
  }
}


void loop() {
  sensors();
  SendDataToInfluxdbServer();
  deep_sleep();
}


void deep_sleep(){
  Serial.print("Passage en mode Deep Sleep");
  //Passe en mode deepsleep 5 secondes
//  ESP.deepSleep( 5e6 );
  
  //Passe en mode deepsleep 8 secondes (multiplie les secondes par 1 000 000)
  ESP.deepSleep( 20 * 1000000 ); 
}




void sensors() {
  unsigned long currentMillis = millis();
    // Every X number of seconds (interval = 2 seconds) 
  if (currentMillis - previousMillis >= interval) {
    // Save the last time a new reading was published
  previousMillis = currentMillis;
  
    // New DHT sensor readings
  humidite_in = dht_in.readHumidity();
  humidite_ex = dht_ex.readHumidity();  
    // Read temperature as Celsius (the default)
  temperature_in = dht_in.readTemperature();
  temperature_ex = dht_ex.readTemperature();  

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidite_in) || isnan(temperature_in)) {
    Serial.println(F("Failed to read from DHT sensor IN !"));
    lcd.setCursor(0,0); // Sets the location at which subsequent text written to the LCD will be displayed
    lcd.print(" Read IN failed ");
 //   return;
  }
  else {
     // Printing the results on the serial monitor
    Serial.print("Temperature IN = ");
    Serial.print(temperature_in);
    Serial.print(" °C ");
    Serial.print("    Humidity IN = ");
    Serial.print(humidite_in);
    Serial.println(" % ");

  // LCD première ligne
    lcd.setCursor(0,0); // Sets the location at which subsequent text written to the LCD will be displayed
    lcd.print("in: "); // Prints string "Temp." on the LCD
    lcd.print(temperature_in,1); // Prints the temperature value from the sensor
    lcd.print((char)223);
    lcd.print("C | ");
    lcd.print(humidite_in,0);
    lcd.print("%");   
  }
  

  // Check if any reads failed and exit early (to try again).
  if (isnan(humidite_ex) || isnan(temperature_ex)) {
    Serial.println(F("Failed to read from DHT sensor EX !"));
    lcd.setCursor(0,1);
    lcd.print(" Read EX failed ");
 //   return;
  }
  else {
    Serial.print("Temperature EX = ");
    Serial.print(temperature_ex);
    Serial.print(" °C ");
    Serial.print("    Humidity EX = ");
    Serial.print(humidite_ex);
    Serial.println(" % ");
      
  // LCD seconde ligne
    lcd.setCursor(0,1);
    lcd.print("ex: "); // Prints string "Temp." on the LCD
    lcd.print(temperature_ex,1); // Prints the temperature value from the sensor
    lcd.print((char)223);
    lcd.print("C | ");
    lcd.print(humidite_ex,0);
    lcd.print("%");  
  }

  
//————————————————
//IN: 26,5°C | 45%
//EX: 35,3°C | 62%
//————————————————
  }
}


void SendDataToInfluxdbServer() { //Writing data with influxdb HTTP API: https://docs.influxdata.com/influxdb/v1.5/guides/writing_data/
                                  //Querying Data: https://docs.influxdata.com/influxdb/v1.5/query_language/
  // Comparaison du Checksum calculé au Checksum reçu

    unsigned long currentMillis = millis();
  // Every X number of seconds (interval = 6 seconds) 
  if (currentMillis - previousMillisDB >= intervalDB) {
    // Save the last time a new reading was published
    previousMillisDB = currentMillis;
     
    dbMeasurement rowDATA("Data");
    rowDATA.addField("Temperature_interieure", temperature_in);
    rowDATA.addField("Humidite_interieure", humidite_in);
    rowDATA.addField("Temperature_exterieure", temperature_ex);
    rowDATA.addField("Humidite_exterieure", humidite_ex);
    Serial.println(influxdb.write(rowDATA) == DB_SUCCESS ? " - rowDATA write success" : " - Writing failed");
    influxdb.write(rowDATA);
  
    // Vide les données - Empty field object.
    rowDATA.empty();
  }
}

