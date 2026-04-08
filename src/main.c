#include <stdio.h>
#include <stdlib.h>

typedef struct
{
    double disturbance;
    double actuator_position;
} AlignmentPlantState;

typedef struct
{
    int step;
    double true_error;
    double measured_error;
} SensorMeasurement;

typedef struct
{
    int step;
    double requested_control;
    double applied_control;
    int saturated;
} ControlCommand;

typedef struct
{
    int step;
    double time_s;
    double disturbance;
    double true_error;
    double measured_error;
    double requested_control;
    double applied_control;
    double actuator_position;
    int saturated;
} TelemetrySnapshot;

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

static void update_disturbance(AlignmentPlantState *plant, int step)
{
    if (step < 5)
    {
        plant->disturbance = 0.0;
    }
    else if (step < 10)
    {
        plant->disturbance = 0.5;
    }
    else
    {
        plant->disturbance = 0.2;
    }
}

static SensorMeasurement measure_sensor(const AlignmentPlantState *plant, int step)
{
    SensorMeasurement measurement;

    measurement.step = step;
    measurement.true_error = plant->disturbance - plant->actuator_position;
    measurement.measured_error = measurement.true_error + sample_noise();

    return measurement;
}

static ControlCommand run_controller(const SensorMeasurement *measurement, double kp, double max_control)
{
    ControlCommand command;

    command.step = measurement->step;
    command.requested_control = kp * measurement->measured_error;
    command.applied_control = clamp(command.requested_control, -max_control, max_control);
    command.saturated = (command.requested_control != command.applied_control);

    return command;
}

static void apply_actuator(AlignmentPlantState *plant, const ControlCommand *command)
{
    plant->actuator_position += command->applied_control;
}

static TelemetrySnapshot make_telemetry_snapshot(
    const AlignmentPlantState *plant,
    const SensorMeasurement *measurement,
    const ControlCommand *command,
    double sample_period_s)
{
    TelemetrySnapshot snapshot;

    snapshot.step = measurement->step;
    snapshot.time_s = measurement->step * sample_period_s;
    snapshot.disturbance = plant->disturbance;
    snapshot.true_error = measurement->true_error;
    snapshot.measured_error = measurement->measured_error;
    snapshot.requested_control = command->requested_control;
    snapshot.applied_control = command->applied_control;
    snapshot.actuator_position = plant->actuator_position;
    snapshot.saturated = command->saturated;

    return snapshot;
}

static void log_telemetry(const TelemetrySnapshot *snapshot)
{
    printf("step %d time %.2f s: disturbance = %.2f, true_error = %.2f, measured_error = %.2f, requested = %.2f, applied = %.2f, actuator_position = %.2f, saturated = %d\n",
           snapshot->step,
           snapshot->time_s,
           snapshot->disturbance,
           snapshot->true_error,
           snapshot->measured_error,
           snapshot->requested_control,
           snapshot->applied_control,
           snapshot->actuator_position,
           snapshot->saturated);
}

int main(void)
{
    const double kp = 0.4;
    const double max_control = 0.12;
    const double sample_period_s = 0.01;

    srand(1);

    AlignmentPlantState plant = {
        .disturbance = 0.0,
        .actuator_position = 0.0};

    for (int step = 0; step < 15; step++)
    {
        update_disturbance(&plant, step);

        SensorMeasurement measurement = measure_sensor(&plant, step);
        ControlCommand command = run_controller(&measurement, kp, max_control);

        apply_actuator(&plant, &command);

        TelemetrySnapshot snapshot =
            make_telemetry_snapshot(&plant, &measurement, &command, sample_period_s);
        log_telemetry(&snapshot);
    }

    return 0;
}
