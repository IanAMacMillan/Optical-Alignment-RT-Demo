#ifndef VISUALIZER_RUNTIME_H
#define VISUALIZER_RUNTIME_H

#include "FreeRTOS.h"
#include "semphr.h"

typedef struct
{
    unsigned long sensor_tick;
    unsigned long sensor_sample_count;
    unsigned long control_tick;
    unsigned long control_cycle_count;
    int controller_enabled;
    double setpoint;
    double disturbance;
    double true_value;
    double actuator_position;
    double true_error;
    double measured_error;
    double requested_control;
    double applied_control;
} SharedStatus;

typedef struct
{
    SharedStatus *shared_status;
    SemaphoreHandle_t status_mutex;
    unsigned long sensor_period_ms;
} VisualizerRuntimeContext;

void visualizer_runtime_prepare_files(void);
BaseType_t visualizer_runtime_start(const VisualizerRuntimeContext *context);

#endif
