#include "Medicion.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// :: vincula la función con la clase; la lista tras : inicia los campos al crearla.
// El constructor deja las cuatro lecturas como desconocidas.
Medicion::Medicion()
    : temperaturaDht(NAN), humedad(NAN), temperaturaBmp(NAN), presion(NAN), rssi(0) {}

bool Medicion::cargar(const char *datos, int nuevoRssi) {
    // Arreglo temporal: la medición anterior se conserva si falla cualquier campo.
    float valores[4];
    // campo apunta al carácter donde empieza el campo que se está leyendo.
    const char *campo = datos;

    // Se esperan exactamente cuatro campos, separados por | excepto el último.
    for (int i = 0; i < 4; i++) {
        const char *fin;
        // '\0' es el carácter que marca el final de una cadena C.
        char separador = i == 3 ? '\0' : '|';

        // Los nodos envían nan al faltar un sensor. Se reconoce ese texto
        // directamente y se usa strtof para convertir las mediciones numéricas.
        if (strncmp(campo, "nan", 3) == 0 && campo[3] == separador) {
            // El puntero avanza tres posiciones, justo hasta el separador.
            valores[i] = NAN;
            fin = campo + 3;
        } else {
            char *finNumero;
            // strtof convierte texto a float y deja en finNumero dónde terminó.
            // &finNumero pasa la dirección de esa variable para que strtof la complete.
            valores[i] = strtof(campo, &finNumero);
            // Si no consumió caracteres o produjo infinito, se rechaza el paquete.
            if (finNumero == campo || isinf(valores[i])) return false;
            fin = finNumero;
        }

        // Cada campo debe terminar exactamente en una barra o al final.
        if (*fin != separador) return false;
        // *fin lee el carácter apuntado; después del separador comienza el siguiente campo.
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
    // snprintf devuelve la longitud necesaria; si no entra con su '\0', se informa fallo.
    return escritos >= 0 && static_cast<unsigned>(escritos) < capacidad;
}

// Usa el BMP180 para el frío en los dos nodos y la humedad que mide el DHT.
bool Medicion::hayAlarma(float limiteFrio, float limiteHumedad) {
    // isfinite descarta NAN e infinitos; un dato faltante no cambia el estado de frío.
    if (isfinite(temperaturaBmp)) {
        // Se libera el aviso con medio grado de margen para evitar encendidos repetidos.
        if (temperaturaBmp <= limiteFrio) alarmaFrio = true;
        else if (temperaturaBmp >= limiteFrio + 0.5f) alarmaFrio = false;
    }
    // || significa «o»: basta frío activo o humedad alta con lectura válida.
    return (isfinite(temperaturaBmp) && alarmaFrio) ||
           (isfinite(humedad) && humedad >= limiteHumedad);
}
