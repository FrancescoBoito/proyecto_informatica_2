# TP9 — Cálculo numérico aplicado a El Sereno

## Problema

El Sereno avisa cuando la temperatura baja del límite de frío, pero esa alarma no me dice cuánto duró el frío. Quiero calcular cuánto frío se acumuló durante el funcionamiento del panel, por separado para cada nodo.

## Método elegido y fundamento

Para elegir el método consulté a una IA y comparé el promedio móvil, la extrapolación lineal y la integración numérica. Elegí **Euler explícito** porque ya tengo la temperatura y el tiempo entre mediciones, y necesito acumular ambos datos.

En cada intervalo supongo que la temperatura anterior se mantuvo hasta recibir la siguiente lectura. Calculo:

```text
frío = max(0, límite - temperatura anterior)        [°C]
aporte = frío × intervalo en milisegundos / 60000  [°C·min]
acumulado nuevo = acumulado anterior + aporte      [°C·min]
```

Por ejemplo, con límite de 0 °C, temperatura anterior de −2 °C y un intervalo de 6 segundos, sumo `2 × (6000 / 60000) = 0,2 °C·min`. Si la temperatura anterior estaba por encima del límite, sumo cero.

## Código definitivo en el proyecto final

Implementé el método en `Processing/ElSereno/Nodo.java`. El cálculo que uso es:

```java
static final long VIGENCIA_MS = 15000;

static double integrarFrio(float anterior, float actual,
                           float limite, long intervaloMs) {
    // Descarto datos faltantes e intervalos de 15 segundos o más.
    if (!Float.isFinite(anterior) || !Float.isFinite(actual) ||
        intervaloMs <= 0 || intervaloMs >= VIGENCIA_MS) return 0;

    // Divido por 60000 para convertir milisegundos en minutos.
    return Math.max(0.0, limite - anterior) * intervaloMs / 60000.0;
}
```

Cuando llega otra lectura, `recibir()` agrega el aporte al total de ese nodo:

```java
if (enLinea && !omitirProximoTramo) {
    exposicion += integrarFrio(temperaturaBmp, tBmp,
                               limite, ahora - ultimaLectura);
}
```

La primera lectura inicia el historial. Si falta un dato, pasa demasiado tiempo o cambio el límite, no sumo ese tramo. Muestro `exposicion` en la tarjeta del nodo y la guardo como `frio_c_min` en el CSV. La alarma de helada sigue usando la temperatura instantánea del BMP180.

## Pruebas y conclusiones

Con límite de 0 °C, comprobé que −2 °C durante 6 segundos aporta **0,2 °C·min**, que una temperatura anterior de 3 °C aporta **0**, y que un intervalo de 15 segundos o un dato faltante también aportan **0**.

Este cálculo me permite comparar la exposición al frío entre nodos durante una sesión. Es una aproximación porque uso la temperatura anterior para representar todo el intervalo. El total vuelve a cero al reiniciar Processing y, por sí solo, no indica si hubo daños en un cultivo.
