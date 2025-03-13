#include <Arduino.h>
#include <U8g2lib.h>
#include <WiFi.h>
#include <time.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

// SSD1306 OLED (I2C)
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 42, /* data=*/ 41, /* reset=*/ U8X8_PIN_NONE);

// 替换为你的网络信息
const char* ssid = "lulu";
const char* password = "19941024";

// 定义时区信息
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 28800; // 北京时间，东八区，偏移秒数
const int   daylightOffset_sec = 0;

// 要查询的地点
const char* location = "北京";  // 可替换为你要查询的城市

// 定义缓存变量
String cachedWeatherDesc;
String cachedTemperature;

// 定义更新天气气温和状况的时间间隔（10分钟，单位：毫秒）
const unsigned long interval = 10 * 60 * 1000;
unsigned long previousMillis = 0;

// 定义按钮引脚
const int buttonPin = 5;
// 定义当前页面
int currentPage = 0;

// 获取当前天气状况（中文）
String getCurrentWeatherCondition() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String apiUrl = "https://wttr.in/" + String(location) + "?lang=zh&format=j2";
    http.begin(apiUrl.c_str());
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();

      JsonDocument doc;
      deserializeJson(doc, payload);

      JsonArray currentCondition = doc["current_condition"];
      if (!currentCondition.isNull() && currentCondition.size() > 0) {
        JsonArray weatherDesc = currentCondition[0]["lang_zh"];
        if (!weatherDesc.isNull() && weatherDesc.size() > 0)  {
          String langData = weatherDesc[0]["value"].as<String>();
          if (!langData.isEmpty()) {
            cachedWeatherDesc = langData;
            return cachedWeatherDesc;  
          }
        }
      }
    }
    http.end();
  }
  return "未知";
}

// 获取当前温度
String getCurrentTemperature() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String apiUrl = "https://wttr.in/" + String(location) + "?format=j2";
    http.begin(apiUrl.c_str());
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();

      JsonDocument doc;
      deserializeJson(doc, payload);

      JsonArray currentCondition = doc["current_condition"];
      if (!currentCondition.isNull() && currentCondition.size() > 0) {
        String tempC = currentCondition[0]["temp_C"].as<String>();
        cachedTemperature = tempC + "°C";
        return cachedTemperature;
      }
    }
    http.end();
  }
  return "未知";
}

// ------------------------------开始-------------------------------
void setup(void) {
  Serial.begin(9600);
  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.clearBuffer(); 

  // 添加初始化提示
  u8g2.drawLine(0, 0, 124, 0); // 上方框线
  u8g2.drawLine(0, 0, 0, 64); // 左侧框线
  u8g2.drawLine(124, 0, 124, 64); // 右侧框线
  u8g2.drawLine(0, 63, 124, 63); // 下方框线
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(14, 24);
  u8g2.print("正在初始化系统");
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(54, 37);
  u8g2.print("...");
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(26, 50);
  u8g2.print("Version 0.0.3");
  u8g2.sendBuffer();

  // 连接到WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // 初始化RTC
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // 首次获取温度和天气状况
  cachedTemperature = getCurrentTemperature();
  cachedWeatherDesc = getCurrentWeatherCondition();

  // 初始化按钮引脚
  pinMode(buttonPin, INPUT_PULLUP);
  u8g2.clearBuffer(); 
}
// ------------------------------执行-------------------------------

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

// 修改原代码
void loop(void) {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }

  // 使用新函数组合日期和星期
  String dateStr = getFormattedDate(timeinfo) + " " + getChineseWeekday(timeinfo);

  char timeStringBuff[50];
  char timeStringWithoutSec[50];

  // 格式化时间
  strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
  // 格式化时间（不显示秒）
  strftime(timeStringWithoutSec, sizeof(timeStringWithoutSec), "%H:%M", &timeinfo);

  // 获取当前时间
  unsigned long currentMillis = millis();

  // 每10分钟更新一次温度和天气状况
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    cachedTemperature = getCurrentTemperature();
    cachedWeatherDesc = getCurrentWeatherCondition();
  }

  // 检测按钮状态
  if (digitalRead(buttonPin) == LOW) {
    // 消抖处理
    delay(20);
    if (digitalRead(buttonPin) == LOW) {
      // 切换页面
      currentPage = (currentPage + 1) % 4;
      // 等待按钮释放
      while (digitalRead(buttonPin) == LOW);
    }
  }

  u8g2.clearBuffer(); 
  u8g2.firstPage();
  do {
    switch (currentPage) {
      case 0:
        // ------------------------第一页：显示日期时间----------------------------------
        // 显示日期
        u8g2.setFont(u8g2_font_wqy14_t_gb2312);
        u8g2.setCursor(5, 15);
        u8g2.print(getFormattedDate(timeinfo));  
        // 显示星期
        u8g2.setFont(u8g2_font_wqy14_t_gb2312);
        u8g2.setCursor(81, 16);
        u8g2.print(getChineseWeekday(timeinfo));
        // 日期时间分割线
        u8g2.drawLine(77, 0, 77, 20);
        // 显示时间
        u8g2.setFont(u8g2_font_courB18_tf);
        u8g2.setCursor(3, 42); 
        u8g2.print(timeStringBuff);
        // 底部栏信息
        u8g2.drawLine(0, 20, 124, 20); // 上方分割线
        u8g2.drawLine(0, 48, 124, 48); // 下方分割线
        u8g2.drawLine(0, 0, 124, 0); // 上方框线
        u8g2.drawLine(0, 0, 0, 64); // 左侧框线
        u8g2.drawLine(124, 0, 124, 64); // 右侧框线
        u8g2.drawLine(0, 63, 124, 63); // 下方框线
        // 左下角时间
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(4, 60);
        u8g2.print(timeStringWithoutSec);
        // 指示器
        u8g2.drawDisc(48, 56, 2); // 绘制一个半径为2的实心圆形，圆心坐标为(X50,Y58)
        u8g2.drawCircle(58, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawCircle(68, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawCircle(78, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        // 右下角天气温度
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(100, 60);
        u8g2.print(cachedTemperature);
        break;
      case 1:
        // ------------------------第二页：显示气温----------------------------------
        u8g2.setFont(u8g2_font_wqy14_t_gb2312);
        u8g2.setCursor(7, 16); 
        u8g2.print(String(location)+"今日当前气温");
        // 天气气温
        u8g2.setFont(u8g2_font_courB18_tf);
        u8g2.setCursor(36, 42); 
        u8g2.print(cachedTemperature);
         // 底部栏信息
         u8g2.drawLine(0, 20, 124, 20); // 上方分割线
         u8g2.drawLine(0, 48, 124, 48); // 下方分割线
         u8g2.drawLine(0, 0, 124, 0); // 上方框线
         u8g2.drawLine(0, 0, 0, 64); // 左侧框线
         u8g2.drawLine(124, 0, 124, 64); // 右侧框线
         u8g2.drawLine(0, 63, 124, 63); // 下方框线
        // 指示器
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(4, 60);
        u8g2.print(timeStringWithoutSec);
        u8g2.drawCircle(48, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawDisc(58, 56, 2); // 绘制一个半径为2的实心圆形，圆心坐标为(X50,Y58)
        u8g2.drawCircle(68, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawCircle(78, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        // 右下角天气温度
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(100, 60);
        u8g2.print(cachedTemperature);
        break;
      case 2:
        // ------------------------第三页：显示天气状态----------------------------------
        // 标题
        u8g2.setFont(u8g2_font_wqy14_t_gb2312);
        u8g2.setCursor(7, 16); 
        u8g2.print(String(location)+"今日天气状况");
        // 天气状况
        u8g2.setFont(u8g2_font_wqy16_t_gb2312);
        u8g2.setCursor(48, 40); 
        u8g2.print(cachedWeatherDesc);
        // 底部栏信息
        u8g2.drawLine(0, 20, 124, 20); // 上方分割线
        u8g2.drawLine(0, 48, 124, 48); // 下方分割线
        u8g2.drawLine(0, 0, 124, 0); // 上方框线
        u8g2.drawLine(0, 0, 0, 64); // 左侧框线
        u8g2.drawLine(124, 0, 124, 64); // 右侧框线
        u8g2.drawLine(0, 63, 124, 63); // 下方框线
        // 指示器
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(4, 60);
        u8g2.print(timeStringWithoutSec);
        
        u8g2.drawCircle(48, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawCircle(58, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawDisc(68, 56, 2); // 绘制一个半径为2的实心圆形，圆心坐标为(X50,Y58)
        u8g2.drawCircle(78, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        // 右下角天气温度
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(100, 60);
        u8g2.print(cachedTemperature);
        break;
      case 3:
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
        // 底部栏信息
        u8g2.drawLine(0, 20, 124, 20); // 上方分割线
        u8g2.drawLine(0, 48, 124, 48); // 下方分割线
        u8g2.drawLine(0, 0, 124, 0); // 上方框线
        u8g2.drawLine(0, 0, 0, 64); // 左侧框线
        u8g2.drawLine(124, 0, 124, 64); // 右侧框线
        u8g2.drawLine(0, 63, 124, 63); // 下方框线
        // 指示器
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(4, 60);
        u8g2.print(timeStringWithoutSec);
        u8g2.drawCircle(48, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawCircle(58, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawCircle(68, 56, 2);  // 绘制一个半径为2的圆形，圆心坐标为(X60,Y58)
        u8g2.drawDisc(78, 56, 2); // 绘制一个半径为2的实心圆形，圆心坐标为(X50,Y58)
        // 右下角天气温度
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.setCursor(100, 60);
        u8g2.print(cachedTemperature);
        break;
    }
  } while (u8g2.nextPage());
  delay(100); 
}