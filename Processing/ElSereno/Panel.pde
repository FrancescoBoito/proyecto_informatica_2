// Dibuja controles, los dos nodos y las últimas 20 filas del archivo.
void dibujarPanel() {
  background(18, 27, 34);
  fill(240);
  textSize(28);
  text("El Sereno V4.0", 20, 38);

  String nombre = puertos.length == 0 ? "Sin puertos" : puertos[puertoElegido] + "  ›";
  boton(20, 50, 202, puerto == null ? nombre : puertoActivo);
  boton(230, 50, 104, "Actualizar");
  boton(342, 50, 116, puerto == null ? "Conectar" : "Desconectar");
  boton(510, 50, 158, "Abrir mediciones");
  boton(676, 50, 143, "Abrir eventos");
  boton(827, 50, 123, "Probar sonido");

  fill(206, 223, 231);
  textSize(14);
  text("Alarma de frío (°C)", 20, 117);
  boton(163, 97, 75, entradas[0] + (campoActivo == 0 ? "|" : ""));
  text("Humedad alta (%)", 265, 117);
  boton(405, 97, 75, entradas[1] + (campoActivo == 1 ? "|" : ""));
  boton(495, 97, 144, esperandoConfig ? "Confirmando…" : "Enviar límites");

  String enlace = estado == MONITOREO ? "Base conectada"
                      : estado == ESPERA ? "Esperando base" : "Base desconectada";
  fill(206, 223, 231);
  textSize(14);
  text(enlace + "  ·  " + aviso, 20, 150);
  text("Aplicados " + decimal(limiteFrio, 1) + " °C / " + decimal(limiteHumedad, 1) + " %", 680, 117);

  dibujarNodo(nodos[0], 20, 165, "DHT11 + BMP180");
  dibujarNodo(nodos[1], 530, 165, "DHT22 + BMP180");
  dibujarHistorial();
  fill(252, 184, 140);
  textSize(13);
  text(errorArchivos.isEmpty() && errorAudio ? "No se pudo reproducir el sonido." : errorArchivos, 20, 780);
}

// Presenta el estado y los valores recientes de una estación.
void dibujarNodo(Nodo nodo, int x, int y, String sensores) {
  noStroke();
  fill(33, 48, 57);
  rect(x, y, 490, 132, 8);
  fill(244);
  textSize(20);
  text("Nodo " + nodo.id + " · " + sensores, x + 12, y + 26);
  textSize(14);
  fill(206, 223, 231);
  String estadoNodo = "Esperando primera lectura";
  if (estado != MONITOREO) estadoNodo = "Sin conexión con la base";
  else if (nodo.enLinea) estadoNodo = "En línea · RSSI " + nodo.rssi + " dBm";
  else if (nodo.recibido) estadoNodo = "Sin señal LoRa";
  text(estadoNodo, x + 12, y + 48);

  if (nodo.enLinea) {
    text("DHT " + valor(nodo.temperaturaDht, "°C") + "     Humedad " + valor(nodo.humedad, "%"), x + 12, y + 70);
    text("BMP " + valor(nodo.temperaturaBmp, "°C") + "     Presión " + valor(nodo.presion, "hPa"), x + 12, y + 90);
  }
  text("Frío acumulado: " + decimal(nodo.exposicion, 2) + " °C·min", x + 12, y + 114);
  boolean frio = nodo.enLinea && numeroValido(nodo.temperaturaBmp) && nodo.frio;
  boolean humedad = nodo.enLinea && numeroValido(nodo.humedad) && nodo.humedadAlta;
  if (frio || humedad) {
    fill(255, 151, 126);
    String alarmas = frio && humedad ? "HELADA Y HUMEDAD" : frio ? "HELADA" : "HUMEDAD ALTA";
    text(alarmas, x + 302, y + 114);
  }
}

// Muestra la tabla de mediciones recientes, empezando por la más nueva.
void dibujarHistorial() {
  int[] x = {28, 229, 292, 397, 500, 594, 693, 796, 890};
  String[] titulos = {"Fecha y hora", "Nodo", "Sec.", "DHT °C", "Hum. %", "BMP °C", "hPa", "dBm", "°C·min"};
  fill(244);
  textSize(19);
  text("Últimas 20 mediciones", 20, 325);
  textSize(13);
  fill(177, 210, 224);
  for (int i = 0; i < titulos.length; i++) text(titulos[i], x[i], 350);
  for (int fila = 0; fila < archivos.cantidadReciente(); fila++) {
    int y = 355 + fila * 20;
    noStroke();
    fill(fila % 2 == 0 ? color(33, 48, 57) : color(27, 39, 47));
    rect(20, y, 1000, 20);
    fill(232);
    String[] datos = archivos.filaReciente(fila);
    for (int i = 0; i < datos.length; i++) text(datos[i].isEmpty() ? "—" : datos[i], x[i], y + 14);
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
    float frio = Float.parseFloat(entradas[0].replace(',', '.'));
    float humedad = Float.parseFloat(entradas[1].replace(',', '.'));
    if (!numeroValido(frio) || frio < -40 || frio > 85 ||
        !numeroValido(humedad) || humedad < 0 || humedad > 100) {
      throw new NumberFormatException();
    }
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
    Desktop.getDesktop().open(ruta.toFile());
  } catch (IOException | RuntimeException error) {
    aviso = "Archivo: " + ruta.toAbsolutePath();
  }
}

// Selecciona el control bajo el puntero y ejecuta su acción.
public void mousePressed() {
  if (sobre(20, 50, 202) && puerto == null && puertos.length > 0) {
    puertoElegido = (puertoElegido + 1) % puertos.length;
  } else if (sobre(230, 50, 104)) {
    listarPuertos();
  } else if (sobre(342, 50, 116)) {
    if (puerto == null) conectar();
    else cerrarPuerto("Puerto cerrado");
  } else if (sobre(510, 50, 158)) {
    abrirCsv(archivos.mediciones);
  } else if (sobre(676, 50, 143)) {
    abrirCsv(archivos.eventos);
  } else if (sobre(827, 50, 123)) {
    iniciarPitido();
  } else if (sobre(163, 97, 75)) {
    campoActivo = 0;
    entradas[0] = "";
  } else if (sobre(405, 97, 75)) {
    campoActivo = 1;
    entradas[1] = "";
  } else if (sobre(495, 97, 144)) {
    aplicarLimites();
  }
}

// Los campos aceptan enteros o decimales con punto o coma.
public void keyPressed() {
  if (campoActivo < 0) return;
  String texto = entradas[campoActivo];
  if (key == ENTER || key == RETURN) {
    aplicarLimites();
  } else if (key == ESC) {
    key = 0;
    entradas[campoActivo] = decimal(campoActivo == 0 ? limiteFrio : limiteHumedad, 1);
    campoActivo = -1;
  } else if (key == BACKSPACE && texto.length() > 0) {
    entradas[campoActivo] = texto.substring(0, texto.length() - 1);
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
