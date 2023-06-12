#include <DFRobot_DHT11.h>
#include <BH1750.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Timer.h>
#include <pwm.h>

#define ledr 24  
#define ledb 23  
#define ledg 22  
#define led1 21  
#define led2 20  
#define btn0 19
#define btn1 18

#define DHT11_PIN 2 //gpio
#define FAN_PIN  0  //gpio
#define PUMP_PIN 1  //gpio
#define SOIL_PIN A3 //analog

#define red_pot A0  //analog
#define blue_pot A1 //analog
#define white_pot A2//analog

#define red_ch 0    //pwm
#define blue_ch 1   //pwm
#define white_ch 2  //pwm

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

DFRobot_DHT11 DHT;
TwoWire Wire(0);
Timer Timer(2); 
BH1750 lightMeter(0x23);

struct displayValues{
  float r=0, b=0, w=0;
}val;

struct pot_values{
  int red, blue, white;
}pot_val;

struct sensor_values{
  long t=millis();
  int soil=0;
  float light, temp, rh;
}sens_val;

struct dev_status{
  bool pump=0, fan=0;
  long t=millis();
}dstat;


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


void setup() {
  //pinMode(16, INPUT);
  pinMode(red_pot, INPUT);
  pinMode(blue_pot, INPUT);
  pinMode(white_pot, INPUT);
  pinMode(SOIL_PIN, INPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  
  pinMode(ledb,OUTPUT);digitalWrite(ledb,HIGH);  // LED OFF
  pinMode(ledr,OUTPUT);digitalWrite(ledr,LOW);  

  PWM.PWMC_Set_Period(red_ch, 800000);
  PWM.PWMC_Set_Period(blue_ch, 800000);
  PWM.PWMC_Set_Period(white_ch, 800000);
  
  Serial.begin(115200);
  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  
  Serial.println("<<** Test begin");
  delay(100);
  digitalWrite(ledr,HIGH);   
}

void loop() {
  digitalWrite(ledb,LOW);  // LED ON
  update_input();
  update_output_channel();
  if(millis()-sens_val.t >1000){
  update_sensors();
  sens_val.t=millis();
  }
  update_display();
  serial_display();
  digitalWrite(ledb,HIGH); // LED OFF
  
}

void update_input(){
  pot_val.red=analogRead(red_pot);
  pot_val.blue=analogRead(blue_pot);
  pot_val.white=analogRead(white_pot);
  if(!digitalRead(btn0)){
    if(millis()-dstat.t>300)
      dstat.pump^=1;
    dstat.t=millis();
  }
  if(!digitalRead(btn1)){
    if(millis()-dstat.t>300)
      dstat.fan^=1;
      dstat.t=millis();
  }
}

void update_sensors(){
  sens_val.soil = analogRead(SOIL_PIN);
  if (lightMeter.measurementReady())
    sens_val.light = lightMeter.readLightLevel();
  DHT.read(DHT11_PIN, 22);
  sens_val.rh = DHT.humidity_f;
  sens_val.temp = DHT.temperature_f;
}

void update_output_channel(){
  analogWrite(red_ch, map(pot_val.red,3,1650,0,800000));
  analogWrite(blue_ch, map(pot_val.blue,3,1650,0,800000));
  analogWrite(white_ch, map(pot_val.white,3,1650,0,800000));
  digitalWrite(FAN_PIN,dstat.fan);
  digitalWrite(PUMP_PIN,dstat.pump);
}

void get_displayPercents(){
  val.r=(float)pot_val.red*100.0/1650;
  val.b=(float)pot_val.blue*100.0/1650;
  val.w=(float)pot_val.white*100.0/1650;
}

void update_display(){
  display.clearDisplay();
  
  display.setTextSize(1);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_INVERSE); // Draw white text
  display.cp437(true);  

  get_displayPercents();
  display.setCursor(0, 0);  display.print(int(val.r)); 
  display.setCursor(13, 0); display.print(int(val.b)); 
  display.setCursor(27, 0); display.print(int(val.w)); 

  int r=pot_val.red*24/1650;
  int b=pot_val.blue*24/1650;
  int w=pot_val.white*24/1650;
  display.drawRect(0, 8, 12, 24, SSD1306_WHITE);
  display.fillRect(1, 31-r, 10, r, SSD1306_WHITE);
  
  display.drawRect(13, 8, 12, 24, SSD1306_WHITE);
  display.fillRect(14, 31-b, 10, b, SSD1306_WHITE);

  display.drawRect(26, 8, 12, 24, SSD1306_WHITE);
  display.fillRect(27, 31-w, 10, w, SSD1306_WHITE);
  
  display.setCursor(2, 12);  display.write('R');
  display.setCursor(16, 12); display.write('B');
  display.setCursor(29, 12); display.write('W');

  display.setCursor(43,0);
  display.print(F("Rh:"));
  display.print(sens_val.rh);
  display.println("%");
  
  display.setCursor(43,8);
  display.print(F("T:"));
  display.print(sens_val.temp);
  display.println(" C");
  
  display.setCursor(43,16);
  display.print(F("soil:"));
  display.print(sens_val.soil);
  
  display.setCursor(43,24);
  display.print(F("lux:"));
  display.print(sens_val.light);

  display.fillRect(109, 0, 18, 11, SSD1306_BLACK);
  display.fillRect(109, 21, 18, 11, SSD1306_BLACK);
  display.setCursor(115, 2); display.write('F');
  display.drawRect(109, 0, 18, 11, SSD1306_WHITE);
  display.setCursor(115, 23); display.write('P');
  display.drawRect(109, 21, 18, 11, SSD1306_WHITE);
  if(dstat.fan)
    display.fillRect(109, 0, 18, 11, SSD1306_INVERSE);
  if(dstat.pump)
    display.fillRect(109, 21, 18, 11, SSD1306_INVERSE);
    
  display.display();
}
void serial_display(){
  Serial.print("Light:");
  Serial.print(sens_val.light);
  Serial.print(" lx, ");
  
  Serial.print(",SM:"); 
  Serial.print(sens_val.soil);
  
  Serial.print(",Rh%:");
  Serial.print(sens_val.rh);
  Serial.print(", temp:");
  Serial.print(sens_val.temp);
  Serial.println("°C");
  
  Serial.print("r_pot:");
  Serial.print(pot_val.red);
  Serial.print(", b_pot:");
  Serial.print(pot_val.blue);
  Serial.print(", w_pot:");
  Serial.println(pot_val.white);

  Serial.print("r%:");
  Serial.print(val.r);
  Serial.print(", b%:");
  Serial.print(val.b);
  Serial.print(", w%:");
  Serial.println(val.w);
}
