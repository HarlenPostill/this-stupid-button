// Table: pivot related fields into columns at each timestamp.
// Pivot BEFORE grouping; combining different _value types first can cause
// schema collisions (string status vs numeric readings vs boolean sensor_ok).
from(bucket: "environment_lab")
    |> range(start: -15m)
    |> filter(fn: (r) => r._measurement == "environment")
    |> filter(fn: (r) => r.group_id == "YOUR_GROUP_ID" and r.device_id == "YOUR_DEVICE_ID")
    |> filter(fn: (r) => r._field == "temperature_c" or r._field == "light_percent" or
        r._field == "environment_status" or r._field == "temperature_threshold_c" or
        r._field == "light_threshold_percent" or r._field == "wifi_rssi_dbm" or
        r._field == "uptime_s" or r._field == "sensor_ok")
    |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
    |> sort(columns: ["_time"])
    |> yield(name: "sample_details")
