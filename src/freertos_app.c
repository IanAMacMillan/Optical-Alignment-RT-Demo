#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "controller.h"
#include "visualizer_runtime.h"

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
    double setpoint;
    double disturbance;
    double true_error;
    double measured_error;
} SensorMeasurement;

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
        double setpoint = 0.0;

        if (xSemaphoreTake(g_status_mutex, portMAX_DELAY) == pdTRUE)
        {
            actuator_position = g_shared_status.actuator_position;
            setpoint = g_shared_status.setpoint;
            xSemaphoreGive(g_status_mutex);
        }

        measurement.sensor_tick = (unsigned long)xTaskGetTickCount();
        measurement.sample_count = ++sample_count;
        measurement.setpoint = setpoint;
        measurement.disturbance = disturbance_for_sample(measurement.sample_count);
        measurement.true_error =
            measurement.setpoint - (measurement.disturbance + actuator_position);
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
        int controller_enabled = 1;

        if (xQueueReceive(g_measurement_queue, &measurement, portMAX_DELAY) == pdPASS)
        {
            if (xSemaphoreTake(g_status_mutex, portMAX_DELAY) == pdTRUE)
            {
                ControlOutput control_output = {0};
                double updated_actuator_position =
                    g_shared_status.actuator_position;
                double updated_true_value =
                    measurement.disturbance + g_shared_status.actuator_position;
                double updated_true_error =
                    measurement.setpoint - updated_true_value;

                controller_enabled = g_shared_status.controller_enabled;

                if (controller_enabled)
                {
                    control_output =
                        controller_compute(measurement.measured_error, &g_controller_config);
                    updated_actuator_position += control_output.applied_control;
                    updated_true_value = measurement.disturbance + updated_actuator_position;
                    updated_true_error = measurement.setpoint - updated_true_value;
                }
                else
                {
                    updated_actuator_position = 0.0;
                    updated_true_value = measurement.disturbance;
                    updated_true_error = measurement.setpoint - updated_true_value;
                }

                g_shared_status.sensor_tick = measurement.sensor_tick;
                g_shared_status.sensor_sample_count = measurement.sample_count;
                g_shared_status.control_tick = (unsigned long)xTaskGetTickCount();
                g_shared_status.control_cycle_count++;
                g_shared_status.setpoint = measurement.setpoint;
                g_shared_status.disturbance = measurement.disturbance;
                g_shared_status.true_value = updated_true_value;
                g_shared_status.true_error = updated_true_error;
                g_shared_status.measured_error = measurement.measured_error;
                g_shared_status.requested_control = control_output.requested_control;
                g_shared_status.applied_control = control_output.applied_control;
                g_shared_status.actuator_position = updated_actuator_position;
                xSemaphoreGive(g_status_mutex);
            }
        }
    }
}

static void LoggerTask(void *pvParameters)
{
    (void)pvParameters;

    double setpoint = 0.0;
    double disturbance = 0.0;
    double true_value = 0.0;
    double actuator_position = 0.0;
    double true_error = 0.0;
    double measured_error = 0.0;
    double requested_control = 0.0;
    double applied_control = 0.0;
    int controller_enabled = 1;

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
            controller_enabled = g_shared_status.controller_enabled;
            setpoint = g_shared_status.setpoint;
            disturbance = g_shared_status.disturbance;
            true_value = g_shared_status.true_value;
            actuator_position = g_shared_status.actuator_position;
            true_error = g_shared_status.true_error;
            measured_error = g_shared_status.measured_error;
            requested_control = g_shared_status.requested_control;
            applied_control = g_shared_status.applied_control;
            xSemaphoreGive(g_status_mutex);
        }

        printf("[LoggerTask ] logger_tick = %lu, sensor_tick = %lu, samples = %lu, control_tick = %lu, cycles = %lu, enabled = %d, setpoint = %.2f, disturbance = %.2f, true_value = %.2f, true_error = %.2f, measured_error = %.2f, requested = %.2f, applied = %.2f, actuator = %.2f\n",
               (unsigned long)xTaskGetTickCount(),
               sensor_tick,
               sensor_sample_count,
               control_tick,
               control_cycle_count,
               controller_enabled,
               setpoint,
               disturbance,
               true_value,
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
    VisualizerRuntimeContext visualizer_runtime = {0};
    srand(1);

    visualizer_runtime_prepare_files();

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

    if (xSemaphoreTake(g_status_mutex, portMAX_DELAY) == pdTRUE)
    {
        g_shared_status.controller_enabled = 1;
        g_shared_status.setpoint = 0.0;
        xSemaphoreGive(g_status_mutex);
    }

    visualizer_runtime.shared_status = &g_shared_status;
    visualizer_runtime.status_mutex = g_status_mutex;
    visualizer_runtime.sensor_period_ms = SENSOR_TASK_PERIOD_MS;

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

    status = visualizer_runtime_start(&visualizer_runtime);

    if (status != pdPASS)
    {
        fprintf(stderr, "Failed to start visualizer runtime tasks\n");
        return 1;
    }

    printf("Starting FreeRTOS scheduler\n");
    vTaskStartScheduler();

    fprintf(stderr, "Scheduler returned unexpectedly\n");
    return 1;
}
