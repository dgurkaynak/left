# Getting up and running

- Assuming you've install Arduino IDE
- Assuming you've installed `esp32` (by Espressif Systems) board inside Arduino IDE > Boards Manager
    - At the time of writing this, its version is 3.1.3
- Go to https://www.waveshare.com/wiki/E-Paper_ESP32_Driver_Board#Demo
- Download the demo file
- Unzip, you must have a folder named `E-Paper_ESP32_Driver_Board_Code`
- Now, we're following this chapter: https://www.waveshare.com/wiki/E-Paper_ESP32_Driver_Board#Demo_Usage_3
    - Copy the `examples/esp32-waveshare-epd` folder into `/Users/dgurkaynak/Documents/Arduino/libraries/esp32-waveshare-epd`
- In Arduino IDE > Library Manager, search and install `GxEPD2` (by Jead-Marc Zingg).
    - At the time of writing this, it's version 1.6.2.
    - Don't forget to install its dependencies as well.
- Now, the libraries are set
- Double click `masabasi.ino` and let the Arduino IDE opens it
- Create a `crendentials.h` file
```c
#ifndef CREDENTIALS_H
#define CREDENTIALS_H

const char *WIFI_SSID = "your_wifi_name";
const char *WIFI_PASSWORD = "your_wifi_password";

#endif 
```
- Click upload & see it's working
