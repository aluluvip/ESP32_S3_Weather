# OLED 天气显示项目

本项目是一个基于 `果云 ESP32-S3-DevKitC-1` 开发板的天气显示系统，通过连接 Wi-Fi 获取指定城市的实时天气信息，并在`SSD1306 OLED`屏幕上显示。

## 功能特点
- **实时时间显示**：通过 NTP 服务器获取当前时间，并在 OLED 屏幕上显示日期和时间。
- **天气信息获取**：通过 `wttr.in` API 获取指定城市的实时天气状况、温度、湿度和体感温度。
- **多页面显示**：OLED 屏幕支持多页面切换，分别显示：
  - 第1页：日期、时间、湿度和温度
  - 第2页：当前气温显示
  - 第3页：当前天气状况
  - 第4页：设备 IP 地址
- **按钮切换**：支持通过按钮手动切换页面。

## 硬件要求
- **开发板**：ESP32-S3-DevKitC-1
- **OLED 屏幕**：SSD1306 128x64 I2C
- **按钮**：1个（连接到 GPIO 5）

## 软件依赖
- **PlatformIO**：用于项目构建和上传。
- **Arduino 框架**：提供开发板的基础支持。
- **库依赖**：
  - `olikraus/U8g2 @ ^2.36.5`：用于 OLED 屏幕显示。
  - `bblanchon/ArduinoJson@^7.3.1`：用于解析 JSON 数据。
  - `nickgammon/Regexp@^0.1.0`：正则表达式库。

## 配置步骤
1. **修改 Wi-Fi 信息**：在 `src/main.cpp` 文件中，将 `ssid` 和 `password` 替换为你的 Wi-Fi 网络信息。
```cpp
// 替换为你的网络信息
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";
```
2. **修改查询地点**：在 `src/main.cpp` 文件中，将 `location` 替换为你要查询的城市名称。
```cpp
// 要查询的地点
const char* location = "your_city";
```
3. **编译和上传**：使用 PlatformIO 编译并上传项目到开发板。

## 运行说明
- 开发板上电后，会自动连接到 Wi-Fi 网络，并获取当前时间和天气信息。
- OLED 屏幕默认显示第1页（日期时间页），显示时间、湿度和温度。
- 可以通过按钮手动切换页面，每次按下按钮切换到下一个页面，循环显示。
- 天气信息（温度、湿度、体感温度、天气状况）每 10 分钟自动更新一次。
- 时间显示优化：只有秒数变化时才刷新屏幕，减少掉帧。

## 注意事项
- 请确保开发板能够正常连接到 Wi-Fi 网络，否则无法获取时间和天气信息。
- 由于 wttr.in API 可能存在访问限制，代码已优化为每次只发送1次 API 请求，获取所有天气数据。
- 建议使用稳定的 Wi-Fi 网络，以确保时间同步和天气数据更新正常。

## 代码优化
- **API 调用优化**：将多次 API 调用合并为1次，减少网络请求次数。
- **屏幕刷新优化**：只有秒数变化时才刷新屏幕，减少掉帧。
- **代码结构优化**：提取公共绘制函数，减少代码重复。
- **内存优化**：使用缓存机制减少计算开销。

## 参考文档
- [PlatformIO 项目配置文档](https://docs.platformio.org/en/latest/projectconf/index.html)
- [ESP32-S3 开发板文档](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html)
- [U8g2 库文档](https://github.com/olikraus/u8g2/wiki)

