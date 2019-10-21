# install
Install the ESP32 IDF library as described in the [documentation](https://docs.espressif.com/projects/esp-idf/en/latest/).

# development
Enable IDF environment:
```bash
. $IDF_PATH/export.sh
```

# build 
```bash
idf.py build
```

or

```bash
idf.py flash
```

# LEDs
Test project for working with NeoPixels on the ESP32

can fit about 1100 frames of pixel data for 8x8 (290KB)

# Architecture
Animations, layer composition, and LED buffer write are performed in a dedicated task, called the LED task.

Commands can be sent to the task through a queue passed in the pvParameters
