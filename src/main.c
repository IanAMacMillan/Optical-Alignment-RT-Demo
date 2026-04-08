#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    double disturbance;
    double actuator_position;
    double true_error;
    double measured_error;
    double requested_control;
    double applied_control;
} AlignmentPlantState;

static double sample_noise(void)
{
    const double noise_amplitude = 0.05;
    double uniform_0_to_1 = (double)rand() / (double)RAND_MAX;
    double centered_noise = (2.0 * uniform_0_to_1) - 1.0;

    return noise_amplitude * centered_noise;
}

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

int main(void)
{
    const double kp = 0.4;
    const double max_control = 0.12;

    srand(1);

    AlignmentPlantState state = {
        .disturbance = 0.0,
        .actuator_position = 0.0,
        .true_error = 0.0,
        .measured_error = 0.0,
        .requested_control = 0.0,
        .applied_control = 0.0};

    for (int step = 0; step < 15; step++)
    {
        if (step < 5)
        {
            state.disturbance = 0.0;
        }
        else if (step < 10)
        {
            state.disturbance = 0.5;
        }
        else
        {
            state.disturbance = 0.2;
        }

        state.true_error = state.disturbance - state.actuator_position;
        state.measured_error = state.true_error + sample_noise();

        state.requested_control = kp * state.measured_error;
        state.applied_control = clamp(state.requested_control, -max_control, max_control);
        state.actuator_position += state.applied_control;

        printf("step %d: disturbance = %.2f, true_error = %.2f, measured_error = %.2f, requested = %.2f, applied = %.2f, actuator_position = %.2f\n",
               step,
               state.disturbance,
               state.true_error,
               state.measured_error,
               state.requested_control,
               state.applied_control,
               state.actuator_position);
    }

    return 0;
}