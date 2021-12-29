# Adio-MIDI-Footswitch
# BLE MIDI Footswitch Project for VOX Adio Air GT/BS Amp 

This project is intended to build a simple wireless footswitch for the Vox Adio Air GT/BS Amp based on the ESP32 microcontroller with integrated Bluetooth funtionality. In the current version the code supports two buttons: The first button is implemented to increment the effect-nummber, the second button to decrement the effect-number. A long press on the increment-button sets the effect back to the first bank in the Adio Amp.

A seven segment LED-display shows the current effect that is selected, another optional LED toogles on/off with every button-press event.

The code is able to handle MIDI effect-switching-events form the ADIO Amp to be able to synchronize the selected effect between footswitch and Adio Amp. Is uses the powerful BLE-MIDI library from @lathoub (https://github.com/lathoub/Arduino-BLE-MIDI) - with one modification: When reconncting the Adio-Amp with the ESP32 the bluetooth client cannot be re-used and must be initiated from scratch. This can be implemented by deleting and creating the client in the disconnt callback routine of the library. It seems that the Adio Amp is a kind of sensitive MIDI server - in the connection phase a delay must be implemented with the right timing in order enable the Adio Amp to send MIDI callback messages back to the ESP32 module. With the current delay settings this works reliable

To implement a footswitch on your own you really do not need many components: Mandatory is an ESP32 microcontroller with two push buttons for effect-switching. Optional is a LED 7-segement display with 8 resistors for each LED-segment. Another optional LED connected with a resistor to the ESP32 indicates a button-press-evvent by toogling on/off. Power supply can be implemented by an USB-cable or an external battery module.

If I find the time I will design a tiny PCB that hosts the ESP32 and the external components. Stay tuned ...
