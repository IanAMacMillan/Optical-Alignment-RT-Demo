#include <stdio.h>
#include <stdlib.h>

#include "FreeRTOS.h"
#include "task.h"


static void Task(void *pvParameters)
{
    (void)pvParameters;

    TickType_t last_wake_time = xTaskGetTickCount();

    for (;;)
    {
        printf("Task running at tick %lu\n", (unsigned long)xTaskGetTickCount());
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(500));
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;

    fprintf(stderr, "FreeRTOS stack overflow detected in task: %s\n",
            (pcTaskName != NULL) ? pcTaskName : "unknown");
    abort();
}

int main(void)
{
    BaseType_t status;

    status = xTaskCreate(
        Task,
        "Task",
        configMINIMAL_STACK_SIZE,
        NULL,
        1,
        NULL);

    if (status != pdPASS)
    {
        fprintf(stderr, "Failed to create Task\n");
        return 1;
    }

    printf("Starting FreeRTOS scheduler\n");
    vTaskStartScheduler();

    fprintf(stderr, "Scheduler returned unexpectedly\n");
    return 1;
}
