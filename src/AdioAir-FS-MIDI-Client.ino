/**
   --------------------------------------------------------
   This example shows how to use client MidiBLE
   Client BLEMIDI works im a similar way Server (Common) BLEMIDI, but with some exception.

   The most importart exception is read() method. This function works as usual, but
   now it manages machine-states BLE connection too. The
   read() function must be called several times continuously in order to scan BLE device
   and connect with the server. In this example, read() is called in a "multitask function of
   FreeRTOS", but it can be called in loop() function as usual.

   Some BLEMIDI_CREATE_INSTANCE() are added in MidiBLE-Client to be able to choose a specific server to connect
   or to connect to the first server which has the MIDI characteristic. You can choose the server by typing in the name field
   the name of the server or the BLE address of the server. If you want to connect
   to the first MIDI server BLE found by the device, you just have to set the name field empty ("").

   FOR ADVANCED USERS: Other advanced BLE configurations can be changed in hardware/BLEMIDI_Client_ESP32.h
   #defines in the head of the file (IMPORTANT: Only the first user defines must be modified). These configurations
   are related to security (password, pairing and securityCallback()), communication params, the device name
   and other stuffs. Modify defines at your own risk.



   @auth RobertoHE
   --------------------------------------------------------
*/

// Uncomment for DEBUG-messages on serial port
#define MY_ADIO_DEBUG

#include <Arduino.h>

#include <avdweb_Switch.h>
#include <AdvancedSevenSegment.h>

#include <BLEMIDI_Transport.h>
#include <hardware/BLEMIDI_Client_ESP32.h>

BLEMIDI_CREATE_INSTANCE("Adio Air GT MIDI", MIDI)   //Connect to Vox Adio Air BLE-MIDI server 
//BLEMIDI_CREATE_DEFAULT_INSTANCE();                //Connect to first server found
//BLEMIDI_CREATE_INSTANCE("",MIDI)                  //Connect to the first server found
//BLEMIDI_CREATE_INSTANCE("f2:c1:d9:36:e7:6b",MIDI) //Connect to a specific BLE address server
//BLEMIDI_CREATE_INSTANCE("MyBLEserver",MIDI)       //Connect to a specific name server

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 //modify for match with yout board
#endif

#define PIN_LED    4
#define PIN_BUTTON_UP 13
#define PIN_BUTTON_DOWN 12
#define MAX_EFFECT_COUNT 8

#define SEG_G 21
#define SEG_F 19
#define SEG_A 18
#define SEG_B 5
#define SEG_E 32
#define SEG_D 33
#define SEG_C 26
#define SEG_DP 27

AdvanceSevenSegment sevenSegment(SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G, SEG_DP);

Switch pushButtonUp = Switch(PIN_BUTTON_UP);
Switch pushButtonDown = Switch(PIN_BUTTON_DOWN);

void ReadCB(void *parameter);       //Continuos Read function (See FreeRTOS multitasks)

unsigned long t0 = millis();
bool isConnected = false;

byte adioEffect0[] = { 0xF0, 0x42, 0x30, 0x00, 0x01, 0x41, 0x4E, 0x00, 0x00, 0xF7 };
byte adioEffect[] = { 0xF0, 0x42, 0x30, 0x00, 0x01, 0x41, 0x4E, 0x00, 0x00, 0xF7 };
byte adioDeviceInquiry[] = { 0xF0, 0x7E, 0x7F, 0x06, 0x01, 0xF7 };
byte adioModeRequest[] = { 0xF0, 0x42, 0x30, 0x00, 0x01, 0x41, 0x12, 0xF7 };
byte adioModeData[] = { 0xF0, 0x42, 0x30, 0x00, 0x01, 0x41, 0x42, 0x00, 0x00, 0xF7 };
byte adioGlobalDataDumpRequest[] = { 0xF0, 0x42, 0x30, 0x00, 0x01, 0x41, 0x0E, 0xF7 };


byte CurrentEffect = 0;
int LEDState = LOW;
bool FirstRun = true;

/**
   -----------------------------------------------------------------------------
   When BLE is connected, LED will turn on (indicating that connection was successful)
   When receiving a NoteOn, LED will go out, on NoteOff, light comes back on.
   This is an easy and conveniant way to show that the connection is alive and working.
   -----------------------------------------------------------------------------
*/
void setup()
{
  pinMode(PIN_LED, OUTPUT);
  Serial.begin(115200);
  MIDI.begin(MIDI_CHANNEL_OMNI);

  sevenSegment.clean();
  sevenSegment.setNumber(0);

  BLEMIDI.setHandleConnected([]()
  {
    Serial.println("---------CONNECTED---------");
    isConnected = true;
    digitalWrite(LED_BUILTIN, HIGH);
    FirstRun = true;
    // MicroMidi: Define the start of the 400ms delay the Vox amp needs before accepting MIDI messages
    t0 = millis();
  });

  BLEMIDI.setHandleDisconnected([]()
  {
    Serial.println("---------NOT CONNECTED---------");
    isConnected = false;
    digitalWrite(LED_BUILTIN, LOW);
    FirstRun = true;
    sevenSegment.setNumber(0);
  });

  MIDI.setHandleSystemExclusive([](byte * SysExArray, unsigned SysExSize) {
    int adioBank = 0;
#ifdef MY_ADIO_DEBUG
    Serial.print(F("SYSEX: ("));
    Serial.print(SysExSize);
    Serial.print(F(" bytes) "));
    for (uint16_t i = 0; i < SysExSize; i++)
    {
      Serial.print(SysExArray[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
#endif
    for (uint16_t i = 0; i < SysExSize && ((SysExArray[i] == adioEffect0[i]) || (SysExArray[i] == adioModeData[i])); i++)
    {
      adioBank++;
    }
    if (adioBank > 7) {
      Serial.print("Bank Switched at Vox: ");
      Serial.println(CurrentEffect);
      CurrentEffect = SysExArray[8];
      sevenSegment.setNumber(CurrentEffect + 1);
    }
  });


  xTaskCreatePinnedToCore(ReadCB,           //See FreeRTOS for more multitask info
                          "MIDI-READ",
                          3000,
                          NULL,
                          1,
                          NULL,
                          1); //Core0 or Core1

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
}

// -----------------------------------------------------------------------------
//
// -----------------------------------------------------------------------------
void loop()
{
  // MIDI.read();  // This function is called in the other task

  if (isConnected )
  {
    //MicroMidi: Wait 400ms after first connection was established
    if (FirstRun == true && (millis() - t0) > 400) {
      FirstRun = false;
#ifdef MY_ADIO_DEBUG
      Serial.println("First run");
      Serial.print("millis(): ");
      Serial.println(millis());
      Serial.print("t0: ");
      Serial.println(t0);
#endif

      MIDI.sendSysEx(sizeof(adioDeviceInquiry), adioDeviceInquiry, true);
      MIDI.sendSysEx(sizeof(adioModeRequest), adioModeRequest, true);
      // MIDI.sendSysEx(sizeof(adioEffect0), adioEffect0, true);

      sevenSegment.setNumber(1);
    }

    pushButtonUp.poll();

    if (pushButtonUp.longPress()) {
      if (LEDState == LOW)
        LEDState = HIGH;
      else
        LEDState = LOW;

      digitalWrite(PIN_LED, LEDState);
      CurrentEffect = 0;

      adioEffect[8] = CurrentEffect;
      MIDI.sendSysEx(sizeof(adioEffect), adioEffect, true);
      sevenSegment.setNumber(CurrentEffect + 1);
    }

    if (pushButtonUp.pushed()) {
      if (LEDState == LOW)
        LEDState = HIGH;
      else
        LEDState = LOW;

      digitalWrite(PIN_LED, LEDState);

      if (CurrentEffect < (MAX_EFFECT_COUNT - 1))
        CurrentEffect++;
      else
        CurrentEffect = 0;

      adioEffect[8] = CurrentEffect;
      MIDI.sendSysEx(sizeof(adioEffect), adioEffect, true);
      sevenSegment.setNumber(CurrentEffect + 1);
    }

    pushButtonDown.poll();

    if (pushButtonDown.pushed()) {
      if (LEDState == LOW)
        LEDState = HIGH;
      else
        LEDState = LOW;

      digitalWrite(PIN_LED, LEDState);

      if (CurrentEffect > 0)
        CurrentEffect--;
      else
        CurrentEffect = (MAX_EFFECT_COUNT - 1);

      adioEffect[8] = CurrentEffect;
      MIDI.sendSysEx(sizeof(adioEffect), adioEffect, true);
      sevenSegment.setNumber(CurrentEffect + 1);
    }
  }
}

/**
   This function is called by xTaskCreatePinnedToCore() to perform a multitask execution.
   In this task, read() is called every millisecond (approx.).
   read() function performs connection, reconnection and scan-BLE functions.
   Call read() method repeatedly to perform a successfull connection with the server
   in case connection is lost.
*/
void ReadCB(void *parameter)
{
  //  Serial.print("READ Task is started on core: ");
  //  Serial.println(xPortGetCoreID());
  for (;;)
  {
    // MIDI.read();

    if (MIDI.read(MIDI_CHANNEL_OMNI))                // Is there a MIDI message incoming ?
    {
#ifdef MY_ADIO_DEBUG
      Serial.println("MIDI message received!");
      switch (MIDI.getType())     // Get the type of the message we caught
      {
        case midi::SystemExclusive:       // If it is a SysEx message,
          Serial.println("Message-Type SysEx");
          break;
        case midi::ControlChange:       // If it is a ControlChange,
          Serial.println("Message-Type ControlChange");
          break;
        // See the online reference for other message types
        default:
          break;
      }
#endif
    }
    vTaskDelay(1 / portTICK_PERIOD_MS); //Feed the watchdog of FreeRTOS.
    //Serial.println(uxTaskGetStackHighWaterMark(NULL)); //Only for debug. You can see the watermark of the free resources assigned by the xTaskCreatePinnedToCore() function.
  }
  vTaskDelay(1);
}
