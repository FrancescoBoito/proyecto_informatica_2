// Un objeto por nodo conserva la última lectura y el estado de sus alarmas.
final class Nodo {
    static final long VIGENCIA_MS = 15000;

    final int id;
    boolean enLinea;
    boolean recibido;
    boolean frio;
    boolean humedadAlta;
    long ultimaLectura;
    float temperaturaDht;
    float humedad;
    float temperaturaBmp = Float.NaN;
    float presion;
    int rssi;
    double exposicion;
    boolean omitirProximoTramo;

    // Asocia este objeto a la placa con el ID indicado.
    Nodo(int id) {
        this.id = id;
    }

    // Copia la muestra válida y suma el tramo de exposición que acaba de terminar.
    void recibir(long ahora, float tDht, float h, float tBmp, float p, int senal, float limite) {
        if (enLinea && !omitirProximoTramo) {
            exposicion += integrarFrio(temperaturaBmp, tBmp, limite, ahora - ultimaLectura);
        }
        omitirProximoTramo = false;
        temperaturaDht = tDht;
        humedad = h;
        temperaturaBmp = tBmp;
        presion = p;
        rssi = senal;
        ultimaLectura = ahora;
        recibido = true;
        enLinea = true;
    }

    // Euler explícito integra la diferencia con el umbral usando el valor
    // al inicio del intervalo. Elegimos este método porque basta conocer
    // las dos muestras y el tiempo entre ellas. Devuelve el aporte en °C·min.
    // El resultado supone que el valor anterior representa todo ese tramo.
    static double integrarFrio(float anterior, float actual, float limite, long intervaloMs) {
        if (!Float.isFinite(anterior) || !Float.isFinite(actual) ||
            intervaloMs <= 0 || intervaloMs >= VIGENCIA_MS) return 0;
        return Math.max(0.0, limite - anterior) * intervaloMs / 60000.0;
    }

    // Invalida el tramo abierto si se cambió el umbral durante su transcurso.
    void cambioUmbral() {
        omitirProximoTramo = true;
    }
}
