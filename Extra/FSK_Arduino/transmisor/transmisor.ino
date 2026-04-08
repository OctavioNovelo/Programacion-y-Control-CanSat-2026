#include <RadioLib.h>

SX1278 radio = new Module(5, 26, 14, 27);

enum Mode {
  MODE_LORA,
  MODE_FSK
};

Mode currentMode = MODE_LORA;

int telemetryCount = 0;
int imageCount = 0;

unit8_t protocolLoRa[7];
unit8_t protocolFSK[4];







// ===========================================================================================================
// Modos

void initLoRa() {

  // frecuencia en MHz
  // bw ?
  // sf ?
  // cr ?
  // sync
  // potencia
  // preamble ?
  // gain ?
  int state = radio.begin(434.0, 125.0, 7, 5, 0x34, 17, 8);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.println("Error LoRa");
    while(true);
  }
  
  Serial.println("Modo: LoRa: Envio Telemetria... ");
}

void initFSK() {

  // frecuencia en MHz
  // bitrate
  // freqDev (desviacion)
  // rxbw ?
  // potencia
  // preamble ? 
  // gain ?
  int state = radio.beginFSK(434.0, 50.0, 50.0, 125.0, 17, 16);

  if (state != RADIOLIB_ERR_NONE) {
    Serial.println("Error FSK");
    while(true);
  }

  Serial.println("Modo FSK: Imagen");
}







// ==========================================================================================================
// Informacion de los sensores

void bme280() { // Temperatura y precion

  // Wire.beginTransmission(...)
  // Leer registros BME280
  // https://github.com/ProjectsByJRP/stm32-bme280/tree/master
  // 

  // protocolLoRa[1] = temperatura;
  // byte alto // protocolLoRa[4] = Presion >> 8;
  // byte bajo // protocolLoRa[5] = Presion & 0xFF;
  
}

void bno085() {

  // Lectura real IMU por I2C/SPI
  // https://github.com/ufnalski/ahrs_bno085_g474re
  // 

  // byte alto // protocolLoRa[2] = Altitud >> 8;
  // byte bajo // protocolLoRa[3] = Altitud & 0xFF;
  
  // Esto es lo que tenia antes (ax y ay siento que me serviran despues) // String packet = "IMU: Velocidad en X =" + String(ax) + "Velociad en Y =" + String(ay);
}

void esp32_image() {

  // UART con ESP32-CAM
  // Envio de fragmento JPEG

  // protocolFSK[1] = 0; // id
  // protocolFSK[2] = 0; // data

  imageCount++;
}







// ===================================================================================================================================================

void setup() {
  
  Serial.begin(115200);
  delay(2000);

   protocolLoRa[0] = 0; // Numero de paquete
   protocolLoRa[1] = 0;
   protocolLoRa[2] = 0;
   protocolLoRa[3] = 0;
   protocolLoRa[4] = 0;
   protocolLoRa[5] = 0;
   protocolLoRa[6] = 0; // Verificacion

   protocolFSK[0] = 0; // Numero de paquete
   protocolFSK[1] = 0; // id
   protocolFSK[2] = 0; // data
   protocolFSK[3] = 0; // Verificacion
    
  /* struct telemetria {
    unit8_t numpaquet;
    unit8_t temp;
    unit16_t altitud;
    unit16_t presion;
    unit8_t verif;
  }; */

  /*
  struct imagen {
    unit8_t numpaquet;
  }
  */
  
  Serial.println(">>> Transmisor iniciado");
  initLoRa();
}







// ===================================================================================================================================================

void loop() {
  if (currentMode == MODE_LORA) {

    protocolLoRa[0] = telemetryCount++; // Numero de paquete
    protocolLoRa[6] = 0; // Verificacion

    radio.transmit(protocolLoRa, 7);
    // Cambiar Serial.println(protocolLoRa, 7);
    
    if (telemetryCount >= 5) {
      Serial.println(">>> Imagen >>>");
      initFSK();
      currentMode = MODE_FSK;
      imageCount = 0;
    }
  }


  else if (currentMode == MODE_FSK) {

    protocolFSK[0] = imageCount++;
    protocolFSK[3] = 0;

    radio.transmit(protocolFSK, 4);
    // Cambiar Serial.println(protocolFSK);
    
    if (imageCount >= 20) {
      Serial.println(">>> Telemetria >>>");
      initLoRa();
      currentMode = MODE_LORA;
      telemetryCount = 0;
    }
  }
}
