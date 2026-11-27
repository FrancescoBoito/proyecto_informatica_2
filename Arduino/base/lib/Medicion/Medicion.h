#ifndef EL_SERENO_MEDICION_H
#define EL_SERENO_MEDICION_H

// Una instancia (un objeto de esta clase) representa un paquete recibido por la base.
// Los atributos son privados: solo cargar() puede reemplazar sus valores.
class Medicion {
public:
    // El constructor prepara un objeto nuevo antes de usarlo.
    Medicion();

    // Lee las cuatro cifras separadas por '|'. Devuelve false si el formato
    // es incorrecto. Un campo "nan" indica que el sensor no dio una lectura.
    // const char * apunta al primer carácter del texto y no permite cambiarlo.
    // bool devuelve verdadero o falso según la validación.
    bool cargar(const char *datos, int nuevoRssi);

    // Escribe la línea que leerá Processing en un arreglo de caracteres.
    // Devuelve false si el arreglo es demasiado chico.
    // linea apunta al arreglo donde se escribe. El const final promete no cambiar este objeto.
    bool prepararLinea(const char *numero, char *linea, unsigned capacidad) const;

    // Evalúa límites y conserva la histéresis del frío. No se llama desde main.cpp.
    bool hayAlarma(float limiteFrio, float limiteHumedad);

    // Lecturas para el LCD. Cada get devuelve un campo; const impide modificar el objeto.
    float getTemperaturaDht() const { return temperaturaDht; }
    float getHumedad() const { return humedad; }
    float getTemperaturaBmp() const { return temperaturaBmp; }
    float getPresion() const { return presion; }
    int getRssi() const { return rssi; }

private:
    // Desde fuera de la clase se accede a estos datos mediante sus métodos públicos.
    float temperaturaDht;  // °C, sensor DHT
    float humedad;         // %, sensor DHT
    float temperaturaBmp;  // °C, sensor BMP180
    float presion;         // hPa, sensor BMP180
    bool alarmaFrio = false;
    int rssi;             // dBm, medido por el RA-02 de la base
};

#endif

