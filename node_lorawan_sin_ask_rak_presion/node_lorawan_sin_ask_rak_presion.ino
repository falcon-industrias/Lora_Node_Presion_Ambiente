#include <SoftwareSerial.h>
#include <Wire.h>
#include "SHT31.h"
#include <Adafruit_NeoPixel.h>
#include <Average.h>
#include <SparkFun_MicroPressure.h>
#include "ER-EPM0154-2R.h"
#include "epdpaint.h"

SoftwareSerial log_serial(10, 9); // RX | TX

#define cmsg_cmd "at+send=lora:"
#define join_cmd "at+join"
#define status_cmd "at+get_config=lora:status"
#define sleepon_cmd "at+set_config=device:sleep:1"
#define sleepoff_cmd "at+set_config=device:sleep:0"

#define fPort "3"

#define vbatpin A0

#define LED_PIN 2
#define LED_COUNT 2

#define pinDone 8
#define reset_sensor 4
#define reset_lora 3
#define sleep_up  1

#define COLORED     0
#define UNCOLORED   1

String inputString = "";     // a String to hold incoming data
bool stringComplete = false; // whether the string is complete
String join_state = "";
String cmd = "";
String cmd_ask = "";
String date;
String variables;

double t, h, b,p;

unsigned char image[200];

unsigned long tiempo1 = 0;
unsigned long tiempo2 = 0;

SHT31 sht31 = SHT31();
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
Average<float> ave(10);
SparkFun_MicroPressure mpr;

Paint paint(image, 0, 0);
Epd epd;

void setup()
{
  io_ini();
  Serial.begin(9600);
  log_serial.begin(9600);
  log_serial.println("system ini");
  //inputString.reserve(200);
  reset_presion();
  delay(100);
  mpr.begin();
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(50); // Set BRIGHTNESS to about 1/5 (max = 255)
  delay(100);
  sht31.begin(); // inicializacion de sensor de temperatura
  theaterChase(strip.Color( 0,   0, 127 ), 5);
  tiempo1 = millis();
}


void loop()
{

    //  while (log_serial.available())
  //  {
  //    char c = log_serial.read();
  //    Serial.print(c);
  //  } 

  tiempo2 = millis();

  if (tiempo2 > (tiempo1 + 5000)) {
    tiempo1 = millis(); //Actualiza el tiempo actual
    theaterChase(strip.Color( 127,   127, 0 ), 5);
    cmd_ask = "";
    cmd = "";
    log_serial.println("off time");
    Serial.println(sleepon_cmd);
  }


  if (stringComplete) {

    //log_serial.print(inputString);

    //OK Wake Up
    //Initialization

    if (inputString.indexOf("Initialization") >= 0) {
      inputString = "";
      cmd_ask = "encendido";
      log_serial.println(cmd_ask);
    }

    if (inputString.indexOf("OK Wake Up") >= 0) {
      inputString = "";
      cmd_ask = "despierto";
      log_serial.println(cmd_ask);
    }

    if (inputString.indexOf("DownLinkCounter") >= 0) {
      inputString = "";
      cmd_ask = "fin";
    }

    if (inputString.indexOf("OK") >= 0 && cmd == "OK") {
      cmd_ask = "Done";
      cmd = "";
    }

    if (inputString.indexOf("Success") >= 0 && cmd == join_cmd)
    {
      inputString = "";
      cmd_ask = "join ok";
    }

    if (inputString.indexOf("Joined") >= 0) {
      cmd = "";
      join_state = inputString.substring(inputString.indexOf(":") + 1, inputString.indexOf("\n") - 1);
    }

    if (inputString.indexOf("Sleep") >= 0) {
      inputString = "";

      init_epaper();
      write_epaper();

      off_cpu();
    }

    inputString = "";
    stringComplete = false;
  }

  if (cmd_ask == "encendido" ||  cmd_ask == "despierto") {
    cmd_ask = "";
    Serial.println(status_cmd);
  }

  if ( cmd_ask == "fin") {
    cmd_ask = "";
    if (join_state == "true") {
      log_serial.println("join true");
      cmd = "OK";
      join_state = "";
      cmsg();
    } else if (join_state == "false") {
      theaterChase(strip.Color( 127,   64, 0 ), 5);
      log_serial.println("join false");
      join_state = "";
      join();
    }
  }

  if (cmd_ask == "join ok") {
    cmd = "OK";
    cmd_ask = "";
    log_serial.println(cmd);
    cmsg();
  }

  if (cmd_ask == "Done" ) {
    log_serial.println("off data");
    cmd_ask = "";
    cmd = "";
    theaterChase(strip.Color( 0,   127, 0), 5);
    Serial.println(sleepon_cmd);
  }

}

void init_epaper() {
  epd.Init();
  epd.ClearFrameMemory();   // bit set = white, bit reset = black
  paint.SetRotate(ROTATE_270);
}


void write_epaper() {

  char temp [6];
  dtostrf(t, 4, 1, temp);
  temp[4] = '+'; temp[5] = ')';

  char humd  [6] ;

  dtostrf(h, 4, 1, humd);
  humd[4] = '*';
  double pe;
  //pe=p*4.019;
  pe=p;
  char pres [6];
  dtostrf(pe, 4, 1, pres);
  //pres[5] = '*';

  char *p_t = temp;
  char *p_h = humd;
  char *p_p = pres;

  paint.SetWidth(48);
  paint.SetHeight(160);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, p_t , &Font24, COLORED); //
  epd.SetFrameMemoryBlack(paint.GetImage(), 0, 0, paint.GetWidth(), paint.GetHeight());

  paint.SetWidth(48);
  paint.SetHeight(135);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, p_h, &Font24, COLORED);
  epd.SetFrameMemoryBlack(paint.GetImage(), 70, 0, paint.GetWidth(), paint.GetHeight());

  paint.SetWidth(48);
  paint.SetHeight(160);
  paint.Clear(UNCOLORED);
  paint.DrawStringAt(0, 0, p_p, &Font24, COLORED);
  epd.SetFrameMemoryBlack(paint.GetImage(), 140, 0, paint.GetWidth(), paint.GetHeight());

  epd.DisplayFrame();

}

void join()
{
  cmd = join_cmd;
  Serial.println(join_cmd);
}

void cmsg()// funcion de envio de datos al gateway
{
  read_variable();

  uint8_t byte_data[10];
  int8_t  i = 0;

  variables.concat("Temp: ");
  variables.concat(t);
  variables.concat(" Humd: ");
  variables.concat(h);
  variables.concat(" Bat: ");
  variables.concat(b);
  variables.concat(" Presion: ");
  variables.concat(p);

  //  String variables = variables + "Temp: " + temp + " Humd: " + humd + " Bat: " + bat;
  //
  log_serial.println(variables);

  uint16_t te = t * 100;
  uint16_t he = h* 100;
  uint16_t ve = b * 100;
  uint16_t pe = p * 100;

  byte_data[i++] = (uint8_t)(te >> 8);
  byte_data[i++] = (uint8_t)te;

  byte_data[i++] = (uint8_t)(he >> 8);
  byte_data[i++] = (uint8_t)he;

  byte_data[i++] = (uint8_t)(ve >> 8);
  byte_data[i++] = (uint8_t)ve;

  byte_data[i++] = (uint8_t)(pe >> 8);
  byte_data[i++] = (uint8_t)pe;

  char str[10] = "";

  array_to_string(byte_data, 10, str);

  date.concat( cmsg_cmd);
  date.concat(fPort);
  date.concat(":");
  date.concat(String(str));

  log_serial.println(date);
  Serial.println(date);
}

void serialEvent()
{ // funcion de lectura de puerto serial Modulo Lora
  while (Serial.available())
  {
    char inChar = (char)Serial.read();
    inputString += inChar;
    if (inChar == '\n')
    {
      stringComplete = true;
    }
  }
}

void read_variable()// funcion de lectura de variables
{

  p = mpr.readPressure(KPA)  ;
  delay(50);
  t = sht31.getTemperature();
  delay(50);
  h = sht31.getHumidity();
  delay(50);

  for (int i = 0; i < 10; i++) {
    b = analogRead(vbatpin);
    b *= 2;    // we divided by 2, so multiply back
    b *= 3.3;  // Multiply by 3.3V, our reference voltage
    b /= 1024; // convert to voltage
    ave.push(b);
  }

  b = ave.mean();
}

void off_cpu()
{ // funcion de activacion del temporizador
  digitalWrite(pinDone, HIGH);
  delay(50);
  digitalWrite(pinDone, LOW);
}

void  reset_presion()
{ // funcion de activacion del temporizador
  digitalWrite(reset_sensor, LOW );
  delay(50);
  digitalWrite(reset_sensor, HIGH);
}
void io_ini()
{
  pinMode(pinDone, OUTPUT);
  pinMode(reset_sensor, OUTPUT);
}

void theaterChase(uint32_t color, int wait) {
  for (int a = 0; a < 4; a++) {
    for (int b = 0; b < 3; b++) { //  'b' counts from 0 to 2...
      strip.clear();         //   Set all pixels in RAM to 0 (off)
      // 'c' counts up from 'b' to end of strip in steps of 3...
      for (int c = b; c < strip.numPixels(); c += 3) {
        strip.setPixelColor(c, color); // Set pixel 'c' to value 'color'
      }
      strip.show(); // Update strip with new contents
      delay(wait);  // Pause for a moment
    }
  }
}

void array_to_string(byte array[], unsigned int len, char buffer[])
{
  for (unsigned int i = 0; i < len; i++)
  {
    byte nib1 = (array[i] >> 4) & 0x0F;
    byte nib2 = (array[i] >> 0) & 0x0F;
    buffer[i * 2 + 0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
    buffer[i * 2 + 1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
  }
  buffer[len * 2] = '\0';
}