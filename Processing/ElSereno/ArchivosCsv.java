// Esta pestaña .java es Java: importa clases para leer, escribir y fechar CSV.
import java.io.BufferedReader;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

// Clase Java usada por el sketch Processing para guardar y recuperar el historial.
final class ArchivosCsv {
    final Path mediciones;
    final Path eventos;
    // El mismo buffer guarda datos para la tabla y para los gráficos.
    // Arreglo de hasta 120 filas, cada una otro arreglo de campos String.
    final String[][] recientes = new String[120][];
    // private limita el acceso a esta clase; los índices forman un buffer circular.
    private int siguiente;
    private int cantidad;
    private static final String CABECERA_MEDICIONES =
        "fecha_hora,nodo,secuencia,temp_dht_c,humedad_pct,temp_bmp_c,presion_hpa,rssi_dbm,frio_c_min";
    private static final String CABECERA_EVENTOS = "fecha_hora,nodo,tipo,detalle";

    // Constructor: Path y resolve son Java y señalan los archivos dentro de datos/.
    ArchivosCsv(Path carpeta) {
        mediciones = carpeta.resolve("mediciones.csv");
        eventos = carpeta.resolve("eventos.csv");
    }

    // Prepara los encabezados y lee el historial sin guardar todo el archivo en memoria.
    // throws IOException avisa al llamador que debe atender un fallo de disco.
    void iniciar() throws IOException {
        Files.createDirectories(mediciones.getParent());
        preparar(mediciones, CABECERA_MEDICIONES);
        preparar(eventos, CABECERA_EVENTOS);
        cantidad = 0;
        siguiente = 0;
        // try con recurso cierra el lector automáticamente al salir del bloque.
        try (BufferedReader lector = Files.newBufferedReader(mediciones, StandardCharsets.UTF_8)) {
            lector.readLine(); // La primera línea contiene los nombres de columnas.
            String linea;
            // Lee hasta fin de archivo; solo conserva las últimas 120 filas en memoria.
            while ((linea = lector.readLine()) != null) {
                // -1 conserva celdas vacías: importan si un sensor informó NaN.
                String[] campos = linea.split(",", -1);
                if (campos.length == 9) agregarReciente(campos);
            }
        }
    }

    // Escribe el encabezado solamente si se estrena el archivo.
    private static void preparar(Path ruta, String encabezado) throws IOException {
        if (!Files.exists(ruta) || Files.size(ruta) == 0) {
            anexar(ruta, encabezado);
        }
    }

    // Agrega una fila al CSV. Si falla la escritura, igual se actualiza la vista en vivo.
    void guardarMedicion(String[] campos) throws IOException {
        agregarReciente(campos);
        preparar(mediciones, CABECERA_MEDICIONES);
        anexar(mediciones, String.join(",", campos));
    }

    // Los eventos tienen su propio CSV. El detalle se encierra entre comillas
    // y las comillas internas se duplican según el formato CSV.
    void guardarEvento(int nodo, String tipo, String detalle) throws IOException {
        preparar(eventos, CABECERA_EVENTOS);
        String id = nodo == 0 ? "" : Integer.toString(nodo);
        String texto = "\"" + detalle.replace("\"", "\"\"").replace('\n', ' ').replace('\r', ' ') + "\"";
        anexar(eventos, fechaHora() + "," + id + "," + tipo + "," + texto);
    }

    // APPEND agrega al final y Files.write cierra el archivo al terminar.
    private static void anexar(Path ruta, String linea) throws IOException {
        Files.write(ruta, (linea + "\n").getBytes(StandardCharsets.UTF_8),
                    StandardOpenOption.CREATE, StandardOpenOption.APPEND);
    }

    // Al llenar las 120 posiciones, la siguiente escritura reemplaza la más vieja.
    private void agregarReciente(String[] campos) {
        recientes[siguiente] = campos;
        // % devuelve el resto y hace que el índice vuelva a cero al llegar al final.
        siguiente = (siguiente + 1) % recientes.length;
        if (cantidad < recientes.length) cantidad++;
    }

    // La tabla muestra como máximo 20; el buffer guarda hasta 120 para gráficos.
    int cantidadReciente() {
        return Math.min(cantidad, 20);
    }

    // Devuelve la fila indicada contando desde la más reciente.
    String[] filaReciente(int posicion) {
        // siguiente señala la próxima escritura; restar 1 llega a la fila más nueva.
        return recientes[(siguiente + recientes.length - 1 - posicion) % recientes.length];
    }

    // Los gráficos leen desde la muestra más antigua hasta la más nueva.
    int cantidadGrafico() {
        return cantidad;
    }

    String[] filaGrafico(int posicion) {
        // Se parte de la más antigua conservada y se avanza hacia la más nueva.
        return recientes[(siguiente + recientes.length - cantidad + posicion) % recientes.length];
    }

    // Usa la hora local de la PC, ya que la base no tiene reloj de calendario.
    static String fechaHora() {
        return LocalDateTime.now().format(DateTimeFormatter.ofPattern("uuuu-MM-dd HH:mm:ss"));
    }
}
