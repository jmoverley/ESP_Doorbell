ESP_Doorbell
==============
IoT style doorbell using 433 lloydtron MIP recieve and ESP to MQTT
Parts:
  * ESP12F based D1 Mini : https://www.aliexpress.com/item/D1-mini-Mini-NodeMcu-4M-bytes-Lua-WIFI-Internet-of-Things-development-board-based-ESP8266-by/32651747570.html
  * 433 MIP receiver board : https://www.aliexpress.com/item/433Mhz-Superheterodyne-3400-RF-Transmitter-Receiver-Link-Kit-For-Arduino-ARM-MCU/32845551758.html
  * Bit of 17cm wire for antenna ;)

Wiring:
    ESP12F mini d1 used : pin D2 -> 433 receiver data pin, GND/5V
    <TODO: wiring diagram>  


Notables:
---------

* mDNS used allow the device to discover the MQTT service to talk to (config reduction)\
Kudos : http://dagrende.blogspot.com/2017/02/find-mqtt-broker-without-hard-coded-ip.html

create: /etc/avahi/services/mosquitto.service
~~~
---- snip -----
<!DOCTYPE service-group SYSTEM "avahi-service.dtd">
<service-group>
 <name replace-wildcards="yes">Mosquitto MQTT server on %h</name>
  <service>
   <type>_mqtt._tcp</type>
   <port>1883</port>
   <txt-record>info=Publish, Publish! Read all about it! mqtt.org</txt-record>
  </service>

</service-group>
---- snip -----
~~~

* MIP value check : to trigger to MQTT, this code will wait till it see the 433 24bit id announcements 3 times (to deal with situation when 2 devices announce at same time and garble received values..)

* MQTT connect announce

* Heartbeat indicator on internal LED (let you know its still ticking)

* [TODO] : Adding sound trigger and MP3 BOARD
