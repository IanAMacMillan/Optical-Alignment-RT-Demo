#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "controller.h"

#define SENSOR_TASK_PRIORITY 3
#define CONTROL_TASK_PRIORITY 2
#define LOGGER_TASK_PRIORITY 1

#define SENSOR_TASK_PERIOD_MS 200
#define LOGGER_TASK_PERIOD_MS 1000

typedef struct
{
    unsigned long sensor_tick;
    unsigned long sample_count;
} SensorMeasurement;

typedef struct
{
    unsigned long sensor_tick;
    unsigned long sensor_sample_count;
    unsigned long control_tick;
    unsigned long control_cycle_count;
} SharedStatus;

static SharedStatus g_shared_status = {0};
static SemaphoreHandle_t g_status_mutex = NULL;
static QueueHandle_t g_measurement_queue = NULL;

static void SensorTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t last_wake_time = xTaskGetTickCount();
    unsigned long sample_count = 0;

    for (;;)
    {
        SensorMeasurement measurement;

        measurement.sensor_tick = (unsigned long)xTaskGetTickCount();
        measurement.sample_count = ++sample_count;

        if (xQueueSend(g_measurement_queue, &measurement, portMAX_DELAY) != pdPASS)
        {
            fprintf(stderr, "Failed to send measurement to queue\n");
        }

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SENSOR_TASK_PERIOD_MS));
    }
}

static void ControlTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        SensorMeasurement measurement;

        if (xQueueReceive(g_measurement_queue, &measurement, portMAX_DELAY) == pdPASS)
        {
            if (xSemaphoreTake(g_status_mutex, portMAX_DELAY) == pdTRUE)
            {
                g_shared_status.sensor_tick = measurement.sensor_tick;
                g_shared_status.sensor_sample_count = measurement.sample_count;
                g_shared_status.control_tick = (unsigned long)xTaskGetTickCount();
                g_shared_status.control_cycle_count++;
                xSemaphoreGive(g_status_mutex);
            }
        }
    }
}

static void LoggerTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t last_wake_time = xTaskGetTickCount();

    for (;;)
    {
        unsigned long sensor_tick = 0;
        unsigned long sensor_sample_count = 0;
        unsigned long control_tick = 0;
        unsigned long control_cycle_count = 0;

        if (xSemaphoreTake(g_status_mutex, portMAX_DELAY) == pdTRUE)
        {
            sensor_tick = g_shared_status.sensor_tick;
            sensor_sample_count = g_shared_status.sensor_sample_count;
            control_tick = g_shared_status.control_tick;
            control_cycle_count = g_shared_status.control_cycle_count;
            xSemaphoreGive(g_status_mutex);
        }

        printf("[LoggerTask ] logger_tick = %lu, sensor_tick = %lu, sensor_samples = %lu, control_tick = %lu, control_cycles = %lu\n",
               (unsigned long)xTaskGetTickCount(),
               sensor_tick,
               sensor_sample_count,
               control_tick,
               control_cycle_count);

        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(LOGGER_TASK_PERIOD_MS));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    fprintf(stderr, "FreeRTOS stack overflow detected\n");
    abort();
}

int main(void)
{
    BaseType_t status;

    g_measurement_queue = xQueueCreate(8, sizeof(SensorMeasurement));

    if (g_measurement_queue == NULL)
    {
        fprintf(stderr, "Failed to create measurement queue\n");
        return 1;
    }

    g_status_mutex = xSemaphoreCreateMutex();

    if (g_status_mutex == NULL)
    {
        fprintf(stderr, "Failed to create status mutex\n");
        return 1;
    }

    status = xTaskCreate(
        SensorTask,
        "SensorTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        SENSOR_TASK_PRIORITY,
        NULL);

    if (status != pdPASS)
    {
        fprintf(stderr, "Failed to create SensorTask\n");
        return 1;
    }

    status = xTaskCreate(
        ControlTask,
        "ControlTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        CONTROL_TASK_PRIORITY,
        NULL);

    if (status != pdPASS)
    {
        fprintf(stderr, "Failed to create ControlTask\n");
        return 1;
    }

    status = xTaskCreate(
        LoggerTask,
        "LoggerTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        LOGGER_TASK_PRIORITY,
        NULL);

    if (status != pdPASS)
    {
        fprintf(stderr, "Failed to create LoggerTask\n");
        return 1;
    }

    printf("Starting FreeRTOS scheduler\n");
    vTaskStartScheduler();

    fprintf(stderr, "Scheduler returned unexpectedly\n");
    return 1;
}
