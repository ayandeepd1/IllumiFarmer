//https://gitlab.com/riscv-vega/vega-arduino/-/raw/main/package_vega_index.json
#include <DFRobot_DHT11.h>
#include <BH1750.h>
#include <Servo.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <Timer.h>
#include <pwm.h>

#define ledr 24  
#define ledb 23  
#define ledg 22  
#define led1 21  
#define led2 20  
#define btn0 19
#define btn1 18
#define dswt1 16
#define dswt2 17

#define DHT22_PIN 6 //gpio
#define SOIL_PIN A3 //analog

#define COLD_PIN 4  //GPIO
#define HEAT_PIN 3  //GPIO
#define FAN_PIN  0  //gpio
#define PUMP_PIN 5  //gpio
#define SERVO_PIN 5 //PWM

#define red_pot A0  //analog
#define blue_pot A1 //analog
#define white_pot A2//analog

#define red_ch 0    //pwm
#define blue_ch 1   //pwm
#define white_ch 2  //pwm

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

DFRobot_DHT11 DHT;
TwoWire Wire(0);
Timer timer(1); 
//Timer Timer(2); 
BH1750 lightMeter(0x23);
Servo servo;

struct displayValues{
  float r=0, b=0, w=0;
}val;

struct pot_values{
  int red, blue, white;
}pot_val;

struct sensor_values{
  long t=millis();
  int soil=0;
  float light=30000, temp=33.0, rh=65.0;
}sens_val;

struct dev_status{
  bool pump=0, fan=0, heat=0, cold=0, window=0;
  bool last_pump=0, last_fan=0;
  long t=millis();
  long t_window=t;
}dstat;

struct dev_pin_status{
  bool pump=0, window=0;
}dpstat;

struct auto_status{
  bool devs=0, lights=0;
}astat;

struct threshold_values{
  //long t=millis();
  const int soil=1460;
  const float rh=80.0;
  const float templ=31.5, temph=32.0;
  const float light=30000;
}th_val;

struct debug_values{
  long dht_stop_time=0;
}dbg_val;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  //pinMode(16, INPUT);
  pinMode(red_pot, INPUT);
  pinMode(blue_pot, INPUT);
  pinMode(white_pot, INPUT);
  pinMode(SOIL_PIN, INPUT);
  digitalWrite(SERVO_PIN,OUTPUT);
  pinMode(FAN_PIN, OUTPUT);digitalWrite(FAN_PIN,LOW);
  digitalWrite(COLD_PIN,OUTPUT);digitalWrite(COLD_PIN,LOW);
  digitalWrite(HEAT_PIN,OUTPUT);digitalWrite(HEAT_PIN,LOW);
  pinMode(10, OUTPUT);digitalWrite(10,HIGH);
  
  pinMode(ledb,OUTPUT);digitalWrite(ledb,HIGH);  // LED OFF
  pinMode(ledr,OUTPUT);digitalWrite(ledr,LOW);  
  pinMode(led2,OUTPUT);digitalWrite(led2,HIGH);
  
  PWM.PWMC_Set_Period(red_ch, 800000);
  PWM.PWMC_Set_Period(blue_ch, 800000);
  PWM.PWMC_Set_Period(white_ch, 800000);

  servo.attach(SERVO_PIN);
  
  Serial.begin(115200);
  Wire.begin();
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  
  //Timer.attachInterrupt(toggle_window, 1000000);
    
  Serial.println("<<** Test begin");
  delay(100);
  digitalWrite(ledr,HIGH);   
}

void loop() {
  digitalWrite(ledb,LOW);  // LED ON
  update_sensors();
  update_auto_status();
  update_input();
  update_output_channel();
  
  update_display();
  serial_display();
  digitalWrite(ledb,HIGH); // LED OFF
  
}

void update_sensors(){
  sens_val.soil = analogRead(SOIL_PIN);
  
  if(millis()-sens_val.t >1000){
    long t1=millis();
    while (!lightMeter.measurementReady(true)){
      //Serial.println(millis()-t1);
      if(millis()-t1 >2)
        break;
    };
    sens_val.light = lightMeter.readLightLevel();
    //Serial.println(millis()-t1);
    
    DHT.read(DHT22_PIN, 22);
    sens_val.rh = DHT.humidity_f;
    sens_val.temp = DHT.temperature_f;
    
    sens_val.t=millis();

    if(sens_val.rh == 0.0 && dbg_val.dht_stop_time==0)
      dbg_val.dht_stop_time=sens_val.t;
  }
}

void update_auto_status(){
    astat.devs=!digitalRead(dswt1);
    astat.lights=!digitalRead(dswt2);
}

void update_input(){
  if(!astat.lights){
    pot_val.red=analogRead(red_pot);
    pot_val.blue=analogRead(blue_pot);
    pot_val.white=analogRead(white_pot);
  }else
    update_light_auto();
    
  if(!astat.devs){
    if(!digitalRead(btn0)){
      if(millis()-dstat.t>300)
        dstat.pump^=1;
      dstat.t=millis();
    }
    if(!digitalRead(btn1)){
      if(millis()-dstat.t>300)
        dstat.fan^=1;
        dstat.window^=1;
        dstat.t=millis();
    }
  }else if(!digitalRead(btn0) && !digitalRead(btn1)){
    set_auto_thresholds();
  }else
    update_dev_auto();
  
  if(!dstat.window && (millis()-dstat.t_window>10000)){
    dstat.window=1;
    dstat.t_window=millis();
  }
  else if(dstat.window && (millis()-dstat.t_window>2000)){
    dstat.window=0;
    dstat.t_window=millis();
  }
}

void update_dev_auto(){
  if(sens_val.rh>th_val.rh){
    dstat.fan=1;
    dstat.window=1;
  }else{
    dstat.fan=0;
    dstat.window=0;
  }
  
  if(sens_val.soil<th_val.soil)
    dstat.pump=1;
  else
    dstat.pump=0;

  if(sens_val.temp<th_val.templ)
    dstat.heat=1;
  else
    dstat.heat=0;
  
  if(sens_val.temp>th_val.temph)
    dstat.cold=1;
  else
    dstat.cold=0;
}

void update_light_auto(){
  int output_level=map(sens_val.light, th_val.light, 0, 0,1650);
  pot_val.red=output_level;
  pot_val.blue=output_level;
  pot_val.white=output_level;
}

void set_auto_thresholds(){
  
}

void update_output_channel(){
  analogWrite(red_ch, map(pot_val.red,0,1650,0,800000));
  analogWrite(blue_ch, map(pot_val.blue,0,1650,0,800000));
  analogWrite(white_ch, map(pot_val.white,0,1650,0,800000));
  digitalWrite(FAN_PIN,dstat.fan);
  digitalWrite(COLD_PIN,dstat.cold);
  digitalWrite(HEAT_PIN,dstat.heat);
  
  update_pump();
  update_window();
}

void update_pump(){
  if(dstat.pump==1 && dstat.last_pump==0){
    pinMode(PUMP_PIN, OUTPUT);
    timer.attachInterrupt(run_pump, 1000000);
    dstat.last_pump=1;
  }
  else if(dstat.pump==0){
    timer.detachInterrupt();
    digitalWrite(PUMP_PIN, 0);     
    pinMode(PUMP_PIN, INPUT);
    dstat.last_pump=0;
  }
}

void run_pump(){
  digitalWrite(PUMP_PIN, dpstat.pump);
  digitalWrite(led2, dpstat.pump);
  digitalWrite(10,!dpstat.pump);
  dpstat.pump^=1;
}

void update_window(){
  if(dstat.window)
    servo.write(20);
  else
    servo.write(100);
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

  display.setCursor(0, 56);
  display.print(F("d_s:"));
  display.print(dbg_val.dht_stop_time);

  if(!DHT.error){
    display.setCursor(43,0);
    display.print(F("Rh:"));
    display.print(sens_val.rh);
    display.println("%");
    
    display.setCursor(43,8);
    display.print(F("T:"));
    display.print(sens_val.temp);
    display.println(" C");
  }else{
    display.setCursor(43,0);
    display.print(F("DHT ERROR"));
  }
  
  display.setCursor(43,16);
  display.print(F("soil:"));
  display.print(sens_val.soil);
  
  display.setCursor(43,24);
  display.print(F("lux:"));
  display.print(sens_val.light);
  
  display.fillRect(109, 0, 18, 11, SSD1306_BLACK);
  display.fillRect(109, 21, 18, 11, SSD1306_BLACK);
  display.setCursor(115, 0); display.write('F');
  display.drawRect(109, -1, 18, 11, SSD1306_WHITE);
  display.setCursor(115, 25); display.write('P');
  display.drawRect(109, 23, 18, 11, SSD1306_WHITE);
  if(dstat.fan)
    display.fillRect(109, -1, 18, 11, SSD1306_INVERSE);
  if(dstat.pump)
    display.fillRect(109, 23, 18, 11, SSD1306_INVERSE);
  if(astat.devs){
    display.setCursor(113, 12); display.write('D');
  }
  if(astat.lights){
    display.setCursor(120, 12); display.write('L');
  }
  display.display();
}
void serial_display(){
  Serial.print("Light:");
  Serial.print(sens_val.light);
  Serial.print(" lx");
  
  Serial.print(",SM:"); 
  Serial.print(sens_val.soil);
  
  Serial.print(",Rh%:");
  Serial.print(sens_val.rh);
  Serial.print(",temp:");
  Serial.print(sens_val.temp);
  Serial.println("°C");
  
  Serial.print("r_pot:");
  Serial.print(pot_val.red);
  Serial.print(", b_pot:");
  Serial.print(pot_val.blue);
  Serial.print(", w_pot:");
  Serial.print(pot_val.white);
  Serial.print(",\tdstat.pump:");
  Serial.println(dpstat.pump);
  
  Serial.print("r%:");
  Serial.print(val.r);
  Serial.print(", b%:");
  Serial.print(val.b);
  Serial.print(", w%:");
  Serial.print(val.w);
  Serial.print(",\tdstat.fan:");
  Serial.println(dstat.fan);
  
}
