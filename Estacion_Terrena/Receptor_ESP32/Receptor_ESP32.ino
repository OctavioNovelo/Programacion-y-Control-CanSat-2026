#include <RadioLib.h>

// Pines para ESP32 + SX1278
// NSS: 5, DIO0: 2, RST: 14, DIO1: 27
SX1278 radio = new Module(5, 2, 14, 27);

// ================== ESTRUCTURAS ==================
#pragma pack(push, 1)
typedef struct {
  uint8_t magic;
  uint8_t pkt;
  int16_t temperatura;
  int16_t altitud;
  uint16_t presion;
  uint8_t verificacion;
  uint8_t checksum;
} TelemetryPacketLoRa;

typedef struct {
  float accel_x;
  float accel_y;
  float accel_z;
  float giro_x;
  float giro_y;
  float giro_z;
} PacketLoRaBNO;

typedef struct {
  TelemetryPacketLoRa base;
  PacketLoRaBNO bno;
} FullTelemetryPacket;
#pragma pack(pop)

// ================== MODOS ==================
enum Mode {
  MODE_LORA,
  MODE_FSK
};

Mode currentMode = MODE_LORA;
unsigned long lastPacketTime = 0;

// ================== CONFIG LORA ==================
float freq = 434.0;
float bw = 125.0;
uint8_t sf = 7;
uint8_t cr = 5;               // 4/5
uint8_t syncWord = 0x34;     // 🔥 FIX aquí
int8_t power = 17;
uint16_t preamble = 8;

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println(F("\n--- Estacion Terrena CanSat 2026 ---"));
  Serial.println(F("Iniciando Receptor ESP32..."));

  initLoRa();
}

// ================== INIT LORA ==================
void initLoRa() {
  Serial.print(F("[LoRa] Configurando... "));

  int state = radio.begin(freq, bw, sf, cr, syncWord, power, preamble);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Éxito!"));
    currentMode = MODE_LORA;
  } else {
    Serial.print(F("Error, código: "));
    Serial.println(state);
    while (true);
  }
}

// ================== INIT FSK ==================
void initFSK() {
  Serial.print(F("[FSK] Configurando... "));

  int state = radio.beginFSK(freq, 50.0, 50.0, 125.0, power, 16);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Éxito!"));
    currentMode = MODE_FSK;
  } else {
    Serial.print(F("Error, código: "));
    Serial.println(state);
  }
}

// ================== LOOP ==================
void loop() {
  if (currentMode == MODE_LORA) {
    FullTelemetryPacket packet;

    int state = radio.receive((uint8_t*)&packet, sizeof(FullTelemetryPacket));

    if (state == RADIOLIB_ERR_NONE) {
      lastPacketTime = millis();

      if (packet.base.magic == 0xCA) {
        processPacket(packet);
      } else {
        Serial.println(F("Recibido paquete con Magic Byte incorrecto."));
      }
    } 
    else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
      // Normal
    } 
    else if (state != RADIOLIB_ERR_SPI_WRITE_FAILED) {
      if (state == RADIOLIB_ERR_CRC_MISMATCH) {
        Serial.println(F("Error de CRC en paquete recibido."));
      }
    }

    // Timeout para cambio de modo
    if (millis() - lastPacketTime > 5000 && lastPacketTime != 0) {
      Serial.println(F("\n>>> Sin telemetría LoRa. ¿Cambiar a FSK?"));
      // initFSK(); // opcional
    }
  } 
  else if (currentMode == MODE_FSK) {
    byte byteReceived;

    int state = radio.receive(&byteReceived, 1);

    if (state == RADIOLIB_ERR_NONE) {
      lastPacketTime = millis();
      Serial.print((char)byteReceived);
    }
  }
}

// ================== PROCESAMIENTO ==================
void processPacket(FullTelemetryPacket p) {
  Serial.println(F("--- Paquete Recibido ---"));

  Serial.print(F("ID: "));
  Serial.println(p.base.pkt);

  float temp = p.base.temperatura / 100.0;
  Serial.print(F("Temp: "));
  Serial.print(temp);
  Serial.println(F(" C"));

  Serial.print(F("Alt: "));
  Serial.print(p.base.altitud);
  Serial.println(F(" m"));

  Serial.print(F("Pres: "));
  Serial.print(p.base.presion);
  Serial.println(F(" hPa"));

  Serial.print(F("IMU Accel: "));
  Serial.print(p.bno.accel_x); Serial.print(F(", "));
  Serial.print(p.bno.accel_y); Serial.print(F(", "));
  Serial.println(p.bno.accel_z);

  Serial.print(F("IMU Giro: "));
  Serial.print(p.bno.giro_x); Serial.print(F(", "));
  Serial.print(p.bno.giro_y); Serial.print(F(", "));
  Serial.println(p.bno.giro_z);

  Serial.println(F("-----------------------"));
}
