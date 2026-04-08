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

#define SENSOR_NOISE_AMPLITUDE 0.05

static const ControllerConfig g_controller_config = {
    .control_gain = 0.4,
    .max_control_effort = 0.12};

typedef struct
{
    unsigned long sensor_tick;
    unsigned long sample_count;
    double disturbance;
    double true_error;
    double measured_error;
} SensorMeasurement;

typedef struct
{
    unsigned long sensor_tick;
    unsigned long sensor_sample_count;
    unsigned long control_tick;
    unsigned long control_cycle_count;
    double disturbance;
    double actuator_position;
    double true_error;
    double measured_error;
    double requested_control;
    double applied_control;
} SharedStatus;

static SharedStatus g_shared_status = {0};
static SemaphoreHandle_t g_status_mutex = NULL;
static QueueHandle_t g_measurement_queue = NULL;

static double sample_noise(void)
{
    double uniform_0_to_1 = (double)rand() / (double)RAND_MAX;
    double centered_noise = (2.0 * uniform_0_to_1) - 1.0;

    return SENSOR_NOISE_AMPLITUDE * centered_noise;
}

static double disturbance_for_sample(unsigned long sample_count)
{
    if (sample_count < 5)
    {
        return 0.0;
    }
    else if (sample_count < 10)
    {
        return 0.5;
    }

    return 0.2;
}

static void SensorTask(void *pvParameters)
{
    (void)pvParameters;

    TickType_t last_wake_time = xTaskGetTickCount();
    unsigned long sample_count = 0;

    for (;;)
    {
        SensorMeasurement measurement;
        double actuator_position = 0.0;

        if (xSemaphoreTake(g_status_mutex, portMAX_DELAY) == pdTRUE)
        {
            actuator_position = g_shared_status.actuator_position;
            xSemaphoreGive(g_status_mutex);
        }

        measurement.sensor_tick = (unsigned long)xTaskGetTickCount();
        measurement.sample_count = ++sample_count;
        measurement.disturbance = disturbance_for_sample(measurement.sample_count);
        measurement.true_error = measurement.disturbance - actuator_position;
        measurement.measured_error = measurement.true_error + sample_noise();

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
            ControlOutput control_output =
                controller_compute(measurement.measured_error, &g_controller_config);
            if (xSemaphoreTake(g_status_mutex, portMAX_DELAY) == pdTRUE)
            {
                g_shared_status.sensor_tick = measurement.sensor_tick;
                g_shared_status.sensor_sample_count = measurement.sample_count;
                g_shared_status.control_tick = (unsigned long)xTaskGetTickCount();
                g_shared_status.control_cycle_count++;
                g_shared_status.disturbance = measurement.disturbance;
                g_shared_status.true_error = measurement.true_error;
                g_shared_status.measured_error = measurement.measured_error;
                g_shared_status.requested_control = control_output.requested_control;
                g_shared_status.applied_control = control_output.applied_control;
                g_shared_status.actuator_position += control_output.applied_control;
                xSemaphoreGive(g_status_mutex);
            }
        }
    }
}

static void LoggerTask(void *pvParameters)
{
    (void)pvParameters;

    double disturbance = 0.0;
    double actuator_position = 0.0;
    double true_error = 0.0;
    double measured_error = 0.0;
    double requested_control = 0.0;
    double applied_control = 0.0;

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
            disturbance = g_shared_status.disturbance;
            actuator_position = g_shared_status.actuator_position;
            true_error = g_shared_status.true_error;
            measured_error = g_shared_status.measured_error;
            requested_control = g_shared_status.requested_control;
            applied_control = g_shared_status.applied_control;
            xSemaphoreGive(g_status_mutex);
        }

        printf("[LoggerTask ] logger_tick = %lu, sensor_tick = %lu, samples = %lu, control_tick = %lu, cycles = %lu, disturbance = %.2f, true_error = %.2f, measured_error = %.2f, requested = %.2f, applied = %.2f, actuator = %.2f\n",
               (unsigned long)xTaskGetTickCount(),
               sensor_tick,
               sensor_sample_count,
               control_tick,
               control_cycle_count,
               disturbance,
               true_error,
               measured_error,
               requested_control,
               applied_control,
               actuator_position);

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
    srand(1);
    
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
