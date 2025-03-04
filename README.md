# Sentinel esp wroom 32e implementation
## General thoughts

The sentinel sleeps, wakes up, reads the sensors, sends the data to the server, and goes back to sleep. More or less.
The sentinal shuold know if the rasp4 is asleep (or such) and only then store data on the flash. There's a command to flush the data from the flash to the serial port, which then updates the rasp to non-critical issues which have happened when it was sleeping.

## sensors

#### Sound sensor
The sentinel has at a sound detector which is manually calibrated in the HW module. It will make an event when the sounds go over the configured threshold. 

#### Microwave radar
The microwave radar is used to detect movement. It will make an event when the movement is detected.

#### Photoresistor
The photoresistor is used to detect the light level. It can publish this as data into serial and write a log entry into the flash.

#### IR receiver
Not a sensor in the sense that it would be used to detect the environment. It is used to receive commands from the remote control.
It is also used to notice unknown IR signals and log them into the flash and/or serial.

## Comms plan

The problem is that the sensor data as well as the debug data are both pushed into serial. The node on the ros2 side must have a method of distinguishing which is which. The messages are prefixed to denote the type of the message. This interface doesn't distinguish between ros2 topics nor types. It only serves to talk from the hw to the ros2 node, called sentinel_node.

#### Command and Response Prefixes

| Prefix | Description          |
|--------|----------------------|
| `#`    | Sensor data          |
| `!`    | Command              |
| `+`    | Response             |
| `-`    | Debug Message        |



## Storing data on flash

Currently there's only a single file called **event_log.txt**.

| date_time | timestamp | event_type | sensor/topic | message |
|-----------|-----------|------------|--------------|---------|
| Date and time of the event.  | Unix timestamp of the event | Type of event (e.g., sensor reading, command) | Sensor or topic related to the event | The actual message content |


The HW is ESP32-WROOM-32E

initialize the module with
!sensors_on
!flush_data_to_serial
!flash_on
!serial_data_on
!sensors_off
!flash_off
!serial_data_off

!reboot

Echoes "!hello" back
!hello

A command begins with !
An answer begins with ?
Ignore messages which begin "--" these are debug messages begin with --