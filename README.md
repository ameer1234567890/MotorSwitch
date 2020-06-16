# MotorSwitch
An ESP8266 based switch which turns on and off a broadlink smart socket, along with an indicator LED.

There are two parts to this tool:
- A python script, which communicates with the broadlink socket and performs toggling the switch, while periodically checking status. This can ideally be run on a Raspberry Pi, or any linux machine for that matter.
- An Arduino app running on an ESP8266, which listens for HTTP requests denoting state changes from the python script, and send requests to the python script, when a button is pressed for toggling the switch.

#### Installation (Python Script)
- Python 3, pip and virtualenv is required
- Clone this repo or copy files to a local directory
- `cd` into the local directory
- Run `virtualenv env`
- Run `source env/bin/activate`
- Run `pip install -r requirements.txt`
- Create a file named `config.py` and write below, replacing YOUR_IFTTT_KEY_HERE as required
```
class Config:
   IFTTT_KEY = "YOUR_IFTTT_KEY_HERE"
```
- Run `sudo ./install-service.sh motorswitch.py` to install service

#### Uninstallation (Python Script)
- Run `sudo ./uninstall-service.py motorswitch.py`
- Remove local directory and its contents

#### Installation (Arduino App)
- Clone this repo or copy files to a local directory
- Create a file named `Secrets.h` and write below, replacing details as required
```
#ifndef WIFI_SSID
#define WIFI_SSID "xxxxxxxxxx"
#define WIFI_PASSWORD "xxxxxxxxxx"
#define IFTTT_ERROR_URL "http://maker.ifttt.com/trigger/motor_switch_error/with/key/xxxxxxxxxxxxx" // IFTTT webhook notify url
#define CALLBACK_URL "http://nas2.lan:8989/rebootprocess"
#define TURN_ON_URL "http://nas2.lan:8989/turnon"
#define TURN_OFF_URL "http://nas2.lan:8989/turnoff"
#endif
```
- Flash `MotorSwitch.ino` to ESP8266 via Arduino.
