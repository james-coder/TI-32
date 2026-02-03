// Project: TI-32 v0.1
// Author:  ChromaLock
// Date:    2024

#include "./secrets.h"
#include "./launcher.h"
#include <TICL.h>
#include <CBL2.h>
#include <TIVar.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <UrlEncode.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

// #define CAMERA

#ifdef CAMERA
#include <esp_camera.h>
#define CAMERA_MODEL_XIAO_ESP32S3
#include "./camera_pins.h"
#include "./camera_index.h"
#endif

constexpr auto TIP = D1;
constexpr auto RING = D10;
constexpr auto MAXHDRLEN = 16;
constexpr auto MAXDATALEN = 4096;
constexpr auto MAXARGS = 5;
constexpr auto MAXSTRARGLEN = 256;
constexpr auto PICSIZE = 756;
constexpr auto PICVARSIZE = PICSIZE + 2;
constexpr auto PASSWORD = 42069;
constexpr auto RX_WATCHDOG_MS = 5000;
constexpr auto WIFI_CONNECT_TIMEOUT_MS = 15000;
constexpr auto AP_PORTAL_PORT = 80;
constexpr auto AP_DNS_PORT = 53;
constexpr auto LIGHT_SLEEP_IDLE_MS = 2000;
constexpr auto WIFI_SCAN_MAX = 4;
const char WIFI_ALPHA[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_.:/!?";
constexpr int WIFI_ALPHA_LEN = sizeof(WIFI_ALPHA) - 1;
#ifdef GPIO_INTR_LOW_LEVEL
constexpr gpio_int_type_t GPIO_WAKE_LEVEL = GPIO_INTR_LOW_LEVEL;
#elif defined(GPIO_INTR_LOW)
constexpr gpio_int_type_t GPIO_WAKE_LEVEL = GPIO_INTR_LOW;
#else
constexpr gpio_int_type_t GPIO_WAKE_LEVEL = GPIO_INTR_HIGH_LEVEL;
#endif

#ifndef POWER_MGMT_ENABLED
#define POWER_MGMT_ENABLED 1
#endif

CBL2 cbl;
Preferences prefs;
DNSServer dnsServer;
WebServer webServer(AP_PORTAL_PORT);

String cfg_wifi_ssid;
String cfg_wifi_pass;
String cfg_server;
String cfg_chat_name;
String ap_ssid;
bool ap_mode = false;
bool should_reboot = false;
unsigned long reboot_at_ms = 0;

// whether or not the user has entered the password
bool unlocked = true;

// Arguments
int currentArg = 0;
char strArgs[MAXARGS][MAXSTRARGLEN];
double realArgs[MAXARGS];

// the command to execute
int command = -1;
// whether or not the operation has completed
bool status = 0;
// whether or not the operation failed
bool error = 0;
// error or success message
char message[MAXSTRARGLEN];
// list data
constexpr auto LISTLEN = 256;
constexpr auto LISTENTRYLEN = 20;
char list[LISTLEN][LISTENTRYLEN];
// http response
constexpr auto MAXHTTPRESPONSELEN = 4096;
char response[MAXHTTPRESPONSELEN];
// image variable (96x63)
uint8_t frame[PICVARSIZE] = { PICSIZE & 0xff, PICSIZE >> 8 };
unsigned long last_rx_ms = 0;
unsigned long last_activity_ms = 0;
String scan_ssids[WIFI_SCAN_MAX];
int scan_rssi[WIFI_SCAN_MAX];
int scan_count = 0;
bool scan_valid = false;

void connect();
void disconnect();
void gpt();
void send();
void launcher();
void snap();
void solve();
void image_list();
void fetch_image();
void fetch_chats();
void send_chat();
void program_list();
void fetch_program();
void wifi_scan();
void wifi_set();

struct Command {
  int id;
  const char* name;
  int num_args;
  void (*command_fp)();
  bool wifi;
};

struct Command commands[] = {
  { 0, "connect", 0, connect, false },
  { 1, "disconnect", 0, disconnect, false },
  { 2, "gpt", 1, gpt, true },
  { 3, "wifi_scan", 0, wifi_scan, false },
  { 4, "send", 2, send, true },
  { 5, "launcher", 0, launcher, false },
  { 6, "wifi_set", 4, wifi_set, false },
  { 7, "snap", 0, snap, false },
  { 8, "solve", 1, solve, true },
  { 9, "image_list", 1, image_list, true },
  { 10, "fetch_image", 1, fetch_image, true },
  { 11, "fetch_chats", 2, fetch_chats, true },
  { 12, "send_chat", 2, send_chat, true },
  { 13, "program_list", 1, program_list, true },
  { 14, "fetch_program", 1, fetch_program, true },
};

constexpr int NUMCOMMANDS = sizeof(commands) / sizeof(struct Command);
constexpr int MAXCOMMAND = 14;

int commandExpectedArgs(int cmd) {
  for (int i = 0; i < NUMCOMMANDS; ++i) {
    if (commands[i].id == cmd) {
      return commands[i].num_args;
    }
  }
  return -1;
}

String htmlEscape(const String& input) {
  String out;
  out.reserve(input.length());
  for (size_t i = 0; i < input.length(); ++i) {
    char c = input[i];
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      case '"':
        out += "&quot;";
        break;
      case '\'':
        out += "&#39;";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

String portalPage() {
  String page = "<!DOCTYPE html><html><head><meta charset='utf-8' />";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1' />";
  page += "<title>TI-32 Setup</title>";
  page += "<style>";
  page += "body{font-family:system-ui,Segoe UI,Helvetica,Arial,sans-serif;margin:24px;background:#0f172a;color:#e2e8f0;}";
  page += "h1{font-size:20px;margin:0 0 12px 0;}";
  page += "p{margin:0 0 12px 0;line-height:1.4;}";
  page += "form{background:#111827;padding:16px;border-radius:12px;max-width:420px;}";
  page += "label{display:block;margin:10px 0 4px 0;font-size:13px;color:#cbd5f5;}";
  page += "input{width:100%;padding:10px;border-radius:8px;border:1px solid #334155;background:#0b1020;color:#e2e8f0;}";
  page += "button{margin-top:14px;padding:10px 14px;border-radius:10px;border:0;background:#38bdf8;color:#0b1020;font-weight:600;}";
  page += ".hint{font-size:12px;color:#94a3b8;}";
  page += "</style></head><body>";
  page += "<h1>TI-32 Wi-Fi Setup</h1>";
  page += "<p>Configure Wi-Fi and server settings. After saving, the device will reboot and try to connect.</p>";
  page += "<form method='POST' action='/save'>";
  page += "<label>Wi-Fi SSID</label>";
  page += "<input name='ssid' value='" + htmlEscape(cfg_wifi_ssid) + "' required />";
  page += "<label>Wi-Fi Password</label>";
  page += "<input name='pass' type='password' placeholder='Leave blank to keep current' />";
  page += "<label>Server URL</label>";
  page += "<input name='server' value='" + htmlEscape(cfg_server) + "' placeholder='http://192.168.1.50:8080' />";
  page += "<div class='hint'>Include http:// and port if needed.</div>";
  page += "<label>Chat Name</label>";
  page += "<input name='chat' value='" + htmlEscape(cfg_chat_name) + "' maxlength='8' />";
  page += "<button type='submit'>Save &amp; Reboot</button>";
  page += "</form>";
  page += "<p class='hint'>AP SSID: " + htmlEscape(ap_ssid) + "</p>";
  page += "</body></html>";
  return page;
}

void handlePortal() {
  webServer.send(200, "text/html", portalPage());
}

void handleSave() {
  String ssid = webServer.arg("ssid");
  String pass = webServer.arg("pass");
  String server = webServer.arg("server");
  String chat = webServer.arg("chat");

  ssid.trim();
  server.trim();
  chat.trim();

  if (ssid.length() == 0) {
    webServer.send(400, "text/plain", "SSID required");
    return;
  }

  prefs.putString("wifi_ssid", ssid);
  if (pass.length() > 0) {
    prefs.putString("wifi_pass", pass);
  }
  if (server.length() > 0) {
    prefs.putString("server", server);
  }
  if (chat.length() > 0) {
    prefs.putString("chat_name", chat);
  }

  cfg_wifi_ssid = ssid;
  if (pass.length() > 0) {
    cfg_wifi_pass = pass;
  }
  if (server.length() > 0) {
    cfg_server = server;
  }
  if (chat.length() > 0) {
    cfg_chat_name = chat;
  }

  webServer.send(200, "text/html", "<html><body>Saved. Rebooting...</body></html>");
  should_reboot = true;
  reboot_at_ms = millis() + 1500;
}

void startConfigPortal() {
  if (ap_mode) {
    return;
  }

  WiFi.mode(WIFI_AP);
  uint32_t chip = (uint32_t)ESP.getEfuseMac();
  ap_ssid = String("TI-32-SETUP-") + String(chip & 0xFFFF, HEX);
  ap_ssid.toUpperCase();
  WiFi.softAP(ap_ssid.c_str());

  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(AP_DNS_PORT, "*", apIP);

  webServer.on("/", HTTP_GET, handlePortal);
  webServer.on("/save", HTTP_POST, handleSave);
  webServer.on("/generate_204", HTTP_GET, handlePortal);
  webServer.on("/hotspot-detect.html", HTTP_GET, handlePortal);
  webServer.on("/connecttest.txt", HTTP_GET, handlePortal);
  webServer.on("/ncsi.txt", HTTP_GET, handlePortal);
  webServer.onNotFound(handlePortal);
  webServer.begin();

  ap_mode = true;
  Serial.print("config portal AP: ");
  Serial.print(ap_ssid);
  Serial.print(" @ ");
  Serial.println(apIP);
}

void stopConfigPortal() {
  if (!ap_mode) {
    return;
  }
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  ap_mode = false;
}

void loadConfig() {
  cfg_wifi_ssid = prefs.getString("wifi_ssid", WIFI_SSID);
  cfg_wifi_pass = prefs.getString("wifi_pass", WIFI_PASS);
  cfg_server = prefs.getString("server", SERVER);
  cfg_chat_name = prefs.getString("chat_name", CHAT_NAME);
}

bool connectToWifi(unsigned long timeoutMs) {
  if (cfg_wifi_ssid.length() == 0) {
    Serial.println("no wifi ssid set");
    return false;
  }

  stopConfigPortal();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(cfg_wifi_ssid.c_str(), cfg_wifi_pass.c_str());

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("connected, ip: ");
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.print("wifi connect failed: ");
  Serial.println(WiFi.status());
  return false;
}

int hexDigit(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return 10 + (c - 'A');
  }
  if (c >= 'a' && c <= 'f') {
    return 10 + (c - 'a');
  }
  return -1;
}

bool decodeAlphaHex(const char* hex, String& out) {
  out = "";
  size_t len = strlen(hex);
  if (len == 0) {
    return true;
  }
  if (len % 2 != 0) {
    return false;
  }
  for (size_t i = 0; i < len; i += 2) {
    int hi = hexDigit(hex[i]);
    int lo = hexDigit(hex[i + 1]);
    if (hi < 0 || lo < 0) {
      return false;
    }
    int idx = (hi << 4) | lo;
    if (idx < 0 || idx >= WIFI_ALPHA_LEN) {
      return false;
    }
    out += WIFI_ALPHA[idx];
  }
  return true;
}

String formatRssi(int rssi) {
  String r = String(rssi);
  while (r.length() < 4) {
    r = String(" ") + r;
  }
  if (r.length() > 4) {
    r = r.substring(0, 4);
  }
  return r;
}

String buildWifiList() {
  String out = "";
  for (int i = 0; i < WIFI_SCAN_MAX; ++i) {
    if (i < scan_count) {
      String prefix = String(i) + ":" + scan_ssids[i];
      if (prefix.length() > 12) {
        prefix = prefix.substring(0, 12);
      }
      while (prefix.length() < 12) {
        prefix += " ";
      }
      out += prefix + formatRssi(scan_rssi[i]);
    } else {
      out += "                ";
    }
  }
  return out;
}

uint8_t header[MAXHDRLEN];
uint8_t data[MAXDATALEN];

// lowercase letters make strings weird,
// so we have to truncate the string
void fixStrVar(char* str) {
  int end = strlen(str);
  for (int i = 0; i < end; ++i) {
    if (isLowerCase(str[i])) {
      --end;
    }
  }
  str[end] = '\0';
}

int onReceived(uint8_t type, enum Endpoint model, int datalen);
int onRequest(uint8_t type, enum Endpoint model, int* headerlen,
              int* datalen, data_callback* data_callback);

void startCommand(int cmd) {
  command = cmd;
  status = 0;
  error = 0;
  currentArg = 0;
  last_rx_ms = millis();
  last_activity_ms = last_rx_ms;
  for (int i = 0; i < MAXARGS; ++i) {
    memset(&strArgs[i], 0, MAXSTRARGLEN);
    realArgs[i] = 0;
  }
  strncpy(message, "no command", MAXSTRARGLEN);
}

void setError(const char* err) {
  Serial.print("ERROR: ");
  Serial.println(err);
  error = 1;
  status = 1;
  command = -1;
  strncpy(message, err, MAXSTRARGLEN);
}

void setSuccess(const char* success) {
  Serial.print("SUCCESS: ");
  Serial.println(success);
  error = 0;
  status = 1;
  command = -1;
  strncpy(message, success, MAXSTRARGLEN);
}

int sendProgramVariable(const char* name, uint8_t* program, size_t variableSize);

bool camera_sign = false;

void setup() {
  Serial.begin(115200);
  Serial.println("[CBL]");

  cbl.setLines(TIP, RING);
  cbl.resetLines();
  cbl.setupCallbacks(header, data, MAXDATALEN, onReceived, onRequest);
  // cbl.setVerbosity(true, (HardwareSerial *)&Serial);

  pinMode(TIP, INPUT);
  pinMode(RING, INPUT);

  Serial.println("[preferences]");
  prefs.begin("ccalc", false);
  auto reboots = prefs.getUInt("boots", 0);
  Serial.print("reboots: ");
  Serial.println(reboots);
  prefs.putUInt("boots", reboots + 1);
  loadConfig();

#ifdef CAMERA
  Serial.println("[camera]");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  // this needs to be pixformat grayscale in the future
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  } else {
    Serial.println("camera ready");
    camera_sign = true;  // Camera initialization check passes
  }

  sensor_t* s = esp_camera_sensor_get();
  // enable grayscale
  s->set_special_effect(s, 2);
#endif

  strncpy(message, "default message", MAXSTRARGLEN);
  delay(100);
  memset(data, 0, MAXDATALEN);
  memset(header, 0, 16);
  Serial.println("[ready]");

  last_activity_ms = millis();
  if (!connectToWifi(WIFI_CONNECT_TIMEOUT_MS)) {
    startConfigPortal();
  }
}

void (*queued_action)() = NULL;

void loop() {
  if (queued_action) {
    // dont ask me why you need this, but it fails otherwise.
    // probably relates to a CBL2 timeout thing?
    delay(1000);
    Serial.println("executing queued actions");
    // dont ask me
    void (*tmp)() = queued_action;
    queued_action = NULL;
    tmp();
  }
  if (ap_mode) {
    dnsServer.processNextRequest();
    webServer.handleClient();
  }
  if (should_reboot && millis() > reboot_at_ms) {
    ESP.restart();
  }
  if (command >= 0 && command <= MAXCOMMAND) {
    const int expectedArgs = commandExpectedArgs(command);
    if (expectedArgs >= 0 && currentArg < expectedArgs) {
      if (millis() - last_rx_ms > RX_WATCHDOG_MS) {
        setError("rx timeout");
      }
    }
  }
  if (command >= 0 && command <= MAXCOMMAND) {
    for (int i = 0; i < NUMCOMMANDS; ++i) {
      if (commands[i].id == command && commands[i].num_args == currentArg) {
        if (commands[i].wifi && !WiFi.isConnected()) {
          setError("wifi not connected");
        } else {
          Serial.print("processing command: ");
          Serial.println(commands[i].name);
          commands[i].command_fp();
        }
      }
    }
  }
  cbl.eventLoopTick();

#if POWER_MGMT_ENABLED
  if (!ap_mode && command == -1 && queued_action == NULL) {
    if (millis() - last_activity_ms > LIGHT_SLEEP_IDLE_MS) {
      gpio_wakeup_enable((gpio_num_t)TIP, GPIO_WAKE_LEVEL);
      gpio_wakeup_enable((gpio_num_t)RING, GPIO_WAKE_LEVEL);
      esp_sleep_enable_gpio_wakeup();
      Serial.flush();
      esp_light_sleep_start();
      last_activity_ms = millis();
    }
  }
#endif
}

int onReceived(uint8_t type, enum Endpoint model, int datalen) {
  char varName = header[3];
  last_rx_ms = millis();
  last_activity_ms = last_rx_ms;

  Serial.print("unlocked: ");
  Serial.println(unlocked);

  // check for password
  if (!unlocked && varName == 'P') {
    auto password = TIVar::realToLong8x(data, model);
    if (password == PASSWORD) {
      Serial.println("successful unlock");
      unlocked = true;
      return 0;
    } else {
      Serial.println("failed unlock");
    }
  }

  if (!unlocked) {
    return -1;
  }

  // check for command
  if (varName == 'C') {
    if (type != VarTypes82::VarReal) {
      return -1;
    }
    int cmd = TIVar::realToLong8x(data, model);
    if (cmd >= 0 && cmd <= MAXCOMMAND) {
      Serial.print("command: ");
      Serial.println(cmd);
      startCommand(cmd);
      return 0;
    } else {
      Serial.print("invalid command: ");
      Serial.println(cmd);
      return -1;
    }
  }

  if (currentArg >= MAXARGS) {
    Serial.println("argument overflow");
    setError("argument overflow");
    return -1;
  }

  switch (type) {
    case VarTypes82::VarString:
      Serial.print("len: ");
      strncpy(strArgs[currentArg++], TIVar::strVarToString8x(data, model).c_str(), MAXSTRARGLEN);
      fixStrVar(strArgs[currentArg - 1]);
      Serial.print("Str");
      Serial.print(currentArg - 1);
      Serial.print(" ");
      Serial.println(strArgs[currentArg - 1]);
      break;
    case VarTypes82::VarReal:
      realArgs[currentArg++] = TIVar::realToFloat8x(data, model);
      Serial.print("Real");
      Serial.print(currentArg - 1);
      Serial.print(" ");
      Serial.println(realArgs[currentArg - 1]);
      break;
    default:
      // maybe set error here?
      return -1;
  }
  return 0;
}

uint8_t frameCallback(int idx) {
  return frame[idx];
}

char varIndex(int idx) {
  return '0' + (idx == 9 ? 0 : (idx + 1));
}

int onRequest(uint8_t type, enum Endpoint model, int* headerlen, int* datalen, data_callback* data_callback) {
  char varName = header[3];
  last_activity_ms = millis();
  char strIndex = header[4];
  char strname[5] = { 'S', 't', 'r', varIndex(strIndex), 0x00 };
  char picname[5] = { 'P', 'i', 'c', varIndex(strIndex), 0x00 };
  Serial.print("request for ");
  Serial.println(varName == 0xaa ? strname : varName == 0x60 ? picname
                                                             : (const char*)&header[3]);
  memset(header, 0, sizeof(header));
  switch (varName) {
    case 0x60:
      if (type != VarTypes82::VarPic) {
        return -1;
      }
      *datalen = PICVARSIZE;
      TIVar::intToSizeWord(*datalen, &header[0]);
      header[2] = VarTypes82::VarPic;
      header[3] = 0x60;
      header[4] = strIndex;
      *data_callback = frameCallback;
      break;
    case 0xAA:
      if (type != VarTypes82::VarString) {
        return -1;
      }
      // TODO right now, the only string variable will be the message, but ill need to allow for other vars later
      *datalen = TIVar::stringToStrVar8x(String(message), data, model);
      TIVar::intToSizeWord(*datalen, header);
      header[2] = VarTypes82::VarString;
      header[3] = 0xAA;
      // send back as same variable that was requested
      header[4] = strIndex;
      *headerlen = 13;
      break;
    case 'E':
      if (type != VarTypes82::VarReal) {
        return -1;
      }
      *datalen = TIVar::longToReal8x(error, data, model);
      TIVar::intToSizeWord(*datalen, header);
      header[2] = VarTypes82::VarReal;
      header[3] = 'E';
      header[4] = '\0';
      *headerlen = 13;
      break;
    case 'S':
      if (type != VarTypes82::VarReal) {
        return -1;
      }
      *datalen = TIVar::longToReal8x(status, data, model);
      TIVar::intToSizeWord(*datalen, header);
      header[2] = VarTypes82::VarReal;
      header[3] = 'S';
      header[4] = '\0';
      *headerlen = 13;
      break;
    default:
      return -1;
  }
  return 0;
}

int makeRequest(String url, char* result, int resultLen, size_t* len) {
  memset(result, 0, resultLen);

#ifdef SECURE
  WiFiClientSecure client;
  client.setInsecure();
#else
  WiFiClient client;
#endif
  HTTPClient http;
  http.setAuthorization(HTTP_USERNAME, HTTP_PASSWORD);

  Serial.println(url);
  http.begin(client, url.c_str());

  // Send HTTP GET request
  int httpResponseCode = http.GET();
  Serial.print(url);
  Serial.print(" ");
  Serial.println(httpResponseCode);

  int responseSize = http.getSize();
  WiFiClient* httpStream = http.getStreamPtr();

  Serial.print("response size: ");
  Serial.println(responseSize);

  if (httpResponseCode != 200) {
    return httpResponseCode;
  }

  if (httpStream->available() > resultLen) {
    Serial.print("response size: ");
    Serial.print(httpStream->available());
    Serial.println(" is too big");
    return -1;
  }

  while (httpStream->available()) {
    *(result++) = httpStream->read();
  }
  *len = responseSize;

  http.end();

  return 0;
}

void connect() {
  Serial.print("SSID: ");
  Serial.println(cfg_wifi_ssid);
  Serial.print("PASS: ");
  Serial.println("<hidden>");
  if (connectToWifi(WIFI_CONNECT_TIMEOUT_MS)) {
    setSuccess("connected");
  } else {
    startConfigPortal();
    setError("wifi setup");
  }
}

void disconnect() {
  WiFi.disconnect(true);
  setSuccess("disconnected");
}

void wifi_scan() {
  stopConfigPortal();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false);

  scan_count = 0;
  scan_valid = false;

  int n = WiFi.scanNetworks(false, true);
  if (n <= 0) {
    setSuccess(buildWifiList().c_str());
    return;
  }

  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);
    if (ssid.length() == 0) {
      continue;
    }

    int insertAt = scan_count;
    for (int j = 0; j < scan_count; ++j) {
      if (rssi > scan_rssi[j]) {
        insertAt = j;
        break;
      }
    }

    if (insertAt >= WIFI_SCAN_MAX) {
      continue;
    }

    int limit = min(scan_count, WIFI_SCAN_MAX - 1);
    for (int j = limit; j > insertAt; --j) {
      scan_ssids[j] = scan_ssids[j - 1];
      scan_rssi[j] = scan_rssi[j - 1];
    }

    scan_ssids[insertAt] = ssid;
    scan_rssi[insertAt] = rssi;
    if (scan_count < WIFI_SCAN_MAX) {
      scan_count++;
    }
  }

  WiFi.scanDelete();
  scan_valid = true;
  setSuccess(buildWifiList().c_str());
}

void wifi_set() {
  int index = (int)realArgs[0];

  if (!scan_valid || index < 0 || index >= scan_count) {
    setError("scan first");
    return;
  }

  String passDecoded;
  String serverDecoded;
  String chatDecoded;

  if (!decodeAlphaHex(strArgs[0], passDecoded)) {
    setError("bad pass");
    return;
  }
  if (!decodeAlphaHex(strArgs[1], serverDecoded)) {
    setError("bad server");
    return;
  }
  if (!decodeAlphaHex(strArgs[2], chatDecoded)) {
    setError("bad chat");
    return;
  }

  cfg_wifi_ssid = scan_ssids[index];
  prefs.putString("wifi_ssid", cfg_wifi_ssid);

  if (passDecoded.length() > 0) {
    cfg_wifi_pass = passDecoded;
    prefs.putString("wifi_pass", cfg_wifi_pass);
  }
  if (serverDecoded.length() > 0) {
    cfg_server = serverDecoded;
    prefs.putString("server", cfg_server);
  }
  if (chatDecoded.length() > 0) {
    cfg_chat_name = chatDecoded;
    prefs.putString("chat_name", cfg_chat_name);
  }

  if (connectToWifi(WIFI_CONNECT_TIMEOUT_MS)) {
    setSuccess("wifi saved");
  } else {
    startConfigPortal();
    setError("wifi setup");
  }
}

void gpt() {
  const char* prompt = strArgs[0];
  Serial.print("prompt: ");
  Serial.println(prompt);

  auto url = String(cfg_server) + String("/gpt/ask?question=") + urlEncode(String(prompt));

  size_t realsize = 0;
  if (makeRequest(url, response, MAXHTTPRESPONSELEN, &realsize)) {
    setError("error making request");
    return;
  }

  Serial.print("response: ");
  Serial.println(response);

  setSuccess(response);
}

void send() {
  const char* recipient = strArgs[0];
  const char* message = strArgs[1];
  Serial.print("sending \"");
  Serial.print(message);
  Serial.print("\" to \"");
  Serial.print(recipient);
  Serial.println("\"");
  setSuccess("OK: sent");
}

void _sendLauncher() {
  sendProgramVariable("TI32", __launcher_var, __launcher_var_len);
}

void launcher() {
  // we have to queue this action, since otherwise the transfer fails
  // due to the CBL2 library still using the lines
  queued_action = _sendLauncher;
  setSuccess("queued transfer");
}

void snap() {
#ifdef CAMERA
  if (!camera_sign) {
    setError("camera failed to initialize");
  }
#else
  setError("pictures not supported");
#endif
}

void solve() {
#ifdef CAMERA
  if (!camera_sign) {
    setError("camera failed to initialize");
  }
#else
  setError("pictures not supported");
#endif
}

void image_list() {
  int page = realArgs[0];
  auto url = String(cfg_server) + String("/image/list?p=") + urlEncode(String(page));

  size_t realsize = 0;
  if (makeRequest(url, response, MAXSTRARGLEN, &realsize)) {
    setError("error making request");
    return;
  }

  Serial.print("response: ");
  Serial.println(response);

  setSuccess(response);
}

void fetch_image() {
  memset(frame + 2, 0, 756);
  // fetch image and put it into the frame variable
  int id = realArgs[0];
  Serial.print("id: ");
  Serial.println(id);

  auto url = String(cfg_server) + String("/image/get?id=") + urlEncode(String(id));

  size_t realsize = 0;
  if (makeRequest(url, response, MAXHTTPRESPONSELEN, &realsize)) {
    setError("error making request");
    return;
  }

  if (realsize != PICSIZE) {
    Serial.print("response size:");
    Serial.println(realsize);
    setError("bad image size");
    return;
  }

  // load the image
  frame[0] = realsize & 0xff;
  frame[1] = (realsize >> 8) & 0xff;
  memcpy(&frame[2], response, 756);

  setSuccess(response);
}

void fetch_chats() {
  int room = realArgs[0];
  int page = realArgs[1];
  auto url = String(cfg_server) + String("/chats/messages?p=") + urlEncode(String(page)) + String("&c=") + urlEncode(String(room));

  size_t realsize = 0;
  if (makeRequest(url, response, MAXSTRARGLEN, &realsize)) {
    setError("error making request");
    return;
  }

  Serial.print("response: ");
  Serial.println(response);

  setSuccess(response);
}

void send_chat() {
  int room = realArgs[0];
  const char* msg = strArgs[1];

  auto url = String(cfg_server) + String("/chats/send?c=") + urlEncode(String(room)) + String("&m=") + urlEncode(String(msg)) + String("&id=") + urlEncode(String(cfg_chat_name));

  size_t realsize = 0;
  if (makeRequest(url, response, MAXSTRARGLEN, &realsize)) {
    setError("error making request");
    return;
  }

  Serial.print("response: ");
  Serial.println(response);

  setSuccess(response);
}

void program_list() {
  int page = realArgs[0];
  auto url = String(cfg_server) + String("/programs/list?p=") + urlEncode(String(page));

  size_t realsize = 0;
  if (makeRequest(url, response, MAXSTRARGLEN, &realsize)) {
    setError("error making request");
    return;
  }

  Serial.print("response: ");
  Serial.println(response);

  setSuccess(response);
}


char programName[256];
char programData[4096];
size_t programLength;

void _resetProgram() {
  memset(programName, 0, 256);
  memset(programData, 0, 4096);
  programLength = 0;
}

void _sendDownloadedProgram() {
  if (sendProgramVariable(programName, (uint8_t*)programData, programLength)) {
    Serial.println("failed to transfer requested download");
    Serial.print(programName);
    Serial.print("(");
    Serial.print(programLength);
    Serial.println(")");
  }
  _resetProgram();
}

void fetch_program() {
  int id = realArgs[0];
  Serial.print("id: ");
  Serial.println(id);

  _resetProgram();

  auto url = String(cfg_server) + String("/programs/get?id=") + urlEncode(String(id));

  if (makeRequest(url, programData, 4096, &programLength)) {
    setError("error making request for program data");
    return;
  }

  size_t realsize = 0;
  auto nameUrl = String(cfg_server) + String("/programs/get_name?id=") + urlEncode(String(id));
  if (makeRequest(nameUrl, programName, 256, &realsize)) {
    setError("error making request for program name");
    return;
  }

  queued_action = _sendDownloadedProgram;

  setSuccess("queued download");
}

/// OTHER FUNCTIONS

int sendProgramVariable(const char* name, uint8_t* program, size_t variableSize) {
  Serial.print("transferring: ");
  Serial.print(name);
  Serial.print("(");
  Serial.print(variableSize);
  Serial.println(")");

  int dataLength = 0;

  // IF THIS ISNT SET TO COMP83P, THIS DOESNT WORK
  // seems like ti-84s cant silent transfer to each other
  uint8_t msg_header[4] = { COMP83P, RTS, 13, 0 };

  uint8_t rtsdata[13] = { variableSize & 0xff, variableSize >> 8, VarTypes82::VarProgram, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  int nameSize = strlen(name);
  if (nameSize == 0) {
    return 1;
  }
  memcpy(&rtsdata[3], name, min(nameSize, 8));

  auto rtsVal = cbl.send(msg_header, rtsdata, 13);
  if (rtsVal) {
    Serial.print("rts return: ");
    Serial.println(rtsVal);
    return rtsVal;
  }

  cbl.resetLines();
  auto ackVal = cbl.get(msg_header, NULL, &dataLength, 0);
  if (ackVal || msg_header[1] != ACK) {
    Serial.print("ack return: ");
    Serial.println(ackVal);
    return ackVal;
  }

  auto ctsRet = cbl.get(msg_header, NULL, &dataLength, 0);
  if (ctsRet || msg_header[1] != CTS) {
    Serial.print("cts return: ");
    Serial.println(ctsRet);
    return ctsRet;
  }

  msg_header[1] = ACK;
  msg_header[2] = 0x00;
  msg_header[3] = 0x00;
  ackVal = cbl.send(msg_header, NULL, 0);
  if (ackVal || msg_header[1] != ACK) {
    Serial.print("ack cts return: ");
    Serial.println(ackVal);
    return ackVal;
  }

  msg_header[1] = DATA;
  msg_header[2] = variableSize & 0xff;
  msg_header[3] = (variableSize >> 8) & 0xff;
  auto dataRet = cbl.send(msg_header, program, variableSize);
  if (dataRet) {
    Serial.print("data return: ");
    Serial.println(dataRet);
    return dataRet;
  }

  ackVal = cbl.get(msg_header, NULL, &dataLength, 0);
  if (ackVal || msg_header[1] != ACK) {
    Serial.print("ack data: ");
    Serial.println(ackVal);
    return ackVal;
  }

  msg_header[1] = EOT;
  msg_header[2] = 0x00;
  msg_header[3] = 0x00;
  auto eotVal = cbl.send(msg_header, NULL, 0);
  if (eotVal) {
    Serial.print("eot return: ");
    Serial.println(eotVal);
    return eotVal;
  }

  Serial.print("transferred: ");
  Serial.println(name);
  return 0;
}
