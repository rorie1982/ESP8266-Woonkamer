#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#define Stop D5
#define Up D6
#define Down D7
#define DHTPIN D4
#define DHTTYPE DHT22   // DHT 22  (AM2302)

float currentlightSensorValue  = 0;
float currentTemperature = 0;
float currentHumidity = 0;
long dht_lastInterval  = 0;
long receiver_lastInterval  = 0;
boolean duskMode = false;
boolean receiver = false;
boolean receivertemp = false;
int counter = 0;
String currentHostName = "ESP-Woonkamer-01";
String currentMode = "";
String currentVersion = "1.0.8";
String lightsLivingRoom = "Uitgeschakelt";
String standZonnescherm = "Omhoog";
const char* username = "Rorie";
const char* password = "j10HIaueQWKM";

DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer httpServer(80);
IPAddress ip(192, 168, 1, 109);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
ESP8266HTTPUpdateServer httpUpdater;

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

  httpServer.on("/", handlePage);
  httpServer.on("/xml",handle_xml);
  httpServer.on("/action",handle_buttonPressed);

  IntializeAndConfigureTSL2561();

  IntializeAM2315();
  
  readLightSensorValue();
  
  readTemperatureAndHumidity();

  receiver = ReceiverPoweredOn();

  //set dusw dawn switch pilight to off at startup ESP8266
  updatePilightGenericSwitch(false,130);
  //set receiver switch pilight to current power status of receiver
  updatePilightGenericSwitch(false, 1009);

  httpUpdater.setup(&httpServer, username, password);
  httpServer.begin();
}

void loop(void) 
{  
  //elke 10 seconde
  if (millis() - dht_lastInterval > 10000)
  {
    readLightSensorValue();  
    updatePilightLightGenericLabel(currentlightSensorValue,"black",125);
    
    readTemperatureAndHumidity();
    UpdatePiligtTemperatureAndHumidity(currentTemperature, currentHumidity,127);
    
    if (currentlightSensorValue <=20 && !duskMode)
    {
      counter = counter + 1;
  
      if(counter > 2)
      {
        updatePilightGenericSwitch(true,130);
        duskMode = true;
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
      counter = 0;
      }
    }
    else
    {
      counter = 0;
    }
    
    dht_lastInterval = millis();
  }

  //elke 5 minuten
  if (millis() - receiver_lastInterval > 300000)
  {                                      
    receivertemp = ReceiverPoweredOn();
    
    if (receivertemp != receiver)
    {
       receiver = receivertemp;
       updatePilightGenericSwitch(receivertemp, 1009);
    }
    
    receiver_lastInterval = millis();
  }
  
  httpServer.handleClient();
  
}

void IntializeAndConfigureTSL2561()
{
   /* Initialise the sensor */
  if(!tsl.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    //Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  
  /* Setup the sensor gain and integration time */
  configureLightSensor();
}

void IntializeAM2315()
{
  dht.begin();
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

void readTemperatureAndHumidity()
{
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  float h = dht.readHumidity();          // Read humidity (percent)
  float t = dht.readTemperature();     // Read temperature as Fahrenheit
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) 
  {
    return;
  }

  currentHumidity = h;          // Read humidity (percent)
  currentTemperature = t;     // Read temperature as Fahrenheit
}

void updatePilightGenericSwitch(boolean value, int id)
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
  const char* host = "192.168.1.185";
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

  if (xmlOutput.substring(237,239) == "ON")
  {
      returnValue = true;
  }

  return returnValue;
}

void handlePage()
{
  
  String webPage;
  String currentIpAdres = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);

  webPage += "<!DOCTYPE html>";
  webPage += "<html lang=\"en\">";
  webPage += "<head>";
  webPage += "<title>Woonkamer</title>";
  webPage += "<meta charset=\"utf-8\">";
  webPage += "<meta name=\"apple-mobile-web-app-capable\" content=\"yes\">";
  webPage += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  webPage += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  webPage += GenerateFavIcon();
  webPage += "<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.2.0/jquery.min.js\"></script>";
  webPage += "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js\"></script>";
  webPage += GenerateJavaScript();
  webPage += "</head>";
  webPage += "<body>";
  webPage += "<div class=\"container\">";
  webPage += "<h2>Woonkamer</h2>";
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Zonnescherm</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-menu-hamburger'></span>&nbsp;";
  webPage += "Somfy bediening:<span class='pull-right'>";
  webPage += "<button type='button' class='btn btn-default' id='btnUp' onclick='ButtonPressed(this.id)'><span class=\"glyphicon glyphicon-menu-up\"></span></button>&nbsp;";
  webPage += "<button type='button' class='btn btn-default' id='btnStop' onclick='ButtonPressed(this.id)'><span class=\"glyphicon glyphicon-stop\"></span></button>&nbsp;";
  webPage += "<button type='button' class='btn btn-default' id='btnDown'onclick='ButtonPressed(this.id)'><span class=\"glyphicon glyphicon-menu-down\"></span></button>&nbsp;";
  webPage += "</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;";
  webPage += "Stand: <span class='pull-right' id='WZonnescherm01'>"+ standZonnescherm +"</span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Klimaat</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-fire'></span>&nbsp;";
  webPage += "Temperatuur: <span class='pull-right' id='WTemp01'></span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-tint'></span>&nbsp;";
  webPage += "Luchtvogtigheid: <span class='pull-right' id='WHum01'></span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div id=\"WPanelLights\" class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Verlichting</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-lamp'></span>&nbsp;";
  webPage += "Woonkamer verlichting: <span class='pull-right' id='WLight01'></span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-certificate'></span>&nbsp;";
  webPage += "Gemeten lichtsterkte: <span class='pull-right' id='WLux01'></span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-cloud'></span>&nbsp;";
  webPage += "Schemersensor: <span class='pull-right' id='WDuskDawn01'></span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div id=\"WPanelReceiver\" class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Aparaten</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-off'></span>&nbsp;";
  webPage += "Marantz versterker:<span class='pull-right' id ='WReceiver01'></span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "<div class=\"panel panel-default\">";
  webPage += "<div class=\"panel-heading\">Algemeen</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;";
  webPage += "Firmware: <span class='pull-right'>"+ currentVersion +"</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;";
  webPage += "IP-adres: <span class='pull-right'>" + currentIpAdres + "</span>";
  webPage += "</div>";
  webPage += "<div class=\"panel-body\">";
  webPage += "<span class='glyphicon glyphicon-info-sign'></span>&nbsp;";
  webPage += "Host naam: <span class='pull-right'>" + currentHostName + "</span>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "</div>";
  webPage += "</body>";
  webPage += "</html>";

  httpServer.send ( 200, "text/html", webPage);
 
}

String GenerateFavIcon()
{
  String returnValue;
  returnValue += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"57x57\" href=\"http://192.168.1.108/apple-icon-57x57.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"60x60\" href=\"http://192.168.1.108/apple-icon-60x60.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"72x72\" href=\"http://192.168.1.108/apple-icon-72x72.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"76x76\" href=\"http://192.168.1.108/apple-icon-76x76.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"114x114\" href=\"http://192.168.1.108/apple-icon-114x114.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"120x120\" href=\"http://192.168.1.108/apple-icon-120x120.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"144x144\" href=\"http://192.168.1.108/apple-icon-144x144.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"152x152\" href=\"http://192.168.1.108/apple-icon-152x152.png\">";
  returnValue += "<link rel=\"apple-touch-icon\" sizes=\"180x180\" href=\"http://192.168.1.108/apple-icon-180x180.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"192x192\"  href=\"http://192.168.1.108/android-icon-192x192.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"32x32\" href=\"http://192.168.1.108/favicon-32x32.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"96x96\" href=\"http://192.168.1.108/favicon-96x96.png\">";
  returnValue += "<link rel=\"icon\" type=\"image/png\" sizes=\"16x16\" href=\"http://192.168.1.108/favicon-16x16.png\">";
  returnValue += "<link rel=\"manifest\" href=\"http://192.168.1.108/manifest.json\">";
  returnValue += "<meta name=\"msapplication-TileColor\" content=\"#ffffff\">";
  returnValue += "<meta name=\"msapplication-TileImage\" content=\"http://192.168.1.108/ms-icon-144x144.png\">";
  returnValue += "<meta name=\"theme-color\" content=\"#ffffff\">";
  return returnValue;
}

void handle_xml()
{
  String returnValue;
  String versterker;
  String verlichting;
  String schemerSensor;

   if(receiver)
  {
    versterker = "Ingeschakelt";
  }
  else
  {
    versterker = "Uitgeschakelt";
  }

  if(duskMode)
  {
    schemerSensor = "Ingeschakelt";
  }
  else
  {
    schemerSensor = "Uitgeschakelt";
  }
  
  returnValue += "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
  returnValue += "<woonkamer>";
  returnValue += "<sensors>";
  returnValue += "<sensor>";
  returnValue += "<id>WTemp01</id>";
  returnValue += "<value>" + String(currentTemperature) + " °C</value>";
  returnValue += "</sensor>";
  returnValue += "<sensor>";
  returnValue += "<id>WHum01</id>";
  returnValue += "<value>" + String(currentHumidity) + " %</value>";
  returnValue += "</sensor>";
  returnValue += "<sensor>";
  returnValue += "<id>WLux01</id>";
  returnValue += "<value>" + String(currentlightSensorValue) + " Lux</value>";
  returnValue += "</sensor>";
  returnValue += "<sensor>";
  returnValue += "<id>WDuskDawn01</id>";
  returnValue += "<value>" + schemerSensor + "</value>";
  returnValue += "</sensor>";
  returnValue += "</sensors>";
  returnValue += "<devices>";
  returnValue += "<device>";
  returnValue += "<id>WReceiver01</id>";
  returnValue += "<value>" + versterker + "</value>";
  returnValue += "</device>";

   returnValue += "<device>";
  returnValue += "<id>WZonnescherm01</id>";
  returnValue += "<value>" + standZonnescherm + "</value>";
  returnValue += "</device>";
  
  returnValue += "</devices>";
  returnValue += "<lights>";
  returnValue += "<light>";
  returnValue += "<id>WLight01</id>";
  returnValue += "<value>" + lightsLivingRoom + "</value>";
  returnValue += "</light>";
  returnValue += "</lights>";
  returnValue += "</woonkamer>";
  httpServer.send ( 200, "text/html", returnValue);
}

void handle_buttonPressed()
{
  if (httpServer.args() > 0)
  {
    if (httpServer.arg(0) == "btnUp")
    {
      standZonnescherm = "Omhoog";
      handle_up();
    }
    else if (httpServer.arg(0) == "btnStop")
    {
      standZonnescherm = "Stop";
      handle_stop();
    }
    else if (httpServer.arg(0) == "btnDown")
    {
      standZonnescherm = "Omlaag";
      handle_down();
    }
    else if (httpServer.arg(0) == "pilightWLights")
    {
      lightsLivingRoom = httpServer.arg(1);
    }
  }
}

String GenerateJavaScript()
{
  String returnValue;
  returnValue += "<script type=\"text/javascript\">";
  returnValue += "function LoadData(){";
  returnValue += "$.ajax({";
  returnValue += "type: \"GET\",";
  returnValue += "url: \"http://192.168.1.109/xml\",";
  returnValue += "cache: false,";
  returnValue += "dataType: \"xml\",";
  returnValue += "success: DisplayData";
  returnValue += "});";
  returnValue += "}";
  
  returnValue += "function ButtonPressed(id){";
  returnValue += "$.post('http://192.168.1.109/action?id=' + id);";
  returnValue += "}";
  
  returnValue += "function DisplayData(result){";
  returnValue += "$(result).find('sensor').each(function(){";
  returnValue += "var id = $(this).find(\"id\").text();";
  returnValue += "var value = $(this).find(\"value\").text();";
  returnValue += "$('#' + id).text(value);";
  returnValue += "});";
  returnValue += "$(result).find('device').each(function(){";
  returnValue += "var id = $(this).find(\"id\").text();";
  returnValue += "var value = $(this).find(\"value\").text();";
  returnValue += "$('#' + id).text(value);";
  returnValue += "});";
  returnValue += "$(result).find('light').each(function(){";
  returnValue += "var id = $(this).find(\"id\").text();";
  returnValue += "var value = $(this).find(\"value\").text();";
  returnValue += "$('#' + id).text(value);";
  returnValue += "}); ";
  returnValue += "UpdatePanels();";
  returnValue += "}";

  returnValue += "function UpdatePanels(){";
  returnValue += "var statusDuskDawn = $('#WLight01').html();";
  returnValue += "var statusReceiver = $('#WReceiver01').html();";
  returnValue += "if (statusDuskDawn == \"Ingeschakelt\")";
  returnValue += "{";
  returnValue += "$('#WPanelLights').removeClass('panel panel-default').addClass('panel panel-success');";
  returnValue += "}";
  returnValue += "else";
  returnValue += "{";
  returnValue += "$('#WPanelLights').removeClass('panel panel-success').addClass('panel panel-default');";
  returnValue += "}";
  returnValue += "if (statusReceiver == \"Ingeschakelt\")";
  returnValue += "{";
  returnValue += "$('#WPanelReceiver').removeClass('panel panel-default').addClass('panel panel-success');";
  returnValue += "}";
  returnValue += "else";
  returnValue += "{";
  returnValue += "$('#WPanelReceiver').removeClass('panel panel-success').addClass('panel panel-default');";
  returnValue += "}";
  returnValue += "}";
  
  returnValue += "$(document).ready(function (){";
  returnValue += "LoadData();";
  returnValue += "setInterval(LoadData,3000);";
  returnValue += "})";
  returnValue += "</script>";
  
  return returnValue;
}

void handle_up()
{
  currentMode = "Up";
  digitalWrite(Up, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Up, HIGH); // Pin 5 LOW
  handlePage();
}

void handle_stop()
{
  currentMode = "Stop";
  digitalWrite(Stop, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Stop, HIGH); // Pin 5 LOW
  handlePage();
}

void handle_down()
{
  currentMode = "Down";
  digitalWrite(Down, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Down, HIGH); // Pin 5 LOW
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
  char ssid[] = "Rorie";
  char pass[] = "BIAW7W9H7Y8D";
  int i = 0;

  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);

  WiFi.hostname(currentHostName);
  WiFi.begin(ssid, pass);
  WiFi.mode(WIFI_STA);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

}
