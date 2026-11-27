// Serial.list() es de Processing: devuelve un arreglo con los puertos detectados.
// Busca los puertos disponibles y selecciona el primero de la lista nueva.
void listarPuertos() {
  puertos = Serial.list();
  puertoElegido = 0;
  // null significa que no hay conexión; ?: elige el texto según haya puertos.
  if (puerto == null) aviso = puertos.length == 0 ? "No hay puertos serie." : "Elegí el puerto USB de la base.";
}

// Abre el puerto elegido. La base debe identificarse antes de aceptar el estado en línea.
void conectar() {
  if (puertos.length == 0) return;
  // try/catch es Java: un puerto ocupado o ausente puede lanzar una excepción.
  try {
    puertoActivo = puertos[puertoElegido];
    // new crea el puerto Processing a 115200 baudios; this refiere al sketch.
    puerto = new Serial(this, puertoActivo, 115200);
    puerto.clear();
    ultimaActividadBase = relojMs();
    estado = ESPERA;
    aviso = "Esperando a la base en " + puertoActivo;
  } catch (RuntimeException error) {
    cerrarPuerto("No se pudo abrir el puerto. Revisá si lo usa el monitor serie.");
  }
}

// Libera el USB y deja fuera de línea las lecturas, conservando el historial.
void cerrarPuerto(String motivo) {
  Serial anterior = puerto;
  puerto = null;
  if (estado == MONITOREO) evento(0, "BASE_DESCONECTADA", motivo);
  estado = DESCONEXION;
  esperandoConfig = false;
  // Este for de Java recorre los dos objetos Nodo sin usar un índice.
  for (Nodo nodo : nodos) nodo.enLinea = false;
  if (anterior != null) {
    try {
      anterior.stop();
    } catch (RuntimeException error) {
      println("El puerto ya no estaba disponible.");
    }
  }
  aviso = motivo;
}

// Lee como máximo 25 líneas completas por draw() para mantener ágil la pantalla.
void leerSerie() {
  if (puerto == null) return;
  try {
    for (int i = 0; i < 25 && puerto.available() > 0; i++) {
      // readStringUntil es de Processing y espera el fin de línea enviado por la base.
      String linea = puerto.readStringUntil('\n');
      if (linea == null) break;
      // trim es de Processing; quita el salto de línea y espacios de los extremos.
      procesarLinea(trim(linea), relojMs());
      if (puerto == null) return;
    }
  } catch (RuntimeException error) {
    cerrarPuerto("Se perdió el puerto USB.");
  }
}

// Separa las mediciones y respuestas de control de los mensajes de diagnóstico.
void procesarLinea(String linea, long ahora) {
  if (linea.equals("BASE_LISTA") || linea.equals("ESTADO_BASE")) {
    verBase(ahora, linea.equals("BASE_LISTA"));
    return;
  }
  if (linea.equals("CONFIG_ERROR") && esperandoConfig) {
    esperandoConfig = false;
    aviso = "La base rechazó los límites.";
    return;
  }

  // split es de String (Java); -1 conserva campos vacíos para validar el formato.
  String[] campos = linea.split(",", -1); // Conserva los campos vacíos.
  if (campos.length == 3 && campos[0].equals("CONFIG_OK")) {
    confirmarLimites(campos);
    return;
  }
  // || significa «o» y ! niega. Ocho campos: etiqueta, nodo, secuencia,
  // cuatro mediciones y RSSI. Las líneas de diagnóstico quedan fuera.
  if (campos.length != 8 || !campos[0].equals("SERENO")) return;
  // Las expresiones regulares admiten nodo 1/2 y secuencia de 1 a 10 dígitos.
  if (!campos[1].matches("[12]") || !campos[2].matches("[0-9]{1,10}")) return;
  try {
    float tDht = leerNumero(campos[3]);
    float humedad = leerNumero(campos[4]);
    float tBmp = leerNumero(campos[5]);
    float presion = leerNumero(campos[6]);
    int rssi = Integer.parseInt(campos[7]);
    // Una medición válida también prueba que la base está activa.
    verBase(ahora, false);
    if (estado != MONITOREO) return;
    // Los ID comienzan en 1, pero los índices del arreglo comienzan en 0.
    Nodo nodo = nodos[Integer.parseInt(campos[1]) - 1];
    guardarPaquete(nodo, ahora, campos[2], tDht, humedad, tBmp, presion, rssi);
  } catch (NumberFormatException error) {
    println("Se omitió una línea de medición inválida.");
  }
}

// Float.NaN es el «dato faltante» de Java; no debe activar alarmas ni sumar frío.
// Convierte nan o un campo vacío en dato faltante. Rechaza el resto de errores.
float leerNumero(String texto) {
  if (texto.isEmpty() || texto.equalsIgnoreCase("nan") || texto.equalsIgnoreCase("-nan")) return Float.NaN;
  float valor = Float.parseFloat(texto);
  if (!numeroValido(valor)) throw new NumberFormatException();
  return valor;
}

// Actualiza el estado de la base y vuelve a sincronizar límites al reconectar.
void verBase(long ahora, boolean reinicio) {
  ultimaActividadBase = ahora;
  // Si ya se monitorea y no hubo BASE_LISTA, solo se renueva la marca de actividad.
  if (estado == MONITOREO && !reinicio) return;

  if (reinicio && estado == MONITOREO) {
    evento(0, "BASE_REINICIADA", puertoActivo);
    for (Nodo nodo : nodos) nodo.enLinea = false;
  } else {
    evento(0, baseVista ? "BASE_RECONEXION" : "BASE_CONECTADA", puertoActivo);
  }
  baseVista = true;
  estado = MONITOREO;
  // Tras conectar o reiniciar, se reenvían a la base los límites ya aceptados.
  enviarLimites(limiteFrio, limiteHumedad);
}

// Controla plazos. La pérdida de USB y la pérdida LoRa se registran por separado.
void vigilarEnlaces(long ahora) {
  if (estado == DESCONEXION) return;
  // ESTADO_BASE llega cada 3 s; 15 s sin mensaje señalan pérdida de la base.
  if (ahora - ultimaActividadBase >= Nodo.VIGENCIA_MS) {
    if (estado == MONITOREO) {
      evento(0, "BASE_DESCONECTADA", "Sin mensajes de la base durante 15 s");
      estado = ESPERA;
      esperandoConfig = false;
      for (Nodo nodo : nodos) nodo.enLinea = false;
    }
    aviso = "Sin mensajes de la base en " + puertoActivo;
    return;
  }
  if (estado != MONITOREO) return;
  // Se abandonan los límites pendientes tras 4 s sin CONFIG_OK.
  if (esperandoConfig && ahora - momentoConfig >= PLAZO_CONFIG) {
    esperandoConfig = false;
    aviso = "La base no confirmó los límites. Podés volver a enviarlos.";
  }
  for (Nodo nodo : nodos) {
    // Cada nodo tiene su propio plazo de 15 s para distinguir pérdida LoRa.
    if (nodo.enLinea && ahora - nodo.ultimaLectura >= Nodo.VIGENCIA_MS) {
      nodo.enLinea = false;
      evento(nodo.id, "NODO_SIN_SENAL", "No se recibió durante 15 s");
    }
  }
}

// Envía los límites. La PC espera la confirmación antes de usarlos.
void enviarLimites(float temperatura, float humedad) {
  if (puerto == null || estado != MONITOREO) return;
  try {
    // write es de Processing. El salto de línea permite a la base cerrar el comando.
    puerto.write("CONFIG," + decimal(temperatura, 1) + "," + decimal(humedad, 1) + "\n");
    frioPendiente = temperatura;
    humedadPendiente = humedad;
    esperandoConfig = true;
    momentoConfig = relojMs();
    aviso = "Esperando confirmación de la base.";
  } catch (RuntimeException error) {
    cerrarPuerto("No se pudo comunicar con la base.");
  }
}

// Acepta la respuesta solo si corresponde a los valores solicitados.
void confirmarLimites(String[] campos) {
  if (!esperandoConfig || estado != MONITOREO) return;
  try {
    float temperatura = leerNumero(campos[1]);
    float humedad = leerNumero(campos[2]);
    // Solo se aplica una confirmación que coincide con la solicitud pendiente.
    if (temperatura != frioPendiente || humedad != humedadPendiente) return;
    esperandoConfig = false;
    if (temperatura != limiteFrio || humedad != limiteHumedad) {
      evento(0, "CAMBIO_UMBRAL", temperatura + " °C / " + humedad + " %");
    }
    if (temperatura != limiteFrio) {
      // El tramo térmico en curso cruza dos límites; cada nodo lo omitirá.
      for (Nodo nodo : nodos) nodo.cambioUmbral();
    }
    limiteFrio = temperatura;
    limiteHumedad = humedad;
    aviso = "Límites confirmados por la base.";
    for (Nodo nodo : nodos) {
      if (nodo.enLinea) actualizarAlarmas(nodo);
    }
  } catch (NumberFormatException error) {
    aviso = "La confirmación de la base no tenía valores válidos.";
  }
}

// Guarda la muestra del nodo correcto y registra solo cambios de estado.
void guardarPaquete(Nodo nodo, long ahora, String secuencia,
                   float tDht, float humedad, float tBmp, float presion, int rssi) {
  if (!nodo.enLinea) {
    evento(nodo.id, nodo.recibido ? "NODO_RECUPERADO" : "NODO_EN_LINEA", "Se recibió una medición");
  }
  // recibir integra primero el tramo anterior y luego reemplaza la lectura.
  nodo.recibir(ahora, tDht, humedad, tBmp, presion, rssi, limiteFrio);
  // La fecha viene de Java en la PC. str es de Processing. El último campo
  // guarda el frío acumulado total del nodo, no solo el aporte de este paquete.
  String[] fila = { ArchivosCsv.fechaHora(), str(nodo.id), secuencia, campoCsv(tDht),
    campoCsv(humedad), campoCsv(tBmp), campoCsv(presion), str(rssi), decimal(nodo.exposicion, 3) };
  try {
    archivos.guardarMedicion(fila);
  } catch (IOException error) {
    errorArchivos = "No se pudo guardar una medición: " + error.getMessage();
  }
  actualizarAlarmas(nodo);
}

// Compara las lecturas actuales con los límites y deja constancia de las transiciones.
void actualizarAlarmas(Nodo nodo) {
  boolean sonar = false;
  if (numeroValido(nodo.temperaturaBmp)) {
    // La alarma de frío se libera con medio grado de margen para evitar oscilaciones.
    boolean frio = nodo.frio ? nodo.temperaturaBmp < limiteFrio + 0.5f
                            : nodo.temperaturaBmp <= limiteFrio;
    if (frio != nodo.frio) {
      nodo.frio = frio;
      evento(nodo.id, frio ? "HELADA_ACTIVA" : "HELADA_RECUPERADA",
             decimal(nodo.temperaturaBmp, 1) + " °C");
      // Solo una transición a alarma activa pide sonido; la recuperación no.
      sonar = frio;
    }
  }
  if (numeroValido(nodo.humedad)) {
    boolean alta = nodo.humedad >= limiteHumedad;
    if (alta != nodo.humedadAlta) {
      nodo.humedadAlta = alta;
      evento(nodo.id, alta ? "HUMEDAD_ACTIVA" : "HUMEDAD_RECUPERADA",
             decimal(nodo.humedad, 1) + " %");
      // Si una de las dos alarmas acaba de activarse, se pide un único pitido.
      sonar = sonar || alta;
    }
  }
  if (sonar) iniciarPitido();
}

// Fija el punto decimal porque la coma separa campos del CSV y del puerto serie.
String decimal(double valor, int decimales) {
  // String.format y Locale son Java; ROOT fuerza punto decimal en cualquier PC.
  return String.format(Locale.ROOT, "%." + decimales + "f", valor);
}

// En los archivos, una celda vacía indica que el sensor no informó.
String campoCsv(float valor) {
  return numeroValido(valor) ? decimal(valor, 1) : "";
}
