#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    double disturbance;
    double actuator_position;
    double true_error;
    double measured_error;
    double control_effort;
} AlignmentPlantState;

static double sample_noise(void)
{
    const double noise_amplitude = 0.05;
    double uniform_0_to_1 = (double)rand() / (double)RAND_MAX;
    double centered_noise = (2.0 * uniform_0_to_1) - 1.0;

    return noise_amplitude * centered_noise;
}

int main(void)
{
    const double kp = 0.4;

    srand(1);

    AlignmentPlantState state = {
        .disturbance = 0.0,
        .actuator_position = 0.0,
        .true_error = 0.0,
        .measured_error = 0.0,
        .control_effort = 0.0};

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
        state.control_effort = kp * state.measured_error;
        state.actuator_position += state.control_effort;

        printf("step %d: disturbance = %.2f, true_error = %.2f, measured_error = %.2f, actuator_position = %.2f, control_effort = %.2f\n",
               step,
               state.disturbance,
               state.true_error,
               state.measured_error,
               state.actuator_position,
               state.control_effort);
    }

    return 0;
}