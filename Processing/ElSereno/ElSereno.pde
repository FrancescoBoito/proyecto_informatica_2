import processing.serial.*;
import java.awt.Desktop;
import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.Locale;
import javax.sound.sampled.*;

// El estado del enlace de la PC se maneja aparte del enlace LoRa de cada nodo.
final int DESCONEXION = 0, ESPERA = 1, MONITOREO = 2;
final int PLAZO_CONFIG = 4000;
int estado = DESCONEXION;
Serial puerto;
String puertoActivo = "";
String[] puertos = new String[0];
int puertoElegido = 0;
long ultimaActividadBase;
boolean baseVista;
boolean cerrando;

Nodo[] nodos = {new Nodo(1), new Nodo(2)};
ArchivosCsv archivos;
String aviso = "";
String errorArchivos = "";

// Se separan los límites escritos de los aceptados por la base.
float limiteFrio = 0.0f, limiteHumedad = 90.0f;
float frioPendiente, humedadPendiente;
boolean esperandoConfig;
long momentoConfig;
String[] entradas = {"0.0", "90.0"};
int campoActivo = -1;

// Prepara la ventana, recupera el histórico y busca puertos serie.
public void setup() {
  size(1040, 794);
  surface.setTitle("El Sereno V4.0");
  textFont(createFont("SansSerif", 16));
  archivos = new ArchivosCsv(Paths.get(sketchPath("datos")));
  try {
    archivos.iniciar();
    evento(0, "INICIO", "Processing iniciado");
  } catch (IOException error) {
    errorArchivos = "No se pudo abrir el historial: " + error.getMessage();
  }
  listarPuertos();
}

// Atiende la base, comprueba los plazos y vuelve a dibujar el panel.
public void draw() {
  leerSerie();
  vigilarEnlaces(relojMs());
  dibujarPanel();
}

// Tiempo monotónico para medir intervalos aunque se ajuste el reloj de pared.
long relojMs() {
  return System.nanoTime() / 1000000L;
}

// Devuelve true para una lectura utilizable como temperatura o humedad.
boolean numeroValido(float valor) {
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

// Registra un cierre normal y libera el puerto. exit también cierra la ventana.
public void exit() {
  if (!cerrando && archivos != null) {
    cerrando = true;
    cerrarPuerto("Cierre de Processing");
    evento(0, "CIERRE", "Processing cerrado normalmente");
  }
  super.exit();
}
