# Archived lab-9: MQTT fire alarm

This is the complete previous active lab, preserved before the InfluxDB lab was
installed. `src/main.cpp` is the fire alarm; `src/week8_demo.cpp` is its MQTT
pre-lab demo. The configuration includes the local edits that existed at archive
time. Older independent labs remain beside this directory.

From this directory run `pio run -e upesy_wrover` or `pio run -e week8_demo`.
Run `python tools/build_lab_guide.py` here to rebuild the original PDF guide.
`LAB9_GUIDE.md` is preserved as originally written; its references to the demo
file now refer to `src/week8_demo.cpp`. No archived code is built by the root
project. A local copy of the previous `include/secrets.h` is ignored by Git;
new clones must create that file with LAB_WIFI_SSID, LAB_WIFI_PASSWORD and
LAB_GROUP_ID definitions.
