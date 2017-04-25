#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>
#include <ESP8266WiFi.h>
#include <DHT.h>
#include <ESP8266WebServer.h>

#define DHTPIN D4     // what pin we're connected to
#define DHTTYPE DHT22   // DHT 22  (AM2302)
#define Up D2
#define Stop D1
#define Down D3

float light = 0;
long dht_lastInterval  = 0;
long receiver_lastInterval  = 0;
boolean duskMode = false;
boolean receiver = false;
boolean receivertemp = false;
int counter = 0;
float CurrentTemperature = 0;
float CurrentHumidity = 0;
DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);
String CurrentHostName = "Woonkamer";
IPAddress ip(192, 168, 1, 109);
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);
String currentMode = "";

void setup(void) 
{
  //Serial.begin(9600);

  ConnectToWifi();

  server.on("/", handlePage);
  server.on("/down", handle_down);
  server.on("/stop", handle_stop);
  server.on("/up", handle_up);

  pinMode(Up, OUTPUT); // Pin 5 an output
  pinMode(Stop, OUTPUT); // Pin 5 an output
  pinMode(Down, OUTPUT); // Pin 5 an output
  
  digitalWrite(Up, HIGH); // Pin 5 LOW
  digitalWrite(Stop, HIGH); // Pin 5 LOW
  digitalWrite(Down, HIGH); // Pin 5 LOW
  
  /* Initialise the sensor */
  if(!tsl.begin())
  {
    /* There was a problem detecting the ADXL345 ... check your connections */
    //Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  
  /* Setup the sensor gain and integration time */
  configureSensor();

  dht.begin(); 
  
  readLight();
  
  readTemperatureAndHumidity();

  receiver = ReceiverPoweredOn();

  server.on("/", handlePage);

  server.begin();
}

/**************************************************************************/
/*
    Arduino loop function, called once 'setup' is complete (your own code
    should go here)
*/
/**************************************************************************/
void loop(void) 
{  
  //elke 10 seconde
  if (millis() - dht_lastInterval > 10000)
  {
    readLight();  
    updatePilightLightGenericLabel(light,"black",125);
    
    readTemperatureAndHumidity();
    UpdatePiligtTemperatureAndHumidity(CurrentTemperature, CurrentHumidity,127);
    
    if (light <=20 && !duskMode)
    {
      counter = counter + 1;

      if(counter > 2)
      {
        updatePilightGenericSwitch(true,130);
        duskMode = true;
        counter = 0;
      }
    }
    else if (light >=25 && duskMode)
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

void readLight()
{
  /* Get a new sensor event */ 
  sensors_event_t event;
  tsl.getEvent(&event);

   /* Display the results (light is measured in lux) */
  if (event.light)
  {
    light = event.light;
  }
  else
  {
    /* If event.light = 0 lux the sensor is probably saturated
    and no reliable data could be generated! */
    //Serial.println("Sensor overload");
    light = 0;
  }
}

void readTemperatureAndHumidity()
{
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  CurrentHumidity = dht.readHumidity();          // Read humidity (percent)
  CurrentTemperature = dht.readTemperature();     // Read temperature as Fahrenheit
  // Check if any reads failed and exit early (to try again).
  if (isnan(CurrentHumidity) || isnan(CurrentTemperature)) 
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
    client.print(light);
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
  url += light;
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

  if(receiver)
  {
    versterker = "Ingeschakelt";
  }
  else
  {
    versterker = "uitgeschakelt";
  }
  
  String webPage;
  String CurrentIpAdres = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  String header = "<html lang='en'><head><title>Somfy control paneel</title><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' href='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/css/bootstrap.min.css'><script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js'></script><script src='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/js/bootstrap.min.js'></script></head><body>";
  String navbar = "<nav class='navbar navbar-default'><div class='container-fluid'><div class='navbar-header'><a class='navbar-brand' href='/'>Somfy control panel</a></div><div><ul class='nav navbar-nav'><li><a href='/'><span class='glyphicon glyphicon-question-sign'></span> Status</a></li><li class='dropdown'><a class='dropdown-toggle' data-toggle='dropdown' href='#'><span class='glyphicon glyphicon-cog'></span> Tools<span class='caret'></span></a><ul class='dropdown-menu'><li><a href='/updatefwm'>Firmware</a></li><li><a href='/api?action=reset&value=true'>Restart</a></ul></li><li><a href='https://github.com/incmve/Itho-WIFI-remote' target='_blank'><span class='glyphicon glyphicon-question-sign'></span> Help</a></li></ul></div></div></nav>  ";
  String containerStart = "<div class='container'><div class='row'>";
  String Title = "<div class='col-md-4'><div class='page-header'><h1>Woonkamer control paneel</h1></div>";
  String Version = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globeglobe'></span> Version:<span class='pull-right'>1.0</span></div></div>";
  String IPAddClient = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globeglobe'></span> IP-adres:<span class='pull-right'>" + CurrentIpAdres + "</span></div></div>";
  String ClientName = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globetag'></span> Hostnaam:<span class='pull-right'>" + CurrentHostName + "</span></div></div>";
  String DHTTemp = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globefire'></span> Temperatuur:<span class='pull-right'>" + (String)CurrentTemperature + " °C</span></div></div>";
  String DHTHum =  "<div class='panel-body'><span class='glyphicon glyphicon-tint'></span> Luchtvogtigheid:<span class='pull-right'>" + (String)CurrentHumidity + " %</span></div></div>";
  String MeasuredLight = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-certificate'></span> Gemeten hoeveelheid licht:<span class='pull-right'>" + String(light) + " Lux</span></div></div>";
  String receiverMode = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globeok'></span> Marantz versterker is:<span class='pull-right'>" + versterker + "</span></div></div>";
  String somfyMode = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globeok'></span> Modus:<span class='pull-right'>" + currentMode + "</span></div></div>";
  String SomfyButtonCommands = "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-globe'></span> <div class='row'><div class='span6' style='text-align:center'><a href=\"up\"><button type='button' class='btn btn-default'>Up</button></a><a href=\"stop\"><button type='button' class='btn btn-default'>Stop</button></a><a href=\"down\"><button type='button' class='btn btn-default'>Down</button><br></div></span></div></div></div>";
  String containerEnd = "<div class='clearfix visible-lg'></div></div></div>";
  String siteEnd = "</body></html>";
  webPage = header + containerStart + Title + Version + IPAddClient + ClientName + MeasuredLight + DHTTemp + DHTHum + receiverMode + SomfyButtonCommands + somfyMode + containerEnd + siteEnd;
  server.send ( 200, "text/html", webPage);
}

void handle_up()
{
  currentMode = "Up";
  digitalWrite(Up, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Up, HIGH); // Pin 5 LOW
  Serial.println("Weg request UP");
  handlePage();
}

void handle_stop()
{
  currentMode = "Stop";
  digitalWrite(Stop, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Stop, HIGH); // Pin 5 LOW
  Serial.println("Weg request Stop");
  handlePage();
}

void handle_down()
{
  currentMode = "Down";
  digitalWrite(Down, LOW); // Pin 5 LOW
  delay(1000);
  digitalWrite(Down, HIGH); // Pin 5 LOW
  Serial.println("Weg request Down");
  handlePage();
}

void configureSensor(void)
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

/**************************************************************************/
/*
    Arduino setup function (automatically called at startup)
*/
/**************************************************************************/

void ConnectToWifi()
{
  char ssid[] = "";
  char pass[] = "";
  int i = 0;

  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);

  WiFi.hostname(CurrentHostName);
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
