#include <RadioLib.h>

SX1278 radio = new Module(5, 26, 14, 27);

enum Mode {
  MODE_LORA,
  MODE_SWITCH_TO_FSK,
  MODE_FSK,
  MODE_SWITCH_TO_LORA
};

#pragma pack(push,1)
struct TelemetryPacketLoRa {
  uint8_t magic;
  uint8_t pkt;
  int16_t temperatura;
  int16_t altitud;
  uint16_t presion;
  uint8_t verificacion;
  uint8_t extra;   // temporal
};
#pragma pack(pop)
  
Mode currentMode = MODE_LORA;


unsigned long lastPacketTime = 0;
int fskPacketCount = 0;

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
  
  Serial.println("Modo: LoRa: Recepcion Telemetria... ");
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

  Serial.println("Modo: FSK: Imagen");
}

// ===========================================================================================================
// Recepcion

void receiveLoRa() {

  uint8_t buffer[32];

  int state = radio.receive(buffer, sizeof(TelemetryPacketLoRa));

  if(state == RADIOLIB_ERR_NONE) {

    Serial.print("RAW: ");
    for(int i = 0; i < sizeof(TelemetryPacketLoRa); i++) {
      Serial.print(buffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();

    TelemetryPacketLoRa p;
    memcpy(&p, buffer, sizeof(p));

    Serial.print("MAGIC: "); Serial.println(p.magic, HEX);
    Serial.print("PKT: "); Serial.println(p.pkt);
    Serial.print("TEMP: "); Serial.println(p.temperatura);
    Serial.print("ALT: "); Serial.println(p.altitud);
    Serial.print("PRES: "); Serial.println(p.presion);
    Serial.print("VERIF: "); Serial.println(p.verificacion);
    Serial.print("CHK: "); Serial.println(p.extra);

    Serial.println("----------------");

    lastPacketTime = millis();
  }
}


void receiveFSK() {

  String str;

  int state = radio.receive(str);

  if(state == RADIOLIB_ERR_NONE) {

    Serial.print("Imagen: ");
    Serial.println(str);

    fskPacketCount++;
    lastPacketTime = millis();
  }
}

// ===========================================================================================================
// Checksum
uint8_t telemetry_checksum(uint8_t *data, uint8_t len) {
  uint8_t c = 0;

  for(int i = 0; i < len; i++)
    c ^= data[i];

  return c;
}


// ===========================================================================================================

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(">>> Receptor iniciado");

  initLoRa();
  lastPacketTime = millis();
}

// ===========================================================================================================

void loop() {

  switch(currentMode) {
    
    // =======================================================================================================
    // Modo LoRa
    case MODE_LORA:

      receiveLoRa();
      
      if (millis() - lastPacketTime > 3000) {
        Serial.println(">>> Cambiando a FSK");
        currentMode = MODE_SWITCH_TO_FSK;
      }
      break;

    case MODE_SWITCH_TO_FSK:

      initFSK();
      fskPacketCount = 0;
      lastPacketTime = millis();
      currentMode = MODE_FSK;
      break;

    // =======================================================================================================
    // Modo FSK
    
    case MODE_FSK:

      receiveFSK();

      // Si pasan 3 segundos sin paquetes FSK, asumimos fin de imagen
      if(millis() - lastPacketTime > 3000) {
        Serial.println(">>> Cambiando a LoRa");
        currentMode = MODE_SWITCH_TO_LORA;
      }
      break;
      
    case MODE_SWITCH_TO_LORA:

      initLoRa();
      lastPacketTime = millis();
      currentMode = MODE_LORA;
      break;
  }
}
