// Data Explorer, Graph. Both quantities are selected; prefer separate charts
// for C and relative percent. No device filter: distinguish actual boards.
// Change range to range(start: -24h, stop: -15m) for older-only data.
from(bucket: "environment_lab")
    |> range(start: -24h)
    |> filter(fn: (r) => r._measurement == "environment")
    |> filter(fn: (r) => r.group_id == "YOUR_GROUP_ID")
    |> filter(fn: (r) => r._field == "temperature_c" or r._field == "light_percent")
    |> group(columns: ["group_id", "device_id", "_field"])
    |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)
    |> yield(name: "historical_by_device")
