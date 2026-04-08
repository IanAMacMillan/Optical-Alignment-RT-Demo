#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "FreeRTOS.h"
#include "task.h"

#include "visualizer_runtime.h"

#define COMMAND_TASK_PRIORITY 1
#define SNAPSHOT_TASK_PRIORITY 1

#define COMMAND_TASK_PERIOD_MS 100
#define SNAPSHOT_TASK_PERIOD_MS 100

#define RUNTIME_DIR "runtime"
#define SNAPSHOT_FILE_PATH RUNTIME_DIR "/snapshot.json"
#define SNAPSHOT_TEMP_FILE_PATH RUNTIME_DIR "/snapshot.json.tmp"
#define COMMAND_FILE_PATH RUNTIME_DIR "/command.json"

static VisualizerRuntimeContext g_runtime_context = {0};

static void write_snapshot_file(const SharedStatus *status)
{
    FILE *snapshot_file = fopen(SNAPSHOT_TEMP_FILE_PATH, "w");
    double sample_period_s = (double)g_runtime_context.sensor_period_ms / 1000.0;
    double time_s = status->sensor_sample_count * sample_period_s;

    if (snapshot_file == NULL)
    {
        fprintf(stderr, "Failed to open snapshot file for writing: %s\n", strerror(errno));
        return;
    }

    fprintf(snapshot_file,
            "{\n"
            "  \"sensor_tick\": %lu,\n"
            "  \"sensor_sample_count\": %lu,\n"
            "  \"control_tick\": %lu,\n"
            "  \"control_cycle_count\": %lu,\n"
            "  \"controller_enabled\": %d,\n"
            "  \"time_s\": %.3f,\n"
            "  \"setpoint\": %.6f,\n"
            "  \"disturbance\": %.6f,\n"
            "  \"true_value\": %.6f,\n"
            "  \"actuator_position\": %.6f,\n"
            "  \"true_error\": %.6f,\n"
            "  \"measured_error\": %.6f,\n"
            "  \"requested_control\": %.6f,\n"
            "  \"applied_control\": %.6f\n"
            "}\n",
            status->sensor_tick,
            status->sensor_sample_count,
            status->control_tick,
            status->control_cycle_count,
            status->controller_enabled,
            time_s,
            status->setpoint,
            status->disturbance,
            status->true_value,
            status->actuator_position,
            status->true_error,
            status->measured_error,
            status->requested_control,
            status->applied_control);

    fclose(snapshot_file);

    if (rename(SNAPSHOT_TEMP_FILE_PATH, SNAPSHOT_FILE_PATH) != 0)
    {
        fprintf(stderr, "Failed to publish snapshot file: %s\n", strerror(errno));
    }
}

static void poll_setpoint_command(void)
{
    FILE *command_file = fopen(COMMAND_FILE_PATH, "r");
    char buffer[256];
    size_t bytes_read = 0;
    char *setpoint_key = NULL;
    char *enabled_key = NULL;
    char *colon = NULL;

    if (command_file == NULL)
    {
        return;
    }

    bytes_read = fread(buffer, 1, sizeof(buffer) - 1, command_file);
    if (bytes_read == 0)
    {
        fclose(command_file);
        return;
    }

    fclose(command_file);
    buffer[bytes_read] = '\0';

    setpoint_key = strstr(buffer, "\"setpoint\"");
    if (setpoint_key != NULL)
    {
        colon = strchr(setpoint_key, ':');
        if (colon != NULL)
        {
            double requested_setpoint = strtod(colon + 1, NULL);

            if (xSemaphoreTake(g_runtime_context.status_mutex, portMAX_DELAY) == pdTRUE)
            {
                g_runtime_context.shared_status->setpoint = requested_setpoint;
                xSemaphoreGive(g_runtime_context.status_mutex);
            }
        }
    }

    enabled_key = strstr(buffer, "\"controller_enabled\"");
    if (enabled_key != NULL)
    {
        colon = strchr(enabled_key, ':');
        if (colon != NULL)
        {
            int requested_enabled = (int)strtol(colon + 1, NULL, 10);

            if (xSemaphoreTake(g_runtime_context.status_mutex, portMAX_DELAY) == pdTRUE)
            {
                g_runtime_context.shared_status->controller_enabled =
                    requested_enabled ? 1 : 0;
                xSemaphoreGive(g_runtime_context.status_mutex);
            }
        }
    }
}

static void CommandTask(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)pvParameters;

    for (;;)
    {
        poll_setpoint_command();
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(COMMAND_TASK_PERIOD_MS));
    }
}

static void SnapshotTask(void *pvParameters)
{
    TickType_t last_wake_time = xTaskGetTickCount();

    (void)pvParameters;

    for (;;)
    {
        SharedStatus snapshot = {0};

        if (xSemaphoreTake(g_runtime_context.status_mutex, portMAX_DELAY) == pdTRUE)
        {
            snapshot = *g_runtime_context.shared_status;
            xSemaphoreGive(g_runtime_context.status_mutex);
        }

        write_snapshot_file(&snapshot);
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(SNAPSHOT_TASK_PERIOD_MS));
    }
}

void visualizer_runtime_prepare_files(void)
{
    if (mkdir(RUNTIME_DIR, 0777) != 0 && errno != EEXIST)
    {
        fprintf(stderr, "Failed to create runtime directory: %s\n", strerror(errno));
    }
}

BaseType_t visualizer_runtime_start(const VisualizerRuntimeContext *context)
{
    BaseType_t status = pdPASS;

    g_runtime_context = *context;

    status = xTaskCreate(
        CommandTask,
        "CommandTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        COMMAND_TASK_PRIORITY,
        NULL);

    if (status != pdPASS)
    {
        return status;
    }

    return xTaskCreate(
        SnapshotTask,
        "SnapshotTask",
        configMINIMAL_STACK_SIZE,
        NULL,
        SNAPSHOT_TASK_PRIORITY,
        NULL);
}
