#include <Wire.h>
#include <Adafruit_MotorShield.h>
#include "utility/Adafruit_PWMServoDriver.h"
#include <string.h>

#define DEV_ID 1

Adafruit_MotorShield AFMS = Adafruit_MotorShield();
Adafruit_DCMotor *motors[4];
static int v;

const int bufsize = 256;
const int safesize = bufsize / 2;
char buf[bufsize];
char msg[bufsize];
char wbuf[safesize];
unsigned long msecs;
char numbuf[4];

class QuadEncoder {
  public:
    //long long pos;
    long pos; //switch with top statement??
    bool reversed; // set
    char pin[2];
    QuadEncoder() {
      reset();
    }
    void attach(int pin1, int pin2) {
      pinMode(pin1, INPUT);
      pinMode(pin2, INPUT);
      pin[0] = pin1;
      pin[1] = pin2;
      pin_state[0] = digitalRead(pin[0]) == HIGH;
      pin_state[1] = digitalRead(pin[1]) == HIGH;
    }
    long long read() {
      update();
      return pos;
    }
    void reset() {
      pin[0] = 0;
      pin[1] = 0;
      pos = 0;
      velocity = 1; // velocity can either be 1 or -1
      reversed = false;
      pin_state[0] = 0;
      pin_state[1] = 0;
    }
  private:
    void update() {
      if (pin[0] == 0 || pin[1] == 0)
        return;
      // FSA : reg :: 00 01 11 10
      //     : rev :: 00 10 11 01
      char new_state[2] = {
        digitalRead(pin[0]) == HIGH,
        digitalRead(pin[1]) == HIGH
      };
      char delta_state[2] = {
        new_state[0] != pin_state[0],
        new_state[1] != pin_state[1]
      };
      if (delta_state[0] && delta_state[1]) {
        pos += velocity * 2 * (reversed ? -1 : 1);
      } else if (delta_state[1]) {
        velocity = (new_state[0] == new_state[1]) ? -1 : 1;
        pos += velocity * (reversed ? -1 : 1);
      } else if (delta_state[0]) {
        velocity = (new_state[0] == new_state[1]) ? 1 : -1;
        pos += velocity * (reversed ? -1 : 1);
      }
      pin_state[0] = new_state[0];
      pin_state[1] = new_state[1];
    }
    char pin_state[2];
    long long velocity;  // estimated
};

int limit(int x, int a, int b)
{
	if (x > b)
	{
		return b;
	}
	else if (x < a)
	{
		return a;
	}
	else
	{
		return x;
	}
}

void setmotors(int topleft, int topright, int botleft, int botright)
{
	bool isneg[4] = {topright < 0, topleft < 0, botleft < 0, botright < 0};
	topright = limit(abs(topright), 0, 255);
	topleft = limit(abs(topleft), 0, 255);
	botleft = limit(abs(botleft), 0, 255);
	botright = limit(abs(botright), 0, 255);
	motors[0]->setSpeed(topright);
	motors[1]->setSpeed(topleft);
	motors[2]->setSpeed(botleft);
	motors[3]->setSpeed(botright);

	for (int i = 0; i < 4; i++)
	{
		if (isneg[i])
		{
			motors[i]->run(FORWARD);
		}
		else
		{
			motors[i]->run(BACKWARD);
		}
	}
}

  QuadEncoder encoder1;
  QuadEncoder encoder2;
  QuadEncoder encoder3;
  QuadEncoder encoder4;

void setup()
{
	motors[0] = AFMS.getMotor(1);
	motors[1] = AFMS.getMotor(2);
	motors[2] = AFMS.getMotor(3);
	motors[3] = AFMS.getMotor(4);

  encoder1.attach(2, 3); //pin nums
  encoder2.attach(4, 5);
  encoder3.attach(6, 7);
  encoder4.attach(8, 11);

	pinMode(13, OUTPUT); // set status LED to OUTPUT and HIGH
	digitalWrite(13, HIGH);

	AFMS.begin();
	setmotors(0, 0, 0, 0);
	Serial.begin(57600);
	msecs = millis();
}

static int targetv[4];
static int prevv[4];

int rampmotor(int curr, int target)
{
	int delta = target - curr;
	delta = limit(delta, -8, 8);
	curr = limit(curr + delta, -255, 255);
	return curr;
}

void loop()
{

  long value1 = encoder1.read();
  long value2 = encoder2.read();
  long value3 = encoder3.read();
  long value4 = encoder4.read();
  
	int nbytes = 0;
	if ((nbytes = Serial.available()))
	{
		// read + attach null byte
		int obytes = strlen(buf);
		Serial.readBytes(&buf[obytes], nbytes);
		buf[nbytes + obytes] = '\0';

		// resize just in case
		if (strlen(buf) > safesize)
		{
			memmove(buf, &buf[strlen(buf) - safesize], safesize);
			buf[safesize] = '\0'; // just in case
		}

		// extract possible message
		char *s, *e;
		if ((e = strchr(buf, '\n')))
		{
			e[0] = '\0';
			if ((s = strrchr(buf, '[')))
			{
				// CUSTOMIZE
				sscanf(s, "[%d %d %d %d]\n",
						&targetv[0], &targetv[1], &targetv[2], &targetv[3]);
			}
			memmove(buf, &e[1], strlen(&e[1]) + sizeof(char));
		}
	}
	for (int i = 0; i < 4; i++)
	{
		prevv[i] = rampmotor(prevv[i], targetv[i]);
	}
	setmotors(-prevv[0], -prevv[1], prevv[2], prevv[3]);


//Last 4 values are encoders
	if (millis() - msecs > 100)
	{
		sprintf(wbuf, "[%d %d %d %d %d %ld %ld %ld %ld]\n",
				DEV_ID,
				prevv[0],
				prevv[1],
				prevv[2],
				prevv[3],
				value1,
				value2,
				value3,
				value4);
		Serial.print(wbuf);
		msecs = millis();
	}
}