In `User_Setup.h` of the TFT_eSPI library, uncomment the following line to enable the use of the ILI9488 display driver:

```c
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15  // Chip select control pin
#define TFT_DC    2  // Data Command control pin
#define TFT_RST   4  // Reset pin (could connect to RST pin)
```

If the display flickers during operation, you may need to adjust the SPI frequency settings in the same `User_Setup.h` file. Look for the following lines and modify them as needed:

```c
#define SPI_FREQUENCY  27000000 
```
The value `1000000 (1 MHz)` corresponds to `1` second in real time. Larger values will make display updates faster, but may increase flickering or result in output mistakes (font rendering issues).

I prefer a smaller value like `500000 (500 kHz)` (display updates every 2 seconds) for a calmer display.

Web portal provides WiFi and timezone configuration options for an automatic time update from the internet. It also provides a manual time and date setting option. 

In order to use the web portal, users need to connect to the clock's WiFi access point (AP) first. The AP name is `MultifunctionClock` and the password is `12345678`. After connecting to the AP, users can access the web portal by navigating to `http://192.168.4.1`.