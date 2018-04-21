//This program was written by Brandon Saffell for use by Kory Easterday.

//Operation
//
//Every 8 second the controller reads the battery voltage and the reed switch.
//The reed switch is correlated to a gate position, and both the battery voltage
//status (high or low) and the gate position are sent to the base station. The
//controller then sleeps for 8 seconds.
//
//If the battery voltage drops below 2.1 volts the system only reports information when
//the gate moves to conserve as much battery power as possible.
//
//The battery voltage is read using a voltage divider with 2 100kohm (I think) resistors
//and is enabled with a transistor to maximize battery life. The voltage divider is only 
//enabled for less than 15 ms out of every 8 seconds. If the voltage drops below 2.1 volts
//the battery will only be read when the gate is operated. The battery is read before the 
//first sleep, so the system will operate normally (i.e. not in low battery mode) after
//the batteries are changed.
//
//Device
//The device is normally powered via 3 AA batteries. The base device is a moteino,
//an arduino compatible microcontroller with a builer in RF transmitter. In addition to
//Vin the battery is connected to A0 for monitoring through a transistor and a 200k voltage 
//divider. This setup minimizes battery loss though measurement circuitry. A reed switch 
//is connected from the 3.3 output to the D5 pin to determine if the gate is open.

// **********************************************************************************
// License
// **********************************************************************************
// This program is free software; you can redistribute it 
// and/or modify it under the terms of the GNU General    
// Public License as published by the Free Software       
// Foundation; either version 3 of the License, or        
// (at your option) any later version.                    
//                                                        
// This program is distributed in the hope that it will   
// be useful, but WITHOUT ANY WARRANTY; without even the  
// implied warranty of MERCHANTABILITY or FITNESS FOR A   
// PARTICULAR PURPOSE. See the GNU General Public        
// License for more details.                              
//                                                        
// You should have received a copy of the GNU General    
// Public License along with this program.
// If not, see <http://www.gnu.org/licenses/>.
//                                                        
// Licence can be viewed at                               
// http://www.gnu.org/licenses/gpl-3.0.txt
//
// Please maintain this license information along with authorship
// and copyright notices in any redistribution of this code
// **********************************************************************************

#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <LowPower.h> //get library from: https://github.com/lowpowerlab/lowpower
#include <RFM69_ATC.h>//get it here: https://www.github.com/lowpowerlab/rfm69

//*********************************************************************************************
// *********** IMPORTANT SETTINGS - YOU MUST CHANGE/ONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define NETWORKID     100  //the same on all nodes that talk to each other
#define NODEID        2  
#define RECIEVERID    1
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
#define FREQUENCY     RF69_433MHZ
//#define FREQUENCY     RF69_868MHZ
//#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "Easterday1234567" //exactly the same 16 characters/bytes on all nodes!
//#define IS_RFM69HW    //uncomment only for RFM69HW! Remove/comment if you have RFM69W!
//*********************************************************************************************
#define SERIAL_BAUD   115200

const int SWITCHPIN=3;  //The pin the reed switch is attached to.
const int SWITCHINTERRUPT=1;  //Using interrupt #1
const int BATTERYREADPIN=5;  //Base of battery reading transistor is connected to D5
const int BATTERYSENSEPIN=A0;  //Battery voltage is read with A0
const int LEDPIN=9;  //Internal LED
const float MOTEINOVOLTAGE=3.3;  //Max ADC voltage
const float BATTERYLOWVOLTAGE=2.4;  //0.8 volts per cell. The device will probably operate below this for a time, but cell rupture is risked.
const float BATTERYLOWSHUTDOWN=2.1; //Below this voltage the system goes into a deeper sleep.
const float BATTERYLOWVOLTAGERESET=2.7; //Hystersis
int batterySenseValue=0;  //Output of ADC
volatile int switchStatus=0;  //Used to turn batter monitoring transistor on.
float batterySenseValueFP=0.0;  //Battery sense value as a FP.
float batteryVoltage=0.0;  //Actual battery voltage.

//const byte PRECISION = 3;  //Used for dtostrf() later.
//char floatBuffer[PRECISION+4];

char transmitBuffer[10];  //Buffer to send messages to base station.
int sendSize=0;  //Used for radio transmissions.

RFM69_ATC radio;

void readBattery();  //Prototype Functions
void readSwitch();

void setup() 
{
  //Serial.begin(SERIAL_BAUD); //Debug info
  
  radio.initialize(FREQUENCY,NODEID,NETWORKID); //Startup radio
  radio.encrypt(ENCRYPTKEY);
  char buff[50];
  //sprintf(buff, "\nSending at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  //Serial.println(buff);
  radio.enableAutoPower(-60);
  //Serial.println("RFM69_ATC Enabled (Auto Transmission Control)\n");
  
  pinMode(SWITCHPIN, INPUT);  //Setup control pins.
  pinMode(LEDPIN, OUTPUT);
  pinMode(BATTERYREADPIN, OUTPUT);
}


void loop() 
{
  readBattery();  //Reads and sends the battery voltage.
  readSwitch();  //Reads and sends the reed switch information.
  
  if (radio.ACKRequested())    
  {
    radio.sendACK();
  	//Serial.print(" - ACK sent");
  }
  
  attachInterrupt(SWITCHINTERRUPT, readSwitch, CHANGE);  //Make sure the interrupt is attached.
  radio.sleep();  //Puts radio to sleep.
  if (batteryVoltage>=BATTERYLOWSHUTDOWN)
  {
  	LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_ON); //sleep for 8s
  }
  else
  {
  	LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_ON);
  }
  detachInterrupt(SWITCHINTERRUPT);  //Prevent conflicts while doing other things.
}

void readBattery()
{
	//detachInterrupt(SWITCHINTERRUPT);  //Should already be detached.
	batterySenseValue=analogRead(BATTERYSENSEPIN);  //Read ADC fort he battery
	batterySenseValueFP=float(batterySenseValue);  //Converty to a FP
	batteryVoltage=(batterySenseValueFP*MOTEINOVOLTAGE*2)/1023;  //Convery to a voltage
	
	if (batteryVoltage<BATTERYLOWVOLTAGE)  //Transmits BL if voltage is low.
	{
		sprintf(transmitBuffer, "BL");
		sendSize = strlen(transmitBuffer);
		if (radio.sendWithRetry(RECIEVERID, transmitBuffer, sendSize))  //Battery Voltage is low.
   		{
   			//Serial.print("battery low send ok!");
   		}
  		else
  		{
  			//Serial.print(" nothing...");
  		}
  	}
	else if (batteryVoltage>BATTERYLOWVOLTAGERESET)  //Transmits BH if voltage is high.
	{
		sprintf(transmitBuffer, "BH");
		sendSize = strlen(transmitBuffer);
		if (radio.sendWithRetry(RECIEVERID, transmitBuffer, sendSize))  //Battery Voltage is high.
   		{
   			//Serial.print("battery high send ok!");
   		}
  		else 
  		{
  			//Serial.print(" nothing...");
  		}
  	}
  	/*
  	dtostrf(batteryVoltage, PRECISION+3, PRECISION, floatBuffer);  //Converts battery voltage to a string to be sent out for debugging.
  	sprintf(transmitBuffer, "BV: %s", floatBuffer);
  	Serial.println(transmitBuffer);
  	sendSize = strlen(transmitBuffer);
  	if (radio.sendWithRetry(RECIEVERID, transmitBuffer, sendSize))
   		Serial.print("Battery Voltage send ok!");
  	else Serial.print(" nothing...");
  		Serial.println();
  	*/
	//attachInterrupt(SWITCHINTERRUPT, readSwitch, CHANGE);  //Reenable interrupts.
	
	return;
}

void readSwitch()  //Reads the reed switch.
{
	detachInterrupt(SWITCHINTERRUPT);  //Prevents conflicts.
	
	delay(200);  //Debounce time.
	
	switchStatus=digitalRead(SWITCHPIN);  //Determines if the switch is open or closed.
	
	if (switchStatus==0)  //Gate is open.
	{
		sprintf(transmitBuffer, "GO");
		//Serial.println(transmitBuffer);
		sendSize = strlen(transmitBuffer);
		if (radio.sendWithRetry(RECIEVERID, transmitBuffer, sendSize)) //Gate Open
   		{
   			//Serial.print("gate open send ok!");
   		}
  		else Serial.print(" nothing...");
  		{
  			//Serial.println();
  		}
	}
	else  //Gate is cosed.
	{
		sprintf(transmitBuffer, "GC");
		//Serial.println(transmitBuffer);
		sendSize = strlen(transmitBuffer);
		if (radio.sendWithRetry(RECIEVERID, transmitBuffer, sendSize)) //Gate Closed
   		{
   			//Serial.print("gate closed send ok!");
   		}
  		else Serial.print(" nothing...");
  		{
  			//Serial.println();
  		}
	}
	
	attachInterrupt(SWITCHINTERRUPT, readSwitch, CHANGE); //Reenable interrupts.
	
	return;
}