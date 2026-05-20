#include <RadioLib.h>

// NSS: 5, DIO0: 2, RST: 14, DIO1: 27
SX1278 radio = new Module(5, 2, 14, 27);

// ================== HEADERS Y PROTOCOLO ==================
#define HEADER_TELEMETRY      0x10  // Telemetría general (BME280 + BNO085)
#define HEADER_DESCENT_STATUS 0x20  // Señal de estado de descenso
#define SYNC_BYTE             0x5A  // Byte de sincronización 'Z'

#define LED_AZUL D1
#define LED_VERDE D2
#define LED_ROJO D3

unsigned long t_azul = 0;
unsigned long t_verde = 0;
unsigned long t_rojo = 0;

const int LED_OFF_MS = 500;

// ================== ESTRUCTURAS DE DATOS ==================
#pragma pack(push, 1)
typedef struct 
{
  uint8_t magic;
  uint8_t pkt;
  int16_t temperatura;
  int16_t altitud;
  uint16_t presion;
  uint8_t verificacion;
  uint8_t checksum;
} TelemetryPacketLoRa;

typedef struct 
{
  float accel_x;
  float accel_y;
  float accel_z;
  float giro_x;
  float giro_y;
  float giro_z;
} PacketLoRaBNO;

typedef struct 
{
  TelemetryPacketLoRa base;
  PacketLoRaBNO bno;
} FullTelemetryPacket;
#pragma pack(pop)


// ================== VARIABLES DE CONTROL GLOBALES ==================
volatile bool recibeFlag = false;
unsigned long lastPacketTime = 0;

// ================== FUNCIONES Y CALLBACKS ==================

// Callback de interrupción
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
void setFlag(void) {
  recibeFlag = true;
}

/**
* Cálculo de CRC-8 para integridad de datos.
*/
uint8_t calculateCRC8(const uint8_t *data, size_t len) {
  uint8_t crc = 0x00;
  for (size_t i = 0; i < len; i++) {
    uint8_t extract = data[i];
    for (uint8_t tempI = 8; tempI; tempI--) {
      uint8_t sum = (crc ^ extract) & 0x01;
      crc >>= 1;
      if (sum) crc ^= 0x8C;
      extract >>= 1;
    }
  }
  return crc;
}

/**
* Empaqueta y envía datos por Serial.
* Estructura: [SYNC_BYTE][HEADER][LENGTH][BUFFER][CRC8]
*/
void forwardToSerial(uint8_t header, uint8_t* buffer, size_t len) {
  uint8_t crc = calculateCRC8(buffer, len);
  Serial.write(SYNC_BYTE);
  Serial.write(header);
  Serial.write((uint8_t)len);
  Serial.write(buffer, len);
  Serial.write(crc);
  Serial.flush();
}

// ================== INICIALIZACIÓN LoRa ==================
void initLoRa() {
  int state = radio.begin(434.0, 125.0, 7, 5, 0x34, 17, 8);

  if (state == RADIOLIB_ERR_NONE) {
    radio.setPacketReceivedAction(setFlag); // Vinculamos la interrupción
    radio.startReceive(); // Recibimos por LoRa
  } 
  else
  {
    while (true)
    {
      digitalWrite(LED_ROJO, HIGH);
      delay(500);
      digitalWrite(LED_ROJO, LOW);
      delay(500);
    }  
  }
}

// ================== RUTINA DE RECEPCIÓN ==================
void recibe() {
  if (recibeFlag) {
    recibeFlag = false; 
    
    size_t len = radio.getPacketLength();
    uint8_t buffer[256];
    
    int state = radio.readData(buffer, len);

    if (state == RADIOLIB_ERR_NONE) {
      lastPacketTime = millis();

      digitalWrite(LED_ROJO, HIGH);
      t_rojo = millis();
      
      if (len == sizeof(FullTelemetryPacket)) {
        FullTelemetryPacket* pkg = (FullTelemetryPacket*)buffer;

        if (pkg->base.verificacion == 1)
        {
          digitalWrite(LED_AZUL, HIGH);
          t_azul = millis(); 
        }
        
        forwardToSerial(HEADER_TELEMETRY, buffer, len);
      } 
      else if (len <= 4) 
      {
        digitalWrite(LED_VERDE, HIGH);
        t_verde = millis();
        forwardToSerial(HEADER_DESCENT_STATUS, buffer, len);
      }
    }
    
    // Volver a poner el radio en modo recepción
    radio.startReceive(); 
  }
}

// ================== SETUP Y LOOP ==================
void setup()
{
  pinMode(LED_AZUL, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO, OUTPUT);
  
  Serial.begin(115200);
  while (!Serial);
  
  initLoRa();
}

void loop()
{
  recibe();

  unsigned long now = millis();
  if (now - t_azul >= LED_OFF_MS){
    digitalWrite(LED_AZUL, LOW);
  }
  if (now - t_verde >= LED_OFF_MS){
    digitalWrite(LED_VERDE, LOW);
  }
  if (now - t_rojo >= 100){
    digitalWrite(LED_ROJO, LOW);
  }
}
