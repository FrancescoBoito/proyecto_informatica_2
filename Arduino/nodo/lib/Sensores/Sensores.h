// Esta protección evita definir dos veces las mismas clases si se incluye el archivo más de una vez.
#ifndef SERENO_SENSORES_H
#define SERENO_SENSORES_H

// Arduino aporta tipos como uint8_t; las otras bibliotecas controlan los sensores físicos.
#include <Arduino.h>
#include <DHT.h>
#include <Adafruit_BMP085.h>

// Reúne las cuatro lecturas que espera la base. NAN significa dato faltante.
// struct crea un tipo de dato propio; cada campo float admite decimales.
struct Lecturas {
    float temperaturaDht;
    float humedad;
    float temperaturaBmp;
    float presionHpa;
};

// Un objeto Sensores reúne un DHT y un BMP180 y ofrece las operaciones de lectura.
class Sensores {
public:
    // Constructor: recibe el pin y el tipo de DHT al crear el objeto.
    Sensores(uint8_t pinDht, uint8_t tipoDht);
    // void indica que iniciar no devuelve un valor; leer devuelve una estructura Lecturas.
    void iniciar(int sda, int scl);
    Lecturas leer();

private:
    // Estos miembros pertenecen a cada objeto; desde main.cpp no se accede a ellos directamente.
    DHT dht;
    Adafruit_BMP085 bmp;
    // Recuerda si ya se logró iniciar el BMP180, para no hacerlo en cada lectura.
    bool bmpIniciado = false;
};

#endif
