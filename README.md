# Multi-Tasking Sensor & Alert Controller (ESP32 + FreeRTOS)

A real-time embedded monitoring and alert controller built on ESP32 using FreeRTOS tasks, priority-based preemptive scheduling, thread-safe queues, and mutex-protected UART telemetry.

## Architecture
- **Task 1 (`TaskSensorRead`, Priority 2):** Samples a 12-bit ADC channel (4-sample moving average) every 500 ms, converts readings to simulated temperature (20°C – 100°C), and sends structured packets to `xSensorQueue`.
- **Task 2 (`TaskButtonMonitor`, Priority 3):** Polls the manual alert-mute push button every 50 ms with software debouncing for immediate operator response.
- **Task 3 (`TaskAlertLogger`, Priority 1):** Blocks on `xQueueReceive()`, drives Red/Green status LEDs based on the 65°C threshold and mute flag, and prints telemetry over UART (`115200` baud) guarded by `xSerialMutex`.

## Live Simulation
- **Wokwi Link:** [Run in Browser](https://wokwi.com/projects/481313380042329089)
- **Demo Screenshot:**
  ![Simulation Demo](simulator_proof)

## Embedded Concepts Demonstrated
- FreeRTOS preemptive task scheduling and priority assignment
- Inter-task communication using `xQueueCreate`, `xQueueSend`, and `xQueueReceive`
- Shared peripheral protection (UART) using a FreeRTOS Mutex (`xSemaphoreCreateMutex`)
- Non-blocking delays via `vTaskDelay(pdMS_TO_TICKS(...))` instead of busy-wait `delay()`
