# Twike Base Station

Start charging my Twike remotely, e.g. from the office, so it's full when I get home.

A Shelly Pro EM-50 measures the energy the Twike uses, so I can track the Wh per day and month and see what charging costs me.

- [esphome/](esphome/): firmware for the Waveshare ESP32-S3 board (charger relays, safety logic, power meter, MQTT)
- [docs/](docs/): static web page to control the charger and view the energy data (GitHub Pages)
- [src/](src/): first hardware test sketch (PlatformIO)
