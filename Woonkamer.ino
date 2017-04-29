#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266WebServer.h>

#define Stop D1
#define Up D2
#define Down D3
#define DHTPIN D4
#define DHTTYPE DHT22   // DHT 22  (AM2302)

float currentlightSensorValue  = 0;
float currentTemperatureSensorValue = 0;
float currentHumiditySensorValue = 0;
long dht_lastInterval  = 0;
long receiver_lastInterval  = 0;
boolean duskMode = false;
boolean receiver = false;
boolean receivertemp = false;
int counter = 0;
String currentHostName = "ESP-Woonkamer-01";
String currentMode = "";
String currentVersion = "1.0.0";
String lightsLivingRoom = "Uitgeschakelt";

DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);
IPAddress ip(192, 168, 1, 109);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

void setup(void) 
{
  //Serial.begin(9600);

  pinMode(Up, OUTPUT);
  pinMode(Stop, OUTPUT);
  pinMode(Down, OUTPUT);
  
  digitalWrite(Up, HIGH);
  digitalWrite(Stop, HIGH);
  digitalWrite(Down, HIGH);
  
  ConnectToWifi();

  server.on("/", handlePage);
  server.on("/down", handle_down);
  server.on("/stop", handle_stop);
  server.on("/up", handle_up);

  /* Initialise the sensor */
  if(!tsl.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    //Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  
  /* Setup the sensor gain and integration time */
  configureLightSensor();

  dht.begin(); 
  
  readLightSensorValue();
  
  readTemperatureAndHumiditySensorValue();

  receiver = ReceiverPoweredOn();

  server.begin();
}

void loop(void) 
{  
  //elke 10 seconde
  if (millis() - dht_lastInterval > 10000)
  {
    readLightSensorValue();  
    updatePilightLightGenericLabel(currentlightSensorValue,"black",125);
    
    readTemperatureAndHumiditySensorValue();
    UpdatePiligtTemperatureAndHumidity(currentTemperatureSensorValue, currentHumiditySensorValue,127);
    
    if (currentlightSensorValue <=20 && !duskMode)
    {
      counter = counter + 1;
  
      if(counter > 2)
      {
        updatePilightGenericSwitch(true,130);
        duskMode = true;
        lightsLivingRoom = "ingeschakelt";
        counter = 0;
      }
    }
    else if (currentlightSensorValue >=25 && duskMode)
    {
      counter = counter + 1;
      if(counter > 2)
      {
      updatePilightGenericSwitch(false,130);
      duskMode = false;
      lightsLivingRoom = "uitgeschakelt";
      counter = 0;
      }
    }
    else
    {
      counter = 0;
    }
    
    dht_lastInterval = millis();
  }

  //elke 10 minuten
  if (millis() - receiver_lastInterval > 600000)
  {                                      
    receivertemp = ReceiverPoweredOn();
    
    if (receivertemp != receiver)
    {
       receiver = receivertemp;
       updatePilightGenericSwitch(receivertemp, 1009);
    }
    
    receiver_lastInterval = millis();
  }
  
  server.handleClient();
  
}

void readLightSensorValue()
{
  /* Get a new sensor event */ 
  sensors_event_t event;
  tsl.getEvent(&event);

   /* Display the results (light is measured in lux) */
  if (event.light)
  {
    currentlightSensorValue = event.light;
  }
  else
  {
    /* If event.light = 0 lux the sensor is probably saturated
    and no reliable data could be generated! */
    //Serial.println("Sensor overload");
    currentlightSensorValue = 0;
  }
}

void readTemperatureAndHumiditySensorValue()
{
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  currentHumiditySensorValue = dht.readHumidity();          // Read humidity (percent)
  currentTemperatureSensorValue = dht.readTemperature();     // Read temperature as Fahrenheit
  // Check if any reads failed and exit early (to try again).
  if (isnan(currentHumiditySensorValue) || isnan(currentTemperatureSensorValue)) 
  {
    return;
  }
}

void updateDomoticz()
{
  int idx = 2;
  const char* host = "192.168.1.108";
  const int httpPort = 8080;
  WiFiClient client;
  
  if (client.connect(host,httpPort))
  {  
    client.print("GET /json.htm?type=command&param=udevice&idx=");
    client.print(idx);
    client.print("&nvalue=0&svalue=");
    client.print(currentlightSensorValue);
    client.println(" HTTP/1.1");
    client.print("Host: ");
    client.print(host);
    client.print(":");
    client.println(httpPort);
    client.println("User-Agent: Arduino-ethernet");
    client.println("Connection: close");
    client.println();
    client.stop(); 
   }
}

void updatePilightGenericSwitch(bool value, int id)
{
  String url = "";

  if(value)
  {
    url += "/send?protocol=generic_switch&on=1&id=";
    url += id;
  }
  else
  {
    url += "/send?protocol=generic_switch&off=1&id=";
    url += id;
  }
  
  UpdatePilight(url);
}

void updatePilightLightGenericLabel(float value, String color,int id)
{
  String url = "/send?protocol=generic_label&label=";
  url += value;
  url += "&color=";
  url += color;
  url += "&id=";
  url += id;
  
  UpdatePilight(url);
}

void UpdatePiligtTemperatureAndHumidity(float temperature,float humidity, int id)
  {
    String url = "/send?protocol=generic_weather&temperature=";
    url += temperature;
    url += "&humidity=";
    url += humidity;
    url += "&id=";
    url += id;
    
    UpdatePilight(url);
  }

void UpdatePilight(String url)
  {    
    const char* host = "192.168.1.108";
    // Use WiFiClient class to create TCP connections
    WiFiClient client;
    const int httpPort = 5001;
    
    if (client.connect(host, httpPort)) {
      // This will send the request to the server
      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" + 
                 "Connection: close\r\n\r\n");
            
    }
  }

boolean ReceiverPoweredOn()
{
  const char* host = "192.168.1.113";
  WiFiClient client2;
  const int httpPort = 80;
  String url = "/goform/formMainZone_MainZoneXml.xml";
  String xmlOutput;
  boolean returnValue = false;
    
  if (client2.connect(host, httpPort)) 
  {
      client2.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" + 
                 "Connection: close\r\n\r\n");        
  }
  
  delay(1000);

  while(client2.available()){
      xmlOutput += client2.readStringUntil('\r');    
  }
  
  if (xmlOutput.substring(235,237) == "ON")
  {
      returnValue = true;
  }

  return returnValue;
}

void handlePage()
{
  String versterker;
  String versterkerPanelClass;
  String verlichting;
  String verlichtingPanelClass;
  String webPage;
  String currentIpAdres = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  
  if(receiver)
  {
    versterker = "Ingeschakelt";
    versterkerPanelClass = "panel-success";
  }
  else
  {
    versterker = "uitgeschakelt";
    versterkerPanelClass = "panel-default";
  }

  if(duskMode)
  {
    verlichting = "Ingeschakelt";
    verlichtingPanelClass = "panel-success";
  }
  else
  {
    verlichting = "uitgeschakelt";
    verlichtingPanelClass = "panel-default";
  }

  webPage += "<!DOCTYPE html>";
  webPage += "<html lang=\"en\">";
  webPage += "<head>";
  webPage += "<title>Woonkamer</title>";
  webPage += "<meta charset=\"utf-8\">";
  webPage += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  webPage += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  webPage += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.2.0/jquery.min.js\"></script>";
  webPage += "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\"></script>";
  webPage += "</head>";
  webPage += "<body>";
  webPage += "<div class=\"container\">";
  webPage += "<h2>Woonkamer</h2>";
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Zonnescherm</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-menu-hamburger'></span>";
  webPage += "Somfy bediening:<span class='pull-right'>";
  webPage += "<a href=\"up\"><button type='button' class='btn btn-default'><span class=\"glyphicon glyphicon-menu-up\"></span></button></a>";
  webPage += "<a href=\"stop\"><button type='button' class='btn btn-default'><span class=\"glyphicon glyphicon-stop\"></span></button></a>";
  webPage += "<a href=\"down\"><button type='button' class='btn btn-default'><span class=\"glyphicon glyphicon-menu-down\"></span></button></a>";
  webPage += "</span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Klimaat</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-fire'></span>";
  webPage += "Temperatuur: <span class='pull-right'>" + String(currentTemperatureSensorValue) + "°C</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-tint'></span>";
  webPage += "Luchtvogtigheid: <span class='pull-right'>" + String(currentHumiditySensorValue) + "%</span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div class=\"panel " + verlichtingPanelClass + "\">";
  webPage += "<div class=\"panel-heading\">Verlichting</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-lamp'></span>";
  webPage += "Woonkamer verlichting: <span class='pull-right'>" + lightsLivingRoom + "</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-certificate'></span>";
  webPage += "Gemeten lichtsterkte: <span class='pull-right'>" + String(currentlightSensorValue) + "Lux</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-cloud'></span>";
  webPage += "Schemersensor: <span class='pull-right'>" + verlichting + "</span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div class=\"panel " + versterkerPanelClass + "\">";
  webPage += "<div class=\"panel-heading\">Aparaten</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-off'></span>";
  webPage += "Marantz versterker<span class='pull-right'>" + versterker + "</span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Algemeen</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>";
  webPage += "Versie: <span class='pull-right'>"+ currentVersion +"</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>";
  webPage += "IP-adres: <span class='pull-right'>" + currentIpAdres + "</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>";
  webPage += "Host naam: <span class='pull-right'>" + currentHostName + "</span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "</body>";
  webPage += "</html>";

  server.send ( 200, "text/html", webPage);
 
}

void handle_up()
{
  currentMode = "Up";
  digitalWrite(Up, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Up, HIGH); // Pin 5 LOW
  //Serial.println("Weg request UP");
  handlePage();
}

void handle_stop()
{
  currentMode = "Stop";
  digitalWrite(Stop, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Stop, HIGH); // Pin 5 LOW
  //Serial.println("Weg request Stop");
  handlePage();
}

void handle_down()
{
  currentMode = "Down";
  digitalWrite(Down, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Down, HIGH); // Pin 5 LOW
  //Serial.println("Weg request Down");
  handlePage();
}

void configureLightSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
   tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  //Serial.println("------------------------------------");
  //Serial.print  ("Gain:         "); Serial.println("Auto");
  //Serial.print  ("Timing:       "); Serial.println("13 ms");
  //Serial.println("------------------------------------");
}

void ConnectToWifi()
{
  char ssid[] = "";
  char pass[] = "";
  int i = 0;

  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);

  WiFi.hostname(currentHostName);
  WiFi.begin(ssid, pass);

  if (WiFi.status() != WL_CONNECTED && i >= 30)
  {
    WiFi.disconnect();
    delay(1000);
  }
  else
  {
    delay(5000);
    ip = WiFi.localIP();
  }

}
