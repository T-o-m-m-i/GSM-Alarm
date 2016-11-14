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
char sms_received_msg[SMS_LENGTH];

char sms_requesting_number[NUMBER_LENGTH];

char call_received_number[NUMBER_LENGTH];
byte call_received_status = 0;
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

	if (gsm.begin(9600))
	{
		Serial.println("\nstatus=READY");
		started = true;
	} else
	{
		Serial.println("\nstatus=IDLE");
		started = false;
	}
	if(started)
	{
		//Check if the right numbers are stored in SIM. If not, store them.
		for(unsigned int i = 0; i < NUMBERS; i++)
		{
			if(gsm.ComparePhoneNumber(i+1, number[i]) == 0)
			{
				if(gsm.DelPhoneNumber(i+1))
				{
					gsm.WritePhoneNumber(i+1, number[i]);
				}
			}
		}
	}

}

void loop()
{
	if(started)
	{
		//Chekcs the status of the incoming call
		call_received_status = call.CallStatusWithAuth(call_received_number, 1, NUMBERS);

		if(call_received_status == CALL_INCOM_VOICE_AUTH)
		{
			Serial.println("Auth nro. : " + String(call_received_number));
			delay(2000);

			call.PickUp();
			delay(1000);
			call.HangUp();

			delay(2000);

			//Send an SMS to the calling number with the values red previously.
			String message = "Lämpötila: " + String(temperature) +
							"\nKosteus: " + String(humidity);
			message.toCharArray(smsbuffer, 100);
			Serial.println(smsbuffer);
			//sms.SendSMS(number, smsbuffer);

		}
		else if(call_received_status == CALL_INCOM_VOICE_NOT_AUTH)
		{
			Serial.println("Ei Auth nro. : " + String(call_received_number));
			delay(2000);

			Serial.println("Picking Up!");
			call.PickUp();
			Serial.println("Hanging Up!");
			call.HangUp();

			delay(2000);
		}
		else if(call_received_status == CALL_NONE)
		{
			Serial.println("Ei soittoa.");
		}
		delay(2000);

		//Check if there is unread messages
		sms_received_position = sms.IsSMSPresent(SMS_UNREAD);

		if (sms_received_position)
		{
			// Read the new SMS
			sms.GetSMS(sms_received_position, sms_received_number, NUMBER_LENGTH, sms_received_msg, SMS_LENGTH);

			//SALDO
			if((sms_received_number == number[0]) || (sms_received_number == number[1]))
			{
				if(strcasecmp(sms_received_msg, "saldo") == 0)
				{
					//Save requesting number
					sms_requesting_number = sms_received_number;

					//Send SALDO to the operator
					sms.SendSMS(OPERATOR, "PREPAID SALDO");
				}
			}
			else if(sms_received_number == OPERATOR)
			{
				sms.SendSMS(sms_requesting_number, sms_received_msg);
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
					Serial.println("Something wrong with the DHT.");
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
					//Send an SMS alarm
					String message = "KYLMÄÄ! \nLämpötila: " + String(temperature) +
												"\nKosteus: " + String(humidity);
					message.toCharArray(smsbuffer,SMS_LENGTH);
					Serial.println(smsbuffer);
					//sms.SendSMS(number[0], smsbuffer);
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
}
