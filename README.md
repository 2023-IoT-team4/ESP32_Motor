# ESP32_Motor
Motor 관련 기능을 구현한 ESP32 보드 코드입니다.


해당 보드의 주 목적은 반려동물에게 규칙적이고 정량적인 사료를 급식하는 것입니다.


이를 위해 AWS_IOT와 MQTT 통신을 진행하며, Dual Core를 사용하고 있습니다.


### 사용되는 라이브러리
```
AWS_IOT
Arduino_JSON
ESP32_Servo
```

### 새롭게 Define 해야하는 변수
```
// Wifi
const char* ssid = "Your WiFi ID";
const char* password = "Your WiFi Password";

// AWS IoT
char HOST_ADDRESS[] = "Your AWS IoT End point";
```
