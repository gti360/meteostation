#include <Wire.h>
#include <LiquidCrystal_I2C.h>
//#include "DHT.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Adafruit_Si7021.h"
#include "GyverButton.h"
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

#define DEBUG_MODE 0

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

//#define DHT_1_PIN 12  // D6 
//#define DHT_1_TYPE DHT11
//#define DHT_2_PIN 13  // D6 
//#define DHT_2_TYPE DHT22
#define BUTTON_1 16 // D0
//#define BUTTON_1 2 // D4

//I2C D1 - scl; D2 - SDA
LiquidCrystal_I2C lcd(0x27, 20, 4); 
//DHT dht1(DHT_1_PIN, DHT_1_TYPE);
//DHT dht2(DHT_2_PIN, DHT_2_TYPE);
//Adafruit_BMP085 bmp;
Adafruit_BME280 bme; // I2C
Adafruit_Si7021 SiSensor = Adafruit_Si7021();
GButton butt1(BUTTON_1, HIGH_PULL, NORM_CLOSE);

#if DEBUG_MODE == 1
#define DHT_PERIOD_READ 2000 //5000
#define BMP_PERIOD_READ 5000 //300000
#define WEATHER_COUNT 96  // 1440m/15m = 96 times
#define STORE_WEATHER_PERIOD 10000 // 900000 = 15m
#define REDRAW_PERIOD 1000
#else
#define DHT_PERIOD_READ 5000 //5000
#define BMP_PERIOD_READ 5000 //300000
#define WEATHER_COUNT 96  // 1440m/15m = 96 times
#define STORE_WEATHER_PERIOD 900000 // 900000 = 15m
#define REDRAW_PERIOD 1000
#endif

#define DISPLAY_ON_TM 60000
#define VIEW_MAIN 0
#define VIEW_CHART_TIN 1
#define VIEW_CHART_TIN2 2
#define VIEW_CHART_TIN3 3
#define VIEW_CHART_TIN4 4
#define VIEW_CHART_HIN 200
#define VIEW_CHART_TOUT 5
#define VIEW_CHART_TOUT2 6
#define VIEW_CHART_TOUT3 7
#define VIEW_CHART_TOUT4 8
#define VIEW_CHART_HOUT 9
#define VIEW_CHART_P 10
#define VIEW_TOTAL 10


struct Weather {
  float   t_in;
  unsigned char   h_in;
  float       t_out;
  unsigned char   h_out;
  int   pressure;
};

Weather weatherParameters[WEATHER_COUNT];
Weather currWeather;  
int weatherStoredCounter = 0;       
bool weatherIsChanged = false;
bool pressureIsChanged = false;
bool viewChanged = true;
int useIndex[13] = {0, 1, 2, 3, 7, 11, 23, 35, 47, 59, 71, 83, 95};
int useIndex2[13] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
int useIndex3[13] = {0, 3, 7, 11, 15, 19, 23, 27, 31, 35, 39, 43, 47};
int useIndex4[13] = {0, 7, 15, 23, 31, 39, 47, 55, 63, 71, 79, 87, 95};
byte cuurView = 0;

bool bmpConnected = false;
bool errorHappened = false;
bool errorHappenedPrev = false;
bool lcdBacklight = true;

//timers
long timerDht;
long timerBmp;
long timerStoreWeather;
long timerRedraw;
long timerDisplay;
long uptimeBegin;

void setup() {
  Serial.begin(115200);
  Serial.println();

  //timerDht = millis();
  //timerStoreWeather = millis();
  //timerRedraw = millis();
  //uptimeBegin = millis();
  //timerBmp = millis();
  timerDisplay = millis();
  
  initDevice();
  initPlot();

  //testChart(1);
  //lcd.clear();
}

void loop() {
  meteostationMain();
}

void meteostationMain () {
  
  if((long)millis() - timerDisplay > DISPLAY_ON_TM) {
    //lcd.noBacklight();
    //lcdBacklight = false;
  }

    //testdevices();
#if DEBUG_MODE == 1
  //readTemperatureMock();
  readTemperature();
#else
  readTemperature();
#endif  
  
  storeTemperature();

  butt1.tick();
  if (butt1.isPress()) {

    timerDisplay = millis();

    if(lcdBacklight == false) {
      lcd.backlight();
      lcdBacklight = true;
    } else {
      if (++cuurView > VIEW_TOTAL) { 
        cuurView = 0;
      }
      viewChanged = true;
      lcd.clear();
#if DEBUG_MODE == 1    
      Serial.print(F("cuurView: "));
      Serial.println(cuurView);
#endif    
    }
  }
  
  switch (cuurView) {
    case VIEW_MAIN: 
      mainView();
      break;
    case VIEW_CHART_TIN: 
      tInChartView();
      break;
    case VIEW_CHART_TIN2: 
      tIn2ChartView();
      break;
    case VIEW_CHART_TIN3: 
      tIn3ChartView();
      break;
    case VIEW_CHART_TIN4: 
      tIn4ChartView();
      break;
    case VIEW_CHART_HIN:
      hInChartView(); 
      break;
    case VIEW_CHART_TOUT: 
      tOutChartView();
      break;
    case VIEW_CHART_TOUT2: 
      tOut2ChartView();
      break;
    case VIEW_CHART_TOUT3: 
      tOut3ChartView();
      break;
    case VIEW_CHART_TOUT4: 
      tOut4ChartView();
      break;
    case VIEW_CHART_HOUT: 
      hOutChartView();
      break;
    case VIEW_CHART_P: 
      pressureChartView();
      break;  
  }
}

void readTemperature ()
{ 
  if ((long)millis() - timerDht > BMP_PERIOD_READ) {
    timerDht  = millis();
    
    int pressure = bme.readPressure();
    float h1 = bme.readHumidity();
    float t1 = bme.readTemperature(); /// <<< BME sensor instead dht1

    if (isnan(h1) || isnan(t1)) {
#if DEBUG_MODE == 1
      Serial.println(F("Failed to read from BME sensor!"));
#endif  
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("Error DHT_1 sensor"));
      
      errorHappened = true;
      return;
    } else {
      errorHappened = false;
    }

    float h2 = SiSensor.readHumidity();
    float t2 = SiSensor.readTemperature();

    if (isnan(h2) || isnan(t2)) {
#if DEBUG_MODE == 1
      Serial.println(F("Failed to read from Si7021 sensor!"));
#endif  
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(F("Error DHT_2 sensor"));
      
      errorHappened = true;
      return;
    } else {
      errorHappened = false;
    }    

#if DEBUG_MODE == 1
    Serial.print(F("T1: "));
    Serial.print(t1);
    Serial.print(" H1:");
    Serial.println(h1);
    Serial.print(F("T2: "));
    Serial.print(t2);
    Serial.print(" H2:");
    Serial.println(h2);
#endif
    
    if(t1 != currWeather.t_in || h1 != currWeather.h_in || t2 != currWeather.t_out || t2 != currWeather.t_out || pressure != currWeather.pressure) {
      if(pressure != currWeather.pressure) {
        pressureIsChanged = true;
      }
      weatherIsChanged = true;    
    } else {
      weatherIsChanged = false;     
    }

#if DEBUG_MODE == 1
    Serial.print("weatherIsChanged: ");
    Serial.println(weatherIsChanged ? "TRUE" : "FALSE");
#endif 
    
    currWeather.t_in = t1;
    currWeather.h_in = (int)h1;
    currWeather.t_out = t2;
    currWeather.h_out = (int)h2;
    currWeather.pressure = pressure;

    //Serial.println(currWeather.t_in);

    if(errorHappenedPrev && !errorHappened) {
      lcd.clear();
    }
    errorHappenedPrev = errorHappened;
  }
}

void readTemperatureMock ()
{ 
  if ((long)millis() - timerDht > DHT_PERIOD_READ) {
    timerDht  = millis();

    float h1 = rand() %100;
    float t1 = rand() % 10 / 10.0 + 25.0;

    float h2 = rand() %100;
    float t2 = rand() % 60 -20;

#if DEBUG_MODE == 1
    Serial.print(F("T1: "));
    Serial.print(t1);
    Serial.print(" H1:");
    Serial.println(h1);
    Serial.print(F("T2: "));
    Serial.print(t2);
    Serial.print(" H2:");
    Serial.println(h2);
#endif

    int pressure = rand() % 120000;;

#if DEBUG_MODE == 1
      Serial.print("P: ");
      Serial.println(pressure);
#endif    
  
    if(t1 != currWeather.t_in || h1 != currWeather.h_in || t2 != currWeather.t_out || t2 != currWeather.t_out || pressure != currWeather.pressure) {
      if(pressure != currWeather.pressure) {
        pressureIsChanged = true;
      }
      weatherIsChanged = true;    
    } else {
      weatherIsChanged = false;     
    }

    currWeather.t_in = t1;
    currWeather.h_in = (int)h1;
    currWeather.t_out = t2;
    currWeather.h_out = (int)h2;
    currWeather.pressure = pressure;
  }
}


void storeTemperature ()
{
  if ((long)millis() - timerStoreWeather > STORE_WEATHER_PERIOD) {
    timerStoreWeather  = millis();
    
    if(weatherStoredCounter < WEATHER_COUNT) {
      weatherStoredCounter++;
    }

    //move all to one pos 
    for (int i=weatherStoredCounter-1; i>0; i--) {
      weatherParameters[i].t_in = weatherParameters[i-1].t_in;
      weatherParameters[i].h_in = weatherParameters[i-1].h_in;
      weatherParameters[i].t_out = weatherParameters[i-1].t_out;
      weatherParameters[i].h_out = weatherParameters[i-1].h_out;
      weatherParameters[i].pressure = weatherParameters[i-1].pressure;
    }
    
    weatherParameters[0].t_in = currWeather.t_in;
    weatherParameters[0].h_in = currWeather.h_in;
    weatherParameters[0].t_out = currWeather.t_out;
    weatherParameters[0].h_out = currWeather.h_out;
    weatherParameters[0].pressure = currWeather.pressure;

#if DEBUG_MODE == 1    
    Serial.println(F("store temperature"));    
    for (int i=0; i<weatherStoredCounter; i++) {
      Serial.print(F("T1:"));
      Serial.print(weatherParameters[i].t_in);
      Serial.print(F(" H1:"));
      Serial.print(weatherParameters[i].h_in);
      Serial.print(F(" T2:"));
      Serial.print(weatherParameters[i].t_out);
      Serial.print(F(" H2:"));
      Serial.print(weatherParameters[i].h_out); 
      Serial.print(F(" P:"));
      Serial.print(weatherParameters[i].pressure);           
      Serial.println();
    }
    
    Serial.println();
#endif
  }
}

void initDevice () {
  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(6, 1);
  lcd.print("Loading...");
  delay(1000);

  unsigned status;
  status = bme.begin();  

  if (!status) {
#if DEBUG_MODE == 1    
    Serial.println(F("Failed to read from BME sensor!"));
#endif
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Error BME sensor"));
    
    while (1) {
      delay(1);
    }
  }

  if (!SiSensor.begin()) {
#if DEBUG_MODE == 1    
    Serial.println(F("Failed to read from Si7021 sensor!"));
#endif
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(F("Error Si7021 sensor"));
    
    while (1) {
      delay(1);
    }
  }

  lcd.clear();
}

void mainView() {

  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();

    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      char tInChange = 0;
      char tOutChange = 0;
      char pressureChange = 0;
      
      if(weatherStoredCounter > 0) {
          if(currWeather.t_in > weatherParameters[1].t_in) {
            tInChange = 1;  
          } else if (currWeather.t_in < weatherParameters[1].t_in) {
            tInChange = -1;
          }

          if(currWeather.t_out > weatherParameters[1].t_out) {
            tOutChange = 1;  
          } else if (currWeather.t_out < weatherParameters[1].t_out) {
            tOutChange = -1;
          }
      }

      if(weatherStoredCounter > 3) {
          if(currWeather.pressure > weatherParameters[3].pressure) {
            pressureChange = 1;  
          } else if (currWeather.pressure < weatherParameters[3].pressure) {
            pressureChange = -1;
          }
      }
  
      lcd.clear();
      
      lcd.setCursor(0, 0);
      lcd.print(F("Tin :"));
      lcd.setCursor(5, 0);
      lcd.print(currWeather.t_in, 1);
      lcd.setCursor(9, 0);
      lcd.print("C");
    
      if(tInChange == 1) {
        lcd.setCursor(10, 0);
        lcd.print("+");
      } else if(tInChange == -1) {
        lcd.setCursor(10, 0);
        lcd.print("-");
      }

      lcd.setCursor(12, 0);
      lcd.print(F("Hin :"));
      lcd.setCursor(17, 0);
      lcd.print(currWeather.h_in);
      lcd.setCursor(19, 0);
      lcd.print(F("%"));

      lcd.setCursor(0, 1);
      lcd.print(F("Tout:"));
      lcd.setCursor(5, 1);
      lcd.print(currWeather.t_out, 1);
      lcd.setCursor(9, 1);
      lcd.print("C");

      if(tOutChange == 1) {
        lcd.setCursor(10, 1);
        lcd.print("+");
      } else if(tOutChange == -1) {
        lcd.setCursor(10, 1);
        lcd.print("-");
      }
    
      lcd.setCursor(12, 1);
      lcd.print(F("Hout:"));
      lcd.setCursor(17, 1);
      lcd.print(currWeather.h_out);
      lcd.setCursor(19, 1);
      lcd.print(F("%"));

      lcd.setCursor(0, 2);
      lcd.print(F("P:"));
      lcd.setCursor(2, 2);
      lcd.print(currWeather.pressure);
      lcd.print(F("Pa("));
      lcd.print(currWeather.pressure/133.322, 0);
      lcd.print(F("mmHg)"));

      if(pressureChange == 1) {
        lcd.setCursor(10, 1);
        lcd.print("+");
      } else if(pressureChange == -1) {
        lcd.setCursor(10, 1);
        lcd.print("-");
      }

    }  

    lcd.setCursor(0, 3);
    lcd.print(F("Uptime:"));
    lcd.setCursor(7, 3);
    
    long uptime  = round((millis() - uptimeBegin) / 1000);
    
    lcd.print(uptime);
  }
}

void pressureChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(pressureIsChanged || viewChanged) {

      pressureIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex3[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex3[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      int textData[chartLength];
      
      chartData[chartLength-1] = currWeather.pressure;
      textData[chartLength-1] = currWeather.pressure / 133.322;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex3[i]].pressure;
        textData[chartLength-i-2] = weatherParameters[useIndex3[i]].pressure / 133.322;
      }
      
      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      int pt_min = find_min(textData, chartLength);
      int pt_max = find_max(textData, chartLength);

#if DEBUG_MODE == 1
      Serial.println("pressureChartView");
      Serial.print("chartLength: ");
      Serial.println(chartLength);
      for (int i=0; i<chartLength-1; i++) {
        Serial.print(chartData[i]);
        Serial.print(" ");
      }
      Serial.println(" ");
      Serial.print("p_min: ");
      Serial.print(p_min);
      Serial.print(" p_max: ");
      Serial.print(p_max);
      Serial.print(" pt_min: ");
      Serial.print(pt_min);
      Serial.print(" pt_max: ");
      Serial.println(pt_max);
#endif      
      
      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      lcd.setCursor(17, 0);
      lcd.print("P");
      lcd.setCursor(17, 1);
      lcd.print(pt_max);
      lcd.setCursor(16, 2);
      lcd.print(">");
      lcd.print(textData[chartLength-1]);
      lcd.setCursor(17, 3);
      lcd.print(pt_min);
      
    }
  }
}

void tInChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      float textData[chartLength];
      
      chartData[chartLength-1] = currWeather.t_in * 10;
      textData[chartLength-1] = currWeather.t_in;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex[i]].t_in * 10;
        textData[chartLength-i-2] = weatherParameters[useIndex[i]].t_in;
      }

      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      float pt_min = find_min(textData, chartLength);
      float pt_max = find_max(textData, chartLength);

      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      int cursorPosition_1 = 16;
      int cursorPosition_2 = 15;
      int cursorPosition_3 = 16;

      if(pt_max < 0) {
         cursorPosition_1--;
      }
      if (abs(pt_max) < 10) {
        cursorPosition_1++;
      }
      if(currWeather.t_in < 0) {
         cursorPosition_2--;
      }
      if (abs(currWeather.t_in) < 10) {
        cursorPosition_2++;
      }
      if(pt_min < 0) {
         cursorPosition_3--;
      }
      if (abs(pt_min) < 10) {
        cursorPosition_3++;
      }

      lcd.setCursor(cursorPosition_1, 0);
      lcd.print("Tin");
      lcd.setCursor(15, 1);
      lcd.print("     ");     
      lcd.setCursor(cursorPosition_1, 1); 
      lcd.print(pt_max,1);
      lcd.setCursor(14, 2);
      lcd.print("      ");
      lcd.setCursor(cursorPosition_2, 2);
      lcd.print(">");
      lcd.print(currWeather.t_in,1);
      lcd.setCursor(15, 3);
      lcd.print("     ");
      lcd.setCursor(cursorPosition_3, 3);
      lcd.print(pt_min,1);
      
    }
  }
}

void tIn2ChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex2[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex2[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      float textData[chartLength];
      
      chartData[chartLength-1] = currWeather.t_in * 10;
      textData[chartLength-1] = currWeather.t_in;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex2[i]].t_in * 10;
        textData[chartLength-i-2] = weatherParameters[useIndex2[i]].t_in;
      }

      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      float pt_min = find_min(textData, chartLength);
      float pt_max = find_max(textData, chartLength);

      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      int cursorPosition_1 = 16;
      int cursorPosition_2 = 15;
      int cursorPosition_3 = 16;

      if(pt_max < 0) {
         cursorPosition_1--;
      }
      if (abs(pt_max) < 10) {
        cursorPosition_1++;
      }
      if(currWeather.t_in < 0) {
         cursorPosition_2--;
      }
      if (abs(currWeather.t_in) < 10) {
        cursorPosition_2++;
      }
      if(pt_min < 0) {
         cursorPosition_3--;
      }
      if (abs(pt_min) < 10) {
        cursorPosition_3++;
      }

      lcd.setCursor(cursorPosition_1, 0);
      lcd.print("Tin2");
      lcd.setCursor(15, 1);
      lcd.print("     ");     
      lcd.setCursor(cursorPosition_1, 1); 
      lcd.print(pt_max,1);
      lcd.setCursor(14, 2);
      lcd.print("      ");
      lcd.setCursor(cursorPosition_2, 2);
      lcd.print(">");
      lcd.print(currWeather.t_in,1);
      lcd.setCursor(15, 3);
      lcd.print("     ");
      lcd.setCursor(cursorPosition_3, 3);
      lcd.print(pt_min,1);
      
    }
  }
}

void tIn3ChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex3[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex3[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      float textData[chartLength];
      
      chartData[chartLength-1] = currWeather.t_in * 10;
      textData[chartLength-1] = currWeather.t_in;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex3[i]].t_in * 10;
        textData[chartLength-i-2] = weatherParameters[useIndex3[i]].t_in;
      }

      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      float pt_min = find_min(textData, chartLength);
      float pt_max = find_max(textData, chartLength);

      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      int cursorPosition_1 = 16;
      int cursorPosition_2 = 15;
      int cursorPosition_3 = 16;

      if(pt_max < 0) {
         cursorPosition_1--;
      }
      if (abs(pt_max) < 10) {
        cursorPosition_1++;
      }
      if(currWeather.t_in < 0) {
         cursorPosition_2--;
      }
      if (abs(currWeather.t_in) < 10) {
        cursorPosition_2++;
      }
      if(pt_min < 0) {
         cursorPosition_3--;
      }
      if (abs(pt_min) < 10) {
        cursorPosition_3++;
      }

      lcd.setCursor(cursorPosition_1, 0);
      lcd.print("Tin3");
      lcd.setCursor(15, 1);
      lcd.print("     ");     
      lcd.setCursor(cursorPosition_1, 1); 
      lcd.print(pt_max,1);
      lcd.setCursor(14, 2);
      lcd.print("      ");
      lcd.setCursor(cursorPosition_2, 2);
      lcd.print(">");
      lcd.print(currWeather.t_in,1);
      lcd.setCursor(15, 3);
      lcd.print("     ");
      lcd.setCursor(cursorPosition_3, 3);
      lcd.print(pt_min,1);
      
    }
  }
}

void tIn4ChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex4[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex4[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      float textData[chartLength];
      
      chartData[chartLength-1] = currWeather.t_in * 10;
      textData[chartLength-1] = currWeather.t_in;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex4[i]].t_in * 10;
        textData[chartLength-i-2] = weatherParameters[useIndex4[i]].t_in;
      }

      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      float pt_min = find_min(textData, chartLength);
      float pt_max = find_max(textData, chartLength);

      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      int cursorPosition_1 = 16;
      int cursorPosition_2 = 15;
      int cursorPosition_3 = 16;

      if(pt_max < 0) {
         cursorPosition_1--;
      }
      if (abs(pt_max) < 10) {
        cursorPosition_1++;
      }
      if(currWeather.t_in < 0) {
         cursorPosition_2--;
      }
      if (abs(currWeather.t_in) < 10) {
        cursorPosition_2++;
      }
      if(pt_min < 0) {
         cursorPosition_3--;
      }
      if (abs(pt_min) < 10) {
        cursorPosition_3++;
      }

      lcd.setCursor(cursorPosition_1, 0);
      lcd.print("Tin4");
      lcd.setCursor(15, 1);
      lcd.print("     ");     
      lcd.setCursor(cursorPosition_1, 1); 
      lcd.print(pt_max,1);
      lcd.setCursor(14, 2);
      lcd.print("      ");
      lcd.setCursor(cursorPosition_2, 2);
      lcd.print(">");
      lcd.print(currWeather.t_in,1);
      lcd.setCursor(15, 3);
      lcd.print("     ");
      lcd.setCursor(cursorPosition_3, 3);
      lcd.print(pt_min,1);
      
    }
  }
}

void hInChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex[weatherStoredCounter-1], 13);      
        while (i<13 && useIndex[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      
      chartData[chartLength-1] = currWeather.h_in;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex[i]].h_in;
      }
      
      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      
      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      lcd.setCursor(16, 0);
      lcd.print("Hin");
      lcd.setCursor(17, 1);
      lcd.print("   ");
      lcd.setCursor(17, 1);
      lcd.print(p_max);
      lcd.print("%");
      lcd.setCursor(16, 2);
      lcd.print("    ");
      lcd.setCursor(16, 2);
      lcd.print(">");
      lcd.print(currWeather.h_in);
      lcd.print("%");
      lcd.setCursor(17, 3);
      lcd.print("   ");
      lcd.setCursor(17, 3);
      lcd.print(p_min);
      lcd.print("%");
    }
  }

}


void tOutChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      float textData[chartLength];
      
      chartData[chartLength-1] = currWeather.t_out * 10;
      textData[chartLength-1] = currWeather.t_out;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex[i]].t_out * 10;
        textData[chartLength-i-2] = weatherParameters[useIndex[i]].t_out;
      }


      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      float pt_min = find_min(textData, chartLength);
      float pt_max = find_max(textData, chartLength);

      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      int cursorPosition_1 = 16;
      int cursorPosition_2 = 15;
      int cursorPosition_3 = 16;

      if(pt_max < 0) {
         cursorPosition_1--;
      }
      if (abs(pt_max) < 10) {
        cursorPosition_1++;
      }
      if(currWeather.t_out < 0) {
         cursorPosition_2--;
      }
      if (abs(currWeather.t_out) < 10) {
        cursorPosition_2++;
      }
      if(pt_min < 0) {
         cursorPosition_3--;
      }
      if (abs(pt_min) < 10) {
        cursorPosition_3++;
      }

      lcd.setCursor(cursorPosition_1-1, 0);
      lcd.print("Tout");
      lcd.setCursor(15, 1);
      lcd.print("     ");     
      lcd.setCursor(cursorPosition_1, 1); 
      lcd.print(pt_max,1);
      lcd.setCursor(14, 2);
      lcd.print("      ");
      lcd.setCursor(cursorPosition_2, 2);
      lcd.print(">");
      lcd.print(currWeather.t_out,1);
      lcd.setCursor(15, 3);
      lcd.print("     ");
      lcd.setCursor(cursorPosition_3, 3);
      lcd.print(pt_min,1);
      
    }
  }
}

void tOut2ChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex2[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex2[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      float textData[chartLength];
      
      chartData[chartLength-1] = currWeather.t_out * 10;
      textData[chartLength-1] = currWeather.t_out;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex2[i]].t_out * 10;
        textData[chartLength-i-2] = weatherParameters[useIndex2[i]].t_out;
      }


      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      float pt_min = find_min(textData, chartLength);
      float pt_max = find_max(textData, chartLength);

      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      int cursorPosition_1 = 16;
      int cursorPosition_2 = 15;
      int cursorPosition_3 = 16;

      if(pt_max < 0) {
         cursorPosition_1--;
      }
      if (abs(pt_max) < 10) {
        cursorPosition_1++;
      }
      if(currWeather.t_out < 0) {
         cursorPosition_2--;
      }
      if (abs(currWeather.t_out) < 10) {
        cursorPosition_2++;
      }
      if(pt_min < 0) {
         cursorPosition_3--;
      }
      if (abs(pt_min) < 10) {
        cursorPosition_3++;
      }

      lcd.setCursor(cursorPosition_1-1, 0);
      lcd.print("Tout2");
      lcd.setCursor(15, 1);
      lcd.print("     ");     
      lcd.setCursor(cursorPosition_1, 1); 
      lcd.print(pt_max,1);
      lcd.setCursor(14, 2);
      lcd.print("      ");
      lcd.setCursor(cursorPosition_2, 2);
      lcd.print(">");
      lcd.print(currWeather.t_out,1);
      lcd.setCursor(15, 3);
      lcd.print("     ");
      lcd.setCursor(cursorPosition_3, 3);
      lcd.print(pt_min,1);
      
    }
  }
}

void tOut3ChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex3[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex3[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      float textData[chartLength];
      
      chartData[chartLength-1] = currWeather.t_out * 10;
      textData[chartLength-1] = currWeather.t_out;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex3[i]].t_out * 10;
        textData[chartLength-i-2] = weatherParameters[useIndex3[i]].t_out;
      }


      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      float pt_min = find_min(textData, chartLength);
      float pt_max = find_max(textData, chartLength);

      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      int cursorPosition_1 = 16;
      int cursorPosition_2 = 15;
      int cursorPosition_3 = 16;

      if(pt_max < 0) {
         cursorPosition_1--;
      }
      if (abs(pt_max) < 10) {
        cursorPosition_1++;
      }
      if(currWeather.t_out < 0) {
         cursorPosition_2--;
      }
      if (abs(currWeather.t_out) < 10) {
        cursorPosition_2++;
      }
      if(pt_min < 0) {
         cursorPosition_3--;
      }
      if (abs(pt_min) < 10) {
        cursorPosition_3++;
      }

      lcd.setCursor(cursorPosition_1-1, 0);
      lcd.print("Tout3");
      lcd.setCursor(15, 1);
      lcd.print("     ");     
      lcd.setCursor(cursorPosition_1, 1); 
      lcd.print(pt_max,1);
      lcd.setCursor(14, 2);
      lcd.print("      ");
      lcd.setCursor(cursorPosition_2, 2);
      lcd.print(">");
      lcd.print(currWeather.t_out,1);
      lcd.setCursor(15, 3);
      lcd.print("     ");
      lcd.setCursor(cursorPosition_3, 3);
      lcd.print(pt_min,1);
      
    }
  }
}

void tOut4ChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex4[weatherStoredCounter-1], 13);
      
        while (i<13 && useIndex4[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      float textData[chartLength];
      
      chartData[chartLength-1] = currWeather.t_out * 10;
      textData[chartLength-1] = currWeather.t_out;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex4[i]].t_out * 10;
        textData[chartLength-i-2] = weatherParameters[useIndex4[i]].t_out;
      }

      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      float pt_min = find_min(textData, chartLength);
      float pt_max = find_max(textData, chartLength);

      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      int cursorPosition_1 = 16;
      int cursorPosition_2 = 15;
      int cursorPosition_3 = 16;

      if(pt_max < 0) {
         cursorPosition_1--;
      }
      if (abs(pt_max) < 10) {
        cursorPosition_1++;
      }
      if(currWeather.t_out < 0) {
         cursorPosition_2--;
      }
      if (abs(currWeather.t_out) < 10) {
        cursorPosition_2++;
      }
      if(pt_min < 0) {
         cursorPosition_3--;
      }
      if (abs(pt_min) < 10) {
        cursorPosition_3++;
      }

      lcd.setCursor(cursorPosition_1-1, 0);
      lcd.print("Tout4");
      lcd.setCursor(15, 1);
      lcd.print("     ");     
      lcd.setCursor(cursorPosition_1, 1); 
      lcd.print(pt_max,1);
      lcd.setCursor(14, 2);
      lcd.print("      ");
      lcd.setCursor(cursorPosition_2, 2);
      lcd.print(">");
      lcd.print(currWeather.t_out,1);
      lcd.setCursor(15, 3);
      lcd.print("     ");
      lcd.setCursor(cursorPosition_3, 3);
      lcd.print(pt_min,1);
      
    }
  }
}


void hOutChartView () {
  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    
    if(weatherIsChanged || viewChanged) {
      weatherIsChanged = false;
      viewChanged = false;

      int chartLength = 0;
      int i = 0;
      
      if (weatherStoredCounter) {
        chartLength = MIN(useIndex3[weatherStoredCounter-1], 13);      
        while (i<13 && useIndex3[i] <= weatherStoredCounter-1) {
          i++;
        }
      }

      chartLength = i+1;
      
      int chartData[chartLength];
      
      chartData[chartLength-1] = currWeather.h_out;
      for (int i=0; i<chartLength-1; i++) {
        chartData[chartLength-i-2] = weatherParameters[useIndex3[i]].h_out;
      }
      
      int p_min = find_min(chartData, chartLength);
      int p_max = find_max(chartData, chartLength);
      
      drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);

      lcd.setCursor(16, 0);
      lcd.print("Hout");
      lcd.setCursor(17, 1);
      lcd.print("   ");
      lcd.setCursor(17, 1);
      lcd.print(p_max);
      lcd.print("%");
      lcd.setCursor(16, 2);
      lcd.print("    ");
      lcd.setCursor(16, 2);
      lcd.print(">");
      lcd.print(currWeather.h_out);
      lcd.print("%");
      lcd.setCursor(17, 3);
      lcd.print("   ");
      lcd.setCursor(17, 3);
      lcd.print(p_min);
      lcd.print("%");
    }
  }

}


void initPlot() {
  // необходимые символы для работы
  // создано в http://maxpromer.github.io/LCD-Character-Creator/
  byte row8[8] = {0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
  byte row7[8] = {0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
  byte row6[8] = {0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
  byte row5[8] = {0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111,  0b11111};
  byte row4[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111,  0b11111};
  byte row3[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111,  0b11111};
  byte row2[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111,  0b11111};
  byte row1[8] = {0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b00000,  0b11111};
  lcd.createChar(0, row8);
  lcd.createChar(1, row1);
  lcd.createChar(2, row2);
  lcd.createChar(3, row3);
  lcd.createChar(4, row4);
  lcd.createChar(5, row5);
  lcd.createChar(6, row6);
  lcd.createChar(7, row7);
}

void drawPlot(int plot_array[], byte pos, byte row, byte width, byte height, int min_val, int max_val) {

  for (byte i = 0; i < width; i++) {                  // каждый столбец параметров
    byte infill, fract;
    // найти количество целых блоков с учётом минимума и максимума для отображения на графике
    infill = floor((float)(plot_array[i] - min_val) / (max_val - min_val) * height * 10);
    fract = (infill % 10) * 8 / 10;                   // найти количество оставшихся полосок
    infill = infill / 10;

    for (byte n = 0; n < height; n++) {     // для всех строк графика
      if (n < infill && infill > 0) {       // пока мы ниже уровня
        lcd.setCursor(i, (row - n));        // заполняем полными ячейками
        lcd.write(0);
      }
      if (n >= infill) {                    // если достигли уровня
        lcd.setCursor(i, (row - n));
        if (fract > 0) lcd.write(fract);          // заполняем дробные ячейки
        else lcd.write(16);                       // если дробные == 0, заливаем пустой
        for (byte k = n + 1; k < height; k++) {   // всё что сверху заливаем пустыми
          lcd.setCursor(i, (row - k));
          lcd.write(16);
        }
        break;
      }
    }
  }
}

int find_min(int a[], int n) {
  int c, min, index;
 
  min = a[0];
  index = 0;
 
  for (c = 1; c < n; c++) {
    if (a[c] < min) {
       index = c;
       min = a[c];
    }
  }
 
  return min;
}

int find_max(int a[], int n) {
  int c, max, index;
 
  max = a[0];
  index = 0;
 
  for (c = 1; c < n; c++) {
    if (a[c] > max) {
       index = c;
       max = a[c];
    }
  }
 
  return max;
}

float find_min(float a[], int n) {
  int c, index;
  float min;
 
  min = a[0];
  index = 0;
 
  for (c = 1; c < n; c++) {
    if (a[c] < min) {
       index = c;
       min = a[c];
    }
  }
 
  return min;
}

float find_max(float a[], int n) {
  int c, index;
  float max;
  
  max = a[0];
  index = 0;
 
  for (c = 1; c < n; c++) {
    if (a[c] > max) {
       index = c;
       max = a[c];
    }
  }
 
  return max;
}

/*
void testdevices () {
  float h = dht1.readHumidity();
  float t = dht1.readTemperature();     

  Serial.print("Current humidity = ");
  Serial.print(h);
  Serial.print("%  ");
  Serial.print("temperature = ");
  Serial.print(t); 
  Serial.println("C  ");

  delay(800);

  int32_t p = 0;
  float t2 = 0;
      
  p = bme.readPressure();
  t2 = bme.readTemperature();

  Serial.print("Pressure= ");
  Serial.print(p);
  Serial.print(" ");
  Serial.print("temperature2 = ");
  Serial.print(t2); 
  Serial.println("C  ");
   
  lcd.setCursor(0, 0);
  lcd.print("Hello, World!");
  delay(400);
  lcd.clear();
  lcd.setCursor(0,1);
  lcd.print("Hello, World!");
  delay(400);
  lcd.clear(); 
  lcd.setCursor(0,2);
  lcd.print("Hello, World!");
  delay(400);
  lcd.clear(); 
  lcd.setCursor(0,3);
  lcd.print("Hello, World!");
  delay(400);
  lcd.clear(); 

  butt1.tick();             // обязательная функция отработки. Должна постоянно опрашиваться
  if (butt1.isPress()) {    // правильная отработка нажатия с защитой от дребезга
    Serial.println("Button pressed");
  }
}*/

void testChart (byte dir) {

  if ((long)millis() - timerRedraw > REDRAW_PERIOD || viewChanged) {
    timerRedraw  = millis();
    viewChanged = false;

    int chartLength = 15;
    int chartData[chartLength];
    
    Serial.println("pressureChartView");
    
    for (int i=0; i<chartLength; i++) {

      if(dir) {
        chartData[i] = i * 10;
      } else {
        chartData[i] = i * -10;
      }   
      Serial.print(chartData[i]);
      Serial.print(" ");
    }
    
    int p_min = find_min(chartData, chartLength);
    int p_max = find_max(chartData, chartLength);
    
    Serial.println(" ");
    lcd.clear();
    drawPlot(chartData, 0, 3, chartLength, 4, p_min, p_max);
  }  

}
