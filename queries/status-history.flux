// Table / Raw Data: status is a string; do NOT aggregate with mean.
from(bucket: "environment_lab")
    |> range(start: -15m)
    |> filter(fn: (r) => r._measurement == "environment")
    |> filter(fn: (r) => r.group_id == "YOUR_GROUP_ID" and r.device_id == "YOUR_DEVICE_ID")
    |> filter(fn: (r) => r._field == "environment_status")
    |> keep(columns: ["_time", "_value", "group_id", "device_id", "student_id", "room", "zone"])
    |> sort(columns: ["_time"])
    |> yield(name: "status_history")
