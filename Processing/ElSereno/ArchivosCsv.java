import java.io.BufferedReader;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.time.LocalDateTime;
import java.time.format.DateTimeFormatter;

// Biblioteca local de Processing para guardar e ir recuperando el historial.
final class ArchivosCsv {
    final Path mediciones;
    final Path eventos;
    final String[][] recientes = new String[20][];
    private int siguiente;
    private int cantidad;
    private static final String CABECERA_MEDICIONES =
        "fecha_hora,nodo,secuencia,temp_dht_c,humedad_pct,temp_bmp_c,presion_hpa,rssi_dbm,frio_c_min";
    private static final String CABECERA_EVENTOS = "fecha_hora,nodo,tipo,detalle";

    // Usa una subcarpeta del sketch para que los archivos se encuentren fácil.
    ArchivosCsv(Path carpeta) {
        mediciones = carpeta.resolve("mediciones.csv");
        eventos = carpeta.resolve("eventos.csv");
    }

    // Prepara los encabezados y lee el historial sin guardar todo el archivo en memoria.
    void iniciar() throws IOException {
        Files.createDirectories(mediciones.getParent());
        preparar(mediciones, CABECERA_MEDICIONES);
        preparar(eventos, CABECERA_EVENTOS);
        cantidad = 0;
        siguiente = 0;
        try (BufferedReader lector = Files.newBufferedReader(mediciones, StandardCharsets.UTF_8)) {
            lector.readLine(); // La primera línea contiene los nombres de columnas.
            String linea;
            while ((linea = lector.readLine()) != null) {
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

    // Los eventos tienen su propio CSV. Se cita el detalle por si contiene comas.
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

    // Cuando se llenan las 20 posiciones, la siguiente escritura reemplaza la más vieja.
    private void agregarReciente(String[] campos) {
        recientes[siguiente] = campos;
        siguiente = (siguiente + 1) % recientes.length;
        if (cantidad < recientes.length) cantidad++;
    }

    // Indica cuántas filas de historial se pueden mostrar.
    int cantidadReciente() {
        return cantidad;
    }

    // Devuelve la fila indicada contando desde la más reciente.
    String[] filaReciente(int posicion) {
        return recientes[(siguiente + recientes.length - 1 - posicion) % recientes.length];
    }

    // Usa la hora local de la PC, ya que la base no tiene reloj de calendario.
    static String fechaHora() {
        return LocalDateTime.now().format(DateTimeFormatter.ofPattern("uuuu-MM-dd HH:mm:ss"));
    }
}
