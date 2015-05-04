pool-safety-device
==================

<iframe width="640" height="480" src="https://sketchfab.com/models/5c79c15477054004a583eb45c18d26ab/embed" frameborder="0" allowfullscreen mozallowfullscreen="true" webkitallowfullscreen="true" onmousewheel=""></iframe>

<p style="font-size: 13px; font-weight: normal; margin: 5px; color: #4A4A4A;">
    <a href="https://sketchfab.com/models/5c79c15477054004a583eb45c18d26ab?utm_source=oembed&utm_medium=embed&utm_campaign=5c79c15477054004a583eb45c18d26ab" target="_blank" style="font-weight: bold; color: #1CAAD9;">Pool Safety Device</a>
    by <a href="https://sketchfab.com/hieu?utm_source=oembed&utm_medium=embed&utm_campaign=5c79c15477054004a583eb45c18d26ab" target="_blank" style="font-weight: bold; color: #1CAAD9;">hieu</a>
    on <a href="https://sketchfab.com?utm_source=oembed&utm_medium=embed&utm_campaign=5c79c15477054004a583eb45c18d26ab" target="_blank" style="font-weight: bold; color: #1CAAD9;">Sketchfab</a>
</p>

## Overview

Collaborating with marketing and fine arts students, we designed a device to prevent children from drowning in home swimming pools. The device consisted of two parts: an external sensor (wristband worn by the user), and an internal control panel (base station mounted in the home).

This project was developed in the spring of 2011 for USC's Embedded System Design Laboratory (EE-459) course, taught by professor Allan Weber. It was a senior capstone design, which involved the specification, design, implementation, testing, and documentation of a digital system project using embedded processors, programmable logic, analog I/O interfaces and application specific hardware. My engineering teammates included Christopher Cortes, Bauryrzhan Krykpayev, Alexander Nobles, Steven Spinn, and Steph Shinohara.

## Hardware
The wristband alert system contains a water-detection circuit, which uses the conductivity of water to bridge the connection between two probes. Once water is detected&mdash;indicating unwanted pool access&mdash;a message is sent to the base station over an RF link, which then sounds an alarm to alert the homeowners. 

### Electrical Prototype
A functional prototype was built on wire-wrapped boards. The wristband prototype housed the op amp based water detection circuit, Freescale MC908JL15 microcontroller, wireless RF transceiver, monochromatic graphic LCD, 4MHz clock, voltage regulator, battery monitor, buzzer, and push-buttons/LEDs for user input/output. 

<img src="http://niftyhedgehog.com/pool-safety-device/images/wristband_prototype.jpg">

The base station prototype included a 24x2 character display, 4x4 keypad interface and encoder, Freescale MC908JL15 microcontroller, wireless RF transceiver, and a high-decibel buzzer.

<img src="http://niftyhedgehog.com/pool-safety-device/images/monitor_prototype.jpg">

A mockup PCB was designed to ensure the prototype could fit on a watchface-sized wristband. Not including the batteries, all of the electrical components can fit on the back of the LCD, which is 45mm x 45mm.  The thickness is about 4mm.

<img src="http://niftyhedgehog.com/pool-safety-device/images/new_wristband.jpg">

### Production Mockup
A production-ready mockup was created in collaboration with marketing and fine arts students. With their expertise, we developed a product that was both commercially viable and aesthetically pleasing.

<img src="http://niftyhedgehog.com/pool-safety-device/images/wristband_mockup.png">

<img src="http://niftyhedgehog.com/pool-safety-device/images/base_station_mockup.png">


## Software

### Wristband
The wristband is used to display and communicate status to a base station monitor. Once the presence of water is detected, the LEDs and buzzer alert the user of unwanted pool access. A GPIO interrupt is then generated on the microcontroller, commanding it to transmit the alert status to the monitor. 

The RFM22B 433MHz ISM band wireless transceiver uses a standard 4-wire SPI interface to communicate with the microcontroller. Because the JL16 does not have dedicated SPI pins, the serial protocol was implemented in software on GPIO lines (bit banging). This entailed writing low-level software functions to perform reading and writing of the transceiver’s registers, which involved controlling the clock, data, and device select pins.

The LCD features a battery life indicator, which displays the output of an 8-bit ADC used to monitor the system voltage input. The display also features a fish animation to entertain the child user. 

<iframe width="560" height="315" src="https://www.youtube.com/embed/oqmXTrBO6uU" frameborder="0" allowfullscreen></iframe>

### Base Station
The RF transceiver is used to provide a wireless communication interface for both the wristband and the base station monitor, allowing transmission of the alarm signal as well as a few other remote functions.

The "Sync" function establishes a connection between the wristband and base station. If the two devices are not synchronized, then any action that the wristband does will not be acted upon by the base station, and vice-versa. The Sync function is achieved through a series of handshaking between the wristband and the base station monitor. Both devices will display a message if the sync is successful.

The "Locate" function is designed to allow the user to find a misplaced wristband by emitting a continuous beep from the wristband. When the user selects the Locate function in the monitor's menu, all wristbands synced with the base station will begin to beep.

When the alarm is enabled, the wristband will send out an alarm packet on the transceiver if it detects water on its water detection circuit. When the base station receives the alarm, it checks its wristband list for a match on the ID bytes. If they match, a warning message is displayed the the buzzer alarm sounds. The user must then enter their password to disable the system, which will stop the alarm.

<img src="http://niftyhedgehog.com/pool-safety-device/images/alert_message.jpg">
