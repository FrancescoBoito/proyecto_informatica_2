#include "Sensores.h"
#include <Wire.h>
#include <math.h>

// El programa principal decide si esta placa tiene DHT11 o DHT22.
Sensores::Sensores(uint8_t pinDht, uint8_t tipoDht) : dht(pinDht, tipoDht) {}

// Prepara el DHT y el bus I²C donde está conectado el BMP180.
void Sensores::iniciar(int sda, int scl) {
    dht.begin();
    Wire.begin(sda, scl, 100000);
}

// Devuelve una muestra. Cada dato se puede usar aunque falte el otro sensor.
Lecturas Sensores::leer() {
    Lecturas datos = {dht.readTemperature(), dht.readHumidity(), NAN, NAN};

    // Si el BMP180 no responde en 0x77, se podrá intentar de nuevo en el próximo turno.
    Wire.beginTransmission(0x77);
    if (Wire.endTransmission() != 0) {
        bmpIniciado = false;
        return datos;
    }
    if (!bmpIniciado) bmpIniciado = bmp.begin(BMP085_STANDARD, &Wire);
    if (bmpIniciado) {
        datos.temperaturaBmp = bmp.readTemperature();
        int32_t presionPa = bmp.readPressure();
        if (presionPa > 0) datos.presionHpa = presionPa / 100.0f;
    }
    return datos;
}
