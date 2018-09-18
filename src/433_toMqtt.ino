
/*
  Written by: James moverley
  Description: 433 MIPS receiver (doorbells/pir/etc)

  libraries:
    rc-switch  https://github.com/sui77/rc-switch/
    simpleMap  https://github.com/spacehuhn/SimpleMap
    pubsub     https://github.com/knolleary/pubsubclient

*/

#include <SimpleMap.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <RCSwitch.h>

// wifi settings
const char* ssid = "YOUR_IOT_SSID"; //your WiFi Name
const char* password = "YOUR_SSID_PASSWD";  //Your Wifi Password

// MQTT settings
String mqtt_server = "x.x.x.x"
int mqtt_port = 1883;
String mqtt_clientId = "433_receiver";
String mqtt_base = "sensors";
String mqtt_topic = mqtt_base + "/" + mqtt_clientId + "/";
String msg = "";

// const globals (settings)
const byte interrupt_pin = 4; // d2 on esp12f mini
const byte led_pin = 2; // internal LED pin on esp12g

// globals - settings
// how often whilst waiting for input to do HB indicator
int HB = 10000;
int micro_sleep = 50;
// number of chars on output for HB indicator line wrap
int max_width = 40;
// ms to blink LED on for
int blink_rate = 50;
// ms to ignore button press
int ignore_period = 5000;
// time(ms) to check button ID before triggering
int trigger_chk_count = 3;
// time(ms) allowed for check count increment
int check_ms_gap = 100;

// general globals
long unsigned int last_check_millis = 0;
int cur_width = 0;

struct device_info {
  long unsigned int last_millis;
  long unsigned int last_triggered;
  byte chk_count;
};

// complex type globals
WiFiClient espClient;
PubSubClient client(espClient);
RCSwitch my_switch = RCSwitch();
SimpleMap<String, device_info>* my_devices;

//---- setup -----------------------------------------------------------------------
void setup() {
  // setup serial port for debug output
  Serial.begin(9600);
  
  //##################################
  Serial.println();
  Serial.println("================================");
  Serial.println("[SETUP] Starting");
  Serial.println("--------------------------------");
  
  // setup WIFI
  init_wifi();

  // mDNS MQTT setup
  init_mqtt();
   
  // setup GPIO
  pinMode(led_pin, OUTPUT);
  digitalWrite(led_pin, HIGH);
  
  // initialise hashmap (will store button id's and last time detected)
  my_devices = new SimpleMap<String, device_info>([](String & a, String & b) -> int {
      if (a == b) return 0;
      if (a > b) return 1;
      /*if (a < b) */ return -1;
  });

  // enable 433 listener (interrupt driven)
  my_switch.enableReceive(interrupt_pin);

  // take timestamp - so we can time how long idle
  last_check_millis = millis();

  // debug output to indicate readiness
  Serial.println("[SETUP] Complete, Receiver ready");
  Serial.println("--------------------------------");
}

//---- loop ------------------------------------------------------------------------
void loop() {
  // call MQTT loop to handle active connection
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // device "HeartBeat" blink LED/debug output regularly to show we're still ticking
  if ( millis() - last_check_millis > HB ) {
    Serial.print (".");
    ledBlink(50);
    last_check_millis = millis();

    // line wrap handler for debug output
    cur_width++;
    if (cur_width > max_width) {
      Serial.println();
      cur_width = 0;
    }
  } // endif

  // check if data has been received from the 433 receiver and buffered for pickup!
  if (my_switch.available()) {

    // have we seen this device before/within time threshold?
    // - MIPS doorbells announce multiple times in single press ^.^
    if ( checkDeviceEvent(String(my_switch.getReceivedValue())) ) {
      // we're interested in this event, lets do stuff!

      String device_val = String(my_switch.getReceivedValue());
      // debug output useful infos
      Serial.println();
      Serial.print("[433] Triggered: id[");
      Serial.print( device_val );
      Serial.print("] ");
      Serial.print( my_switch.getReceivedBitlength() );
      Serial.print("bit Proto: ");
      Serial.println( my_switch.getReceivedProtocol() );


      // MQTT stuff here
      client.publish(String(mqtt_topic + device_val).c_str(), "on");
      // play noise here

      // flash LED to indicate we're got a press we are triggering
      // do this last as it takes time to execute..
      doBlinky(3);

    } else {
      // not interested in this event - so ignoring it
      // output a marker to show something came in..
      Serial.print("*");
    }

    // reset 433 receiver ready for next device detection
    my_switch.resetAvailable();
  }
  delay (micro_sleep);
}

//---- functions -------------------------------------------------------------------

// check if we want to react to device event or note
// also update timestamp of device last seen time
bool checkDeviceEvent (String device_id) {
  // DONE: verify button with triple check of id
  // - MIP device will announce ID multiple times quickly, this is deal with clashes
  // -  we can use this to verify ID before triggering.. 

  // temp struct var
  device_info device;
  
  // have we seen this device already?
  if ( my_devices->has(device_id)) {
    // we HAVE seen this device before - was it recently?
    if ( millis() - my_devices->get(device_id).last_triggered > ignore_period ) {
      // ahh we're interested in this event for count check
      // update map with latest millis
      device = my_devices->get(device_id);
      
      if ( my_devices->get(device_id).chk_count > trigger_chk_count ){
        // we've seen this device announce more than # times, time to trigger!
        // reset device info. set timestamps
        device.last_millis=millis();
        device.last_triggered=millis();
        device.chk_count=0;
        my_devices->put(device_id, device);
        return true;
        
      } else {  // trigger count check
        // we've not seen this device announce enough yet, increment count check
        // but before doing so, make sure last increment was recent - not some time ago..
        if ( millis() - device.last_millis < check_ms_gap ){
          device.chk_count++;
        } else {
          device.chk_count=1;
        }
        
        device.last_millis=millis();
        my_devices->put(device_id, device);
        return false;
      }
      
    } else {
      // repeat event and to be ignored
      return false;
    } // end else
    
  } else {
    // device_id not in list... new kid on the block, lets add you to our list with a timestamp
    device_info device = {millis(),0,0};
    my_devices->put(device_id, device);
    
    // output some meaningful debug
    Serial.println();
    Serial.print("New device ID: ");
    Serial.println(device_id);

    return false;
  } // endif
} // endfunc

// led blink function
void ledBlink(int duration_ms) {
  digitalWrite(led_pin, LOW);
  delay(duration_ms);
  digitalWrite(led_pin, HIGH);
} // endfunc

// small blinky function
void doBlinky(int blinks) {
  for ( int i = 0; i < blinks; i++ ) {
    ledBlink(blink_rate);
    delay(blink_rate);
  } // end for
} // endfunc


void mqtt_callback(char* topic, byte * payload, unsigned int length) {
  Serial.print("[MQTT] Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

// mqtt stuff
void reconnect() {
  int exit = 1;
  Serial.println();
  Serial.print("[MQTT] Not connected! Attempting new connection: ");
  while (!client.connected() && exit == 1 ) {
    Serial.print("#");
    if (client.connect(mqtt_clientId.c_str())) {
      Serial.println("> OK");
      // Once connected, publish an announcement...
      String connected_msg = mqtt_clientId + " joined";
      client.publish("clientJoin", connected_msg.c_str());
      client.loop();
      // ... and resubscribe - need to handle SUBACK!
      //client.subscribe("inTopic");
      //client.loop();
    } else {
      Serial.print("> FAIL, rc=");
      Serial.print(client.state());
      Serial.println(" retry in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void init_wifi() {
  //##################################
  // connect to wifi
  Serial.println("[WIFI] initialising");
  Serial.print("[WIFI] MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("[WIFI] SSID:");
  Serial.println(ssid);

  Serial.print("[WIFI] Attaching: ");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("[WIFI] connected IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("[WIFI] Setup complete");
}

void init_mqtt() {
  // mDNS to discover MQTT server
  if (!MDNS.begin("ESP")) {
    Serial.println("[mDNS] Error setting up mDNS");
  } else {
    Serial.println("[mDNS] Setup - Sending Query");
    int n = MDNS.queryService("mqtt", "tcp");
    if (n == 0) {
      Serial.println("[mDNS] No service found");
    } else {
      // at least one MQTT service is found
      // ip no and port of the first one is MDNS.IP(0) and MDNS.port(0)
      mqtt_server = MDNS.IP(0).toString();
      mqtt_port = MDNS.port(0);
      Serial.print("[mDNS] Service discovered: ");
      Serial.print(mqtt_server);
      Serial.print(":");
      Serial.println(mqtt_port);
      
    }
  }

  // mqtt setup (setup unqiue client id from mac
  Serial.println("[MQTT] initiliasing");
  Serial.print("[MQTT] topic: ");
  Serial.println(mqtt_topic);
  client.setServer(mqtt_server.c_str(), mqtt_port);
  client.setCallback(mqtt_callback);
  //nb mqtt connection is handled later..
}

