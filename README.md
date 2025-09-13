In `User_Setup.h` of the TFT_eSPI library, uncomment the following line to enable the use of the ILI9488 display driver:

```c
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15  // Chip select control pin
#define TFT_DC    2  // Data Command control pin
#define TFT_RST   4  // Reset pin (could connect to RST pin)
```

If you notice display flickering, try adjusting the SPI frequency in `User_Setup.h`. Locate the following line:

```c
#define SPI_FREQUENCY  27000000
```

Lower values (e.g., `500000` for 500 kHz) slow down updates but can reduce flicker and improve stability. Higher values speed up display refresh but may cause rendering issues.

For a smoother experience, a frequency like `500000` (updates every 2 seconds) is recommended.

### Web Portal Setup

The clock features a web portal for WiFi and timezone configuration, as well as manual time and date settings.

To access the portal:

1. Connect to the clock's WiFi AP:
    - **SSID:** `MultifunctionClock`
    - **Password:** `12345678`
2. Open a browser and go to: [http://192.168.4.1](http://192.168.4.1)

### TFT Wiring Table

| TFT Pin      | ESP32 Pin | Description               |
|--------------|-----------|---------------------------|
| VCC          | 3.3V      | Power supply              |
| GND          | GND       | Ground                    |
| CS           | 15        | Chip select               |
| RESET        | 4         | Reset                     |
| DC           | 2         | Data/Command              |
| SDI (MOSI)   | 23        | Master Out Slave In       |
| SCK          | 18        | Serial Clock              |
| LED          | 3.3V (or 5V) | Backlight              |
| SDO (MISO)   | 19        | Master In Slave Out       |