#include "Arduino.h"
#include "DHT.h"
#include "SoftwareSerial.h"

#include "Libs/SIM900.h"
#include "Libs/inetGSM.h"
#include "Libs/sms.h"
#include "Libs/call.h"

#include "numbers.h"


CallGSM call;
SMSGSM sms;

#define DHTPIN 2
#define DHTTYPE DHT22

#define NUMBER_LENGTH 20
#define SMS_LENGTH 100

DHT dht(DHTPIN, DHTTYPE);


bool started=false;
char smsbuffer[SMS_LENGTH];

char sms_received_position;
char sms_received_number[NUMBER_LENGTH];
char sms_received[SMS_LENGTH];

char num[NUMBER_LENGTH];
byte stat = 0;
int value = 0;
int pin = 1;
char value_str[5];

float temperature = 0;
float humidity = 0;

bool warnSent = false;
byte failedDHT = 0;

const unsigned long interval = 60000;  //1min
unsigned long previousMillis = 0;

void setup()
{
	dht.begin();

	Serial.flush();
	//Serial connection.
	Serial.begin(9600);
	Serial.println("GSM Shield testing.");
	//Start configuration of shield with baudrate.
	//For http uses is raccomanded to use 4800 or slower.
	if (gsm.begin(9600))
	{
		Serial.println("\nstatus=READY");
		started=true;
	} else
		Serial.println("\nstatus=IDLE");

	if(started)
	{
		//Check if the right number is stored in SIM if not then store them
		for(unsigned int i = 0; i < numbers; i++)
		{
			if(gsm.ComparePhoneNumber(i+1, number[i]) == 0)
			{
				if(gsm.DelPhoneNumber(i+1))
				{
					gsm.WritePhoneNumber(i+1, number[i]);
				}
			}
		}
		//Enable this two lines if you want to send an SMS.
		//if (sms.SendSMS("3471234567", "Arduino SMS"))
		//Serial.println("\nSMS sent OK");
	}

};

void loop()
{
	if(started)
	{
		//Chekcs status of call
		stat=call.CallStatusWithAuth(num,1,numbers);
		//If the incoming call is from an authorized number
		//saved on SIM in the positions range from 1 to 3.
		if(stat==CALL_INCOM_VOICE_AUTH)
		{
			Serial.println("Auth nro. : " + String(num));
			delay(2000);

			call.PickUp();
			delay(1000);
			call.HangUp();

			delay(2000);

			//Send an SMS to the calling number with the values red previously.
			String message = "Lämpötila: " + String(temperature) +
							"\nKosteus: " + String(humidity);
			message.toCharArray(smsbuffer,100);
			Serial.println(smsbuffer);
			//sms.SendSMS(number, smsbuffer);

		}
		else if(stat == CALL_INCOM_VOICE_NOT_AUTH)
		{
			Serial.println("Ei Auth nro. : " + String(num));
			delay(2000);

			Serial.println("Picking Up!");
			call.PickUp();
			Serial.println("Hanging Up!");
			call.HangUp();

			delay(2000);
		}
		else if(stat == CALL_NONE)
		{
			Serial.println("Ei soittoa.");
		}
		delay(2000);

		//Check if there is unread messages
		sms_received_position = sms.IsSMSPresent(SMS_UNREAD);
		if (sms_received_position)
		{
			// read new SMS
			sms.GetSMS(sms_received_position, sms_received_number, NUMBER_LENGTH, sms_received, SMS_LENGTH);
			// now we have phone number string in phone_num
			// and SMS text in sms_text

			//SALDO
			if((sms_received_number == number[0]) || (sms_received_number == number[1]))
			{

			}
		}

		delay(2000);

		//Measuring
		if ((unsigned long)(millis() - previousMillis) >= interval)
		{
			// Read temperature as Celsius
			temperature = dht.readTemperature(false, true);
			humidity = dht.readHumidity(false);

			// Check if any reads failed and exit early (to try again).
			if (isnan(humidity) || isnan(temperature))
			{
				Serial.println("Failed to read from DHT sensor!");
				delay(1000);

				failedDHT++;
				if(failedDHT > 20)
				{
					Serial.println("Jotaki häikkää DHT-sensorissa.");
					failedDHT = 0;
				}
				return;
			}
			else
			{
				failedDHT = 0;

				Serial.print("Humidity: ");
				Serial.print(humidity);
				Serial.print(" %\t");
				Serial.print("Temperature: ");
				Serial.print(temperature);
				Serial.println(" *C ");

				if(temperature <= 14.0 && warnSent == false)
				{
					//Send an SMS
					/*String message = "KYLMÄÄ! \nLämpötila: " + String(temperature) +
												"\nKosteus: " + String(humidity);
					message.toCharArray(smsbuffer,160);
					Serial.println(smsbuffer);
					sms.SendSMS("+3581234567", smsbuffer);*/
					Serial.println("Kylmääää! " + String(temperature) + " C");
					warnSent = true;
				}
				if(temperature >= 18.0 && warnSent == true)
				{
					Serial.println("Lämminnyt yli " + String(temperature) + " asteen.");
					warnSent = false;
				}

				previousMillis = millis();

				Serial.println("LÄHETETÄÄN PILVEEN!");
			}
		}
     }
};
