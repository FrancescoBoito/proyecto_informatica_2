#include "Medicion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// El constructor deja las cuatro lecturas como desconocidas.
Medicion::Medicion()
    : temperaturaDht(NAN), humedad(NAN), temperaturaBmp(NAN), presion(NAN), rssi(0) {}

bool Medicion::cargar(const char *datos, int nuevoRssi) {
    float valores[4];
    const char *campo = datos;

    for (int i = 0; i < 4; i++) {
        const char *fin;
        char separador = i == 3 ? '\0' : '|';

        // Los nodos envían nan al faltar un sensor. Se reconoce ese texto
        // directamente y se usa strtof para convertir las mediciones numéricas.
        if (strncmp(campo, "nan", 3) == 0 && campo[3] == separador) {
            valores[i] = NAN;
            fin = campo + 3;
        } else {
            char *finNumero;
            valores[i] = strtof(campo, &finNumero);
            if (finNumero == campo || isinf(valores[i])) return false;
            fin = finNumero;
        }

        // Cada campo debe terminar exactamente en una barra o al final.
        if (*fin != separador) return false;
        if (i < 3) campo = fin + 1;
    }

    // Se actualiza la medición solo después de validar los cuatro campos.
    temperaturaDht = valores[0];
    humedad = valores[1];
    temperaturaBmp = valores[2];
    presion = valores[3];
    rssi = nuevoRssi;
    return true;
}

bool Medicion::prepararLinea(const char *numero, char *linea, unsigned capacidad) const {
    // El prefijo SERENO distingue los datos de los mensajes de diagnóstico.
    // snprintf respeta el tamaño del arreglo y evita escribir fuera de él.
    int escritos = snprintf(linea, capacidad, "SERENO,%s,%.1f,%.1f,%.1f,%.1f,%d",
                            numero, temperaturaDht, humedad, temperaturaBmp, presion, rssi);
    return escritos >= 0 && static_cast<unsigned>(escritos) < capacidad;
}

// Usa el BMP180 para el frío en los dos nodos y la humedad que mide el DHT.
bool Medicion::hayAlarma(float limiteFrio, float limiteHumedad) {
    if (isfinite(temperaturaBmp)) {
        // Se libera el aviso con medio grado de margen para evitar encendidos repetidos.
        if (temperaturaBmp <= limiteFrio) alarmaFrio = true;
        else if (temperaturaBmp >= limiteFrio + 0.5f) alarmaFrio = false;
    }
    return (isfinite(temperaturaBmp) && alarmaFrio) ||
           (isfinite(humedad) && humedad >= limiteHumedad);
}
