#ifndef SERENO_SENSORES_H
#define SERENO_SENSORES_H

#include <Arduino.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>

// Reúne las cuatro lecturas que espera la base. NAN significa dato faltante.
struct Lecturas {
    float temperaturaDht;
    float humedad;
    float temperaturaBmp;
    float presionHpa;
};

class Sensores {
public:
    Sensores(uint8_t pinDht, uint8_t tipoDht);
    void iniciar(int sda, int scl);
    Lecturas leer();

private:
    DHT dht;
    Adafruit_BMP085 bmp;
    bool bmpIniciado = false;
};

#endif
