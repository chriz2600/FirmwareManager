# Firmware Manager for DreamcastHDMI

This was designed to provide an easy way to upgrade the firmware of my DreamcastHDMI project using an [ESP-12e][esp12e], but it should be easily adapted to other projects, where a SPI Flash should be programmed (in a failsafe manner, as the ESP12-e firmware is not altered).

## Initial setup:

On first startup the module acts as access point with predefined ssid/password. When connected you can setup the module, so it connects to your wifi router. 

The setup credentials are:

```
SSID: dc-firmware-manager<ID>
PSK:  geheim1234
IP:   192.168.4.1
URL:  http://192.168.4.1
User: Test
Pass: testtest
```

After connecting to [URL](http://192.168.4.1), you will be guided through the setup process.

## Connecting to module after setup:

After that - if your system supports mDNS (MAC OS X, Linux with avahi and Windows with Apple bonjour installed) - you can connect to [dc-firmware-manager.local][dcfwm] to flash a new firmware.

I've also created a small demo, running on a webserver:   
Just type "help" and "details" to get information about how to upgrade firmware.

[dc-fw-manager.i74.de][dcfwdemo]

## To build ESP-12e firmware:

- To build you need platformio:

| Command | Comment |
|-|-|
| `pio run` | to build |
| `pio upload` | to upload to ESP-12e (remember to check upload related settings in platformio.ini) |

## To create inlined index.html:

To build index.html you need the following

- node
- inliner node module (`npm install -g inliner`)

`./local/prepare-index-html` to create compressed, self-contained index.html

## Demo:

- [Demo][dcfwdemo]

## Flashing firmware:

I highly recommend the newer [esptool.py](https://github.com/espressif/esptool) instead of the esptool from platformio, as esptool.py is much faster by using a compressed protocol.

To flash the firmware the first time, you need a serial port (e.g. a usb to serial adapter). Be sure to set it to 3.3V, as the ESP8266 is not 5V tolerant.

The latest firmware is always available on [esp.i74.de](https://esp.i74.de/master/).

The firmware is divided into 2 parts, one (firmware.bin) is the actual firmware, the other (spiffs.bin) is the initial flash file system.

#### First time flash:

To flash the firmware/filesystem image the ESP-12e must be booted into serial programming mode.

See [ESP8266 Boot Mode Selection](https://github.com/espressif/esptool/wiki/ESP8266-Boot-Mode-Selection) for details.

If you have to program more than one ESP-12e [this](https://www.tindie.com/products/petl/esp12-programmer-board-with-pogo-pins/) might come in handy.

```
esptool.py -p <serial_port> write_flash 0x00000000 firmware.bin 0x00100000 spiffs.bin

serial_port: 
    e.g. COM5 on windows, /dev/cu.usbserial-A50285BI on OSX.
```

After that, the ESP-12e can be programmed "over the air".

## Schematic:

- **SOON**

----

[dcfwdemo]: http://dc-fw-manager.i74.de/
[esp12e]: http://www.esp8266.com/wiki/doku.php?id=esp8266-module-family#esp-12-e_q
[dcfwm]: http://dc-firmware-manager.local