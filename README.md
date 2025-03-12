# OLED 天气显示项目

本项目是一个基于 `果云 ESP32-S3-DevKitC-1` 开发板的天气显示系统，通过连接 Wi-Fi 获取指定城市的实时天气信息，并在`SSD1306 OLED`屏幕上显示。

## 功能特点
- **实时时间显示**：通过 NTP 服务器获取当前时间，并在 OLED 屏幕上显示日期和时间。
- **天气信息获取**：通过 `wttr.in` API 获取指定城市的实时天气状况和温度信息。
- **多页面显示**：OLED 屏幕支持多页面切换，分别显示日期时间、气温、天气状况和设备 IP 地址。

## 硬件要求
- **开发板**：ESP32-S3-DevKitC-1
- **OLED 屏幕**：SSD1306 128x64 I2C

## 软件依赖
- **PlatformIO**：用于项目构建和上传。
- **Arduino 框架**：提供开发板的基础支持。
- **库依赖**：
  - `olikraus/U8g2 @ ^2.32.15`：用于 OLED 屏幕显示。
  - `bblanchon/ArduinoJson@^7.3.1`：用于解析 JSON 数据。
  - `nickgammon/Regexp@^0.1.0`：正则表达式库。

## 配置步骤
1. **修改 Wi-Fi 信息**：在 `src/main.cpp` 文件中，将 `ssid` 和 `password` 替换为你的 Wi-Fi 网络信息。
```cpp:/Users/sagit./Documents/PlatformIO/Projects/new_oled/src/main.cpp
// 替换为你的网络信息
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";
```
2. **修改查询地点**：在 `src/main.cpp` 文件中，将 `location` 替换为你要查询的城市名称。
```
// 要查询的地点
const char* location = "your_city";
```
3. **编译和上传**：使用 PlatformIO 编译并上传项目到开发板。


## 运行说明
- 开发板上电后，会自动连接到 Wi-Fi 网络，并获取当前时间和天气信息。
- OLED 屏幕会依次显示日期时间、气温、天气状况和设备 IP 地址，每个页面显示 1 秒后切换到下一个页面。
- 天气信息每 10 分钟更新一次。

## 注意事项
- 请确保开发板能够正常连接到 Wi-Fi 网络，否则无法获取时间和天气信息。
- 由于 wttr.in API 可能存在访问限制，请避免频繁请求，以免影响使用。

## 参考文档
- [PlatformIO 项目配置文档](https://docs.platformio.org/en/latest/projectconf/index.html)
- [GCC 头文件使用文档](https://docs.platformio.org/en/latest/projectconf/index.html)
- [PlatformIO 单元测试文档](https://docs.platformio.org/en/latest/projectconf/index.html)

