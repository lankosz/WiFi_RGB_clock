#include <Wire.h>
#include <SI7021.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_GFX.h>

const char* ssid = "MiKe";
const char* password = "DqP5aQleA!9";
#define NTP_INTERVAL      (2 * 60)     // in seconds
#define TEMPHUM_INTERVAL  (1 * 5)      // in seconds

// sensor
SI7021 si7021;
Ticker TempHumTicker;
int humi, temp;
#define SDA 0 // GPIO0 on ESP-01 module
#define SCL 2 // GPIO2 on ESP-01 module
bool TempHumTick;

// NTP
int GMToffset = 2;
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
Ticker NTPqueryTicker;
WiFiUDP Udp;
unsigned int localPort = 2390;      // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// HTTP server
ESP8266WebServer server(80);
const int led = 2;

// clock
Ticker ticker;
unsigned int hours, minutes, seconds;
unsigned int SynchTime;
bool dots;

// display
const int datapin = 5;
Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(32, 16, datapin,
  NEO_MATRIX_TOP     + NEO_MATRIX_LEFT +
  NEO_MATRIX_ROWS    + NEO_MATRIX_PROGRESSIVE,
  NEO_GRB            + NEO_KHZ800);
//Adafruit_NeoPixel strip = Adafruit_NeoPixel(512, datapin, NEO_GRB + NEO_KHZ800);
int ClockColor = matrix.Color(145, 255, 0);
int BarColor = matrix.Color(0, 50, 0);
int ClockBgColor = matrix.Color(0, 0, 0);

void HalfSecond()
{
  if(CheckNTPreply(&hours, &minutes, &seconds))
    SynchTime = 0;

  DrawTime(hours, minutes, seconds);
  DrawSynch(SynchTime);
  //DrawTemperatureHumidity(temp, humi);
  if((seconds / 5) & 1) DrawTemperature(temp);
  else DrawHumidity(humi);
  matrix.show();

  if(!dots)
  {
    SynchTime++;
    if(++seconds > 59)
    {
      seconds = 0;
      if(++minutes > 59)
      {
        minutes = 0;
        if(++hours > 23)
          hours = 0;
      }
    }
  }
}

void DrawTime(unsigned int h, unsigned int m, unsigned int s)
{
  matrix.fillScreen(0);
  int x = 1;
  if(h > 10) matrix.drawChar(x, 0, (h / 10)+'0', ClockColor, ClockBgColor, 1);
  x += 7;
  matrix.drawChar(x, 0, (h % 10)+'0', ClockColor, ClockBgColor, 1);
  dots = !dots;
  char c = dots ? ':' : ' ';
  x += 5;
  matrix.drawChar(x, 0, c, ClockColor, ClockBgColor, 1);
  x += 5;
  matrix.drawChar(x, 0, (m / 10)+'0', ClockColor, ClockBgColor, 1);
  x += 7;
  matrix.drawChar(x, 0, (m % 10)+'0', ClockColor, ClockBgColor, 1);

/*  x = 1;
  matrix.drawChar(x, 8, (s / 10)+'0', ClockColor, ClockBgColor, 1);
  x += 7;
  matrix.drawChar(x, 8, (s % 10)+'0', ClockColor, ClockBgColor, 1);*/
}

void DrawSynch(unsigned int SynchTime)
{
  if(SynchTime > NTP_INTERVAL) SynchTime = NTP_INTERVAL;
//  matrix.drawLine(0, 8, 32*SynchTime/NTP_INTERVAL, 8);
  for(int i = 0; i < 32-32*SynchTime/NTP_INTERVAL; i++)
    matrix.drawPixel(i, 7, BarColor);
}


void DrawTemperatureHumidity(int t, int h)
{
  t /= 100;
  //TemperatureColor, TemperatureBgColor
  int x = 1;
  if(t > 10) matrix.drawChar(x, 8, (t / 10)+'0', ClockColor, ClockBgColor, 1);
  x += 7;
  matrix.drawChar(x, 8, (t % 10)+'0', ClockColor, ClockBgColor, 1);
  x += 5;
  matrix.drawPixel(x, 8, ClockColor);
  x += 5;
  matrix.drawChar(x, 8, (h / 10)+'0', ClockColor, ClockBgColor, 1);
  x += 7;
  matrix.drawChar(x, 8, (h % 10)+'0', ClockColor, ClockBgColor, 1);
}

void DrawTemperature(int t)
{
  t /= 100;
  //TemperatureColor, TemperatureBgColor
/*  matrix.setCursor(1, 8);
  matrix.print(t);
  matrix.print(" C");
  matrix.drawCircle(x, y, radius, color);*/
  int x = 0;
  if(t < 0)
  {
    matrix.drawChar(x, 8, '-', ClockColor, ClockBgColor, 1);
    x += 6;
  }
  if(abs(t) > 9) matrix.drawChar(x, 8, (abs(t) / 10)+'0', ClockColor, ClockBgColor, 1);
  x += 7;
  matrix.drawChar(x, 8, (t % 10)+'0', ClockColor, ClockBgColor, 1);
  x += 7;
  matrix.drawCircle(x, 9, 1, ClockColor);
  x += 5;
  matrix.drawChar(x, 8, 'C', ClockColor, ClockBgColor, 1);

}
void DrawHumidity(int h)
{
  //TemperatureColor, TemperatureBgColor
  matrix.setCursor(0, 8);
  matrix.setTextColor(ClockColor);
  matrix.print(h);
  matrix.print(" %");
}

void TimeUpdate()
{
  Serial.println("NTP query");
  sendNTPpacket(timeServer); // send an NTP packet to a time server
}

// called by TempHumTicker
void ReadTempHum()
{
  TempHumTick = true;
/*  humi = si7021.getHumidityPercent();
  temp = si7021.getCelsiusHundredths();
  
  Serial.print("TEMP: ");
  Serial.print(temp);
  Serial.print(" HUMI: ");
  Serial.print(humi);
  Serial.println("");*/
}

/***********************
 *  SETUP
 ***********************/
void setup(void){
  pinMode(led, OUTPUT);
  digitalWrite(led, 0);
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.on("/command.html", GetCommand);
  server.on("/colors.html", HandleColors);
  server.on("/inline", [](){
    server.send(200, "text/plain", "this works as well");
  });

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  Udp.begin(localPort);
  ticker.attach(0.5, HalfSecond);
  NTPqueryTicker.attach(NTP_INTERVAL, TimeUpdate);

  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setBrightness(60);
  matrix.setTextColor(ClockColor);
  //matrix.setTextSize(1);
  //matrix.setFont(&FreeSans9pt7b);
  matrix.fillScreen(0);
  matrix.show();
  delay(500);
  TimeUpdate();

  si7021.begin(SDA,SCL); // Runs : Wire.begin() + reset()
  TempHumTicker.attach(TEMPHUM_INTERVAL, ReadTempHum);
  //ReadTempHum();
}

void loop(void){
  server.handleClient();
  if(TempHumTick)
  {
    TempHumTick = false;
    humi = si7021.getHumidityPercent();
    temp = si7021.getCelsiusHundredths();
    
    Serial.print("TEMP: ");
    Serial.print(temp);
    Serial.print(" HUMI: ");
    Serial.print(humi);
    Serial.println("");
  }
}



/********************
 *      NTP
 ********************/

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  //Serial.println("1");
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  //Serial.println("2");
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  //Serial.println("3");

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  //Serial.println("4");
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  //Serial.println("5");
  Udp.endPacket();
  //Serial.println("6");
}

bool CheckNTPreply(unsigned int *h, unsigned int *m, unsigned int *s)
{
  int PacketSize = Udp.parsePacket();
  if(PacketSize == NTP_PACKET_SIZE)
  {
    //Serial.println("packet received");
    // We've received a packet, read the data from it
    Udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    //Serial.print("Seconds since Jan 1 1900 = ");
    //Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    //Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    //Serial.println(epoch);


    // print the hour, minute and second:
    Serial.print("Local time is ");       // UTC is the time at Greenwich Meridian (GMT)
    // to local time
    epoch += GMToffset *60 * 60;
    *h = (epoch  % 86400L) / 3600;
    *m = (epoch  % 3600) / 60;
    *s = epoch % 60;
    Serial.print(*h); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if (*m < 10) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print(*m); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if (*s < 10) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(*s); // print the second
    return true;
  }
  return false;
}


/************************
 *    WEB SERVER
 ************************/
const char index_html[] = R"=====(
<!DOCTYPE HTML>
<html>
<head><title>ESP8266 Arduino Demo Page</title></head>
<body>ESP8266 power!<p><img src="logo.png"></body>
</html>
)=====";

void handleRoot() {
  digitalWrite(led, 1);
  //server.send(200, "text/plain", "hello from esp8266!");
  server.sendContent(index_html);
  digitalWrite(led, 0);
}

void GetCommand() {
  int r, g, b;
  int ind1, ind2;
  String state = server.arg("color");
  ind1 = state.indexOf(',');
  ind2 = state.indexOf(',', ind1+1);
  String ss;
  ss = state.substring(0, ind1);
  r = ss.toInt();
  ss = state.substring(ind1+1, ind2+1);
  g = ss.toInt();
  ss = state.substring(ind2+1);
  b = ss.toInt();
  ClockColor = matrix.Color(r, g, b);
  server.send(200, "text/plain", "OK");
}

void handleNotFound(){
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
}

const char colors_html[] = R"=====(
<html>
<head>
<meta http-equiv="Content-Type" content="text/html; charset=UTF-8">

<title>Colors wheel</title>
<meta name="viewport" content="width=device-width, height=device-height, user-scalable=yes" />

<script type="text/javascript">
window.onload=function(){
  var canvas = document.getElementById("picker");
  var context = canvas.getContext("2d");
  var x = canvas.width / 2;
  var y = canvas.height / 2;
  var radius = 150;
  var counterClockwise = false;
  for(var angle=0; angle<=360; angle+=1){
    var startAngle = (angle-2)*Math.PI/180;
    var endAngle = angle * Math.PI/180;
    context.beginPath();
    context.moveTo(x, y);
    context.arc(x, y, radius, startAngle, endAngle, counterClockwise);
    context.closePath();
    context.fillStyle = 'hsl('+angle+', 100%, 50%)';
    context.fill();
  }
}

function findPos(obj){
    var current_left = 0, current_top = 0;
    if (obj.offsetParent){
        do{
            current_left += obj.offsetLeft;
            current_top += obj.offsetTop;
        }while(obj = obj.offsetParent);
        return {x: current_left, y: current_top};
    }
    return undefined;
}

function PickColor(obj, e){
  position = findPos(obj);
  var w = obj.width / 2;
  var h = obj.height / 2;
  var x = e.pageX - position.x;
  var y = e.pageY - position.y;
  var el = document.getElementById('kolor');
  var angle = Math.atan2(150-y, 150-x) * 180 / Math.PI + 180;
  //el.innerHTML = 'x='+x+', y='+y+', hsl='+'hsl('+angle+', 100%, 50%)';
  el.style.color = 'hsl('+angle+', 100%, 50%)';
  //console.log(el.style.color);
  // rgb(0, 25, 255)
  var cl = 'color=' + el.style.color.replace(/[^0-9,]/g,'');
  command(cl);
}

function command(cmd, paramFunc)
{
  var req = new XMLHttpRequest();
  var postdata = cmd;
  req.open("POST", "command.html", true);

  //Send the proper header information along with the request
  req.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
  //req.setRequestHeader("Content-length", postdata.length);
  req.timeout = 3*60*1000;
  req.ontimeout = function () {
    console.log('req.ontimeout');
  }
  req.onerror = function (e) {
    console.log('req.onerror: '+e);
  }
  req.onreadystatechange = function() {//Call a function when the state changes.
     if(req.readyState == 4 && req.status == 200) {
      if(req.responseText != 'OK')
      {
        console.log('Error: ' + req.responseText);
      }
      else
        console.log(req.responseText);
      if (paramFunc && (typeof paramFunc == "function"))
      {
        paramFunc();   
      }
     }
  }
  try {
    req.send(postdata);
  }
  catch(e) {
    console.log('try-req.send: catch');
  }
}


</script>

</head>
<body>
  <canvas id="picker" width="300" height="300" onclick="PickColor(this, event)"></canvas>
  <div id="kolor"> </div>
</html>
)=====";

void HandleColors(void)
{
      server.sendContent(colors_html);
}

