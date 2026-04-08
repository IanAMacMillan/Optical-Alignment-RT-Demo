#include "controller.h"

static double clamp(double value, double min_value, double max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

ControlOutput controller_compute(double measured_error,
                                 const ControllerConfig *config)
{
    ControlOutput output;

    output.requested_control = config->control_gain * measured_error;
    output.applied_control = clamp(output.requested_control,
                                   -config->max_control_effort,
                                   config->max_control_effort);

    return output;
}
