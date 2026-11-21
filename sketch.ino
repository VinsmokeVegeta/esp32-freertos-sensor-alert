#include <Arduino.h>

// Pin Definitions
#define PIN_SENSOR_ADC    34    // Analog Potentiometer (Simulated Temp Sensor)
#define PIN_BUTTON_MUTE   14    // Push Button (Active LOW with internal pull-up)
#define PIN_LED_ALERT     26    // Red LED: High-Temperature Alert
#define PIN_LED_NORMAL    27    // Green LED: Normal Operation

#define TEMP_THRESHOLD_C  65.0f

// Data structure passed from Sensor Task to Alert/Logger Task
typedef struct {
  int   rawAdc;
  float temperatureC;
  bool  isAlert;
} SensorPacket_t;

// FreeRTOS Handles
QueueHandle_t     xSensorQueue = NULL;
SemaphoreHandle_t xSerialMutex = NULL;

// Shared state flag: volatile prevents compiler caching across tasks
volatile bool g_alertMuted = false;

// -------------------------------------------------------------------
// Task 1: Sensor Acquisition (Priority 2, runs every 500 ms)
// -------------------------------------------------------------------
void TaskSensorRead(void *pvParameters) {
  (void)pvParameters;
  SensorPacket_t packet;

  for (;;) {
    // 4-sample moving average to suppress ADC noise
    long sum = 0;
    for (int i = 0; i < 4; i++) {
      sum += analogRead(PIN_SENSOR_ADC);
      vTaskDelay(pdMS_TO_TICKS(5));
    }
    int avgAdc = (int)(sum / 4);

    // Map 12-bit ADC (0 - 4095) to simulated temperature (20.0 C - 100.0 C)
    float tempC = 20.0f + ((float)avgAdc / 4095.0f) * 80.0f;

    packet.rawAdc       = avgAdc;
    packet.temperatureC = tempC;
    packet.isAlert      = (tempC >= TEMP_THRESHOLD_C);

    // Push reading into the Queue (wait up to 10 ms if full)
    xQueueSend(xSensorQueue, &packet, pdMS_TO_TICKS(10));

    // Yield CPU for 500 ms
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// -------------------------------------------------------------------
// Task 2: Button Monitor / Alert Mute (Priority 3 - Highest)
// -------------------------------------------------------------------
void TaskButtonMonitor(void *pvParameters) {
  (void)pvParameters;
  bool lastState = HIGH;

  for (;;) {
    bool currentState = digitalRead(PIN_BUTTON_MUTE);

    // Detect falling edge (HIGH -> LOW press)
    if (lastState == HIGH && currentState == LOW) {
      vTaskDelay(pdMS_TO_TICKS(40));  // Software debounce
      if (digitalRead(PIN_BUTTON_MUTE) == LOW) {
        g_alertMuted = !g_alertMuted;

        if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
          Serial.printf("[EVENT] Alert Mute Toggled -> %s\n",
                        g_alertMuted ? "MUTED" : "ACTIVE");
          xSemaphoreGive(xSerialMutex);
        }
      }
    }

    lastState = currentState;
    vTaskDelay(pdMS_TO_TICKS(50));    // Check every 50 ms
  }
}

// -------------------------------------------------------------------
// Task 3: Alert Actuator & UART Telemetry Logger (Priority 1)
// -------------------------------------------------------------------
void TaskAlertLogger(void *pvParameters) {
  (void)pvParameters;
  SensorPacket_t rxData;

  for (;;) {
    // Blocks indefinitely until new data arrives in Queue (0% CPU waste)
    if (xQueueReceive(xSensorQueue, &rxData, portMAX_DELAY) == pdTRUE) {
      if (rxData.isAlert && !g_alertMuted) {
        digitalWrite(PIN_LED_ALERT, HIGH);
        digitalWrite(PIN_LED_NORMAL, LOW);
      } else if (rxData.isAlert && g_alertMuted) {
        digitalWrite(PIN_LED_ALERT, LOW);
        digitalWrite(PIN_LED_NORMAL, LOW);
      } else {
        digitalWrite(PIN_LED_ALERT, LOW);
        digitalWrite(PIN_LED_NORMAL, HIGH);
      }

      const char *statusStr = rxData.isAlert
                              ? (g_alertMuted ? "ALERT_MUTED" : "ALERT")
                              : "NORMAL";

      if (xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.printf("ADC:%4d | TEMP:%5.1f C | STATUS:%s\n",
                      rxData.rawAdc, rxData.temperatureC, statusStr);
        xSemaphoreGive(xSerialMutex);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_SENSOR_ADC, INPUT);
  pinMode(PIN_BUTTON_MUTE, INPUT_PULLUP);
  pinMode(PIN_LED_ALERT, OUTPUT);
  pinMode(PIN_LED_NORMAL, OUTPUT);

  // Create Queue (depth = 5) and Mutex for thread-safe UART logging
  xSensorQueue = xQueueCreate(5, sizeof(SensorPacket_t));
  xSerialMutex = xSemaphoreCreateMutex();

  if (xSensorQueue == NULL || xSerialMutex == NULL) {
    Serial.println("[FATAL] Failed to allocate FreeRTOS objects.");
    while (1) {}
  }

  // Spawn 3 concurrent tasks (4096-byte stack safely covers Serial.printf)
  xTaskCreate(TaskSensorRead,    "SensorRead",    4096, NULL, 2, NULL);
  xTaskCreate(TaskButtonMonitor, "ButtonMonitor", 4096, NULL, 3, NULL);
  xTaskCreate(TaskAlertLogger,   "AlertLogger",   4096, NULL, 1, NULL);

  Serial.println("=== FreeRTOS Multi-Tasking Sensor & Alert Controller Started ===");
}

void loop() {
  // Unused: FreeRTOS preemptive scheduler handles all tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}
