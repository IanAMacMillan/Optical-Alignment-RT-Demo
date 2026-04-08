import json
import tkinter as tk
from collections import deque
from pathlib import Path
from tkinter import ttk


REPO_ROOT = Path(__file__).resolve().parent.parent
RUNTIME_DIR = REPO_ROOT / "runtime"
SNAPSHOT_PATH = RUNTIME_DIR / "snapshot.json"
COMMAND_PATH = RUNTIME_DIR / "command.json"
COMMAND_TEMP_PATH = RUNTIME_DIR / "command.json.tmp"

HISTORY_LENGTH = 200
PLOT_RANGE = 1.0


class VisualizerApp(tk.Tk):
    def __init__(self) -> None:
        super().__init__()

        self.title("Optical Alignment Visualizer")
        self.geometry("980x760")
        self.minsize(880, 680)

        self.snapshot = None
        self.last_sample_count = None
        self.history = deque(maxlen=HISTORY_LENGTH)
        self.slider_dragging = False
        self.metrics_expanded = False

        self.metric_vars = {
            "time_s": tk.StringVar(value="0.000 s"),
            "controller_enabled": tk.StringVar(value="enabled"),
            "setpoint": tk.StringVar(value="0.000"),
            "disturbance": tk.StringVar(value="0.000"),
            "true_value": tk.StringVar(value="0.000"),
            "actuator_position": tk.StringVar(value="0.000"),
            "true_error": tk.StringVar(value="0.000"),
            "measured_error": tk.StringVar(value="0.000"),
            "requested_control": tk.StringVar(value="0.000"),
            "applied_control": tk.StringVar(value="0.000"),
            "samples": tk.StringVar(value="0"),
            "cycles": tk.StringVar(value="0"),
        }
        self.status_var = tk.StringVar(value="Waiting for runtime/snapshot.json")
        self.setpoint_var = tk.DoubleVar(value=0.0)
        self.controller_enabled_var = tk.BooleanVar(value=True)
        self.metrics_toggle_var = tk.StringVar(value="Show Live Snapshot")

        self._build_ui()
        self._schedule_poll()

    def _build_ui(self) -> None:
        self.columnconfigure(0, weight=1)
        self.rowconfigure(3, weight=1)
        self.rowconfigure(4, weight=1)

        header = ttk.Frame(self, padding=12)
        header.grid(row=0, column=0, sticky="ew")
        header.columnconfigure(0, weight=1)

        ttk.Label(
            header,
            text="Optical Alignment RT Visualizer",
            font=("Helvetica", 20, "bold"),
        ).grid(row=0, column=0, sticky="w")

        ttk.Label(
            header,
            textvariable=self.status_var,
            font=("Helvetica", 11),
        ).grid(row=1, column=0, sticky="w", pady=(4, 0))

        controls = ttk.Frame(self, padding=(12, 0, 12, 12))
        controls.grid(row=1, column=0, sticky="ew")
        controls.columnconfigure(1, weight=1)

        ttk.Label(controls, text="Setpoint", font=("Helvetica", 12, "bold")).grid(
            row=0, column=0, sticky="w"
        )

        self.scale = tk.Scale(
            controls,
            from_=-1.0,
            to=1.0,
            resolution=0.01,
            orient=tk.HORIZONTAL,
            variable=self.setpoint_var,
            command=self._on_setpoint_change,
            length=420,
            showvalue=True,
        )
        self.scale.grid(row=0, column=1, sticky="ew", padx=(12, 12))
        self.scale.bind("<ButtonPress-1>", self._start_drag)
        self.scale.bind("<ButtonRelease-1>", self._end_drag)

        self.enable_check = ttk.Checkbutton(
            controls,
            text="Controller Enabled",
            variable=self.controller_enabled_var,
            command=self._write_command_from_controls,
        )
        self.enable_check.grid(row=0, column=2, sticky="e")

        metrics_section = ttk.Frame(self, padding=(12, 0, 12, 12))
        metrics_section.grid(row=2, column=0, sticky="ew")
        metrics_section.columnconfigure(0, weight=1)

        ttk.Label(
            metrics_section,
            text="Live Snapshot",
            font=("Helvetica", 12, "bold"),
        ).grid(row=0, column=0, sticky="w")

        self.metrics_toggle_button = ttk.Button(
            metrics_section,
            textvariable=self.metrics_toggle_var,
            command=self._toggle_metrics_panel,
        )
        self.metrics_toggle_button.grid(row=0, column=1, sticky="e")

        self.metrics_frame = ttk.LabelFrame(metrics_section, padding=12)
        self.metrics_frame.columnconfigure(0, weight=1)
        self.metrics_frame.columnconfigure(1, weight=1)
        self.metrics_frame.columnconfigure(2, weight=1)
        self.metrics_frame.columnconfigure(3, weight=1)

        metric_order = [
            ("Time", "time_s"),
            ("Controller", "controller_enabled"),
            ("Samples", "samples"),
            ("Cycles", "cycles"),
            ("Setpoint", "setpoint"),
            ("Disturbance", "disturbance"),
            ("True Value", "true_value"),
            ("Actuator", "actuator_position"),
            ("True Error", "true_error"),
            ("Measured Error", "measured_error"),
            ("Requested", "requested_control"),
            ("Applied", "applied_control"),
        ]

        for index, (label, key) in enumerate(metric_order):
            row = index // 4
            column = index % 4
            card = ttk.Frame(self.metrics_frame, padding=(6, 8))
            card.grid(row=row, column=column, sticky="nsew")
            ttk.Label(card, text=label, font=("Helvetica", 10, "bold")).pack(anchor="w")
            ttk.Label(card, textvariable=self.metric_vars[key], font=("Menlo", 12)).pack(
                anchor="w", pady=(4, 0)
            )

        gauge_frame = ttk.LabelFrame(self, text="Live Gauge", padding=12)
        gauge_frame.grid(row=3, column=0, sticky="nsew", padx=12, pady=(0, 12))
        gauge_frame.columnconfigure(0, weight=1)
        gauge_frame.rowconfigure(0, weight=1)

        self.gauge_canvas = tk.Canvas(
            gauge_frame,
            background="#f8f7f2",
            highlightthickness=0,
            height=180,
        )
        self.gauge_canvas.grid(row=0, column=0, sticky="nsew")

        history_frame = ttk.LabelFrame(self, text="History", padding=12)
        history_frame.grid(row=4, column=0, sticky="nsew", padx=12, pady=(0, 12))
        history_frame.columnconfigure(0, weight=1)
        history_frame.rowconfigure(0, weight=1)

        self.history_canvas = tk.Canvas(
            history_frame,
            background="#f8f7f2",
            highlightthickness=0,
            height=220,
        )
        self.history_canvas.grid(row=0, column=0, sticky="nsew")

    def _start_drag(self, _event) -> None:
        self.slider_dragging = True

    def _end_drag(self, _event) -> None:
        self.slider_dragging = False
        self._write_command_from_controls()

    def _on_setpoint_change(self, _value: str) -> None:
        if self.slider_dragging:
            self._write_command_from_controls()

    def _write_command_from_controls(self) -> None:
        RUNTIME_DIR.mkdir(exist_ok=True)
        payload = {
            "setpoint": round(self.setpoint_var.get(), 4),
            "controller_enabled": 1 if self.controller_enabled_var.get() else 0,
        }
        COMMAND_TEMP_PATH.write_text(json.dumps(payload, indent=2) + "\n")
        COMMAND_TEMP_PATH.replace(COMMAND_PATH)
        self.status_var.set(
            f"Requested setpoint {payload['setpoint']:+.2f}, controller "
            f"{'enabled' if payload['controller_enabled'] else 'disabled'}"
        )

    def _toggle_metrics_panel(self) -> None:
        if self.metrics_expanded:
            self.metrics_frame.grid_forget()
            self.metrics_toggle_var.set("Show Live Snapshot")
            self.metrics_expanded = False
        else:
            self.metrics_frame.grid(row=1, column=0, columnspan=2, sticky="ew", pady=(8, 0))
            self.metrics_toggle_var.set("Hide Live Snapshot")
            self.metrics_expanded = True

    def _schedule_poll(self) -> None:
        self._poll_snapshot()
        self.after(100, self._schedule_poll)

    def _poll_snapshot(self) -> None:
        if not SNAPSHOT_PATH.exists():
            return

        try:
            snapshot = json.loads(SNAPSHOT_PATH.read_text())
        except (OSError, json.JSONDecodeError):
            return

        sample_count = snapshot.get("sensor_sample_count")
        if sample_count != self.last_sample_count:
            self.history.append(
                {
                    "setpoint": snapshot.get("setpoint", 0.0),
                    "true_value": snapshot.get("true_value", 0.0),
                    "actuator_position": snapshot.get("actuator_position", 0.0),
                    "measured_error": snapshot.get("measured_error", 0.0),
                }
            )
            self.last_sample_count = sample_count

        self.snapshot = snapshot
        self._refresh_metrics()
        self._draw_gauge()
        self._draw_history()

    def _refresh_metrics(self) -> None:
        if self.snapshot is None:
            return

        self.metric_vars["time_s"].set(f"{self.snapshot.get('time_s', 0.0):.3f} s")
        controller_enabled = bool(self.snapshot.get("controller_enabled", 1))
        self.metric_vars["controller_enabled"].set(
            "enabled" if controller_enabled else "disabled"
        )
        self.metric_vars["setpoint"].set(f"{self.snapshot.get('setpoint', 0.0):+.3f}")
        self.metric_vars["disturbance"].set(f"{self.snapshot.get('disturbance', 0.0):+.3f}")
        self.metric_vars["true_value"].set(f"{self.snapshot.get('true_value', 0.0):+.3f}")
        self.metric_vars["actuator_position"].set(
            f"{self.snapshot.get('actuator_position', 0.0):+.3f}"
        )
        self.metric_vars["true_error"].set(f"{self.snapshot.get('true_error', 0.0):+.3f}")
        self.metric_vars["measured_error"].set(
            f"{self.snapshot.get('measured_error', 0.0):+.3f}"
        )
        self.metric_vars["requested_control"].set(
            f"{self.snapshot.get('requested_control', 0.0):+.3f}"
        )
        self.metric_vars["applied_control"].set(
            f"{self.snapshot.get('applied_control', 0.0):+.3f}"
        )
        self.metric_vars["samples"].set(str(self.snapshot.get("sensor_sample_count", 0)))
        self.metric_vars["cycles"].set(str(self.snapshot.get("control_cycle_count", 0)))

        if not self.slider_dragging:
            self.setpoint_var.set(self.snapshot.get("setpoint", 0.0))
        self.controller_enabled_var.set(controller_enabled)

        self.status_var.set(
            f"Live at t={self.snapshot.get('time_s', 0.0):.2f}s from {SNAPSHOT_PATH.relative_to(REPO_ROOT)}"
        )

    def _draw_gauge(self) -> None:
        canvas = self.gauge_canvas
        canvas.delete("all")

        width = max(canvas.winfo_width(), 10)
        height = max(canvas.winfo_height(), 10)
        mid_y = height // 2
        left_margin = 60
        right_margin = width - 60

        def x_for_value(value: float) -> float:
            normalized = (value + PLOT_RANGE) / (2 * PLOT_RANGE)
            normalized = max(0.0, min(1.0, normalized))
            return left_margin + normalized * (right_margin - left_margin)

        canvas.create_line(left_margin, mid_y, right_margin, mid_y, fill="#444", width=2)
        for tick_value in (-1.0, -0.5, 0.0, 0.5, 1.0):
            tick_x = x_for_value(tick_value)
            canvas.create_line(tick_x, mid_y - 10, tick_x, mid_y + 10, fill="#666")
            canvas.create_text(tick_x, mid_y + 24, text=f"{tick_value:+.1f}", fill="#555")

        if self.snapshot is None:
            return

        markers = [
            ("Setpoint", self.snapshot.get("setpoint", 0.0), "#2f7d32"),
            ("True", self.snapshot.get("true_value", 0.0), "#ef6c00"),
            ("Actuator", self.snapshot.get("actuator_position", 0.0), "#1565c0"),
            ("Measured Error", self.snapshot.get("measured_error", 0.0), "#c62828"),
            ("Disturbance", self.snapshot.get("disturbance", 0.0), "#6a1b9a"),
        ]

        for index, (label, value, color) in enumerate(markers):
            x_pos = x_for_value(value)
            y_pos = mid_y - 36 + (index * 22)
            canvas.create_line(x_pos, mid_y, x_pos, y_pos, fill=color, dash=(3, 2))
            canvas.create_oval(x_pos - 6, y_pos - 6, x_pos + 6, y_pos + 6, fill=color, outline="")
            canvas.create_text(x_pos + 38, y_pos, text=f"{label} {value:+.2f}", fill=color, anchor="w")

    def _draw_history(self) -> None:
        canvas = self.history_canvas
        canvas.delete("all")

        width = max(canvas.winfo_width(), 10)
        height = max(canvas.winfo_height(), 10)
        left_margin = 60
        right_margin = width - 20
        top_margin = 20
        bottom_margin = height - 30
        plot_height = bottom_margin - top_margin
        plot_width = max(right_margin - left_margin, 1)

        def y_for_value(value: float) -> float:
            normalized = (value + PLOT_RANGE) / (2 * PLOT_RANGE)
            normalized = max(0.0, min(1.0, normalized))
            return bottom_margin - normalized * plot_height

        canvas.create_rectangle(left_margin, top_margin, right_margin, bottom_margin, outline="#999")
        canvas.create_line(left_margin, y_for_value(0.0), right_margin, y_for_value(0.0), fill="#bbb", dash=(4, 4))

        for tick_value in (-1.0, -0.5, 0.0, 0.5, 1.0):
            tick_y = y_for_value(tick_value)
            canvas.create_line(left_margin - 6, tick_y, left_margin, tick_y, fill="#666")
            canvas.create_text(left_margin - 10, tick_y, text=f"{tick_value:+.1f}", fill="#555", anchor="e")

        if len(self.history) < 2:
            return

        series = [
            ("setpoint", "#2f7d32"),
            ("true_value", "#ef6c00"),
            ("actuator_position", "#1565c0"),
            ("measured_error", "#c62828"),
        ]

        for name, color in series:
            points = []
            for index, sample in enumerate(self.history):
                x_pos = left_margin + (index / max(len(self.history) - 1, 1)) * plot_width
                y_pos = y_for_value(sample[name])
                points.extend((x_pos, y_pos))

            canvas.create_line(*points, fill=color, width=2, smooth=True)

        legend_items = [
            ("Setpoint", "#2f7d32"),
            ("True Value", "#ef6c00"),
            ("Actuator", "#1565c0"),
            ("Measured Error", "#c62828"),
        ]
        for index, (label, color) in enumerate(legend_items):
            x_pos = left_margin + index * 160
            canvas.create_line(x_pos, 10, x_pos + 24, 10, fill=color, width=3)
            canvas.create_text(x_pos + 30, 10, text=label, anchor="w", fill=color)


if __name__ == "__main__":
    app = VisualizerApp()
    app.mainloop()
