#ifndef CONTROLLER_H
#define CONTROLLER_H

typedef struct
{
    double control_gain;
    double max_control_effort;
} ControllerConfig;

typedef struct
{
    double requested_control;
    double applied_control;
} ControlOutput;

ControlOutput controller_compute(double measured_error,
                                 const ControllerConfig *config);

#endif
