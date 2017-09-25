#include <FuzzyRule.h>
#include <FuzzyComposition.h>
#include <Fuzzy.h>
#include <FuzzyRuleConsequent.h>
#include <FuzzyOutput.h>
#include <FuzzyInput.h>
#include <FuzzyIO.h>
#include <FuzzySet.h>
#include <FuzzyRuleAntecedent.h>

#include <I2CSoilMoistureSensor.h>
#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_MS_PWMServoDriver.h"

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 16

#define USE_CLOUD 1

#if USE_CLOUD
    #include <PubSubClient.h>
    #include "WiFi.h"
    #include <ArduinoJson.h>
    
    const char* ssid     = "ESPectro1";
    const char* password = "11223344";

    const char* mqtt_server = "cloud.makestro.com";
    const char* topic = "Hisyam_Kamil/plantWatering/data";
    const char* sub = "Hisyam_Kamil/plantWatering/control";

    long lastMsg = 0;
    char msg[50];
    int value = 0;
    long lastReconnectAttempt = 0;

    WiFiClient client;
    PubSubClient mqtt_client(client);
#endif


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensor_temp(&oneWire);

I2CSoilMoistureSensor sensor_humid;
Adafruit_MotorShield AFMS = Adafruit_MotorShield(); 
Adafruit_DCMotor *myMotor2 = AFMS.getMotor(4);

int Humidity;
int indicator;
int Light; 
float temperature; 
float output;
unsigned long timeNow;
unsigned long timeLast;

Fuzzy* fuzzy = new Fuzzy();

// Creating fuzzification of Temperature  
FuzzySet* dingin = new FuzzySet(0, 0, 20, 30);
FuzzySet* normal = new FuzzySet(25, 30, 30, 35); 
FuzzySet* panas = new FuzzySet(30, 35, 40, 40); 

// creating fuzzification of Humidity

FuzzySet* kering = new FuzzySet(0, 0, 237, 380);
FuzzySet* lembab = new FuzzySet(237, 380, 380, 712);
FuzzySet* basah = new FuzzySet(570, 712, 950, 950);

// Fuzzy output watering duration 

FuzzySet* cepat = new FuzzySet(0, 3, 3, 5);
FuzzySet* lumayan = new FuzzySet(3, 5, 5, 7);
FuzzySet* lama = new FuzzySet(5, 7, 7, 10);


void setup (){
    sensor_temp.begin();
    Wire.begin(21,22);
    Serial.begin(9600);
    AFMS.begin(); 
    myMotor2->setSpeed(80);
    sensor_humid.begin();
    delay(1000);
    Serial.print("I2C Soil Moisture Sensor Address: ");
    Serial.println(sensor_humid.getAddress(),HEX);
    Serial.print("Sensor Firmware version: ");
    Serial.println(sensor_humid.getVersion(),HEX);
    Serial.println();
    
    
    FuzzyInput* suhu = new FuzzyInput(1);

    suhu->addFuzzySet(dingin);
    suhu->addFuzzySet(normal); 
    suhu->addFuzzySet(panas);

    fuzzy->addFuzzyInput(suhu); 


    FuzzyInput* kelembapan = new FuzzyInput(2);

    kelembapan->addFuzzySet(kering);
    kelembapan->addFuzzySet(lembab);
    kelembapan->addFuzzySet(basah);

    fuzzy->addFuzzyInput(kelembapan);

    FuzzyOutput* WaktuPengairan = new FuzzyOutput(1);

    WaktuPengairan->addFuzzySet(cepat); 
    WaktuPengairan->addFuzzySet(lumayan); 
    WaktuPengairan->addFuzzySet(lama);
  
    fuzzy->addFuzzyOutput(WaktuPengairan);


    //fuzzy rule 
    
    FuzzyRuleConsequent* thenCepat = new FuzzyRuleConsequent();
    thenCepat->addOutput(cepat);
    FuzzyRuleConsequent* thenLumayan = new FuzzyRuleConsequent(); 
    thenLumayan->addOutput(lumayan);  
    FuzzyRuleConsequent* thenLama = new FuzzyRuleConsequent(); 
    thenLama->addOutput(lama);

    FuzzyRuleAntecedent* ifDinginDanBasah = new FuzzyRuleAntecedent();
    ifDinginDanBasah->joinWithAND(dingin, basah); 
    
    FuzzyRule* fuzzyRule01 = new FuzzyRule(1, ifDinginDanBasah, thenCepat);
    fuzzy->addFuzzyRule(fuzzyRule01); 


    FuzzyRuleAntecedent* ifDinginLembab = new FuzzyRuleAntecedent(); 
    ifDinginLembab->joinWithAND(dingin, lembab);
      
    FuzzyRule* fuzzyRule02 = new FuzzyRule(2, ifDinginLembab, thenCepat);
    fuzzy->addFuzzyRule(fuzzyRule02);

    FuzzyRuleAntecedent* ifDinginKering = new FuzzyRuleAntecedent(); 
    ifDinginKering->joinWithAND(dingin, kering);

  
    FuzzyRule* fuzzyRule03 = new FuzzyRule(3, ifDinginKering, thenLumayan);
    fuzzy->addFuzzyRule(fuzzyRule03); 

    FuzzyRuleAntecedent* ifNormalBasah = new FuzzyRuleAntecedent(); 
    ifNormalBasah->joinWithAND(normal, basah);

    FuzzyRule* fuzzyRule04 = new FuzzyRule(4, ifNormalBasah, thenCepat);
    fuzzy->addFuzzyRule(fuzzyRule04); 

    FuzzyRuleAntecedent* ifNormalLembab = new FuzzyRuleAntecedent(); 
    ifNormalLembab->joinWithAND(normal, lembab); 

    FuzzyRule* fuzzyRule05 = new FuzzyRule(5, ifNormalLembab, thenLumayan); 
    fuzzy->addFuzzyRule(fuzzyRule05); 

    FuzzyRuleAntecedent* ifNormalKering = new FuzzyRuleAntecedent(); 
    ifNormalKering->joinWithAND(normal, kering); 
 
    FuzzyRule* fuzzyRule06 = new FuzzyRule(6, ifNormalKering, thenLama); 
    fuzzy->addFuzzyRule(fuzzyRule06); 

    FuzzyRuleAntecedent* ifPanasBasah = new FuzzyRuleAntecedent(); 
    ifPanasBasah->joinWithAND(panas, basah); 

    FuzzyRule* fuzzyRule07 = new FuzzyRule(7, ifPanasBasah, thenLumayan); 
    fuzzy->addFuzzyRule(fuzzyRule07); 

    FuzzyRuleAntecedent* ifPanasLembab = new FuzzyRuleAntecedent(); 
    ifPanasLembab->joinWithAND(panas, lembab); 
      
    FuzzyRule* fuzzyRule08 = new FuzzyRule(8, ifPanasLembab, thenLumayan); 
    fuzzy->addFuzzyRule(fuzzyRule08);
  
    FuzzyRuleAntecedent* ifPanasKering = new FuzzyRuleAntecedent(); 
    ifPanasKering->joinWithAND(panas, kering); 

    FuzzyRule* fuzzyRule09 = new FuzzyRule(9, ifPanasKering, thenLama); 
    fuzzy->addFuzzyRule(fuzzyRule09);

    #if USE_CLOUD
       WiFi.begin(ssid, password);

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }

        Serial.println("");
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());

         set_mqtt_server();
         

    #endif
}


void loop(){

    timeNow = millis();

    #if USE_CLOUD
        if (!mqtt_client.connected()) {
            reconnect();
        }
  
        mqtt_client.loop();
    #endif

    if (timeNow - timeLast >= 30000){
    
        Humidity = sensor_humid.getCapacitance();
        Light = sensor_humid.getLight(true);

        Serial.print("Requesting temperatures...");
        sensor_temp.requestTemperatures(); // Send the command to get temperatures
        Serial.println("DONE");
  
        Serial.print("Temperature for the device 1 (index 0) is: ");
        temperature = sensor_temp.getTempCByIndex(0);
        Serial.println(temperature);

        fuzzy->setInput(1, temperature);
        fuzzy->setInput(2, Humidity);
     
        fuzzy->fuzzify();
        output = fuzzy->defuzzify(1);
       
        myMotor2->run(FORWARD);
        Serial.print("motor nyala selama:");
        Serial.println(output);
        #if USE_CLOUD
            indicator = 1;
           publishKeyValue("Indicator",indicator);
        #endif

        delay(output*1000);
    
        myMotor2->run(RELEASE);
        Serial.println("motor mati");        
        #if USE_CLOUD
            indicator = 0;
            publishKeyValue("Indicator",indicator); 

            Humidity = sensor_humid.getCapacitance();
            Light = sensor_humid.getLight(true);
            Serial.print("Humidity = ");
            Serial.println(Humidity);
            Serial.print("Light = ");
            Serial.println(Light);

            Serial.print("Requesting temperatures...");
            sensor_temp.requestTemperatures(); // Send the command to get temperatures
            Serial.println("DONE");
  
            Serial.print("Temperature for the device 1 (index 0) is: ");
            temperature = sensor_temp.getTempCByIndex(0);  

            publishKeyValue("Humidity", Humidity);
            publishKeyValue("Temperature", temperature);
            publishKeyValue("Light",Light);
            Serial.print("Publish to Cloud");
        #endif
        timeLast = timeNow;
    
    }

    
    delay(100);
}


#if USE_CLOUD

void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect("Hisyam_Kamil-plantWatering-default","Hisyam_Kamil","CjIzfO689VdBxb0O5krsgLk7zdQWEDlwhz49eA0AaZ7b6rW2ZMAryQiguDIqeyE0")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("outTopic", "hello world");
      // ... and resubscribe
      //client.subscribe("inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void publishKeyValue(const char* key, char Valueval) {
    const int bufferSize = JSON_OBJECT_SIZE(2);
    StaticJsonBuffer<bufferSize> jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root[key] =  Valueval;

    String jsonString;
    root.printTo(jsonString);
    publishData(jsonString);
  }

void publishData(String payload) {
   publish(topic, payload);
}

void publish(String topic, String payload) {
mqtt_client.publish(topic.c_str(), payload.c_str());
}


//****************************************
// Set MQTT Server
//****************************************
void set_mqtt_server(){  
  // Set MQTT server
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);  
}

//****************************************
// Callback MQTT
//****************************************
void callback(char* topic, byte* payload, unsigned int length) {
  // Nothing here because we don't subscribe to any topics 
}

#endif