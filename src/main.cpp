#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Preferences.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// SSD1306 OLED (I2C)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 42, /* data=*/ 41, /* reset=*/ U8X8_PIN_NONE);

// Web服务器
WebServer server(80);

// 配置存储
Preferences preferences;

// 默认配置
String ssid = "";
String password = "";
String location = "北京";  // 可替换为你要查询的城市

// 定义时区信息
const char* ntpServer = "cn.pool.ntp.org";
const long  gmtOffset_sec = 28800; // 北京时间，东八区，偏移秒数
const int   daylightOffset_sec = 0;

// AP模式配置
const char* ap_ssid = "OLED_Weather";
const char* ap_password = "12345678";
bool config_mode = false;

// HTML配置页面
const char* html_config_page = "<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"  <meta charset=\"UTF-8\">\n"
"  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"  <title>OLED 天气配置</title>\n"
"  <style>\n"
"    body {\n"
"      font-family: Arial, sans-serif;\n"
"      margin: 20px;\n"
"      padding: 20px;\n"
"      background-color: #f0f0f0;\n"
"    }\n"
"    .container {\n"
"      max-width: 400px;\n"
"      margin: 0 auto;\n"
"      background-color: white;\n"
"      padding: 20px;\n"
"      border-radius: 10px;\n"
"      box-shadow: 0 0 10px rgba(0,0,0,0.1);\n"
"    }\n"
"    h1 {\n"
"      color: #333;\n"
"      text-align: center;\n"
"    }\n"
"    input {\n"
"      width: 100%;\n"
"      padding: 10px;\n"
"      margin: 10px 0;\n"
"      box-sizing: border-box;\n"
"      border: 1px solid #ccc;\n"
"      border-radius: 5px;\n"
"    }\n"
"    button {\n"
"      background-color: #4CAF50;\n"
"      color: white;\n"
"      padding: 12px 20px;\n"
"      border: none;\n"
"      border-radius: 5px;\n"
"      cursor: pointer;\n"
"      width: 100%;\n"
"      font-size: 16px;\n"
"    }\n"
"    button:hover {\n"
"      background-color: #45a049;\n"
"    }\n"
"    .section {\n"
"      margin-bottom: 20px;\n"
"    }\n"
"    label {\n"
"      display: block;\n"
"      margin-bottom: 5px;\n"
"      font-weight: bold;\n"
"    }\n"
"  </style>\n"
"</head>\n"
"<body>\n"
"  <div class=\"container\">\n"
"    <h1>OLED 天气配置</h1>\n"
"    <form action=\"/save\" method=\"POST\">\n"
"      <div class=\"section\">\n"
"        <h2>WiFi 设置</h2>\n"
"        <label for=\"ssid\">WiFi 名称:</label>\n"
"        <input type=\"text\" id=\"ssid\" name=\"ssid\" placeholder=\"输入WiFi名称\" required>\n"
"        <label for=\"password\">WiFi 密码:</label>\n"
"        <input type=\"password\" id=\"password\" name=\"password\" placeholder=\"输入WiFi密码\">\n"
"      </div>\n"
"      <div class=\"section\">\n"
"        <h2>天气设置</h2>\n"
"        <label for=\"location\">城市名称:</label>\n"
"        <input type=\"text\" id=\"location\" name=\"location\" placeholder=\"输入城市名称，如：北京\" required>\n"
"      </div>\n"
"      <button type=\"submit\">保存配置</button>\n"
"    </form>\n"
"  </div>\n"
"</body>\n"
"</html>\n";

// 定义缓存变量
String cachedWeatherDesc;
String cachedTemperature;
String cachedHumidity;
String cachedFeelsLike;
String cachedPoem; // 每日诗词缓存

// 定义更新天气气温和状况的时间间隔（10分钟，单位：毫秒）
const unsigned long interval = 10 * 60 * 1000;
unsigned long previousMillis = 0;

// 定义按钮引脚
const int buttonPin = 5;
// 定义当前页面（0=时间 1=天气 2=诗词 3=网络 4=日月星辰）
int currentPage = 0;
const int pageCount = 5;

// 每日诗词滚动相关变量
unsigned long scrollPosition = 0;        // 累计滚动位移（像素）
unsigned long lastScrollTime = 0;        // 上次滚动时间
const unsigned long scrollInterval = 80; // 滚动间隔（毫秒）
bool needsFullRedraw = true;             // 切页等场景需要整屏重绘

// 按钮相关：消抖 + 短按切页 + 长按进配置
const unsigned long debounceMs = 30;          // 机械按钮抖动窗口
const unsigned long longPressDuration = 3000; // 长按3秒进入配置模式
bool btnStable = false;                       // 消抖后的状态（true=按下）
bool btnLastRead = false;                     // 上一次读到的原始电平
unsigned long btnLastChange = 0;              // 原始电平最后一次变化的时间
unsigned long btnPressStart = 0;
bool btnLongPressFired = false;
// 加载配置
void loadConfig() {
  preferences.begin("oled_weather", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  location = preferences.getString("location", "北京");
  preferences.end();
}

// 保存配置
void saveConfig() {
  preferences.begin("oled_weather", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("location", location);
  preferences.end();
}

// Web服务器处理函数
void handleRoot() {
  server.send(200, "text/html", html_config_page);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("location")) {
    ssid = server.arg("ssid");
    password = server.arg("password");
    location = server.arg("location");
    
    saveConfig();
    
    // 显示成功页面
    String html = "<html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>配置成功</title><style>body{font-family:Arial,sans-serif;margin:20px;padding:20px;background-color:#f0f0f0;}.container{max-width:400px;margin:0 auto;background-color:white;padding:20px;border-radius:10px;box-shadow:0 0 10px rgba(0,0,0,0.1);}h1{color:#4CAF50;text-align:center;}p{text-align:center;font-size:16px;}button{background-color:#4CAF50;color:white;padding:12px 20px;border:none;border-radius:5px;cursor:pointer;width:100%;font-size:16px;}button:hover{background-color:#45a049;}</style></head><body><div class='container'><h1>配置成功！</h1><p>设备将重启并连接到新的WiFi网络。</p><p>WiFi: " + ssid + "</p><p>城市: " + location + "</p><button onclick=\"window.location.href='/'\">返回配置</button></div></body></html>";
    server.send(200, "text/html", html);
    
    // 重启设备
    delay(3000);
    ESP.restart();
  } else {
    server.send(400, "text/html", "<html><body><h1>配置失败</h1><p>缺少必要参数</p></body></html>");
  }
}

// 检查WiFi连接
bool checkWiFiConnection() {
  int attempts = 0;
  int maxAttempts = 30;
  
  WiFi.begin(ssid.c_str(), password.c_str());
  
  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(1000);
    Serial.print(".");
    attempts++;
  }
  
  return WiFi.status() == WL_CONNECTED;
}

// WiFi 掉线自动重连（非阻塞，每 15 秒最多尝试一次）
// 返回 true 表示本次调用中刚刚恢复了连接
bool ensureWiFi() {
  static unsigned long lastAttempt = 0;
  static bool linkWasUp = true;

  if (WiFi.status() == WL_CONNECTED) {
    if (!linkWasUp) {
      linkWasUp = true;
      Serial.println("[wifi] 连接已恢复，IP: " + WiFi.localIP().toString());
      return true;
    }
    return false;
  }

  unsigned long now = millis();
  if (linkWasUp) {
    linkWasUp = false;
    lastAttempt = now - 20000; // 让首次检查立即触发重连
    Serial.println("[wifi] 连接丢失，开始自动重连");
  }

  if (now - lastAttempt < 15000) {
    return false;
  }

  lastAttempt = now;
  Serial.println("[wifi] 尝试重连: " + ssid);
  WiFi.disconnect();
  WiFi.begin(ssid.c_str(), password.c_str());
  return false;
}



// ---------------------------- 天气描述中文化 ----------------------------
// wttr.in 的 lang_zh 字段并不完整：像 "Smoky haze"/"Clear"/"Sunny" 这类会原样
// 返回英文字符串（还可能带尾随空格）。这里用 天气码 -> 中文 为主映射，
// 英文描述 -> 中文 作兜底，保证屏幕上不出现英文。
struct WeatherZhCode { int code; const char *zh; };
static const WeatherZhCode kWeatherZhCode[] = {
  {113, "晴"},   {116, "多云"}, {119, "阴"},   {122, "阴"},
  {143, "薄雾"}, {149, "烟霾"}, {248, "雾"},   {260, "冻雾"},
  {176, "局部有雨"}, {179, "局部有雪"}, {182, "局部雨夹雪"}, {185, "局部冻雨"},
  {200, "雷暴"}, {227, "吹雪"}, {230, "暴风雪"},
  {263, "零星毛毛雨"}, {266, "毛毛雨"}, {281, "冻毛毛雨"}, {284, "强冻毛毛雨"},
  {293, "零星小雨"}, {296, "小雨"}, {299, "间歇中雨"}, {302, "中雨"},
  {305, "间歇大雨"}, {308, "大雨"}, {311, "冻雨"}, {314, "强冻雨"},
  {317, "小雨夹雪"}, {320, "雨夹雪"},
  {323, "零星小雪"}, {326, "小雪"}, {329, "零星中雪"}, {332, "中雪"},
  {335, "零星大雪"}, {338, "大雪"}, {350, "冰粒"},
  {353, "小阵雨"}, {356, "阵雨"}, {359, "暴雨"},
  {362, "阵雨夹雪"}, {365, "强阵雨夹雪"}, {368, "小阵雪"}, {371, "阵雪"},
  {374, "小冰粒"}, {377, "冰粒"},
  {386, "雷阵雨"}, {389, "强雷阵雨"}, {392, "雷阵雪"}, {395, "强雷阵雪"},
};

struct WeatherZhText { const char *en; const char *zh; };
static const WeatherZhText kWeatherZhText[] = {
  {"Clear", "晴"}, {"Sunny", "晴"}, {"Partly cloudy", "多云"},
  {"Cloudy", "阴"}, {"Overcast", "阴"},
  {"Mist", "薄雾"}, {"Fog", "雾"}, {"Freezing fog", "冻雾"},
  {"Haze", "霾"}, {"Smoky haze", "烟霾"}, {"Smoke", "烟霾"},
  {"Dust", "浮尘"}, {"Widespread dust", "大范围浮尘"},
  {"Sand", "沙"}, {"Duststorm", "沙尘暴"}, {"Sandstorm", "沙尘暴"},
  {"Blowing sand", "扬沙"}, {"Blowing snow", "吹雪"}, {"Blizzard", "暴风雪"},
  {"Patchy rain nearby", "局部有雨"}, {"Patchy rain possible", "可能有雨"},
  {"Patchy snow possible", "可能有雪"}, {"Patchy sleet possible", "可能有雨夹雪"},
  {"Patchy freezing drizzle possible", "可能有冻毛毛雨"},
  {"Thundery outbreaks possible", "可能有雷暴"},
  {"Patchy light drizzle", "零星毛毛雨"}, {"Light drizzle", "毛毛雨"},
  {"Freezing drizzle", "冻毛毛雨"}, {"Heavy freezing drizzle", "强冻毛毛雨"},
  {"Patchy light rain", "零星小雨"}, {"Light rain", "小雨"},
  {"Moderate rain at times", "间歇中雨"}, {"Moderate rain", "中雨"},
  {"Heavy rain at times", "间歇大雨"}, {"Heavy rain", "大雨"},
  {"Light freezing rain", "冻雨"}, {"Moderate or heavy freezing rain", "强冻雨"},
  {"Light sleet", "小雨夹雪"}, {"Moderate or heavy sleet", "雨夹雪"},
  {"Patchy light snow", "零星小雪"}, {"Light snow", "小雪"},
  {"Patchy moderate snow", "零星中雪"}, {"Moderate snow", "中雪"},
  {"Patchy heavy snow", "零星大雪"}, {"Heavy snow", "大雪"},
  {"Ice pellets", "冰粒"},
  {"Light rain shower", "小阵雨"}, {"Moderate or heavy rain shower", "阵雨"},
  {"Torrential rain shower", "暴雨"},
  {"Light sleet showers", "阵雨夹雪"}, {"Moderate or heavy sleet showers", "强阵雨夹雪"},
  {"Light snow showers", "小阵雪"}, {"Moderate or heavy snow showers", "阵雪"},
  {"Light showers of ice pellets", "小冰粒"},
  {"Moderate or heavy showers of ice pellets", "冰粒"},
  {"Patchy light rain with thunder", "雷阵雨"},
  {"Moderate or heavy rain with thunder", "强雷阵雨"},
  {"Patchy light snow with thunder", "雷阵雪"},
  {"Moderate or heavy snow with thunder", "强雷阵雪"},
};

// 纯 ASCII 视为「服务端没翻译」
static bool isPlainAscii(const String &s) {
  for (unsigned int i = 0; i < s.length(); i++) {
    if ((uint8_t)s[i] > 0x7F) return false;
  }
  return true;
}

// 去掉首尾空白（wttr.in 会返回 "Clear " 这种带空格的值）
static String trimCopy(const String &s) {
  int start = 0;
  int end = (int)s.length();
  while (start < end && isspace((unsigned char)s[start])) start++;
  while (end > start && isspace((unsigned char)s[end - 1])) end--;
  return s.substring(start, end);
}

static String weatherCodeToZh(int code) {
  for (const auto &item : kWeatherZhCode) {
    if (item.code == code) return String(item.zh);
  }
  return "";
}

static String weatherTextToZh(const String &en) {
  String key = trimCopy(en);
  if (key.isEmpty()) return "";
  for (const auto &item : kWeatherZhText) {
    if (key.equalsIgnoreCase(item.en)) return String(item.zh);
  }
  return "";
}

// 依次尝试：中文原文 -> 天气码 -> 英文描述 -> 英文描述(天气码位)
static String translateWeatherDesc(int code, const String &langText, const String &enText) {
  if (!langText.isEmpty() && !isPlainAscii(langText)) return trimCopy(langText);

  String zh = weatherCodeToZh(code);
  if (!zh.isEmpty()) return zh;

  zh = weatherTextToZh(enText);
  if (!zh.isEmpty()) return zh;

  zh = weatherTextToZh(langText);
  if (!zh.isEmpty()) return zh;

  return trimCopy(langText.isEmpty() ? enText : langText);
}

// URL 编码：城市名是 UTF-8 中文，直接拼进 URL 遇到空格/特殊字符会请求失败
static String urlEncode(const String &s) {
  static const char hexDigits[] = "0123456789ABCDEF";
  String out;
  out.reserve(s.length() * 3);
  for (unsigned int i = 0; i < s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                      (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
    if (unreserved) {
      out += (char)c;
    } else {
      out += '%';
      out += hexDigits[c >> 4];
      out += hexDigits[c & 0x0F];
    }
  }
  return out;
}

// ---------------------------- 日月天文 ----------------------------
// 这些字段本来就跟着 j1 响应一起返回，解析它们不需要额外请求
String cachedSunrise;      // 原始值形如 "06:39 AM"
String cachedSunset;       // 原始值形如 "05:17 PM"
String cachedMoonPhase;    // 形如 "Waning Gibbous"
int cachedMoonIllum = -1;  // 月面亮度 0~100，-1 表示未知
bool cachedMoonWaxing = false; // true=盈月（右侧亮）

// "06:39 AM" -> 一天中的分钟数；解析失败返回 -1
static int clockToMinutes(const String &src) {
  String s = trimCopy(src);
  int colon = s.indexOf(':');
  if (colon < 1) return -1;

  int h = s.substring(0, colon).toInt();
  int m = s.substring(colon + 1, colon + 3).toInt();
  bool pm = s.indexOf("PM") >= 0 || s.indexOf("pm") >= 0;
  bool am = s.indexOf("AM") >= 0 || s.indexOf("am") >= 0;

  if (pm && h != 12) h += 12;
  if (am && h == 12) h = 0;
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

// 统一成 24 小时制 "06:39"
static String clockTo24(const String &src) {
  int mins = clockToMinutes(src);
  if (mins < 0) return trimCopy(src);
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", mins / 60, mins % 60);
  return String(buf);
}

// 月相英文 -> 中文
static String moonPhaseToZh(const String &phase) {
  static const struct { const char *en; const char *zh; } kMoonName[] = {
    {"New Moon", "新月"},        {"Waxing Crescent", "娥眉月"},
    {"First Quarter", "上弦月"}, {"Waxing Gibbous", "盈凸月"},
    {"Full Moon", "满月"},       {"Waning Gibbous", "亏凸月"},
    {"Last Quarter", "下弦月"},  {"Waning Crescent", "残月"},
  };
  String key = trimCopy(phase);
  if (key.isEmpty()) return "";
  for (const auto &item : kMoonName) {
    if (key.equalsIgnoreCase(item.en)) return String(item.zh);
  }
  return key;
}

// 逐行绘制真实月相：晨昏线按椭圆弧收缩，而不是拿一个圆去硬擦
// f=1 满月，f=0.5 上下弦，f=0 新月；k 控制晨昏线离圆心的距离
static void drawMoonPhase(int cx, int cy, int r, int illumPct, bool waxing) {
  float f = illumPct / 100.0f;
  f = constrain(f, 0.0f, 1.0f);
  float k = fabsf(cosf(PI * f));

  for (int y = -r; y <= r; y++) {
    int w = (int)(sqrtf((float)(r * r - y * y)) + 0.5f);
    if (w <= 0) continue;

    int term = (int)(w * k + 0.5f); // 晨昏线在该行的位置
    int left, right;

    if (waxing) {          // 盈月：右侧亮
      right = w;
      left = (f >= 0.5f) ? -term : term;
    } else {               // 亏月：左侧亮
      left = -w;
      right = (f >= 0.5f) ? term : -term;
    }

    if (left > right) continue;
    u8g2.drawLine(cx + left, cy + y, cx + right, cy + y);
  }

  u8g2.drawCircle(cx, cy, r); // 月盘轮廓
}

// 统一获取所有天气数据，减少API调用次数
bool getAllWeatherData() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String apiUrl = "https://wttr.in/" + urlEncode(location) + "?lang=zh&format=j1";

  bool updated = false;
  if (!http.begin(apiUrl.c_str())) {
    Serial.println("[weather] 连接初始化失败");
    return false;
  }

  http.setTimeout(8000);                        // wttr.in 偶尔较慢，默认 5s 容易白跑
  http.setUserAgent("ESP32-OLED-Weather/1.0");  // 明确 UA，避免服务端按客户端类型变格式

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("[weather] JSON 解析失败: %s\n", err.c_str());
    } else {
      // 用 current_condition（实况）而不是 hourly 的 3 小时段：
      // 实况跟设备时区无关，也不会因为查询外地城市而取错时段，且更贴近"目前天气"。
      JsonArray ccArray = doc["current_condition"];
      if (!ccArray.isNull() && ccArray.size() > 0) {
        JsonObject cur = ccArray[0];

        // 提取天气状况（以天气码为主，避免 lang_zh 缺译文时残留英文）
        int weatherCode = cur["weatherCode"] | 0;

        String enDesc;
        JsonArray enDescArray = cur["weatherDesc"];
        if (!enDescArray.isNull() && enDescArray.size() > 0) {
          enDesc = enDescArray[0]["value"].as<String>();
        }

        String langData;
        JsonArray weatherDesc = cur["lang_zh"];
        if (!weatherDesc.isNull() && weatherDesc.size() > 0) {
          langData = weatherDesc[0]["value"].as<String>();
        }

        String zhDesc = translateWeatherDesc(weatherCode, langData, enDesc);
        if (!zhDesc.isEmpty()) {
          cachedWeatherDesc = zhDesc;
        }

        // 提取温度
        cachedTemperature = cur["temp_C"].as<String>() + "°C";

        // 提取湿度
        cachedHumidity = cur["humidity"].as<String>() + "%";

        // 提取体感温度
        cachedFeelsLike = cur["FeelsLikeC"].as<String>() + "°C";

        // 提取天文数据（同一次响应里就有，白拿）
        JsonArray weatherArray = doc["weather"];
        if (!weatherArray.isNull() && weatherArray.size() > 0) {
          JsonObject astro = weatherArray[0]["astronomy"][0];
          if (!astro.isNull()) {
            String sunrise = astro["sunrise"].as<String>();
            String sunset = astro["sunset"].as<String>();
            String moonPhase = astro["moon_phase"].as<String>();
            JsonVariant moonIllum = astro["moon_illumination"];

            if (!sunrise.isEmpty()) cachedSunrise = sunrise;
            if (!sunset.isEmpty()) cachedSunset = sunset;
            if (!moonPhase.isEmpty()) {
              cachedMoonPhase = moonPhase;
              String lower = moonPhase;
              lower.toLowerCase();
              cachedMoonWaxing = lower.indexOf("waxing") >= 0;
            }
            if (!moonIllum.isNull()) cachedMoonIllum = moonIllum.as<int>();
          }
        }

        updated = true;
      } else {
        Serial.println("[weather] 响应里没有 current_condition");
      }
    }
  } else {
    Serial.printf("[weather] 请求失败，HTTP %d\n", httpCode);
  }

  // 无论成功与否都必须释放连接，不能走 return 绕过
  http.end();
  return updated;
}

// 获取每日诗词
bool getDailyPoem() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String apiUrl = "https://v2.jinrishici.com/one.json";

  bool updated = false;
  if (!http.begin(apiUrl.c_str())) {
    Serial.println("[poem] 连接初始化失败");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);
  http.setUserAgent("ESP32-OLED-Weather/1.0");
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("[poem] JSON 解析失败: %s\n", err.c_str());
    } else {
      // 解析诗词数据
      JsonObject data = doc["data"];
      if (!data.isNull()) {
        String content = data["content"].as<String>();
        if (!content.isEmpty()) {
          cachedPoem = content;
          updated = true;
        }
      }
    }
  } else {
    Serial.printf("[poem] 请求失败，HTTP %d\n", httpCode);
  }

  http.end();
  return updated;
}

// ------------------------------开始-------------------------------
void setup(void) {
  Serial.begin(115200);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.clearBuffer(); 

  // 添加初始化提示
  u8g2.drawLine(0, 0, 127, 0); // 上方框线
  u8g2.drawLine(0, 0, 0, 64); // 左侧框线
  u8g2.drawLine(127, 0, 127, 64); // 右侧框线
  u8g2.drawLine(0, 63, 127, 63); // 下方框线
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(14, 24);
  u8g2.print("小方盒信息系统");
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(54, 37);
  u8g2.print("...");
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(26, 50);
  u8g2.print("Version 0.0.8");
  u8g2.sendBuffer();

  // 加载配置
  loadConfig();

  // 检查是否有WiFi配置
  if (ssid.length() > 0) {
    // 尝试连接WiFi
    Serial.println("尝试连接到WiFi: " + ssid);
    if (checkWiFiConnection()) {
      Serial.println("\nConnected to WiFi");
      Serial.println("IP地址: " + WiFi.localIP().toString());
      
      // 初始化RTC
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      
      // 首次统一获取所有天气数据
      getAllWeatherData();
      // 首次获取每日诗词
      getDailyPoem();
    } else {
      Serial.println("\nWiFi连接失败，进入配置模式");
      config_mode = true;
    }
  } else {
    Serial.println("未找到WiFi配置，进入配置模式");
    config_mode = true;
  }

  // 如果进入配置模式
  if (config_mode) {
    // 启动AP模式
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("\n配置模式已启动");
    Serial.println("AP名称: " + String(ap_ssid));
    Serial.println("AP密码: " + String(ap_password));
    Serial.println("AP IP地址: " + WiFi.softAPIP().toString());
    
    // 显示配置信息
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setCursor(10, 18);
    u8g2.print("配置模式");
    
    // 使用更小的字体显示三行信息
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.setCursor(10, 33);
    u8g2.print("AP：" + String(ap_ssid));
    
    u8g2.setCursor(10, 46);
    u8g2.print("PW：" + String(ap_password));
    
    u8g2.setCursor(10, 59);
    u8g2.print("IP：" + WiFi.softAPIP().toString());
    
    u8g2.sendBuffer();
    
    // 初始化Web服务器
    server.on("/", handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.begin();
    Serial.println("Web服务器已启动");
    Serial.println("请使用手机连接AP，访问 http://" + WiFi.softAPIP().toString() + " 进行配置");
  }

  // 初始化按钮引脚与消抖状态
  pinMode(buttonPin, INPUT_PULLUP);
  btnLastRead = (digitalRead(buttonPin) == LOW);
  btnStable = btnLastRead;
  btnLastChange = millis();
  lastScrollTime = millis();
  u8g2.clearBuffer();
}

// 新增函数：获取格式化日期字符串
String getFormattedDate(struct tm timeinfo) {
  char dateStringBuff[50];
  strftime(dateStringBuff, sizeof(dateStringBuff), "%Y-%m-%d", &timeinfo);
  return String(dateStringBuff);
}

// 新增函数：获取中文星期字符串
String getChineseWeekday(struct tm timeinfo) {
  const char* days[] = {"日", "一", "二", "三", "四", "五", "六"};
  return "星期" + String(days[timeinfo.tm_wday]);
}

// 定义上一次秒数，用于优化刷新频率
int lastSecond = -1;

// 绘制底部信息栏：左下湿度、中间页码指示、右下体感温度
void drawBottomInfo(int activePage) {
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);

  // 左下角湿度
  u8g2.setCursor(4, 60);
  u8g2.print(cachedHumidity.isEmpty() ? "--" : cachedHumidity);

  // 指示器（当前页为实心圆，其他为空心圆）
  for (int i = 0; i < pageCount; i++) {
    if (i == activePage) {
      u8g2.drawDisc(44 + i * 9, 56, 2); // 实心圆
    } else {
      u8g2.drawCircle(44 + i * 9, 56, 2); // 空心圆
    }
  }

  // 右下角体感温度：右对齐，避免 "-10°C" 这种长文本顶出屏幕右边界
  String feels = cachedFeelsLike.isEmpty() ? "--" : cachedFeelsLike;
  int feelsWidth = u8g2.getUTF8Width(feels.c_str());
  int feelsX = 123 - feelsWidth;
  if (feelsX < 84) feelsX = 84; // 不要压到页码指示器（最右圆点到 x=80）
  u8g2.setCursor(feelsX, 60);
  u8g2.print(feels);
}

// 绘制通用边框和分割线
void drawCommonBorder() {
  u8g2.drawLine(0, 0, 127, 0); // 上方框线
  u8g2.drawLine(0, 0, 0, 64); // 左侧框线
  u8g2.drawLine(127, 0, 127, 64); // 右侧框线
  u8g2.drawLine(0, 63, 127, 63); // 下方框线
  u8g2.drawLine(0, 20, 127, 20); // 上方分割线
  u8g2.drawLine(0, 48, 127, 48); // 下方分割线
}

// 页面绘制函数
void drawPage0(struct tm &timeinfo, const char* timeStringBuff) {
  // ------------------------第一页：显示日期时间----------------------------------
  // 显示日期
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(5, 15);
  u8g2.print(getFormattedDate(timeinfo));  
  // 显示星期
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(83, 16);
  u8g2.print(getChineseWeekday(timeinfo));
  // 日期时间分割线
  u8g2.drawLine(77, 0, 77, 20);
  // 显示时间
  u8g2.setFont(u8g2_font_courB18_tf);
  u8g2.setCursor(5, 42); 
  u8g2.print(timeStringBuff);
  // 绘制通用边框和分割线
  drawCommonBorder();
  // 绘制底部信息
  drawBottomInfo(0);
}

void drawPage1() {
  // ------------------------第二页：显示当前天气----------------------------------
  // 顶部标题：{地点}目前天气，左右动态居中
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  String title = String(location) + "目前天气";
  int titleWidth = u8g2.getUTF8Width(title.c_str());
  int titleX = (128 - titleWidth) / 2;
  u8g2.setCursor(titleX, 16); 
  u8g2.print(title);
  
  // 中间显示内容：天气状况|温度|湿度，根据屏幕宽度自动居中
  // 中文描述可能较长（如"零星雨夹雪"），超宽时自动降一档字体，避免左右溢出
  String weatherInfo = cachedWeatherDesc + "|" + cachedTemperature + "|" + cachedHumidity;
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  int weatherInfoWidth = u8g2.getUTF8Width(weatherInfo.c_str());
  if (weatherInfoWidth > 122) {
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    weatherInfoWidth = u8g2.getUTF8Width(weatherInfo.c_str());
  }
  int weatherInfoX = (128 - weatherInfoWidth) / 2;
  if (weatherInfoX < 1) weatherInfoX = 1;
  u8g2.setCursor(weatherInfoX, 40); 
  u8g2.print(weatherInfo);
  
  // 绘制通用边框和分割线
  drawCommonBorder();
  // 绘制底部信息
  drawBottomInfo(1);
}

// 只绘制诗词滚动区文字（供整屏重绘和局部刷新共用）
void drawPoemText() {
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  String poem = cachedPoem.isEmpty() ? "加载中..." : cachedPoem;
  int poemWidth = u8g2.getUTF8Width(poem.c_str());

  const int poemGap = 20;                              // 两段诗词之间的间隔
  int totalScrollWidth = poemWidth + poemGap;
  int currentX = 3 - (int)(scrollPosition % totalScrollWidth);

  // 主实例
  u8g2.setCursor(currentX, 40);
  u8g2.print(poem);

  // 循环实例，实现无缝衔接
  int nextX = currentX + totalScrollWidth;
  if (nextX < 125) {
    u8g2.setCursor(nextX, 40);
    u8g2.print(poem);
  }
}

// 局部刷新：只重发中间 3 行 tile（y=24~47），不重发整屏 1KB
void refreshPoemArea() {
  u8g2.clearBuffer();
  drawPoemText();
  u8g2.updateDisplayArea(0, 3, 16, 3); // tx, ty, tw, th（单位 8px tile）
}

void drawPage2() {
  // ------------------------第三页：每日诗词----------------------------------
  // 顶部标题：[心形图标]每日诗词
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  String title = "每日诗词";
  int titleWidth = u8g2.getUTF8Width(title.c_str());
  int iconWidth = 14; // 心形图标宽度
  int gap = 0; // 图标与标题间距，改为2像素
  int totalWidth = titleWidth + iconWidth + gap; // 标题+图标+间距
  int titleX = (128 - totalWidth) / 2;
  
  // 绘制心形图标
  u8g2.setFont(u8g2_font_open_iconic_human_1x_t);
  u8g2.drawGlyph(titleX, 14, 0x0042); // 心形图标，编码0x0042
  
  // 绘制标题
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(titleX + iconWidth + gap, 16); 
  u8g2.print(title);
  
  // 中间内容：滚动显示每日诗词
  drawPoemText();

  // 绘制通用边框和分割线
  drawCommonBorder();
  // 绘制底部信息
  drawBottomInfo(2);
}

void drawPage3() {
  //--------------------------第四页：显示IP地址----------------------------------
  u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
  u8g2.drawGlyph(14, 14, 0x0048);
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(28, 16); 
  u8g2.print("设备网络地址");
  // 显示IP地址
  u8g2.setFont(u8g2_font_courB10_tf);
  u8g2.setCursor(4, 40); 
  u8g2.print(WiFi.localIP().toString());
  // 绘制通用边框和分割线
  drawCommonBorder();
  // 绘制底部信息
  drawBottomInfo(3);
}

// ------------------------第五页：日月星辰----------------------------------
void drawPage4(struct tm &timeinfo) {
  int nowMin = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int riseMin = clockToMinutes(cachedSunrise);
  int setMin = clockToMinutes(cachedSunset);

  // 标题
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(4, 15);
  u8g2.print("日月星辰");

  // 右上角：距最近一次日出/日落的倒计时（上箭头=等日出，下箭头=等日落）
  if (riseMin >= 0 && setMin >= 0) {
    bool isDay = (nowMin >= riseMin && nowMin <= setMin);
    int delta;
    if (isDay) {
      delta = setMin - nowMin;
    } else if (nowMin > setMin) {
      delta = 1440 - nowMin + riseMin; // 跨午夜
    } else {
      delta = riseMin - nowMin;
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%dh%02dm", delta / 60, delta % 60);

    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    int cdWidth = u8g2.getUTF8Width(buf);
    int cdX = 124 - cdWidth;
    if (cdX < 72) cdX = 72; // 不要压到左侧标题

    // 小箭头：向上=日出倒计时，向下=日落倒计时
    int arrowX = cdX - 7;
    if (isDay) {
      u8g2.drawTriangle(arrowX + 2, 12, arrowX, 6, arrowX + 4, 6);
    } else {
      u8g2.drawTriangle(arrowX + 2, 6, arrowX, 12, arrowX + 4, 12);
    }

    u8g2.setCursor(cdX, 15);
    u8g2.print(buf);
  }

  // 左侧：真实盈亏的月相图
  if (cachedMoonIllum >= 0) {
    drawMoonPhase(26, 35, 12, cachedMoonIllum, cachedMoonWaxing);
  } else {
    u8g2.drawCircle(26, 35, 12);
  }

  // 右侧：月相中文名 + 亮度
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  String phaseZh = moonPhaseToZh(cachedMoonPhase);
  u8g2.setCursor(46, 32);
  u8g2.print(phaseZh.isEmpty() ? "月相 --" : phaseZh);

  if (cachedMoonIllum >= 0) {
    u8g2.setCursor(46, 45);
    u8g2.print(String("亮 ") + cachedMoonIllum + "%");
  }

  // 底部：日出 / 日落时刻（统一成 24 小时制）
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(2, 61);
  u8g2.print(String("日出 ") + (cachedSunrise.isEmpty() ? "--" : clockTo24(cachedSunrise)));

  String sunsetText = String("日落 ") + (cachedSunset.isEmpty() ? "--" : clockTo24(cachedSunset));
  int sunsetWidth = u8g2.getUTF8Width(sunsetText.c_str());
  u8g2.setCursor(124 - sunsetWidth, 61);
  u8g2.print(sunsetText);
}

// ------------------------------执行-------------------------------

// 整屏重绘（所有页面共用）
void renderFullScreen(struct tm &timeinfo) {
  char timeStringBuff[50];
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);

  u8g2.clearBuffer();
  switch (currentPage) {
    case 0: drawPage0(timeinfo, timeStringBuff); break;
    case 1: drawPage1(); break;
    case 2: drawPage2(); break;
    case 3: drawPage3(); break;
    case 4: drawPage4(timeinfo); break;
  }
  u8g2.sendBuffer();
}

// 等待 NTP 授时的提示画面：告诉用户「在干嘛」，而不是停在上电画面不动
void drawSyncScreen() {
  static int dots = 0;

  u8g2.clearBuffer();
  drawCommonBorder();

  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  String tip = "正在同步时间";
  for (int i = 0; i < (dots % 4); i++) {
    tip += ".";
  }
  int tipWidth = u8g2.getUTF8Width(tip.c_str());
  u8g2.setCursor((128 - tipWidth) / 2, 38);
  u8g2.print(tip);

  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  String net = (WiFi.status() == WL_CONNECTED) ? "网络已连接" : "网络未连接";
  int netWidth = u8g2.getUTF8Width(net.c_str());
  u8g2.setCursor((128 - netWidth) / 2, 59);
  u8g2.print(net);

  u8g2.sendBuffer();
  dots++;
}

// 长按进配置模式：只清 WiFi，保留城市等其它设置（避免误触后连城市都要重配）
void enterConfigMode() {
  u8g2.clearBuffer();
  drawCommonBorder();
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(12, 37);
  u8g2.print("清除 WiFi 配置");
  u8g2.setCursor(28, 52);
  u8g2.print("重新配置中...");
  u8g2.sendBuffer();

  preferences.begin("oled_weather", false);
  preferences.remove("ssid");
  preferences.remove("password");
  preferences.end();

  Serial.println("[ui] 已清除 WiFi 配置，重启进入配置模式");
  delay(600);
  ESP.restart();
}

// 按钮轮询：30ms 消抖；短按切页，长按 3 秒进配置
void pollButton() {
  bool raw = (digitalRead(buttonPin) == LOW);
  unsigned long now = millis();

  if (raw != btnLastRead) {
    btnLastRead = raw;
    btnLastChange = now;
    return; // 电平还在跳，等稳定
  }
  if (now - btnLastChange < debounceMs) {
    return; // 稳定时间不够，先不算数
  }
  if (raw == btnStable) {
    return; // 状态没变化
  }

  btnStable = raw;
  if (btnStable) {
    btnPressStart = now;
    btnLongPressFired = false;
    return;
  }

  // 释放：没触发过长按且按住不足 3 秒，算一次短按
  if (!btnLongPressFired && now - btnPressStart < longPressDuration) {
    currentPage = (currentPage + 1) % pageCount;
    scrollPosition = 0;
    lastScrollTime = millis();
    needsFullRedraw = true;
    Serial.printf("[ui] 切换到第 %d 页\n", currentPage + 1);
  }
}

// 长按检测：与消抖解耦，按住期间持续判断
void checkLongPress() {
  if (btnStable && !btnLongPressFired && millis() - btnPressStart >= longPressDuration) {
    btnLongPressFired = true;
    Serial.println("[ui] 长按触发，进入配置模式");
    enterConfigMode();
  }
}

void loop(void) {
  // 配置模式：只处理 Web 请求
  if (config_mode) {
    server.handleClient();
    delay(100);
    return;
  }

  // WiFi 掉线自动重连；刚恢复时把下次刷新提前到 5 秒后（留时间给 DHCP）
  if (ensureWiFi()) {
    previousMillis = millis() - interval + 5000;
  }

  // 定时拉数据，与当前停在哪个页面无关
  if (millis() - previousMillis >= interval) {
    previousMillis = millis();
    getAllWeatherData();
    getDailyPoem();
    needsFullRedraw = true;
  }

  pollButton();
  checkLongPress();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // 还没对上时间：给提示画面，按键逻辑照常运行
    static unsigned long lastSyncDraw = 0;
    if (millis() - lastSyncDraw >= 500) {
      lastSyncDraw = millis();
      drawSyncScreen();
    }
    delay(50);
    return;
  }

  // 诗词页走局部刷新（80ms 一帧），比每秒整屏重绘流畅得多
  if (currentPage == 2 && !needsFullRedraw) {
    unsigned long now = millis();
    if (now - lastScrollTime >= scrollInterval) {
      unsigned long steps = (now - lastScrollTime) / scrollInterval;
      scrollPosition += steps;
      lastScrollTime += steps * scrollInterval;
      refreshPoemArea();
    }
    delay(20);
    return;
  }

  // 其它页面：切页或秒变化时整屏重绘
  if (needsFullRedraw || timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;
    needsFullRedraw = false;
    renderFullScreen(timeinfo);
    return;
  }

  delay(20);
}