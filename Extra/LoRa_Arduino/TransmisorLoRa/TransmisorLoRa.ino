#include <SPI.h>
#include <LoRa.h>

const int csPin = 10;          
const int resetPin = 9;       
const int irqPin = 2;          
const int ledVerde = 7;
const int ledRojo = 6;

unsigned long ultimaRecepcion = 0;

void setup() {
  pinMode(ledVerde, OUTPUT);
  pinMode(ledRojo, OUTPUT);
  
  Serial.begin(9600);
  LoRa.setPins(csPin, resetPin, irqPin); 

  if (!LoRa.begin(433E6)) {
    Serial.println("Error al iniciar LoRa RX");
    while (1);
  }
  Serial.println("Receptor a la escucha...");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  
  if (packetSize) {
    ultimaRecepcion = millis(); // Actualiza el tiempo
    digitalWrite(ledVerde, HIGH);
    digitalWrite(ledRojo, LOW);

    Serial.print("Recibido: ");
    while (LoRa.available()) {
      Serial.print((char)LoRa.read());
    }
    Serial.print(" | RSSI: ");
    Serial.println(LoRa.packetRssi()); // Calidad de señal
  }

  // Si pasan más de 2 segundos sin señal, cambia a LED rojo
  if (millis() - ultimaRecepcion > 2000) {
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledRojo, HIGH);
  }
}
