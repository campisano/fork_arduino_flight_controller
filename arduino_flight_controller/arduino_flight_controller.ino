#define ESC_A 2
#define ESC_B 3
#define ESC_C 4
#define ESC_D 5

#define NO_MOTOR false
#define NO_SENSORS true

#define CALIBRATE_MODE false
// Debug
#define DEBUG true

#define MAX_SIGNAL 2000
#define MIN_SIGNAL 1000

#define SOFT 0
#define HARD 1
#define FLIGHT_MODE SOFT

// Lib needed to connect to the motors by their ESCs
#include <Servo.h>

// Lib needed to connect to the MPU 6050
#include<Wire.h>

Servo motorA;
Servo motorB;
Servo motorC;
Servo motorD;

//MPU6050 address of I2C
const int MPU=0x68;

//Variaveis para armazenar valores dos sensores
int AcX,AcY,AcZ,Tmp,GyX,GyY,GyZ;

//We create variables for the time width values of each PWM input signal
unsigned long counter_1, counter_2, counter_3, counter_4, current_count;

//We create 4 variables to stopre the previous value of the input signal (if LOW or HIGH)
byte last_CH1_state, last_CH2_state, last_CH3_state, last_CH4_state;

//To store the 1000us to 2000us value we create variables and store each channel
int input_YAW;      //In my case channel 4 of the receiver and pin D12 of arduino
int input_PITCH;    //In my case channel 2 of the receiver and pin D9 of arduino
int input_ROLL;     //In my case channel 1 of the receiver and pin D8 of arduino
int input_THROTTLE; //In my case channel 3 of the receiver and pin D10 of arduino

// PID
struct error {
  float yaw;
  float pitch;
  float roll;
};

struct PID {
  float yaw;
  float pitch;
  float roll;
};

struct error lastErrors;

int normalize(float value)
{
  value = map((int) value, 0, 1000, MIN_SIGNAL, MAX_SIGNAL);

  if (value > MAX_SIGNAL) {
      value = MAX_SIGNAL;
  } else if (value < MIN_SIGNAL) {
      value = MIN_SIGNAL;
  }

  return value;
}

void setup() {
  /*
   * Port registers allow for lower-level and faster manipulation of the i/o pins of the microcontroller on an Arduino board. 
   * The chips used on the Arduino board (the ATmega8 and ATmega168) have three ports:
     -B (digital pin 8 to 13)
     -C (analog input pins)
     -D (digital pins 0 to 7)
   
  //All Arduino (Atmega) digital pins are inputs when you begin...
  */  
   
  PCICR |= (1 << PCIE0);    //enable PCMSK0 scan                                                 
  PCMSK0 |= (1 << PCINT0);  //Set pin D8 trigger an interrupt on state change.
  PCMSK0 |= (1 << PCINT1);  //Set pin D9 trigger an interrupt on state change.
  PCMSK0 |= (1 << PCINT2);  //Set pin D10 trigger an interrupt on state change.
  PCMSK0 |= (1 << PCINT3);  //Set pin D11 trigger an interrupt on state change.
  
  // why 12?
  // PCMSK0 |= (1 << PCINT4);  //Set pin D12 trigger an interrupt on state change.  
  
  //Start the serial in order to see the result on the monitor
  //Remember to select the same baud rate on the serial monitor
  Serial.begin(19200);  
  // Serial.begin(9600);
  
  if (DEBUG) {
    Serial.println("Debuging...");
  }

  // input_THROTTLE = 0;
  initMotor();
  initSensors();
  // initRCReceiver();
  

  delay(5000);
  Serial.println("Done....");
}

void loop() {

  readSensors();

  // Serial.print("input_THROTTLE: ");
  // Serial.print(input_THROTTLE);
  // Serial.print(" | input_YAW: ");
  // Serial.print(input_YAW);
  // Serial.print(" | input_PITCH: ");
  // Serial.print(input_PITCH);
  // Serial.print(" | input_ROLL: ");
  // Serial.print(input_ROLL);
  // Serial.println("    ");
  // readRCReceiver();
  // testing
  
  setMotor();
  
  // // send data only when you receive data:
  // if (Serial.available() > 0) {
  //   //Le o valor do potenciometro
  //   int valor = Serial.parseInt();
    
  //   Serial.print("Input: ");
  //   Serial.println(valor);
    
  //   //Envia o valor para o motor
  //   setMotor(valor);
  // }


  if (DEBUG && NO_MOTOR) {
    //Aguarda 300 ms e reinicia o processo
    delay(300);
  }
}

void initMotor() {
  if (NO_MOTOR) {
    Serial.println("No motors");
    return;
  }

  int initValue = 0;

  Serial.println("Init motor 1");
  motorA.attach(ESC_A, MIN_SIGNAL, MAX_SIGNAL);
  motorA.writeMicroseconds(MIN_SIGNAL);

  Serial.println("Init motor 2");
  motorB.attach(ESC_B, MIN_SIGNAL, MAX_SIGNAL);
  motorB.writeMicroseconds(MIN_SIGNAL);

  Serial.println("Init motor 3");
  motorC.attach(ESC_C, MIN_SIGNAL, MAX_SIGNAL);
  motorC.writeMicroseconds(MIN_SIGNAL);

  Serial.println("Init motor 4");
  motorD.attach(ESC_D, MIN_SIGNAL, MAX_SIGNAL);
  motorD.writeMicroseconds(MIN_SIGNAL);

  if (CALIBRATE_MODE) {
    // setMotor(MAX_SIGNAL);
    
    Serial.println("Turn on power source, then Wait 2 seconds and press any key");
    while(!Serial.available());
  }
  
  // setMotor();
}

void setMotor() {

  if (NO_MOTOR) {
    Serial.println("No motors to set");
    return;
  }

  int rxRoll;
  int rxPitch;
  int rxYaw;

  if (FLIGHT_MODE == SOFT)
  {
    rxRoll = map(input_ROLL, MIN_SIGNAL, MAX_SIGNAL, -45, 45);
    rxPitch = map(input_PITCH, MIN_SIGNAL, MAX_SIGNAL, -45, 45);   
    rxYaw = map(input_YAW, MIN_SIGNAL, MAX_SIGNAL, -180, 180);
  }
  else
  {
    rxRoll = map(input_ROLL, MIN_SIGNAL, MAX_SIGNAL, -180, 180);
    rxPitch = map(input_PITCH, MIN_SIGNAL, MAX_SIGNAL, -180, 180);
    rxYaw = map(input_YAW, MIN_SIGNAL, MAX_SIGNAL, -270, 270);
  }

  //Adjust difference - MIN_SIGNAL
  int throttle = map(input_THROTTLE, MIN_SIGNAL, MAX_SIGNAL, 0, 1000);
  float cmd_motA = throttle;
  float cmd_motB = throttle;
  float cmd_motC = throttle;
  float cmd_motD = throttle;

  //Somatório dos erros
  struct error sError = { .yaw = 0, .pitch = 0, .roll = 0};

  //Valor desejado - angulo atual
  struct error anguloAtual = { .yaw = 0, .pitch = 0, .roll = 0};
  struct error anguloDesejado = { .yaw = rxYaw, .pitch = rxPitch, .roll = rxRoll};

  struct error errors = { 
      .yaw = anguloDesejado.yaw - anguloAtual.yaw,
      .pitch = anguloDesejado.pitch - anguloAtual.pitch,
      .roll = anguloDesejado.roll - anguloAtual.roll
  };
  
  struct error deltaError;

  //Variáveis de ajuste
  struct PID Kp = { .yaw = 10, .pitch = 10, .roll = 10};
  struct PID Ki = { .yaw = 0, .pitch = 0, .roll = 0};
  struct PID Kd = { .yaw = 0, .pitch = 0, .roll = 0};
  
  if (throttle > 0) {
      // Calculate sum of errors : Integral coefficients
      sError.yaw += errors.yaw;
      sError.pitch += errors.pitch;
      sError.roll += errors.roll;

      // Calculate error delta : Derivative coefficients
      deltaError.yaw = errors.yaw - lastErrors.yaw;
      deltaError.pitch = errors.pitch - lastErrors.pitch;
      deltaError.roll = errors.roll - lastErrors.roll;

      // Save current error as lastErr for next time
      lastErrors.yaw = errors.yaw;
      lastErrors.pitch = errors.pitch;
      lastErrors.roll = errors.roll;

      // Yaw - Lacet (Z axis)
      int yaw_PID = (errors.yaw * Kp.yaw + sError.yaw * Ki.yaw + deltaError.yaw * Kd.yaw);
      cmd_motA -= yaw_PID;
      cmd_motB += yaw_PID;
      cmd_motC += yaw_PID;
      cmd_motD -= yaw_PID;

      // Pitch - Tangage (Y axis)
      int pitch_PID = (errors.pitch * Kp.pitch + sError.pitch * Ki.pitch + deltaError.pitch * Kd.pitch);
      cmd_motA -= pitch_PID;
      cmd_motB -= pitch_PID;
      cmd_motC += pitch_PID;
      cmd_motD += pitch_PID;

      // Roll - Roulis (X axis)
      int roll_PID = (errors.roll * Kp.roll + sError.roll * Ki.roll + deltaError.roll * Kd.roll);
      cmd_motA -= roll_PID;
      cmd_motB += roll_PID;
      cmd_motC -= roll_PID;
      cmd_motD += roll_PID;
  }

  Serial.print(" | Motor_A ");
  Serial.print(cmd_motA);
  Serial.print(" | Motor_B ");
  Serial.print(cmd_motB);
  Serial.print(" | Motor_C ");
  Serial.print(cmd_motC);
  Serial.print(" | Motor_D ");
  Serial.print(cmd_motD);
  Serial.println("    ");

  // Apply speed for each motor
  motorA.writeMicroseconds(normalize(cmd_motA));
  motorB.writeMicroseconds(normalize(cmd_motB));
  motorC.writeMicroseconds(normalize(cmd_motC));
  motorD.writeMicroseconds(normalize(cmd_motD));

  // if (value < MIN_SIGNAL) {
  //   value = MIN_SIGNAL;
  // } else if (value > MAX_SIGNAL) {
  //   value = MAX_SIGNAL;
  // }

  // // Serial.print("Set motor 1: ");
  // // Serial.println(value);
  // motor1.writeMicroseconds(value);
  // // Serial.print("Set motor 2: ");
  // // Serial.println(value);
  // motor2.writeMicroseconds(value);
  // // Serial.print("Set motor 3: ");
  // // Serial.println(value);
  // motor3.writeMicroseconds(value);
  // // Serial.print("Set motor 4: ");
  // // Serial.println(value);
  // motor4.writeMicroseconds(value);
}

void initSensors() {
  if (NO_SENSORS) {
    Serial.println("No sensors");
    return;
  }

  Wire.begin();
  Wire.beginTransmission(MPU);
  Wire.write(0x6B); 
  
  //Inicializa o MPU-6050
  Wire.write(0); 
  Wire.endTransmission(true);
}

void readSensors() {
  if (NO_SENSORS) {
    return;
  }

  Wire.beginTransmission(MPU);
  Wire.write(0x3B);  // starting with register 0x3B (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  //Solicita os dados do sensor
  Wire.requestFrom(MPU,14,true);  
  //Armazena o valor dos sensores nas variaveis correspondentes
  AcX=Wire.read()<<8|Wire.read();  //0x3B (ACCEL_XOUT_H) & 0x3C (ACCEL_XOUT_L)     
  AcY=Wire.read()<<8|Wire.read();  //0x3D (ACCEL_YOUT_H) & 0x3E (ACCEL_YOUT_L)
  AcZ=Wire.read()<<8|Wire.read();  //0x3F (ACCEL_ZOUT_H) & 0x40 (ACCEL_ZOUT_L)
  Tmp=Wire.read()<<8|Wire.read();  //0x41 (TEMP_OUT_H) & 0x42 (TEMP_OUT_L)
  GyX=Wire.read()<<8|Wire.read();  //0x43 (GYRO_XOUT_H) & 0x44 (GYRO_XOUT_L)
  GyY=Wire.read()<<8|Wire.read();  //0x45 (GYRO_YOUT_H) & 0x46 (GYRO_YOUT_L)
  GyZ=Wire.read()<<8|Wire.read();  //0x47 (GYRO_ZOUT_H) & 0x48 (GYRO_ZOUT_L)

  //Envia valor da temperatura para a serial e o LCD
  //Calcula a temperatura em graus Celsius
  Serial.print("Tmp = "); Serial.print(Tmp/340.00+36.53);
  //Envia valor X do acelerometro para a serial e o LCD
  Serial.print(" | AcX = "); Serial.print(AcX);
  //Envia valor Y do acelerometro para a serial e o LCD
  Serial.print(" | AcY = "); Serial.print(AcY);
  //Envia valor Z do acelerometro para a serial e o LCD
  Serial.print(" | AcZ = "); Serial.print(AcZ);
  //Envia valor X do giroscopio para a serial e o LCD
  Serial.print(" | GyX = "); Serial.print(GyX);
  //Envia valor Y do giroscopio para a serial e o LCD  
  Serial.print(" | GyY = "); Serial.print(GyY);
  //Envia valor Z do giroscopio para a serial e o LCD
  Serial.print(" | GyZ = "); Serial.println(GyZ);
}

// A special thanks to Electronoobs
// https://www.youtube.com/watch?v=if9LZTcy_uk&index=1&list=LL70ziqu5-D8VBDmxa5HUYwg
//This is the interruption routine
//----------------------------------------------

ISR(PCINT0_vect){
  //First we take the current count value in micro seconds using the micros() function
    
  current_count = micros();
  ///////////////////////////////////////Channel 1
  if(PINB & B00000001){                              //We make an AND with the pin state register, We verify if pin 8 is HIGH???
    if(last_CH1_state == 0){                         //If the last state was 0, then we have a state change...
      last_CH1_state = 1;                            //Store the current state into the last state for the next loop
      counter_1 = current_count;                     //Set counter_1 to current value.
    }
  }
  else if(last_CH1_state == 1){                      //If pin 8 is LOW and the last state was HIGH then we have a state change      
    last_CH1_state = 0;                              //Store the current state into the last state for the next loop
    input_ROLL = current_count - counter_1;   //We make the time difference. Channel 1 is current_time - timer_1.
  }
  ///////////////////////////////////////Channel 2
  if(PINB & B00000010 ){                             //pin D9 -- B00000010                                              
    if(last_CH2_state == 0){                                               
      last_CH2_state = 1;                                                   
      counter_2 = current_count;                                             
    }
  }
  else if(last_CH2_state == 1){                                           
    last_CH2_state = 0;                                                     
    input_PITCH = current_count - counter_2;                             
  }
  ///////////////////////////////////////Channel 3
  if(PINB & B00000100 ){                             //pin D10 - B00000100                                         
    if(last_CH3_state == 0){                                             
      last_CH3_state = 1;                                                  
      counter_3 = current_count;                                               
    }
  }
  else if(last_CH3_state == 1){                                             
    last_CH3_state = 0;                                                    
    input_THROTTLE = current_count - counter_3;                            

  }
  ///////////////////////////////////////Channel 4
  if(PINB & B00001000 ){                             //pin D11  -- B00001000
    if(last_CH4_state == 0){                                               
      last_CH4_state = 1;                                                   
      counter_4 = current_count;                                              
    }
  }
  else if(last_CH4_state == 1){                                             
    last_CH4_state = 0;                                                  
    input_YAW = current_count - counter_4;                            
  }   
}