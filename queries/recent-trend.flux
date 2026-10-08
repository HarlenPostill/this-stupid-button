// Data Explorer, Graph. Set field to temperature_c OR light_percent.
// Replace IDs with the values in your Serial output.
bucket = "environment_lab"
groupID = "YOUR_GROUP_ID"
deviceID = "YOUR_DEVICE_ID"
field = "temperature_c"

from(bucket: bucket)
    |> range(start: -15m)
    |> filter(fn: (r) => r._measurement == "environment")
    |> filter(fn: (r) => r.group_id == groupID and r.device_id == deviceID)
    |> filter(fn: (r) => r._field == field)
    |> aggregateWindow(every: 15s, fn: mean, createEmpty: false)
    |> yield(name: "recent")
