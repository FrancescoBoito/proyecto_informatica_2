// volatile es Java: permite que el hilo de audio y el de dibujo vean cambios
// en estas banderas. Son booleanos, es decir, verdadero o falso.
volatile boolean sonando, errorAudio;

// Inicia el pitido sin detener la lectura del USB ni el dibujo del panel.
void iniciarPitido() {
  // El audio corre aparte para que el panel siga leyendo el USB y dibujando.
  if (!sonando) {
    sonando = true;
    errorAudio = false;
    // thread es de Processing y ejecuta emitirPitido en otro hilo.
    thread("emitirPitido");
  }
}

// Genera y reproduce un tono corto usando Java Sound.
public void emitirPitido() {
  // Java Sound genera un tono de 880 Hz y 450 ms con la salida de la PC.
  // Cada muestra PCM ocupa dos bytes. La envolvente suaviza los bordes.
  int frecuenciaMuestreo = 44100;
  int muestras = frecuenciaMuestreo * 450 / 1000;
  // Arreglo Java de bytes: dos por cada muestra de audio de 16 bits.
  byte[] tono = new byte[2 * muestras];
  for (int i = 0; i < muestras; i++) {
    // (double) convierte el índice a decimal para calcular el instante de la muestra.
    double t = (double)i / frecuenciaMuestreo;
    double envolvente = Math.min(1.0, Math.min(i, muestras - 1 - i) / 400.0);
    int muestra = (int)(12000 * envolvente * Math.sin(2 * Math.PI * 880 * t));
    tono[2 * i] = (byte)muestra;              // Byte menos significativo.
    tono[2 * i + 1] = (byte)(muestra >> 8);   // >> desplaza ocho bits; byte más significativo.
  }

  // Java Sound: 44,1 kHz, 16 bits, un canal, PCM con signo y orden de bytes menor primero.
  AudioFormat formato = new AudioFormat(frecuenciaMuestreo, 16, 1, true, false);
  // try con recurso cierra la línea de sonido al salir, aun si hay un error.
  try (SourceDataLine salida = AudioSystem.getSourceDataLine(formato)) {
    salida.open(formato);
    salida.start();
    salida.write(tono, 0, tono.length);
    salida.drain();
  } catch (LineUnavailableException | IllegalArgumentException error) {
    errorAudio = true;
  // finally se ejecuta tanto si sonó como si falló: habilita un pitido futuro.
  } finally {
    sonando = false;
  }
}

