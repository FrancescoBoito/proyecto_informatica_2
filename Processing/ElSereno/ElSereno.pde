// .pde usa sintaxis Java y Processing une estas pestañas en un sketch.
// processing.serial y las funciones de dibujo son de Processing; java.* y
// javax.* pertenecen a Java y aportan archivos, reloj, escritorio y audio.
import processing.serial.*;
import java.awt.Desktop;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Locale;
import javax.sound.sampled.*;

// final fija constantes; int es entero, boolean vale true/false y long admite
// intervalos grandes. El estado del USB se maneja aparte del LoRa de cada nodo.
final int DESCONEXION = 0, ESPERA = 1, MONITOREO = 2;
final int PLAZO_CONFIG = 4000;
int estado = DESCONEXION;
// Serial es un objeto de la biblioteca Processing; null significa «sin puerto».
Serial puerto;
String puertoActivo = "";
String[] puertos = new String[0];
int puertoElegido = 0;
long ultimaActividadBase;
boolean baseVista;
boolean cerrando;

// [] declara un arreglo; new construye un objeto independiente para cada nodo.
Nodo[] nodos = {new Nodo(1), new Nodo(2)};
ArchivosCsv archivos;
String aviso = "";
String errorArchivos = "";

// Se separan los límites escritos de los aceptados por la base: hasta CONFIG_OK
// las alarmas y el frío acumulado siguen usando los límites anteriores.
float limiteFrio = 0.0f, limiteHumedad = 90.0f;
float frioPendiente, humedadPendiente;
boolean esperandoConfig;
long momentoConfig;
String[] entradas = {"0.0", "90.0"};
int campoActivo = -1;

// setup() es el punto de arranque de Processing y se ejecuta una sola vez.
// Prepara la ventana, recupera el histórico y busca puertos serie.
public void setup() {
  // size, surface, textFont y sketchPath son facilidades de Processing.
  size(1240, 940);
  surface.setTitle("El Sereno · Panel de monitoreo");
  textFont(createFont("SansSerif", 16));
  archivos = new ArchivosCsv(Paths.get(sketchPath("datos")));
  // try/catch es Java: intenta abrir los CSV y muestra el error si falla.
  try {
    archivos.iniciar();
    evento(0, "INICIO", "Processing iniciado");
  } catch (IOException error) {
    errorArchivos = "No se pudo abrir el historial: " + error.getMessage();
  }
  listarPuertos();
}

// draw() es llamado repetidamente por Processing; cada vuelta atiende datos,
// comprueba plazos y vuelve a dibujar el panel.
public void draw() {
  leerSerie();
  vigilarEnlaces(relojMs());
  dibujarPanel();
}

// Tiempo monotónico para medir intervalos aunque se ajuste el reloj de pared.
long relojMs() {
  // System.nanoTime() es Java; / 1000000 lo convierte a milisegundos.
  return System.nanoTime() / 1000000L;
}

// Devuelve true para una lectura utilizable como temperatura o humedad.
boolean numeroValido(float valor) {
  // Float.isFinite (Java) rechaza NaN («no es un número») e infinitos.
  return Float.isFinite(valor);
}

// Escribe un evento en el CSV. Un fallo de disco se informa en pantalla.
void evento(int id, String tipo, String detalle) {
  try {
    archivos.guardarEvento(id, tipo, detalle);
  } catch (IOException error) {
    errorArchivos = "No se pudo guardar un evento: " + error.getMessage();
  }
}

// Processing llama exit() al cerrar; super.exit() ejecuta el cierre normal del sketch.
// cerrando evita registrar dos veces el mismo evento.
public void exit() {
  if (!cerrando && archivos != null) {
    cerrando = true;
    cerrarPuerto("Cierre de Processing");
    evento(0, "CIERRE", "Processing cerrado normalmente");
  }
  super.exit();
}
