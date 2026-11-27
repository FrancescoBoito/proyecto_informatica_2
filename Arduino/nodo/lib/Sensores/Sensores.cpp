#include "Sensores.h"
#include <Wire.h>
#include <math.h>

// Sensores:: indica que esta función pertenece a la clase. Los dos puntos inicializan
// el objeto dht antes de entrar al cuerpo vacío del constructor.
// El programa principal decide si esta placa tiene DHT11 o DHT22.
Sensores::Sensores(uint8_t pinDht, uint8_t tipoDht) : dht(pinDht, tipoDht) {}

// Prepara el DHT y el bus I²C donde está conectado el BMP180.
void Sensores::iniciar(int sda, int scl) {
    // begin prepara el DHT; Wire.begin abre I²C con sus pines y reloj de 100 kHz.
    dht.begin();
    Wire.begin(sda, scl, 100000);
}

// Devuelve una muestra. Cada dato se puede usar aunque falte el otro sensor.
Lecturas Sensores::leer() {
    // La lista entre llaves llena los cuatro campos en el orden de Sensores.h.
    // Los dos valores BMP empiezan como NAN, es decir, «sin número válido».
    Lecturas datos = {dht.readTemperature(), dht.readHumidity(), NAN, NAN};

    // Si el BMP180 no responde en 0x77, se podrá intentar de nuevo en el próximo turno.
    Wire.beginTransmission(0x77);
    // endTransmission devuelve 0 si el dispositivo respondió; un valor distinto indica fallo.
    if (Wire.endTransmission() != 0) {
        bmpIniciado = false;
        return datos;
    }
    // ! invierte el booleano. &Wire entrega la dirección del objeto I²C a la biblioteca.
    if (!bmpIniciado) bmpIniciado = bmp.begin(BMP085_STANDARD, &Wire);
    if (bmpIniciado) {
        datos.temperaturaBmp = bmp.readTemperature();
        int32_t presionPa = bmp.readPressure();
        // La biblioteca informa pascales; 100 Pa equivalen a 1 hPa. El sufijo f marca un float.
        if (presionPa > 0) datos.presionHpa = presionPa / 100.0f;
    }
    return datos;
}
