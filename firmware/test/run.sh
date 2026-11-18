#!/bin/sh
# Desk test of the run logic on the PC (no DinMeter): virtual tyres, inflate and deflate runs, prints the log.
# The stub log_stub.h replaces log.h (no flash); the real pump/sensor/storage/config are copied next to it.
cd "$(dirname "$0")" && T=$(mktemp -d) && cp ../wrangler_air/config.h ../wrangler_air/sensor.h ../wrangler_air/storage.h ../wrangler_air/pump.h Arduino.h Preferences.h LittleFS.h test.cpp $T/ \
 && cp log_stub.h $T/log.h && mkdir -p $T/driver && cp driver/gpio.h $T/driver/ && g++ -std=gnu++17 -I$T -include $T/Arduino.h $T/test.cpp -o $T/t && $T/t
