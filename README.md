# WeatherStationAndAlarm
3in1: esp8266 weather station using Open-Meteo API, 433 MHz alarm system with Telegram Bot, ENS160 air quality sensor for indoor

 ![alt text](https://github.com/AlexeyMal/WeatherStationAndAlarm/blob/main/photos/IMG_3806.jpg)
 ![alt text](https://github.com/AlexeyMal/WeatherStationAndAlarm/blob/main/photos/IMG_3949.jpg)
 ![alt text](https://github.com/AlexeyMal/WeatherStationAndAlarm/blob/main/photos/telegram.png)
 
 Can be programmed to ESP8266 using Arduino IDE.
 
 Based on
 https://github.com/ThingPulse/esp8266-weather-station
 Here you can find detailed instructions on building your weather station hardware if you need.
 The disadvantage of the ThingPulse code is that it uses OpenWeatherMap APIs that are limited and require user registration.
 Up to now I have been using OpenWeatherMap OneCall API 2.5 but they have discontinued serving it on 17 Sept 2024, suggesting to change to their paid API 3.0.
 As OpenWeatherMap require a (payed) subscription for API 3.0 with a credit card I have decided to change the provider and just reprogrammed my weather station to use a new, free and flexible open-meteo API.
 
 Features you get with my code: 
 - free Open-Meteo API (see https://open-meteo.com/en/docs)
 - ENS160 air quality sensor for indoor temperature, humidity, air quality 
 - 433 MHz alarm system with Telegram Bot
 - hourly weather forecast for 8 hours with rain amount
 - daily weather forecast for 8 days with min, max temperatures
 - hand-polished fonts and meteoicons
 
 Usable display: SSD1306 0.96 inch OLED or SH1106 1.3 inch OLED.
 
 Parts: 
  - SSD1306 0.96 inch OLED or SH1106 1.3 inch OLED
  - esp8266 module
  - optional: ENS160 sensor
  - optional: 433 MHz receiver

 You have to set the following things in the WeatherStation.ino file to make it work for you:
  - Search for "XXX" and "000" in files and replace with your own keys!
  - Your WiFi SSID and password - WIFI_SSID and WIFI_PWD
  - Your Latitude and Longitude - OPEN_WEATHER_MAP_LOCATTION_LAT and OPEN_WEATHER_MAP_LOCATTION_LON
  - Your timezone in line setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 0);
  - Your timezone in function OpenWeatherMapOneCall::buildPath() like timezone=Europe%2FBerlin
  - Your display - #define SSD1306 or #define SH1106
  - Your SDA_PIN and SDC_PIN for the display (and optional ENS160 sensor)
  - Your telegram token and user id
  - Your 433MHz devices RF codes and names
 
 Enjoy!
 (2026) Alexey