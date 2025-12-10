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
const char* ntpServer = "pool.ntp.org";
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

// 定义更新天气气温和状况的时间间隔（10分钟，单位：毫秒）
const unsigned long interval = 10 * 60 * 1000;
unsigned long previousMillis = 0;

// 定义按钮引脚
const int buttonPin = 5;
// 定义当前页面
int currentPage = 0;

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

// 获取当前小时对应的天气数据索引
int getCurrentHourIndex() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  
  // 计算当前小时对应的索引（每3小时一个数据点，共8个数据点）
  int hour = timeinfo.tm_hour;
  // 0:00 -> 0, 3:00 -> 1, 6:00 -> 2, ..., 21:00 -> 7
  int index = hour / 3;
  return index;
}



// 统一获取所有天气数据，减少API调用次数
bool getAllWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String apiUrl = "https://wttr.in/" + String(location) + "?lang=zh&format=j1";
    http.begin(apiUrl.c_str());
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();

      JsonDocument doc;
      deserializeJson(doc, payload);

      JsonArray weatherArray = doc["weather"];
      if (!weatherArray.isNull() && weatherArray.size() > 0) {
        JsonArray hourlyData = weatherArray[0]["hourly"];
        if (!hourlyData.isNull() && hourlyData.size() > 0) {
          // 获取当前小时对应的索引
          int index = getCurrentHourIndex();
          // 确保索引不超出范围
          if (index < 0 || index >= hourlyData.size()) {
            index = 0;
          }
          
          JsonObject hourData = hourlyData[index];
          
          // 提取天气状况
          JsonArray weatherDesc = hourData["lang_zh"];
          if (!weatherDesc.isNull() && weatherDesc.size() > 0) {
            String langData = weatherDesc[0]["value"].as<String>();
            if (!langData.isEmpty()) {
              cachedWeatherDesc = langData;
            }
          }
          
          // 提取温度
          String tempC = hourData["tempC"].as<String>();
          cachedTemperature = tempC + "°C";
          
          // 提取湿度
          String humidity = hourData["humidity"].as<String>();
          cachedHumidity = humidity + "%";
          
          // 提取体感温度
          String feelsLike = hourData["FeelsLikeC"].as<String>();
          cachedFeelsLike = feelsLike + "°C";
          
          return true;
        }
      }
    }
    http.end();
  }
  return false;
}

// 获取当前小时的天气状况
String getCurrentWeatherCondition() {
  return cachedWeatherDesc.isEmpty() ? "未知" : cachedWeatherDesc;
}

// 获取当前小时的温度
String getCurrentTemperature() {
  return cachedTemperature.isEmpty() ? "未知" : cachedTemperature;
}

// 获取当前小时的湿度
String getCurrentHumidity() {
  return cachedHumidity.isEmpty() ? "未知" : cachedHumidity;
}

// 获取当前小时的体感温度
String getCurrentFeelsLike() {
  return cachedFeelsLike.isEmpty() ? "未知" : cachedFeelsLike;
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
  u8g2.print("天气与时钟系统");
  u8g2.setFont(u8g2_font_wqy14_t_gb2312);
  u8g2.setCursor(54, 37);
  u8g2.print("...");
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(26, 50);
  u8g2.print("Version 0.0.5");
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

  // 初始化按钮引脚
  pinMode(buttonPin, INPUT_PULLUP);
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

// 按钮长按相关变量
unsigned long buttonPressStartTime = 0;
const unsigned long longPressDuration = 3000; // 长按3秒进入配置模式
bool buttonPressed = false;

// 绘制底部信息栏，包括湿度、指示器和温度
void drawBottomInfo(int activePage) {
  // 左下角湿度
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(4, 60);
  u8g2.print(cachedHumidity);
  
  // 指示器（当前页为实心圆，其他为空心圆）
  for (int i = 0; i < 4; i++) {
    if (i == activePage) {
      u8g2.drawDisc(48 + i * 10, 56, 2); // 实心圆
    } else {
      u8g2.drawCircle(48 + i * 10, 56, 2); // 空心圆
    }
  }
  
  // 右下角天气温度
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2.setCursor(100, 60);
  u8g2.print(cachedTemperature);
}

// 绘制通用边框和分割线
void drawCommonBorder() {
  u8g2.drawLine(0, 0, 124, 0); // 上方框线
  u8g2.drawLine(0, 0, 0, 64); // 左侧框线
  u8g2.drawLine(124, 0, 124, 64); // 右侧框线
  u8g2.drawLine(0, 63, 124, 63); // 下方框线
  u8g2.drawLine(0, 20, 124, 20); // 上方分割线
  u8g2.drawLine(0, 48, 124, 48); // 下方分割线
}

// ------------------------------执行-------------------------------
void loop(void) {
  // 如果在配置模式，处理Web请求
  if (config_mode) {
    server.handleClient();
    delay(100);
    return;
  }

  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    delay(100);
    return;
  }

  // 检测按钮状态（独立于秒数变化，实时检测）
  if (digitalRead(buttonPin) == LOW) {
    // 按钮按下
    if (!buttonPressed) {
      buttonPressed = true;
      buttonPressStartTime = millis();
    }
    
    // 检查是否长按超过3秒
    if (millis() - buttonPressStartTime >= longPressDuration) {
      // 进入配置模式
      Serial.println("长按按钮，进入配置模式");
      // 清除WiFi配置，让设备重启后进入配置模式
      preferences.begin("oled_weather", false);
      preferences.clear();
      preferences.end();
      // 重启设备，进入配置模式
      ESP.restart();
    }
  } else {
    // 按钮释放
    if (buttonPressed) {
      buttonPressed = false;
      // 检查是否是短按（小于3秒）
      if (millis() - buttonPressStartTime < longPressDuration) {
        // 切换页面
        currentPage = (currentPage + 1) % 4;
        
        // 立即重新绘制屏幕，响应页面切换
        char timeStringBuff[50];
        // 格式化时间
        strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
        
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
              // 绘制通用边框和分割线
              drawCommonBorder();
              // 绘制底部信息
              drawBottomInfo(0);
              break;
            case 1:
            {
              // ------------------------第二页：显示当前天气----------------------------------
              // 顶部标题：{地点}目前天气，左右动态居中
              u8g2.setFont(u8g2_font_wqy14_t_gb2312);
              String title = String(location) + "目前天气";
              int titleWidth = u8g2.getUTF8Width(title.c_str());
              int titleX = (128 - titleWidth) / 2;
              u8g2.setCursor(titleX, 16); 
              u8g2.print(title);
              
              // 中间显示内容：天气状况|温度|湿度，根据屏幕宽度自动居中
              u8g2.setFont(u8g2_font_wqy12_t_gb2312);
              String weatherInfo = cachedWeatherDesc + "|" + cachedTemperature + "|" + cachedHumidity;
              int weatherInfoWidth = u8g2.getUTF8Width(weatherInfo.c_str());
              int weatherInfoX = (128 - weatherInfoWidth) / 2;
              u8g2.setCursor(weatherInfoX, 40); 
              u8g2.print(weatherInfo);
              
              // 绘制通用边框和分割线
              drawCommonBorder();
              // 绘制底部信息
              drawBottomInfo(1);
              break;
            }
            case 2:
            {
              // ------------------------第三页：规划中----------------------------------
              // 顶部标题：规划中...
              u8g2.setFont(u8g2_font_wqy14_t_gb2312);
              String placeholder = "规划中...";
              int placeholderWidth = u8g2.getUTF8Width(placeholder.c_str());
              int placeholderX = (128 - placeholderWidth) / 2;
              u8g2.setCursor(placeholderX, 16); 
              u8g2.print(placeholder);
              
              // 中间内容：规划中...
              u8g2.setFont(u8g2_font_wqy16_t_gb2312);
              u8g2.setCursor(placeholderX, 40); 
              u8g2.print(placeholder);
              
              // 绘制通用边框和分割线
              drawCommonBorder();
              // 绘制底部信息
              drawBottomInfo(2);
              break;
            }
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
              // 绘制通用边框和分割线
              drawCommonBorder();
              // 绘制底部信息
              drawBottomInfo(3);
              break;
          }
        } while (u8g2.nextPage());
      }
    }
  }

  // 只有当秒数变化时才重新绘制屏幕，减少掉帧
  if (timeinfo.tm_sec != lastSecond) {
    lastSecond = timeinfo.tm_sec;

    char timeStringBuff[50];
    char timeStringWithoutSec[50];

    // 格式化时间
    strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo);
    // 格式化时间（不显示秒）
    strftime(timeStringWithoutSec, sizeof(timeStringWithoutSec), "%H:%M", &timeinfo);

    // 获取当前时间
    unsigned long currentMillis = millis();

    // 每10分钟统一更新一次所有天气数据
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      getAllWeatherData();
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
          // 绘制通用边框和分割线
          drawCommonBorder();
          // 绘制底部信息
          drawBottomInfo(0);
          break;
        case 1:
        {
          // ------------------------第二页：显示当前天气----------------------------------
          // 顶部标题：{地点}目前天气，左右动态居中
          u8g2.setFont(u8g2_font_wqy14_t_gb2312);
          String title = String(location) + "目前天气";
          int titleWidth = u8g2.getUTF8Width(title.c_str());
          int titleX = (128 - titleWidth) / 2;
          u8g2.setCursor(titleX, 16); 
          u8g2.print(title);
          
          // 中间显示内容：天气状况|温度|湿度，根据屏幕宽度自动居中
          u8g2.setFont(u8g2_font_wqy12_t_gb2312);
          String weatherInfo = cachedWeatherDesc + "|" + cachedTemperature + "|" + cachedHumidity;
          int weatherInfoWidth = u8g2.getUTF8Width(weatherInfo.c_str());
          int weatherInfoX = (128 - weatherInfoWidth) / 2;
          u8g2.setCursor(weatherInfoX, 40); 
          u8g2.print(weatherInfo);
          
          // 绘制通用边框和分割线
          drawCommonBorder();
          // 绘制底部信息
          drawBottomInfo(1);
          break;
        }
        case 2:
            {
              // ------------------------第三页：规划中----------------------------------
              // 顶部标题：规划中...
              u8g2.setFont(u8g2_font_wqy14_t_gb2312);
              String placeholder = "规划中...";
              int placeholderWidth = u8g2.getUTF8Width(placeholder.c_str());
              int placeholderX = (128 - placeholderWidth) / 2;
              u8g2.setCursor(placeholderX, 16); 
              u8g2.print(placeholder);
              
              // 中间内容：规划中...
              u8g2.setFont(u8g2_font_wqy16_t_gb2312);
              u8g2.setCursor(placeholderX, 40); 
              u8g2.print(placeholder);
              
              // 绘制通用边框和分割线
              drawCommonBorder();
              // 绘制底部信息
              drawBottomInfo(2);
              break;
            }
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
          // 绘制通用边框和分割线
          drawCommonBorder();
          // 绘制底部信息
          drawBottomInfo(3);
          break;
      }
    } while (u8g2.nextPage());
  }
  
  // 短延迟，减少CPU占用
  delay(50); 
}