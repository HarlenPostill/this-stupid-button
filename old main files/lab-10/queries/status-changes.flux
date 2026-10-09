// Table: initial status and each recorded transition within the chosen range.
// 0=NORMAL, 1=HOT, 2=DARK, 3=HOT & DARK, -1=SENSOR_FAULT.
// Changes between upload samples are not recoverable from the bucket.
from(bucket: "environment_lab")
    |> range(start: -1h)
    |> filter(fn: (r) => r._measurement == "environment")
    |> filter(fn: (r) => r.group_id == "YOUR_GROUP_ID" and r.device_id == "YOUR_DEVICE_ID")
    |> filter(fn: (r) => r._field == "status_code")
    |> group(columns: ["group_id", "device_id"])
    |> sort(columns: ["_time"])
    |> duplicate(column: "_value", as: "change")
    |> difference(columns: ["change"], keepFirst: true)
    |> filter(fn: (r) => not exists r.change or r.change != 0)
    |> keep(columns: ["_time", "_value", "change", "group_id", "device_id"])
    |> yield(name: "status_changes")
