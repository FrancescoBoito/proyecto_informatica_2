// Variables globales del sketch: recuerdan la posición de la tabla y los colores.
int filaInicial = 0;
final int COLOR_NODO_1 = 0xff52dfd2;
final int COLOR_NODO_2 = 0xffffb86f;

// background, fill, rect, text y las variables width/height son de Processing;
// se llaman en cada draw() para pintar controles, nodos, gráficos y tabla.
void dibujarPanel() {
  background(12, 22, 32);
  noStroke();
  fill(22, 37, 49);
  rect(0, 0, width, 160);
  fill(COLOR_NODO_1);
  rect(0, 0, width, 4);
  fill(246);
  textSize(30);
  text("EL SERENO", 20, 40);
  fill(154, 181, 195);
  textSize(13);
  text("RED MICROCLIMÁTICA  /  MONITOREO EN VIVO", 228, 37);

  // ?: selecciona un rótulo sin acceder al arreglo cuando está vacío.
  String nombre = puertos.length == 0 ? "Sin puertos" : puertos[puertoElegido] + "  ›";
  boton(20, 54, 202, puerto == null ? nombre : puertoActivo);
  boton(230, 54, 104, "Actualizar");
  boton(342, 54, 116, puerto == null ? "Conectar" : "Desconectar");
  boton(510, 54, 158, "Abrir mediciones");
  boton(676, 54, 143, "Abrir eventos");
  boton(827, 54, 123, "Probar sonido");

  fill(206, 223, 231);
  textSize(14);
  text("Alarma de frío (°C)", 20, 123);
  boton(163, 102, 75, entradas[0] + (campoActivo == 0 ? "|" : ""));
  text("Humedad alta (%)", 265, 123);
  boton(405, 102, 75, entradas[1] + (campoActivo == 1 ? "|" : ""));
  boton(495, 102, 144, esperandoConfig ? "Confirmando…" : "Enviar límites");

  // El ?: anidado elige el estado de USB que verá el usuario.
  String enlace = estado == MONITOREO ? "Base conectada"
                      : estado == ESPERA ? "Esperando base" : "Base desconectada";
  fill(206, 223, 231);
  textSize(14);
  text(enlace + "  ·  " + aviso, 20, 150);
  text("Aplicados " + decimal(limiteFrio, 1) + " °C / " + decimal(limiteHumedad, 1) + " %", 680, 123);

  dibujarNodo(nodos[0], 20, 175, "DHT22 + BMP180");
  dibujarNodo(nodos[1], 630, 175, "DHT11 + BMP180");
  // Las columnas 5, 4 y 6 del CSV son BMP °C, humedad DHT y presión BMP.
  dibujarGrafico(20, 370, 386, 290, "TEMPERATURA BMP", "°C", 5, limiteFrio, true);
  dibujarGrafico(427, 370, 386, 290, "HUMEDAD DHT", "%", 4, limiteHumedad, true);
  dibujarGrafico(834, 370, 386, 290, "PRESIÓN BMP", "hPa", 6, 0, false);
  dibujarHistorial();
  fill(252, 184, 140);
  textSize(13);
  text(errorArchivos.isEmpty() && errorAudio ? "No se pudo reproducir el sonido." : errorArchivos, 20, height - 7);
}

// Presenta el estado y los valores recientes de una estación.
void dibujarNodo(Nodo nodo, int x, int y, String sensores) {
  noStroke();
  fill(28, 46, 58);
  rect(x, y, 590, 172, 10);
  fill(nodo.id == 1 ? COLOR_NODO_1 : COLOR_NODO_2);
  rect(x, y, 5, 172, 4);
  fill(244);
  textSize(20);
  text("Nodo " + nodo.id + " · " + sensores, x + 18, y + 29);
  textSize(14);
  fill(206, 223, 231);
  String estadoNodo = "Esperando primera lectura";
  if (estado != MONITOREO) estadoNodo = "Sin conexión con la base";
  else if (nodo.enLinea) estadoNodo = "En línea · RSSI " + nodo.rssi + " dBm";
  else if (nodo.recibido) estadoNodo = "Sin señal LoRa";
  text(estadoNodo, x + 18, y + 51);

  // Las cifras instantáneas solo se ven mientras llegan datos recientes.
  if (nodo.enLinea) {
    fill(nodo.id == 1 ? COLOR_NODO_1 : COLOR_NODO_2);
    textSize(26);
    text(valor(nodo.temperaturaBmp, "°C"), x + 18, y + 96);
    text(valor(nodo.humedad, "%"), x + 211, y + 96);
    text(valor(nodo.presion, "hPa"), x + 388, y + 96);
    fill(165, 198, 211);
    textSize(13);
    text("BMP °C", x + 18, y + 115);
    text("Humedad DHT", x + 211, y + 115);
    text("Presión BMP", x + 388, y + 115);
    text("DHT " + valor(nodo.temperaturaDht, "°C"), x + 18, y + 145);
  }
  fill(203, 223, 231);
  textSize(13);
  // Este total persiste en el objeto durante la sesión aunque se pierda el enlace.
  text("Frío acumulado: " + decimal(nodo.exposicion, 2) + " °C·min", x + 211, y + 145);
  boolean frio = nodo.enLinea && numeroValido(nodo.temperaturaBmp) && nodo.frio;
  boolean humedad = nodo.enLinea && numeroValido(nodo.humedad) && nodo.humedadAlta;
  if (frio || humedad) {
    fill(255, 151, 126);
    // El ?: anidado elige un texto para ambas alarmas o solo la activa.
    String alarmas = frio && humedad ? "HELADA Y HUMEDAD" : frio ? "HELADA" : "HUMEDAD ALTA";
    textAlign(RIGHT);
    text(alarmas, x + 574, y + 145);
    textAlign(LEFT);
  }
}

// Muestra la tabla de mediciones recientes, empezando por la más nueva.
void dibujarHistorial() {
  // Dos arreglos paralelos: la posición x[i] corresponde al título y campo i.
  int[] x = {30, 255, 350, 448, 570, 700, 821, 945, 1068};
  String[] titulos = {"Fecha y hora", "Nodo", "Sec.", "DHT °C", "Hum. %", "BMP °C", "hPa", "dBm", "°C·min"};
  fill(244);
  textSize(19);
  text("ÚLTIMAS 20 MEDICIONES", 20, 697);
  fill(155, 181, 195);
  textSize(12);
  text("Desplazá con la rueda para ver más filas", 290, 697);
  textSize(13);
  fill(177, 210, 224);
  for (int i = 0; i < titulos.length; i++) text(titulos[i], x[i], 726);
  int cantidad = archivos.cantidadReciente();
  // Math.min/max son de Java y acotan el desplazamiento a filas existentes.
  filaInicial = Math.min(filaInicial, Math.max(0, cantidad - 9));
  for (int fila = 0; fila < 9 && fila + filaInicial < cantidad; fila++) {
    int y = 732 + fila * 21;
    noStroke();
    fill(fila % 2 == 0 ? color(28, 46, 58) : color(20, 37, 49));
    rect(20, y, 1200, 21, 3);
    fill(232);
    String[] datos = archivos.filaReciente(fila + filaInicial);
    for (int i = 0; i < datos.length; i++) text(datos[i].isEmpty() ? "—" : datos[i], x[i], y + 14);
  }
}

// Processing llama mouseWheel al mover la rueda; mouseY es la posición actual.
public void mouseWheel(processing.event.MouseEvent rueda) {
  if (mouseY >= 690 && mouseY < 930) {
    // constrain es de Processing y mantiene el índice dentro de la tabla.
    filaInicial = constrain(filaInicial + rueda.getCount(), 0,
                           Math.max(0, archivos.cantidadReciente() - 9));
  }
}

// Envía el valor de ambos campos cuando tienen un número dentro del rango.
void aplicarLimites() {
  if (estado != MONITOREO) {
    aviso = "Conectá la base para enviar los límites.";
    return;
  }
  if (esperandoConfig) return;
  try {
    // Float.parseFloat y replace son Java; aceptan coma o punto decimal.
    float frio = Float.parseFloat(entradas[0].replace(',', '.'));
    float humedad = Float.parseFloat(entradas[1].replace(',', '.'));
    if (!numeroValido(frio) || frio < -40 || frio > 85 ||
        !numeroValido(humedad) || humedad < 0 || humedad > 100) {
      throw new NumberFormatException();
    }
    // round es de Processing; se envía una cifra decimal como espera CONFIG_OK.
    frio = round(frio * 10) / 10.0f;
    humedad = round(humedad * 10) / 10.0f;
    entradas[0] = decimal(frio, 1);
    entradas[1] = decimal(humedad, 1);
    campoActivo = -1;
    enviarLimites(frio, humedad);
  } catch (NumberFormatException error) {
    aviso = "Ingresá frío entre −40 y 85 °C y humedad entre 0 y 100 %.";
  }
}

// Abre un CSV con la aplicación asociada en la PC.
void abrirCsv(Path ruta) {
  try {
    // Desktop es Java y pide al sistema operativo abrir el CSV.
    Desktop.getDesktop().open(ruta.toFile());
  } catch (IOException | RuntimeException error) {
    aviso = "Archivo: " + ruta.toAbsolutePath();
  }
}

// Processing llama mousePressed en cada clic; las coordenadas eligen la acción.
public void mousePressed() {
  if (sobre(20, 54, 202) && puerto == null && puertos.length > 0) {
    puertoElegido = (puertoElegido + 1) % puertos.length;
  } else if (sobre(230, 54, 104)) {
    listarPuertos();
  } else if (sobre(342, 54, 116)) {
    if (puerto == null) conectar();
    else cerrarPuerto("Puerto cerrado");
  } else if (sobre(510, 54, 158)) {
    abrirCsv(archivos.mediciones);
  } else if (sobre(676, 54, 143)) {
    abrirCsv(archivos.eventos);
  } else if (sobre(827, 54, 123)) {
    iniciarPitido();
  } else if (sobre(163, 102, 75)) {
    campoActivo = 0;
    entradas[0] = "";
  } else if (sobre(405, 102, 75)) {
    campoActivo = 1;
    entradas[1] = "";
  } else if (sobre(495, 102, 144)) {
    aplicarLimites();
  }
}

// Processing llama keyPressed al teclear; key, ENTER y ESC son datos del sketch.
// Los campos aceptan enteros o decimales con punto o coma.
public void keyPressed() {
  if (campoActivo < 0) return;
  String texto = entradas[campoActivo];
  if (key == ENTER || key == RETURN) {
    aplicarLimites();
  } else if (key == ESC) {
    // Poner key a 0 impide que Processing cierre la ventana con ESC.
    key = 0;
    entradas[campoActivo] = decimal(campoActivo == 0 ? limiteFrio : limiteHumedad, 1);
    campoActivo = -1;
  } else if (key == BACKSPACE && texto.length() > 0) {
    entradas[campoActivo] = texto.substring(0, texto.length() - 1);
  // La condición limita longitud y caracteres; la validación numérica viene al enviar.
  } else if (texto.length() < 7 && (key >= '0' && key <= '9' || key == '.' || key == ',' || key == '-')) {
    entradas[campoActivo] += key;
  }
}

// Comprueba si el mouse está dentro de un botón de 32 píxeles de alto.
boolean sobre(int x, int y, int ancho) {
  return mouseX >= x && mouseX < x + ancho && mouseY >= y && mouseY < y + 32;
}

// Dibuja un botón y cambia su color al pasar por encima.
void boton(int x, int y, int ancho, String titulo) {
  noStroke();
  fill(sobre(x, y, ancho) ? color(61, 88, 100) : color(41, 62, 75));
  rect(x, y, ancho, 32, 6);
  fill(240);
  textSize(14);
  text(titulo, x + 8, y + 21);
}

// Muestra la cifra con unidad o un guion si falta la lectura.
String valor(float dato, String unidad) {
  return numeroValido(dato) ? decimal(dato, 1) + " " + unidad : "—";
}
