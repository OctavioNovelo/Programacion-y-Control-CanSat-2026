#include "esp_camera.h"
#include <ESP32SPISlave.h>

ESP32SPISlave slave;
#define READY_PIN 33 // Conectar al CAM1_READY del STM32

void setup() {
  // ... (Configuración de cámara normal)
  pinMode(READY_PIN, OUTPUT);
  digitalWrite(READY_PIN, LOW);
  
  slave.setDataMode(SPI_MODE0);
  slave.begin(HSPI, 14, 12, 13, 15); // SCK, MISO, MOSI, SS
}

void loop() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) return;

  uint8_t *img_ptr = fb->buf;
  size_t total_len = fb->len;
  
  // Enviamos por fragmentos
  for (size_t i = 0; i < total_len; i += 256) {
    digitalWrite(READY_PIN, HIGH); // Avisar al STM32
    slave.queue(img_ptr + i, NULL, 256);
    slave.wait(); 
    digitalWrite(READY_PIN, LOW);
  }
  
  esp_camera_fb_return(fb);
  delay(5000); // Esperar entre ráfagas de fotos
}
