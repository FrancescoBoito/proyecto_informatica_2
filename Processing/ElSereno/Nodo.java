// Clase Java: un objeto por nodo conserva la última lectura y sus alarmas.
final class Nodo {
    // static comparte esta constante entre objetos; final impide cambiarla.
    static final long VIGENCIA_MS = 15000;

    // Cada objeto conserva sus propios campos; el ID queda fijo al construirlo.
    final int id;
    boolean enLinea;
    boolean recibido;
    boolean frio;
    boolean humedadAlta;
    long ultimaLectura;
    float temperaturaDht;
    float humedad;
    // Float.NaN representa temperatura BMP desconocida antes de la primera lectura.
    float temperaturaBmp = Float.NaN;
    float presion;
    int rssi;
    // Integral acumulada de frío, en °C·min. Cada nodo tiene su propio total.
    // No es una temperatura: combina cuánto bajó del límite y por cuánto tiempo.
    double exposicion;
    boolean omitirProximoTramo; // Se usa después de cambiar el límite de frío.

    // Constructor Java: new Nodo(1) llama a este bloque. this.id distingue
    // el campo del objeto del parámetro que también se llama id.
    Nodo(int id) {
        this.id = id;
    }

    // Llega una muestra nueva: primero calculamos el tramo desde la muestra
    // anterior; después guardamos la nueva para el siguiente tramo.
    void recibir(long ahora, float tDht, float h, float tBmp, float p, int senal, float limite) {
        // Al recibir el primer paquete todavía no hay un intervalo que integrar.
        // Tampoco se integra un tramo si hubo desconexión o cambio de umbral.
        // && exige continuidad y que el umbral no haya cambiado en este tramo.
        if (enLinea && !omitirProximoTramo) {
            exposicion += integrarFrio(temperaturaBmp, tBmp, limite, ahora - ultimaLectura);
        }
        omitirProximoTramo = false;
        // Se reemplaza la muestra previa después de calcular su aporte.
        temperaturaDht = tDht;
        humedad = h;
        temperaturaBmp = tBmp;
        presion = p;
        rssi = senal;
        ultimaLectura = ahora;
        recibido = true;
        enLinea = true;
    }

    // TP9 - Cálculo numérico: integración de Euler con la muestra anterior.
    // Si el límite es 0 °C y la muestra anterior fue -2 °C, hubo 2 °C de frío.
    // Si pasan 6 segundos, se suman 2 × 0,1 = 0,2 °C·min
    // Fórmula: aporte = max(0, límite - anterior) × intervaloMs / 60000.
    // Dividimos por 60000 para convertir milisegundos en minutos.
    // Es una aproximación: supone que la temperatura anterior se mantuvo
    // durante todo el intervalo. La muestra actual se usa en el tramo siguiente.
    // double da más precisión al total acumulado; static indica que el cálculo
    // no usa campos del objeto y puede llamarse desde cualquier Nodo.
    static double integrarFrio(float anterior, float actual, float limite, long intervaloMs) {
        // Una lectura faltante, un tiempo inválido o una pausa de 15 s o más
        // no representan un tramo continuo y aportan cero.
        if (!Float.isFinite(anterior) || !Float.isFinite(actual) ||
            intervaloMs <= 0 || intervaloMs >= VIGENCIA_MS) return 0;
        // Math.max es Java: si no hubo frío, el aporte queda en cero.
        return Math.max(0.0, limite - anterior) * intervaloMs / 60000.0;
    }

    // Si el usuario cambia el límite, descartamos solo el tramo en curso:
    // no sabemos qué parte ocurrió con el límite viejo y cuál con el nuevo.
    void cambioUmbral() {
        omitirProximoTramo = true;
    }
}

