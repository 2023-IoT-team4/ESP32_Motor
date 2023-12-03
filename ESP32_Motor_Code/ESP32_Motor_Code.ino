/*
    반려동물 스마트 홈 프로젝트에서 Motor를 다루는 ESP32 보드 코드
*/
#include <AWS_IOT.h>
#include <WiFi.h>
#include <Arduino_JSON.h>
#include <ESP32_Servo.h>

/***********************************************************************************
// Wifi
const char* ssid = "Your WiFi ID";
const char* password = "Your WiFi Password";

// AWS IoT
char HOST_ADDRESS[] = "Your AWS IoT End point";
***********************************************************************************/

// AWS_IOT
AWS_IOT motor;
char CLIENT_ID[] = "ESP32_Motor";
char sTOPIC_NAME[] = "$aws/things/esp_motor/shadow/update";  // subscribe topic name
char pTOPIC_NAME[] = "$aws/things/esp_motor/shadow/update";  // publish topic name

// callback
int status = WL_IDLE_STATUS;
int msgCount = 0, msgReceived = 0;
char payload[512];
char rcvdPayload[512];

//Task
TaskHandle_t Task1;
TaskHandle_t Task2;
int isFeedingStart = 0;

// servo
Servo servoPI;  // 180도 회전 서보모터
const int servoPI_Pin = 5;

// LED pins
const int led = 16;

// button
const int buttonPin = 15;
unsigned long lastDebounceTime = 0;      // the last time the output pin was toggled
const unsigned long debounceDelay = 50;  // the debounce time; increase if the output flickers
int lastButtonState = LOW;               // the previous reading from the input pin
int buttonState = LOW;                   // the current reading from the input pin

// 유효기간은 5초로 버튼이 클릭된 횟수를 받아온다.
unsigned long lastButtonClicked = 0;
const unsigned long clickTimeout = 5000;
int buttonClicks = 0;

// feeding
bool isFeedingDone = false; // 급식 여부를 저장하는 변수
int feedCount = 0;          // 급식한 횟수를 저장하는 변수

// Ultrasonic Sensor
const int trigPin = 22;
const int echoPin = 23;

// Callback 함수
void mySubCallBackHandler(char* topicName, int payloadLen, char* payLoad) {
  strncpy(rcvdPayload, payLoad, payloadLen);
  rcvdPayload[payloadLen] = 0;
  msgReceived = 1;
}

void feedPet(int amount) {
  isFeedingStart = 1;  // Task2에서 led 점멸 시작

  if (amount > 0) {
    // 180도 모터 회전 시작
    for (int i = 0; i < amount; i++) {
      for (int pos = 0; pos <= 180; pos++) {
        servoPI.write(pos);
        delay(15);
      }
      delay(1000);

      for (int pos = 180; pos >= 0; pos--) {
        servoPI.write(pos);
        delay(15);
      }
      delay(1000);
    }
  }

  isFeedingStart = 0;    // Task2에서 led 점멸 중지
  isFeedingDone = true;
}

// 초음파 센서로 거리측정 함수
long measureDistance() {
  long duration, distance;

  //Triggering by 10us pulse
  digitalWrite(trigPin, LOW);  // trig low for 2us
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);  // trig high for 10us
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  //getting duration for echo pulse
  duration = pulseIn(echoPin, HIGH);

  // sound speed = 340 m/s = 34/1000 cm/us
  // distance = duration * 34/1000 * 1/2
  distance = duration * 17 / 1000;
  delay(10);

  return distance;
}

void setup() {
  Serial.begin(115200);

  // Connecting to WiFi
  Serial.print("WIFI status = ");
  Serial.println(WiFi.getMode());
  WiFi.disconnect(true);
  delay(1000);
  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.print("WIFI status = ");
  Serial.println(WiFi.getMode());
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to wifi");

  // Connecting to AWS
  if (motor.connect(HOST_ADDRESS, CLIENT_ID) == 0) {
    Serial.println("Connected to AWS");
    delay(1000);
    if (0 == motor.subscribe(sTOPIC_NAME, mySubCallBackHandler)) {
      Serial.println("Subscribe Successfull");
    } else {
      Serial.println("Subscribe Failed, Check the Thing Name and Certificates");
      while (1)
        ;
    }
  } else {
    Serial.println("AWS connection failed, Check the HOST Address");
    while (1)
      ;
  }

  // attach servo
  servoPI.attach(servoPI_Pin);
  servoPI.write(0);  // 180도 서보모터 초기화

  // 초음파 센서
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // initialize the button and Interrupt
  pinMode(buttonPin, INPUT);

  // led
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);

  //create a task that will be executed in the Task1code() function,
  //with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
    Task1code, /* Task function. */
    "Task1",   /* name of task. */
    10000,     /* Stack size of task */
    NULL,      /* parameter of the task */
    1,         /* priority of the task */
    &Task1,    /* handle to keep track of task */
    0);        /* pin task to core0*/
  delay(500);
  //create a task that will be executed in the Task2code() function,
  // with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
    Task2code, /* Task function. */
    "Task2",   /* name of task. */
    10000,     /* Stack size of task */
    NULL,      /* parameter of the task */
    1,         /* priority of the task */
    &Task2,    /* handle to keep track of task */
    1);        /* pin task to core 1 */
  delay(500);
}

//Task1code
void Task1code(void* pvParameters) {
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());
  for (;;) {
    if (msgReceived == 1) {
      msgReceived = 0;
      int amount = 0;

      Serial.print("Received Message:");
      Serial.println(rcvdPayload);

      // Parse JSON
      // $aws/things/esp_motor/shadow/update/
      JSONVar myObj = JSON.parse(rcvdPayload);
      amount = myObj["state"]["reported"]["feed"];

      Serial.printf("amount : %d\n", amount);

      if (amount > 0) {
        feedCount = amount;
        feedPet(amount);
        buttonClicks = 0;  // 버튼을 클릭하는 도중에 명령이 왔다면 초기화해주기
      }
    } else {
      unsigned long currentTime = millis();

      // 버튼이 마지막으로 클릭된 후 5초가 지나면 급식 시작
      if (buttonClicks > 0 && (currentTime - lastButtonClicked) > clickTimeout) {
        buttonClicks = min(buttonClicks, 10);  // 최대 10번 feeding 진행 가능
        feedCount = buttonClicks;
        buttonClicks = 0;
        feedPet(feedCount);
      } else {
        // 스위치가 클릭될 때 발생하는 Debounce 문제 해결
        // 버튼의 상태가 변화되고, 변화 후 50ms 이상 지속될 시 클릭되었다고 판단
        int reading = digitalRead(buttonPin);
        
        if (reading != lastButtonState) {
          lastDebounceTime = currentTime;
        }
        if ((currentTime - lastDebounceTime) > debounceDelay) {
          if (reading != buttonState) {
            buttonState = reading;

            if (buttonState == HIGH) {
              Serial.println("버튼이 클릭됨!!!");
              buttonClicks++;    
              lastButtonClicked = currentTime;
            }
          }
        }
        lastButtonState = reading;
      }
    }

    if (isFeedingDone) {
      Serial.print("feed amount : ");
      Serial.println(feedCount);

      long dis = measureDistance();
      Serial.print("distance : ");
      Serial.println(dis);

      // 높이의 비율을 이용해서 남은 사료량의 %를 구했다. 하지만 실제 사료통은 원뿔 형태로
      // 높이와 반지름간에 비율과 원뿔 부피 공식을 이용해서 구해야지 정확하겠다.
      float remaining_percentage = min(100.0f, ((17.0f - (float)dis) / 17.0f) * 100.0f);
      sprintf(payload, "{\"state\": { \"reported\": {\"feed_given\": true, \"feed_enough\": %.2f}}}", remaining_percentage);

      // topic에 publish 될 때까지 진행하기
      while (motor.publish(pTOPIC_NAME, payload) != 0) {
        Serial.println("Feeding Done Publish failed");
        delay(10);
      }
      Serial.print("Feeding Done Publish Message:");
      Serial.println(payload);

      isFeedingDone = false;
    }
    delay(10);
  }
}

//Task2code
void Task2code(void* pvParameters) {
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    // 모터가 돌면서 급식이 시작되면 동시에 led 점멸 시작하기
    if (isFeedingStart) {
      digitalWrite(led, LOW);
      delay(1000);

      digitalWrite(led, HIGH);
      delay(1000);
    }
    delay(50);
  }
}

void loop() {
  // do noting
}
