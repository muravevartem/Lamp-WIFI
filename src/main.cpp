#include <Arduino.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>

#define LED_NUM 26 // 2-3-4-4-4-4-3-2
#define LED_PIN D6
#define DEFAULT_BRIGHTNESS 255
#define WIFI_SSID "Network"
#define WIFI_PASSWORD "ypkwgfwf"
#define HOSTNAME "lamp-webupdate"

#define DEFAULT_STATIC_COLOR 0xFFFFE0

#define LAMP_MODE_STATIC 0
#define LAMP_MODE_RAINBOW 1
#define LAMP_MODE_PULSE 2
#define LAMP_MODE_PAIR 3

#define SUCCESSFULLY_MESSAGE "Successfully"

CRGB leds[LED_NUM];

uint64_t timer = 0;
uint64_t timer_save = 0;

uint8_t counter_8bits = 0;

CRGB static_color = DEFAULT_STATIC_COLOR;

void static_all()
{
  FastLED.showColor(static_color);
}

void rainbow_all()
{
  if (millis() - timer > 30)
  {
    for (int i = 0; i < LED_NUM; i++)
    {
      leds[i].setHue(counter_8bits);
    }
    counter_8bits++;
    timer = millis();
    FastLED.show();
  }
}

void pulse()
{
  if (millis() - timer > 30)
  {
    for (int i = 0; i < LED_NUM; i++)
    {
      leds[i] = ColorFromPalette((CRGBPalette16){CRGB::Black, static_color, CRGB::Black}, counter_8bits);
    }
    FastLED.show();
    timer = millis();
    counter_8bits++;
  }
}

void (*mode)() = static_all;

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer updater;

const char* HOME_PAGE = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\"><title>Title</title><link rel=\"stylesheet\"href=\"css/bootstrap.css\"></head><body><header class=\"bg-primary p-2\"><p class=\"h3 text-white fw-bolder\">WIFI_LAMP</p></header><main class=\"p-3 min-vh-100\"><div class=\"p-3\"><p class=\"h3 fw-bold text-primary\">Control panel</p><div class=\"p-3\"><div class=\"p-3 rounded-2 bg-light\"><label for=\"mode\"class=\"form-label h3 fw-bolder text-primary\">Mode</label><div id=\"mode\"><div class=\"d-flex flex-row justify-content-center \"><button class=\"btn btn-outline-primary m-2\"id=\"rainbow-mode\">RAINBOW</button><button class=\"btn btn-outline-primary m-2\"id=\"pulse-mode\">PULSE</button><button class=\"btn btn-outline-warning m-2\"id=\"static-mode\">STATIC</button></div><div class=\"rounded-2 border border-primary p-2\"><label for=\"mode-extra\"class=\"h4 fw-bolder text-primary\">Settings</label><div id=\"mode-extra\"class=\"p-2\"><div><label for=\"color\"class=\"form-label text-primary\">Color</label><input type=\"color\"id=\"color\"></div><div><label for=\"speed\"class=\"form-label text-primary\">Speed</label><input type=\"number\"id=\"speed\"min=\"1\"max=\"120\"></div></div></div></div></div></div></div></main><footer style=\"min-height: 100px\"class=\"bg-primary p-2 text-white\"><p class=\"fw-bolder\">Made by SeMurA823</p><p>Github:<a href=\"https://github.com/SeMurA823\"class=\"link-light\">Repository</a></p><p class=\"text-center\">2022</p></footer><script>document.getElementById(\"rainbow-mode\").addEventListener(\"click\",async()=>{let response=await fetch('http://lamp-webupdate.local/mode?mode=1',{method:'POST',body:\"\"})});document.getElementById(\"pulse-mode\").addEventListener(\"click\",async()=>{let response=await fetch('http://lamp-webupdate.local/mode?mode=2',{method:'POST',body:\"\"})});document.getElementById(\"static-mode\").addEventListener(\"click\",async()=>{let response=await fetch('http://lamp-webupdate.local/mode?mode=0',{method:'POST',body:\"\"})});document.getElementById(\"color\").addEventListener(\"change\",async()=>{let data=document.getElementById('color').value;let s=data.substr(1).toUpperCase();const symbols=['0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'];let strings=s.split('');let res=0;for(let i=strings.length-1;i>=0;i--){let number=symbols.indexOf(strings[i]);res+=number*Math.pow(16,strings.length-i-1)}let response=await fetch('http://lamp-webupdate.local/mode?mode=0&color='+res,{method:'POST',body:\"\"})});</script></body></html>";

void setMode(void (*new_mode)())
{
  counter_8bits = 0;
  timer = millis();
  mode = new_mode;
}

void handleEditMode()
{
  int mode_num = server.arg("mode").toInt();
  if (mode_num == LAMP_MODE_RAINBOW)
  {
    setMode(rainbow_all);
  }
  else if (mode_num == LAMP_MODE_PULSE)
  {
    setMode(pulse);
  }
  else if (mode_num == LAMP_MODE_STATIC)
  {
    if (server.hasArg("color"))
    {
      static_color = server.arg("color").toInt();
    }
    else
    {
      static_color = DEFAULT_STATIC_COLOR;
    }
    setMode(static_all);
  }
  server.send(200, "text/plain", SUCCESSFULLY_MESSAGE);
}

void handleEditBrightness()
{
  int val = server.arg("value").toInt();
  FastLED.setBrightness(val);
  FastLED.show();
  server.send(200, "text/plain", SUCCESSFULLY_MESSAGE);
}

void handleHomePage()
{
  server.send(200, "text/html", HOME_PAGE);
}

void setup()
{
  Serial.begin(115200);
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, LED_NUM);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.show();
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("WiFi failed, retrying ...");
  }
  MDNS.begin(HOSTNAME);
  updater.setup(&server);

  server.on("/", HTTP_GET, handleHomePage);
  server.on("/mode", HTTP_POST, handleEditMode);
  server.on("/brightness", HTTP_POST, handleEditBrightness);

  server.begin();
  MDNS.addService("http", "tcp", 80);
  Serial.printf("HTTPUpdateServer ready! Open http://%s.local/update in your browser\n", HOSTNAME);
}

void loop()
{
  server.handleClient();
  MDNS.update();
  mode();
}