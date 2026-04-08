#include <stdio.h>

typedef struct
{
    double disturbance;
    double actuator_position;
    double error;
} AlignmentPlantState;

int main(void)
{
    AlignmentPlantState state = {
        .disturbance = 0.5,
        .actuator_position = 0.1,
        .error = 0.0};

    state.error = state.disturbance - state.actuator_position;

    for (int step = 0; step < 10; step++)
    {
        state.error = state.disturbance - state.actuator_position;

        printf("step %d: disturbance = %.2f, actuator_position = %.2f, error = %.2f\n",
               step,
               state.disturbance,
               state.actuator_position,
               state.error);

        state.actuator_position += 0.05;
    }

    return 0;
}