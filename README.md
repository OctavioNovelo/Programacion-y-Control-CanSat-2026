# CanSat 2026: Stereo Vision & Telemetry System (ModelEggs)

Este repositorio contiene la arquitectura de software, especificaciones de hardware y protocolos de comunicación para la misión CanSat 2026 del equipo ModelEggs. El sistema destaca por su capacidad de transmisión mediante conmutación dinámica de modulación de radio frecuencia.

## Hardware
- **OBC:** STM32F401CCU6 (Blackpill).
- **Sensores:** IMU BNO085, BME280.
- **Carga Útil:** 2x ESP32 XIAO Sense.
- **Radio:** Semtech SX1278 (433MHz).
- **Ground Station:** ESP32 XIAO Sense + Receptor ESP32.

## Protocolo de Comunicaciones
Para garantizar la integridad de la imagen en el descenso, se utiliza el siguiente protocolo:

CAPTURE REQUEST (MASTER --> CAMARA):
| Campo | Tamaño | Direccion |
| :--- | :--- | :--- |
| CAPTURE_CMD | 2 Bytes | 0xCC |
| CAM_ID | 124 Bytes | 0xAA y 0xBB |

RETURN IMG SIZE (MASTER --> CAMARA:CAMARA --> MASTER):
| Campo | Tamaño | Direccion |
| :--- | :--- | :--- |
| ACK | 2 Bytes | 0xAC |
| CAM_ID | 1 Byte | 0xAA y 0xBB |
| RETURN_SIZE | 124 Bytes | 0xEE |
| SIZE3 | 1 Byte | 00 |
| SIZE2 | 1 Byte | 00 |
| SIZE1 | 1 Byte | 3A |
| SIZE0 | 1 Byte | 98 |


Para la telemetria usamos el sigiente protocolo:

TELEMETRY (STM32 --> SX1278:SX1278 --> ESP32 XIAO SENSE):
| Campo | Tamaño | Función |
| :--- | :--- | :--- |
| SYNC_BYTE | 2 Bytes | 0x5A |
| HEADER_TELEMETRY | 2 Bytes | 0x10 |
| LENGHT | 1 Byte | Reordenamiento de paquetes |
| BUFFER | 124 Bytes | Datos binarios de imagen (L8 Grayscale) |
| CRC8 | 1 Byte | Validación CRC-8 |


### Funcionamiento el sistema
1. **Telemetría LoRa:** Envío continuo de estado de sensores.
2. **Recepcion de Imagen:** La UI permite solicitar mediante la base terrena (master) a las camaras (slave:slave) la captura de la imagen. La base manda una peticion a las camaras para conectarse, si por alguna razon NO se conectan, la base sigue mandando la misma peticion hasta recibir el paquete correspondiente y asi con cada paquete de imagen.
3. **Sistema de liberacion:** A los 300m de altitud, se envía a base terrena una alerta de trigger. Durante el descenso, a los 300m el sistema de liberacion levanta el pin del supercapacitor libreando las helices. 


##  Procesamiento de Imagen 


##  Estructura del Repo
- `./Firmware`: Código en C++/STM32CubeIDE.
- `./`: Captura y fragmentación SPI para ESP32-CAM.
- `./Estacion_Terrena`: Script de procesamiento C#.
- `./Estacion_Terrena/Interfaz`: UI. 
- `./Estacion_Terrena/Receptor_XIAO`: Codigo C++ para el receptor ESP32 XIAO Sense. 
- `./Documentos`: Documentos enviados a la unam.