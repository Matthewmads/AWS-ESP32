#include <stdio.h>
#include <stdlib.h>
#include <esp32>

#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <dht.h>

#include <SimpleButton.h>
using namespace simplebutton;

#define BUTTON 0 //Built In Button
Button* buttonUser = NULL;

//#define LED 4 // can't see the builtin LED because it's on the back of the board :facepalm:


//DHT sensor attached to gpio 4
#define dhttype DHT11
#define dhtpin 4

uint32_t delayMS;



// define two tasks for Blink & LMIC
void TaskButton( void *pvParameters );
void TaskLMIC( void *pvParameters );
void TaskSensor( void *pvParameters)

//Key endian is incredibly important. DeviceEUI and App EUI should be Least Significant Byte (lsb). AppKey should be Most Significant Byte (msb)

static const u1_t PROGMEM DEVEUI[8]= {0x60, 0x81, 0xF9, 0xDF, 0x62, 0xE9, 0x0F, 0x36}; //Device EUI (lsb)
static const u1_t PROGMEM APPEUI[8]= {0x60, 0x81, 0xF9, 0x36, 0x61, 0x38, 0xE1, 0x4A}; //App EUI (lsb)
static const u1_t PROGMEM APPKEY[16] = {0x32, 0xCE, 0xBF, 0x4B, 0x7A, 0xF1, 0x3D, 0x96, 0xB1, 0x01, 0xA9, 0x5B, 0x9C, 0xA3, 0xC7, 0x8D}; //App Key (msb)
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16);}

//TTGO ESP 32
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26, 33, 32}
};

static uint8_t mydata[] = "Signal Sent!";
static osjob_t sendjob;

void do_send(osjob_t* j) { // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else { // Prepare upstream data transmission at the next possible time.
        LMIC_setTxData2(1, mydata, sizeof(mydata)-1, 0);
        Serial.println(F("Packet queued"));
    } // Next TX is scheduled after TX_COMPLETE event.
}

void printHex2(unsigned v) {
    v &= 0xff;
    if (v < 16)
        Serial.print('0');
    Serial.print(v, HEX);
}

const unsigned TX_INTERVAL = 60*60; //Send a packet every hour

void onEvent (ev_t ev) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            {
              u4_t netid = 0;
              devaddr_t devaddr = 0;
              u1_t nwkKey[16];
              u1_t artKey[16];
              LMIC_getSessionKeys(&netid, &devaddr, nwkKey, artKey);
              Serial.print("netid: ");
              Serial.println(netid, DEC);
              Serial.print("devaddr: ");
              Serial.println(devaddr, HEX);
              Serial.print("AppSKey: ");
              for (size_t i=0; i<sizeof(artKey); ++i) {
                if (i != 0)
                  Serial.print("-");
                printHex2(artKey[i]);
              }
              Serial.println("");
              Serial.print("NwkSKey: ");
              for (size_t i=0; i<sizeof(nwkKey); ++i) {
                      if (i != 0)
                              Serial.print("-");
                      printHex2(nwkKey[i]);
              }
              Serial.println();
            }
            // Disable link check validation (automatically enabled
            // during join, but because slow data rates change max TX
            // size, we don't use it in this example.
            LMIC_setLinkCheckMode(0);
            break;

         case EV_RFU1:
             Serial.println(F("EV_RFU1"));
             break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            digitalWrite(LED_BUILTIN, LOW);
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            digitalWrite(BUILTIN_LED, LOW); //Turn LED off
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
        case EV_SCAN_FOUND:
            Serial.println(F("EV_SCAN_FOUND"));
            break;
        case EV_TXSTART:
            Serial.println(F("EV_TXSTART"));
            digitalWrite(BUILTIN_LED, HIGH); //Turn LED on
            break;
        case EV_TXCANCELED:
            Serial.println(F("EV_TXCANCELED"));
            break;
        case EV_RXSTART:
            /* do not print anything -- it wrecks timing */
            break;
        case EV_JOIN_TXCOMPLETE:
            Serial.println(F("EV_JOIN_TXCOMPLETE: Join not Accepted"));
            break;

        default:
            Serial.print(F("Unknown event: "));
            Serial.println((unsigned) ev);
            break;
    }
}


void TaskSensor(void *pvParameters)
{
    int16_t temperature = 0;
    int16_t humidity = 0;

    // DHT sensors that come mounted on a PCB generally have
    // pull-up resistors on the data pin.  It is recommended
    // to provide an external pull-up resistor otherwise...

    //gpio_set_pullup (dhtpin, false, false); //

    while(1) {
        if ((dhttype, dhtpin, &humidity, &temperature)) {
            printf("Humidity: %d%% Temp: %dC\n", 
                    humidity / 10, 
                    temperature / 10);
        } else {
            printf("Could not read data from sensor\n");
        }

        // Three second delay...
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}




void setup() {
  Serial.begin(9600);

      

  pinMode(BUILTIN_LED, OUTPUT);

  xTaskCreatePinnedToCore(TaskLMIC, "LMIC_Task", 1024, NULL, 1, NULL, 0); //On Core 1
  xTaskCreatePinnedToCore(TaskButton, "TaskButton", 1024, NULL, 2, NULL, 1); //On Core 2
}


void loop() {
  // Delete the arduino loop. Doing work in tasks.
  vTaskDelete(NULL);
}

/*--------------------------------------------------*/
/*---------------------- Tasks ---------------------*/
/*--------------------------------------------------*/

void TaskButton(void *pvParameters) { // Task on core 2
  (void) pvParameters;
  delay(10); //need another task to handle serial printing to avoid collisions.
  Serial.print("Running button check on core: ");
  Serial.println(xPortGetCoreID());

  buttonUser = new ButtonPullup(BUTTON);

  while(true) {
    buttonUser->update();
    if(buttonUser->clicked() || buttonUser->doubleClicked()) {
      do_send(&sendjob);
      Serial.println("Pressed Built In Button");
    }
  }
}

void TaskLMIC(void *pvParameters) {// Task on core 1
  (void) pvParameters;
  Serial.print("Running LMIC Process on core: ");
  Serial.println(xPortGetCoreID());
  // Initialize LMIC
  os_init();
  LMIC_reset();
  LMIC_setClockError(1 * MAX_CLOCK_ERROR / 40);
  LMIC_setLinkCheckMode(0);
  LMIC_setDrTxpow(DR_SF7,14);
  LMIC_selectSubBand(1); //LoRa Band 2, but zero-based index
  do_send(&sendjob);

  while(true) {
    os_runloop_once();
    vTaskDelay(1); //(yield) need this to keep wdt happy.
  }
}



