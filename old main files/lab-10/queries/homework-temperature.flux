// Historical homework data only; the active firmware now writes environment.
// Graph for temperature; Table / Raw Data to display the student_id tag.
from(bucket: "environment_lab")
    |> range(start: -15m)
    |> filter(fn: (r) => r._measurement == "lecture_temperature")
    |> filter(fn: (r) => r.group_id == "YOUR_GROUP_ID" and r.student_id == "YOUR_STUDENT_ID")
    |> filter(fn: (r) => r._field == "temperature_c")
    |> yield(name: "homework_temperature")
