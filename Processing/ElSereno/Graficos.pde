// Gráficos hechos con funciones de dibujo de Processing; leen el buffer que
// ArchivosCsv cargó del CSV y que actualiza con cada medición nueva.
void dibujarGrafico(int x, int y, int ancho, int alto, String titulo,
                    String unidad, int columna, float umbral, boolean mostrarUmbral) {
  noStroke();
  fill(27, 44, 57);
  rect(x, y, ancho, alto, 12);
  fill(241, 248, 251);
  textSize(16);
  text(titulo, x + 16, y + 26);
  fill(147, 173, 188);
  textSize(11);
  textAlign(RIGHT);
  text(unidad, x + ancho - 16, y + 25);
  textAlign(LEFT);

  fill(COLOR_NODO_1);
  ellipse(x + 20, y + 49, 8, 8);
  fill(184, 207, 220);
  textSize(11);
  text("Nodo 1", x + 30, y + 53);
  fill(COLOR_NODO_2);
  ellipse(x + 104, y + 49, 8, 8);
  fill(184, 207, 220);
  text("Nodo 2", x + 114, y + 53);
  if (mostrarUmbral) {
    stroke(255, 139, 124, 185);
    strokeWeight(2);
    line(x + 207, y + 49, x + 222, y + 49);
    noStroke();
    fill(184, 207, 220);
    text("Umbral", x + 228, y + 53);
  }

  int cantidad = archivos.cantidadGrafico();
  // Arreglos temporales: una posición por fila del historial recuperado.
  int[] ids = new int[cantidad];
  float[] valores = new float[cantidad];
  // Los infinitos iniciales permiten hallar los extremos reales en el bucle.
  float minimo = Float.POSITIVE_INFINITY, maximo = Float.NEGATIVE_INFINITY;
  for (int i = 0; i < cantidad; i++) {
    String[] fila = archivos.filaGrafico(i);
    // ?: anidado convierte la columna «nodo» en 1, 2 o 0 si es inválida.
    ids[i] = fila[1].equals("1") ? 1 : fila[1].equals("2") ? 2 : 0;
    valores[i] = leerDatoGrafico(fila[columna]);
    if (ids[i] != 0 && Float.isFinite(valores[i])) {
      minimo = min(minimo, valores[i]);
      maximo = max(maximo, valores[i]);
    }
  }

  // Humedad siempre usa escala 0–100; temperatura y presión se ajustan a datos.
  if (columna == 4) {
    minimo = 0;
    maximo = 100;
  } else if (!Float.isFinite(minimo)) {
    minimo = columna == 5 ? -5 : 900;
    maximo = columna == 5 ? 35 : 1050;
  } else {
    if (mostrarUmbral) {
      minimo = min(minimo, umbral);
      maximo = max(maximo, umbral);
    }
    // max y min son de Processing: el margen evita que la curva toque los bordes.
    float margen = max(columna == 5 ? 2 : 1.5f, (maximo - minimo) * 0.18f);
    minimo -= margen;
    maximo += margen;
  }

  float izq = x + 54, der = x + ancho - 17;
  float arriba = y + 76, abajo = y + alto - 36;
  for (int k = 0; k <= 3; k++) {
    // lerp interpola entre dos posiciones; map transforma un dato a píxeles.
    float py = lerp(abajo, arriba, k / 3.0f);
    stroke(112, 140, 151, 70);
    strokeWeight(1);
    line(izq, py, der, py);
    noStroke();
    fill(146, 171, 184);
    textSize(10);
    text(decimal(lerp(minimo, maximo, k / 3.0f), columna == 6 ? 0 : 1),
         x + 7, py + 3);
  }
  if (mostrarUmbral && umbral >= minimo && umbral <= maximo) {
    float py = map(umbral, minimo, maximo, abajo, arriba);
    stroke(255, 139, 124, 185);
    strokeWeight(1.5f);
    for (float px = izq; px < der; px += 10) {
      line(px, py, min(px + 5, der), py);
    }
  }

  // Se dibuja una serie por nodo usando todas las posiciones del historial.
  for (int id = 1; id <= 2; id++) {
    int tono = id == 1 ? COLOR_NODO_1 : COLOR_NODO_2;
    boolean anterior = false;
    float pxAnterior = 0, pyAnterior = 0;
    for (int i = 0; i < cantidad; i++) {
      if (ids[i] != id) continue;
      // Un campo vacío del CSV se convirtió en NaN y corta la línea de ese nodo.
      if (!Float.isFinite(valores[i])) {
        anterior = false;
        continue;
      }
      // El eje horizontal representa orden de muestras, no tiempo exacto.
      float px = cantidad == 1 ? der : lerp(izq, der, i / (float)(cantidad - 1));
      float py = map(valores[i], minimo, maximo, abajo, arriba);
      if (anterior) {
        noStroke();
        fill(tono, 12);
        quad(pxAnterior, pyAnterior, px, py, px, abajo, pxAnterior, abajo);
        stroke(tono, 37);
        strokeWeight(10);
        line(pxAnterior, pyAnterior, px, py);
        stroke(tono);
        strokeWeight(2.4f);
        line(pxAnterior, pyAnterior, px, py);
      }
      pxAnterior = px;
      pyAnterior = py;
      anterior = true;
    }
    if (anterior) {
      noStroke();
      fill(tono, 48);
      ellipse(pxAnterior, pyAnterior, 17, 17);
      fill(tono);
      ellipse(pxAnterior, pyAnterior, 7, 7);
    }
  }

  noStroke();
  fill(148, 174, 186);
  textSize(10);
  if (cantidad == 0) {
    textSize(13);
    text("Esperando mediciones…", izq + 48, y + 166);
  } else {
    String primera = archivos.filaGrafico(0)[0];
    String ultima = archivos.filaGrafico(cantidad - 1)[0];
    text(primera.length() >= 16 ? primera.substring(11, 16) : "Inicio", izq, y + alto - 12);
    textAlign(RIGHT);
    text(ultima.length() >= 16 ? ultima.substring(11, 16) : "Ahora", der, y + alto - 12);
    textAlign(LEFT);
  }
}

// Convierte una celda CSV a float; un vacío o texto inválido deja un hueco.
float leerDatoGrafico(String texto) {
  try {
    return texto.isEmpty() ? Float.NaN : Float.parseFloat(texto);
  } catch (NumberFormatException error) {
    return Float.NaN;
  }
}
