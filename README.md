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

## Schematic:

- **SOON**

----

[dcfwdemo]: http://dc-fw-manager.i74.de/
[esp12e]: http://www.esp8266.com/wiki/doku.php?id=esp8266-module-family#esp-12-e_q
[dcfwm]: http://dc-firmware-manager.local