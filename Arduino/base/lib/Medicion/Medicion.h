#ifndef EL_SERENO_MEDICION_H
#define EL_SERENO_MEDICION_H

// Una instancia representa un paquete de mediciones recibido por la base.
// Los atributos son privados: solo cargar() puede reemplazar sus valores.
class Medicion {
public:
    Medicion();

    // Lee las cuatro cifras separadas por '|'. Devuelve false si el formato
    // es incorrecto. Un campo "nan" indica que el sensor no dio una lectura.
    bool cargar(const char *datos, int nuevoRssi);

    // Escribe la línea que leerá Processing en un arreglo de caracteres.
    // Devuelve false si el arreglo es demasiado chico.
    bool prepararLinea(const char *numero, char *linea, unsigned capacidad) const;

    // Informa si una lectura válida supera alguno de los límites de alarma.
    bool hayAlarma(float limiteFrio, float limiteHumedad);

    // Lecturas para el LCD. Solo consultan valores, no los modifican.
    float getTemperaturaDht() const { return temperaturaDht; }
    float getHumedad() const { return humedad; }
    float getTemperaturaBmp() const { return temperaturaBmp; }
    float getPresion() const { return presion; }
    int getRssi() const { return rssi; }

private:
    float temperaturaDht;  // °C, sensor DHT
    float humedad;         // %, sensor DHT
    float temperaturaBmp;  // °C, sensor BMP180
    float presion;         // hPa, sensor BMP180
    bool alarmaFrio = false;
    int rssi;             // dBm, medido por el RA-02 de la base
};

#endif

