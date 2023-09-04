#include <math.h>     // Includi la libreria math per le funzioni matematiche standard

#include "TFT_eSPI.h" //include TFT LCD library

#include <SPI.h>

#include <Seeed_FS.h>

#include "SD/Seeed_SD.h"

#include "seeed_line_chart.h"

#include <SparkFunBQ27441.h>

#define FF17 & FreeSans9pt7b
const unsigned int BATTERY_CAPACITY = 650; // Set Wio battery capacity

File myFile;
File listOfSessions;
TFT_eSPI tft; // initialize TFT LCD
TFT_eSprite spr = TFT_eSprite( & tft);

String menuOfSessions[100]; 
int valueCount = 0;  // Contatore delle stringhe lette 
long int sessionsPosition = 0;
int sessionsMax;
boolean recording = false;
const int GSR = A0;
unsigned long previousMillis = 0;
boolean serialSending = false;
int sensorValue = 0;
int gsr_average = 0;
int gsrIndex = 0;

float ohm;
float conductance;
doubles gsr_data;
int previousSoc = 101; // previous value of battery charge

const int max_heartpluse_duty = 2000;
unsigned long valueHeartRate;
float heartRate;
doubles hr_data;
int hrIndex = 0;

/*SETUP*/

void setup() {

  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);

  pinMode(GSR, INPUT); // GSR
  Serial.begin(9600);
  
  
  tft.begin(); // start TFT LCD
  tft.setRotation(3); // set screen rotation
  tft.fillScreen(TFT_BLACK); // fill background
  tft.setTextColor(TFT_WHITE); // set text color
  tft.setTextSize(1); // set text size
  spr.createSprite(TFT_HEIGHT , TFT_WIDTH); //create sprite

  /*INIZIALIZING SD CARD AND BATTERY*/
  tft.drawString("Sd card inzializing", 5, 2);
  delay(1000);
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    tft.drawString("Sd card failed", 5, 20);
    while (1);
  }
  tft.drawString("Sd card ok", 5, 50);
  delay(2000);

  listOfSessions = SD.open("listOfSessions.txt");
  if (listOfSessions) {
    // Leggi il file fino a quando non raggiungi la fine
    while (listOfSessions.available()) {
      // Leggi la stringa dalla riga del file e aggiungila all'array
      String line = listOfSessions.readStringUntil('\n');
      menuOfSessions[valueCount] = line;
      valueCount++;
     
      // Puoi fare qualcos'altro con la stringa qui, se necessario
    }
    sessionsMax=valueCount;
    Serial.println(sessionsMax);
    delay(1000);
    // Chiudi il file
    listOfSessions.close();
  } else {
    // Se non riesci ad aprire il file, stampa un errore
    Serial.println("Error openenig file");
  }
  
  
  tft.fillScreen(TFT_BLACK); // fill background

  setupBQ27441();
  tft.setTextColor(TFT_GREEN);
  tft.setCursor((320 - tft.textWidth("Battery Initialised!")) / 2, 120);
  tft.print("Battery Initialised!");
  delay(1000);
  tft.fillScreen(TFT_BLACK); // fill background
  bar();
  attachInterrupt(digitalPinToInterrupt(2), interrupt, RISING); // start interrupt for HR

  /*END INIZIALIZING*/

}

void loop() {



   if (digitalRead(WIO_5S_LEFT) == LOW) {
    sessionsPosition = sessionsPosition - 1;
    delay(500);
    if (sessionsPosition < 0) 
    {
      sessionsPosition = 0;
    }
    
    bar();
   }
   
  if (digitalRead(WIO_5S_RIGHT) == LOW) {
    sessionsPosition = sessionsPosition + 1;
    delay(500);
    if (sessionsPosition > (sessionsMax-1)) 
    {
      sessionsPosition = (sessionsMax-1);
    }
     
    bar();
  }
  
  if (digitalRead(WIO_KEY_C) == LOW) {
    serialSending = !serialSending;
    delay(500);
    bar();
  }

  if (digitalRead(WIO_KEY_A) == LOW) {
    recording = !recording;
    delay(500);
    bar();
  }

  if (digitalRead(WIO_KEY_B) == LOW) {

    // Empty gsr data
    while (!gsr_data.empty()) {
      gsr_data.pop(); // Remove last element
    }
    while (!hr_data.empty()) {
      hr_data.pop(); // Remove last element
    }
    bar();
  }
  gsr();
  //printGraph();
  printBatteryStats();

}

void display_line_chart(int header_y,
  const char * header_title, int chart_width, int chart_height, doubles data, uint32_t graph_color, uint32_t line_color) {
  // Define the line graph title settings:
  auto header = text(0, header_y)
    .value(header_title)
    .align(center)
    .valign(vcenter)
    .width(chart_width)
    .color(tft.color565(243, 208, 296))
    .thickness(2);
  // Define the header height and draw the line graph title. 
  header.height(header.font_height() * 2);
  header.draw();
  // Define the line chart settings:
  auto content = line_chart(0, header.height() + header_y);
  content
    .height(chart_height) // the actual height of the line chart
    .width(chart_width) // the actual width of the line chart
    .based_on(0.0) // the starting point of the y-axis must be float
    .show_circle(false) // drawing a circle at each point, default is on
    .value(data) // passing the given data array to the line graph
    .color(line_color) // setting the line color 
    .x_role_color(graph_color) // setting the line graph color
    .x_tick_color(graph_color)
    .y_role_color(graph_color)
    .y_tick_color(graph_color)
    .draw();
}

/*GSR DATA READ FUNCTION*/

void bar() {
    
    tft.fillRect(85, 2, 169, 100, TFT_BLACK); // Remove rectangle

    //PULSANTE SERIALE
    tft.fillRect(1, 2, 50, 10, TFT_BLUE);
    tft.setTextSize(1); // set text size
    tft.drawString("SER:", 5, 2);

    //PULSANTE CANCELLA GRAFICO
    tft.fillRect(68, 2, 50, 10, TFT_BLUE);
    tft.setTextSize(1); // set text size
    tft.drawString("CLEAN", 76, 2);

    //PULSANTE RECORD
    tft.fillRect(150, 2, 50, 10, TFT_BLUE);
    tft.setTextSize(1); // set text size
    tft.drawString("CSV:", 152, 2);

    //Session name
    tft.fillRect(10, 50, 150, 20, TFT_BLACK);
    tft.setTextSize(1); // set text size
    tft.drawString(menuOfSessions[sessionsPosition], 10, 50);

   
   

    if (serialSending == true) {
      tft.setTextSize(1);
      tft.drawString("ON  ", 33, 2);
      

    } else if (serialSending == false) {
      tft.setTextSize(1);
      tft.drawString("OFF", 33, 2);
    }

    if (recording == true) {
      tft.setTextSize(1);
      tft.drawString("ON ", 180, 2);
      tft.drawCircle(210, 7, 5, TFT_RED); //A black circle origin at (160, 120) 
      tft.fillCircle(210, 7, 5, TFT_RED);

     

    } else if (recording == false) {
      tft.setTextSize(1);
      tft.drawString("OFF", 180, 2);
      tft.fillCircle(210, 7, 5, TFT_BLACK);

      tft.drawCircle(210, 7, 5, TFT_RED); //A black circle origin at (160, 120) 

    }


  
  
  }


void gsr() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= 1000) //one value every 1000 ms 
  {
    previousMillis = currentMillis;
    long sum = 0;
    for (int i = 0; i < 300; i++) // Average the 300 measurements to remove the glitch
    {
      sensorValue = analogRead(GSR);
      sum += sensorValue;

    }
    gsr_average = sum / 300;
    ohm = ((1024.0 + (2.0 * gsr_average)) * 10000.0) / (512.0 - gsr_average);
    conductance = 1000000 / ohm;

    gsrIndex = gsrIndex + 1;
    gsr_data.push(conductance);

    if (gsr_data.size() == 300) gsr_data.pop();
    

    if (serialSending == true) {
      Serial.print("GSR");
      Serial.print(",");
      Serial.print(menuOfSessions[sessionsPosition]);
      Serial.print(",");
      Serial.print(gsrIndex);
      Serial.print(",");
      Serial.print(currentMillis);
      Serial.print(",");
      Serial.println(conductance);

    } 

    if (recording == true) {
     

      /*write GSR data in a CSV file*/
      myFile = SD.open("gsr.csv", FILE_APPEND);
      if (myFile) {

        myFile.print("GSR");
        myFile.print(",");
        myFile.print(menuOfSessions[sessionsPosition]);
        myFile.print(",");
        myFile.print(gsrIndex);
        myFile.print(",");
        myFile.print(currentMillis);
        myFile.print(",");
        myFile.println(conductance);
        // close the file:
        myFile.close();
      }

    }
    printGraph(); 
  }
}

void setupBQ27441(void) {
  // Use lipo.begin() to initialize the BQ27441-G1A and confirm that it's
  // connected and communicating.
  if (!lipo.begin()) // begin() will return true if communication is successful
  {
    // If communication fails, print an error message and loop forever.
    Serial.println("Error: Unable to communicate with BQ27441.");
    Serial.println("  Check wiring and try again.");
    Serial.println("  (Battery must be plugged into Battery Babysitter!)");
    tft.setTextColor(TFT_RED);
    tft.setCursor((320 - tft.textWidth("Battery Not Initialised!")) / 2, 120);
    tft.print("Battery Not Initialised!");
    while (1);
  }
  Serial.println("Connected to BQ27441!");

  // Uset lipo.setCapacity(BATTERY_CAPACITY) to set the design capacity
  // of your battery.
  lipo.setCapacity(BATTERY_CAPACITY);
}

void printBatteryStats() {
  // Read battery stats from the BQ27441-G1A

  unsigned int soc = lipo.soc(); // Read state-of-charge (%)
  
  delay(1); //necessary to make value printed
  
  // Data
  if (previousSoc != soc) {
    tft.setTextSize(2);
    tft.fillRect(250, 2, 250, 20, TFT_BLACK); // remove rectangle
    tft.setCursor(260, 2);
    tft.print(soc); // draw float

    tft.drawString("%", 290, 2);
    previousSoc = soc;
  }

}



void interrupt() {
  static unsigned long previousInterruptMillis = 0;
  unsigned long currentInterruptMillis = millis();
  if (currentInterruptMillis - previousInterruptMillis >= max_heartpluse_duty) {
        
    previousInterruptMillis = currentInterruptMillis;
  } 
  
  else {
    hrIndex++;
    valueHeartRate = currentInterruptMillis - previousInterruptMillis;
    previousInterruptMillis = currentInterruptMillis;
    heartRate = 60000 / (valueHeartRate);
    
   
    
    hr_data.push(heartRate);
    
    if (hr_data.size() == 300) hr_data.pop();

     if (serialSending == true) {
      Serial.print("HR");
      Serial.print(",");
      Serial.print(menuOfSessions[sessionsPosition]);
      Serial.print(",");
      Serial.print(hrIndex);
      Serial.print(",");
      Serial.print(currentInterruptMillis);
      Serial.print(",");
      Serial.print(heartRate);
      Serial.print(",");
      Serial.println(valueHeartRate);
      

    } 
    

   if (recording == true) {
     

      /*write hr data in a CSV file*/
      myFile = SD.open("hr.csv", FILE_APPEND);
      if (myFile) {

        myFile.print("HR");
        myFile.print(",");
        myFile.print(menuOfSessions[sessionsPosition]);
        myFile.print(",");
        myFile.print(hrIndex);
        myFile.print(",");
        myFile.print(currentInterruptMillis);
        myFile.print(",");
        myFile.print(heartRate);
        myFile.print(",");
        myFile.println(valueHeartRate);
        // close the file:
        myFile.close();
      }

    } 
    
  }
 
}

void printGraph(){
    
    
    //Draw GSR graph
    if (gsr_data.size()>0) {
    spr.fillSprite(black);
    display_line_chart(0, "GSR-", TFT_HEIGHT /2 - 10 , 100, gsr_data, red, tft.color565(165, 40, 44));
    spr.pushSprite(0, 100);
    tft.setTextSize(2);
    tft.setCursor (102, 108);
    tft.print(conductance, 2); // draw float
    }
    
    //Draw HR graph
    
    if (hr_data.size()>0) {
    spr.fillSprite(black);
    display_line_chart(0, "HR-", TFT_HEIGHT /2 - 10 , 100, hr_data, red, tft.color565(165, 40, 44));
    spr.pushSprite(TFT_HEIGHT /2, 100);
    tft.setCursor (252, 108);
    tft.print(heartRate, 0);
    
    }    
        
  
  
  }
