#include "Arduino.h"
#include <SPI.h>
#include <SD.h>
#include "DHT.h"
#include "SoftwareSerial.h"
#include "Libs/SIM900.h"
//#include "Libs/inetGSM.h"
#include "Libs/sms.h"
#include "Libs/call.h"

#include "numbers.h"


CallGSM call;
SMSGSM sms;

#define DHTPIN 2
#define DHTTYPE DHT22

#define NUMBER_LENGTH 20
#define SMS_LENGTH 120

DHT dht(DHTPIN, DHTTYPE);

char gsm_time[22];

bool started = false;
char smsbuffer[SMS_LENGTH];

char sms_received_position;
char sms_received_number[NUMBER_LENGTH];
char sms_received_msg[SMS_LENGTH];

char sms_requesting_number[NUMBER_LENGTH];

char call_received_number[NUMBER_LENGTH];
byte call_received_status;

float temperature = 0;
float humidity = 0;

bool warnSentTemp = false;
bool warnSentAC = false;
bool warnSentDHT = false;
bool requestSMS = false;
byte timesDHTfailed = 0;
bool failedAC = false;

const int chipSelect = 53;  //For SD card.
File root;

const unsigned long interval = 300000;  //5min  15min=900000
unsigned long previousMillis = 300000;

//---------------------------------------------------------------------------------------------
void setup()
{
	//Turn internal pull-ups ON to save power
	for(unsigned int k=0; k<=49; k++)
	{
		if(k != 8 && k != 9)
			pinMode(k,INPUT_PULLUP);
	}

	pinMode(13, OUTPUT);

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
			if(gsm.ComparePhoneNumber(i+1, number[i]) == 1)
			{
				Serial.println("Numero " + (String)number[i] + " oli tallennettu.");
			}
			else
			{
				if(gsm.DelPhoneNumber(i+1))
				{
					gsm.WritePhoneNumber(i+1, number[i]);
					Serial.println("Numero " + (String)number[i] + " tallennettiin.");
				}
			}
		}

		digitalWrite(13, LOW);

		//Delete all SMSs
		/*unsigned int j = 1;
		while(sms.DeleteSMS(j))
		{
		Serial.println("SMS " + (String)j + " poistettiin.");
			j++;
		}*/

		Serial.println("\r\nWaiting for SD card to initialise...");
		if (!SD.begin(chipSelect))
		{
		    Serial.println("Initialisation failed!");
		    return;
		  }
		Serial.println("Initialisation completed.");
	}
}
//---------------------------------------------------------------------------------------------
void loop()
{
	if(started)
	{
		//Chekcs the status of the incoming call
		checkCallStatus();

		//Check unread messages
		checkUnreadMessages();

		//Is AC failed
		//checkAC();


		//Measuring
		if ((unsigned long)(millis() - previousMillis) >= interval)
		{
			// Read temperature as Celsius
			temperature = dht.readTemperature(false, true);
			humidity = dht.readHumidity(false);

			// Check if any reads failed and exit early (to try again).
			if (isnan(humidity) || isnan(temperature))
			{
				Serial.println(F("Failed to read from DHT sensor!"));
				delay(1000);

				timesDHTfailed++;
				if(timesDHTfailed > 20)
				{
					Serial.println(F("Something wrong with the DHT."));
					if(warnSentDHT == false)
					{
						sms.SendSMS(number[0], "Something wrong with the DHT.");
						warnSentDHT = true;
					}
					timesDHTfailed = 0;
				}
				return;
			}
			else
			{
				timesDHTfailed = 0;

				Serial.print("Humidity: ");
				Serial.print(humidity);
				Serial.print(" %\t");
				Serial.print("Temperature: ");
				Serial.print(temperature);
				Serial.println(" *C ");

				if(temperature <= 12.0 && warnSentTemp == false)
				{
					//Send an SMS alarm
					String message = "KYLMAA!\nLampotila: " + String(temperature) +
												"\nKosteus: " + String(humidity);
					message.toCharArray(smsbuffer,SMS_LENGTH);
					Serial.println(smsbuffer);
					sms.SendSMS(number[1], smsbuffer);
					warnSentTemp = true;
				}
				if(temperature >= 18.0 && warnSentTemp == true)
				{
					Serial.println(F("Lämminnyt yli 18 asteen."));
					warnSentTemp = false;
				}

				//Save to SD card
				saveToSD(temperature, humidity);

				previousMillis = millis();


				//Serial.println("SENDING TO CLOUD.");
				//Send to cloud
				//sendToCloud();
			}
		}
     }
}
bool saveToSD(float t, float h)
{
	File file = SD.open("data.txt", FILE_WRITE); // FILE_WRITE opens file for writing and moves to the end of the file, returns 0 if not available
	if(file)
	{
		gsm.GetTime(gsm_time);
		//Serial.println(gsm_time);

		String dataStr = String(gsm_time[6]) + String(gsm_time[7]) + "."	//Day
						+ String(gsm_time[3]) + String(gsm_time[4]) + ".20"	//Month
						+ String(gsm_time[0]) + String(gsm_time[1]) + ","	//Year
						+ String(gsm_time[9]) + String(gsm_time[10]) + ":"	//Hours
						+ String(gsm_time[12]) + String(gsm_time[13]) + ":"	//Minutes
						+ String(gsm_time[15]) + String(gsm_time[16]) + ","	//Seconds
						+ String(t) + ","
						+ String(h);
		file.println(dataStr);
		file.close();
		Serial.println(dataStr);
		Serial.println("Data written to SD.");
		return 1;
	}
	return 0;
}
//Is AC failed
//http://www.insidegadgets.com/2012/01/08/non-contact-blackout-detector/
//http://harizanov.com/2013/08/non-contact-ac-detection/
void checkAC(void)
{
	byte detected = 0;

	for( int i=0; i < 30; i++)
	{
		unsigned int ac = analogRead(A10);
		if(ac > 0)
		{
			detected++;
		}
	}
	if(detected >= 10 && warnSentAC == false)
	{
		failedAC = true;
		Serial.println("AC failed.");
		//sms.SendSMS(number[0], "Sahkot poikki. :(");
	}
	else if(detected < 10 && warnSentAC == true)
	{
		failedAC = false;
		Serial.println("AC On.");
		//sms.SendSMS(number[0], "Sahkot tuli takas. :)");
	}
}

//Chekcs the status of the incoming call
void checkCallStatus(void)
{
	call_received_status = call.CallStatusWithAuth(call_received_number, 1, NUMBERS);

	if(call_received_status == CALL_INCOM_VOICE_AUTH)
	{
		Serial.println("Auth nro. : " + String(call_received_number));
		delay(2000);

		//Serial.println("Picking Up!");
		call.PickUp();
		//delay(500);

		//Serial.println("Hanging Up!");
		call.HangUp();
		//delay(100);

		//Send an SMS to the calling number with the values red previously.
		String message = "Lampotila: " + String(temperature) +
						"\nKosteus: " + String(humidity);
		message.toCharArray(smsbuffer, SMS_LENGTH);
		Serial.println(smsbuffer);
		sms.SendSMS(call_received_number, smsbuffer);

	}
	else if(call_received_status == CALL_INCOM_VOICE_NOT_AUTH)
	{
		Serial.println("UnAuth nro. : " + String(call_received_number));
		delay(2000);

		Serial.println(F("Picking Up!"));
		call.PickUp();
		Serial.println(F("Hanging Up!"));
		call.HangUp();

		delay(2000);
	}
	else if(call_received_status == CALL_NONE)
	{
		//Serial.println("Ei soittoa.");
	}
}

//Check unread messages

void checkUnreadMessages(void)
{
	//Serial.println("Check SMSs.");

	sms_received_position = sms.IsSMSPresent(SMS_ALL);
	//delay(10);
	//Serial.println("sms pos: " + (String)((int)sms_received_position));

	if (sms_received_position > 0)
	{
		// Read the new SMS
		sms.GetSMS(sms_received_position, sms_received_number, NUMBER_LENGTH, sms_received_msg, SMS_LENGTH);
		Serial.println("SMS: " + (String)sms_received_number);
		Serial.println("SMS: " + (String)sms_received_msg);
		//SALDO
		if(validNumber(sms_received_number) == true)
		{
		//if((strcmp(sms_received_number, number[0]) == 0) ||
		//(strcmp(sms_received_number, number[1]) == 0) ||
		//(strcmp(sms_received_number, number[2]) == 0) ||
		//(strcmp(sms_received_number, number[3]) == 0))
		//{
			//Serial.println("Nro match.");
			if(strcasecmp(sms_received_msg, "saldo") == 0)
			{
				//Save requesting number
				strcpy(sms_received_number, sms_requesting_number);

				//Send SALDO to the operator
				sms.SendSMS(operatorNumber, "PREPAID SALDO");
				//Serial.println("SALDO sent.");

				//Delete the SMS
				sms.DeleteSMS(sms_received_position);
				requestSMS = true;
				//Serial.println("SALDO SMS Deleted.");
			}
			else if(strcasecmp(sms_received_msg, "kielto") == 0)
			{
				//Send KIELTO to the operator
				sms.SendSMS(operatorNumber, "KIELTO");

				//Delete the SMS
				sms.DeleteSMS(sms_received_position);
			}
		}
		else if(strcmp(sms_received_number, operatorNumber) == 0 && requestSMS == false)
		{
			sms.SendSMS(number[0], sms_received_msg);
			sms.DeleteSMS(sms_received_position);
		}
		else if(strcmp(sms_received_number, operatorNumber) == 0 && requestSMS == true)
		{
			sms.SendSMS(sms_requesting_number, sms_received_msg);
			sms.DeleteSMS(sms_received_position);
			requestSMS = false;
		}
		else
		{
			sms.DeleteSMS(sms_received_position);
		}
	}
}

bool validNumber(const char* num)
{
    int i;
    for (i = 0; i < NUMBERS; i++)
    {
    	if(strcmp(num, number[i]) == 0)
            return true;
    }
    return false;
}
