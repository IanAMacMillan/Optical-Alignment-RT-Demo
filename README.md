# Optical Alignment RTOS

This is a small FreeRTOS-based optical alignment controller simulator I built. The point was to practice RTOS structure in a project that still feels close to optical instrumentation: sensing, control, actuation, telemetry, and operator interaction.

<video src="assets/GUI_Demo.mov" controls muted playsinline width="960"></video>

## basic Model

- `SensorTask` produces a noisy alignment-error measurement
- `ControlTask` consumes that measurement and computes a proportional correction
- `LoggerTask` reports low-rate telemetry
- a separate GUI reads snapshots and writes operator commands

The current model is intentionally simple:

```text
true_value = external_offset + actuator_position
true_error = setpoint - true_value
measured_error = true_error + sensor_noise
```

When the controller is disabled, actuator correction goes to zero and the plant drifts to the external offset.

## Visualizer Variables

- `Setpoint`: the desired target
- `External Offset`: the outside bias pushing the plant
- `True Value`: the actual plant output
- `Actuator`: the corrective effort
- `True Error`: `setpoint - true_value`
- `Measured Error`: noisy sensor view of the error
- `Requested`: controller output before limiting
- `Applied`: controller output after limiting

## RTOS Structure

- `SensorTask`: priority `3`, periodic `200 ms`, queue producer
- `ControlTask`: priority `2`, queue consumer, computes correction
- `LoggerTask`: priority `1`, periodic `1000 ms`, low-priority telemetry
- `CommandTask`: priority `1`, periodic `100 ms`, reads GUI commands
- `SnapshotTask`: priority `1`, periodic `100 ms`, exports GUI snapshots

Why it is arranged this way:

- sensing and control are more time-sensitive than logging or GUI support
- queue is used for sensor-to-controller handoff
- mutex is used for shared snapshot/state
- GUI tooling stays outside the real-time control path

The FreeRTOS tick rate is `100 Hz`, so one RTOS tick is `10 ms`.

## to Run It

From the repository root:

```bash
cmake -S . -B build
cmake --build build
./build/optical_alignment_rt_demo
```

In another terminal:

```bash
python3 visualizer/gui.py
```