// Las bibliotecas aportan funciones de Arduino, buses SPI/I²C, LoRa, LCD y conversión de texto.
// Las comillas buscan Medicion.h en este proyecto; los <...> corresponden a bibliotecas.
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <stdlib.h>
#include <string.h> 
#include "Medicion.h"

// constexpr fija pines y plazos al compilar; uint32_t es un entero sin signo de 32 bits.
// Los plazos se expresan en milisegundos para compararlos con millis().

// El RA-02 usa SPI. El LCD 1602 usa I²C y conserva la dirección 0x27.
constexpr int LORA_SCK = 18, LORA_MISO = 19, LORA_MOSI = 23;
constexpr int LORA_CS = 5, LORA_RST = 27, LORA_DIO0 = 33;
constexpr int LCD_SDA = 21, LCD_SCL = 22;
// Los dos LEDs indican exclusivamente el resultado de las comunicaciones.
constexpr int LED_VERDE = 26, LED_ROJO = 25;
// Pulsador conectado a GND. Usamos la resistencia interna de pull-up.
constexpr int BOTON = 14;
constexpr uint32_t VIGENCIA_MS = 15000;
constexpr uint32_t ANTIRREBOTE_MS = 40;
constexpr uint32_t PULSACION_LARGA_MS = 1200;
constexpr uint32_t REFRESCO_LCD_MS = 500;
constexpr float ALERTA_HELADA_C = 2.0f;
constexpr uint32_t INTERVALO_ESTADO_MS = 3000;

// Estos valores identifican el registro y los bits de recepción del chip SX1278.
// El CRC detecta paquetes recibidos con errores.
constexpr uint8_t REG_IRQ_FLAGS = 0x12;
constexpr uint8_t IRQ_RX_DONE = 0x40;
constexpr uint8_t IRQ_CRC_ERROR = 0x20;

// La base intenta iniciar un turno cada 2,5 s, alternando nodos; los tiempos
// de transmisión y espera pueden retrasar ese inicio si el turno sigue abierto.
// Si un nodo no responde, se informa en el LCD y el otro conserva su turno.
constexpr uint32_t INTERVALO_TURNOS_MS = 2500;
constexpr uint32_t ESPERA_DATOS_MS = 1200;
constexpr uint32_t ESPERA_OK_MS = 800;
constexpr uint32_t PAUSA_RADIO_MS = 30;

constexpr uint8_t CANTIDAD_NODOS = 2;
// Se crea un objeto LCD con dirección I²C 0x27, 16 columnas y 2 filas.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Hay una sola clase Medicion y un objeto por nodo.
// El índice 0 guarda el Nodo 1 y el índice 1 guarda el Nodo 2.
Medicion mediciones[CANTIDAD_NODOS];
// Cada arreglo tiene dos posiciones; {} inicia sus elementos a cero o false.
// El índice 0 representa al nodo 1, por eso luego se usa ID - 1.
uint32_t ultimaLectura[CANTIDAD_NODOS] = {};
bool recibioDatos[CANTIDAD_NODOS] = {};

// Se guarda el resultado del último turno de cada nodo.
// Un turno es correcto al recibir el OK final (POLL, DATOS, ACK, OK).
bool turnoEvaluado[CANTIDAD_NODOS] = {};
bool turnoCorrecto[CANTIDAD_NODOS] = {};
uint32_t errores[CANTIDAD_NODOS] = {};

// Extremos de temperatura BMP180 de cada nodo, desde que encendió la base.
// Arreglo de 2 elementos: índice 0 = Nodo 1, índice 1 = Nodo 2. Inicializados a NAN.
float temperaturaMin[CANTIDAD_NODOS] = {NAN, NAN};
float temperaturaMax[CANTIDAD_NODOS] = {NAN, NAN};

// Variables de navegación para el LCD de 16 columnas y 2 filas.
// Estas variables globales conservan su valor entre vueltas de loop().
uint8_t pantallaActual = 0; // 0=ambos, 1=nodo1, 2=nodo2, 3=diagnóstico
uint32_t ultimoRefrescoLCD = 0;

// El antirrebote evita contar varios clics por una sola pulsación.
bool lecturaAnteriorBoton = HIGH;
bool estadoEstableBoton = HIGH;
uint32_t ultimoCambioBoton = 0;
uint32_t inicioPulsacion = 0;
bool pulsacionLargaProcesada = false;

// Los límites iniciales coinciden con Processing. El comando CONFIG los actualiza.
float limiteFrio = 0.0f;
float limiteHumedad = 90.0f;
uint32_t ultimoEstado = 0;

// El comando USB se acumula carácter por carácter en este arreglo de 48 posiciones.
// size_t es un entero sin signo apropiado para tamaños; comandoLargo marca desborde.
char comando[48];
size_t largoComando = 0;
bool comandoLargo = false;

// El estado indica qué puede aceptar la base en el turno actual.
// enum define tres nombres para estados. Solo uno puede estar activo en etapa.
enum Etapa { ENTRE_TURNOS, ESPERANDO_DATOS, ESPERANDO_OK };
Etapa etapa = ENTRE_TURNOS;

uint8_t proximoNodo = 1;
uint8_t nodoActual = 1;
uint32_t secuencia = 0;      // La base asigna un número nuevo a cada consulta.
uint32_t inicioTurno = 0;   // Se usa para mantener el intervalo entre consultas.
uint32_t inicioEspera = 0; // Se cuenta desde que terminó de transmitir la base.

// Escribe los 16 caracteres de una fila. Los espacios borran el texto anterior.
// const String & recibe el texto original sin copiarlo ni permitir modificarlo.
void filaLCD(uint8_t fila, const String &texto) {
    // setCursor recibe columna y fila; esta función siempre escribe desde la 0.
    lcd.setCursor(0, fila);
    for (uint8_t columna = 0; columna < 16; columna++) {
        // Se imprimen exactamente 16 posiciones para borrar restos de la vista previa.
        lcd.print(columna < texto.length() ? texto[columna] : ' ');
        // ?: imprime el carácter de texto si existe; si no, imprime un espacio.
    }
}

// Marca el resultado de la última consulta del nodo actual.
// Este registro NO modifica el protocolo de radio ni el envío a Processing.
void terminarTurno(bool correcto) {
    // Un ID 1 o 2 se convierte al índice 0 o 1 del arreglo.
    uint8_t indice = nodoActual - 1;
    turnoEvaluado[indice] = true;
    turnoCorrecto[indice] = correcto;
    // ! niega el booleano; el error se cuenta una vez al cerrar un turno fallido.
    if (!correcto) errores[indice]++;
    // Tras un cierre exitoso o fallido se habilita el próximo turno alternado.
    etapa = ENTRE_TURNOS;
}

// Los LED son excluyentes. Antes de consultar ambos nodos quedan apagados.
// Verde: ambos turnos OK y datos recientes. Rojo: falla alguno de los dos.
void actualizarLeds(uint32_t ahora) {
    // && exige ambos resultados conocidos. Antes de eso los dos LEDs quedan apagados.
    bool conocidos = turnoEvaluado[0] && turnoEvaluado[1];
    bool todoBien = conocidos;
    // El bucle revisa ambos nodos; una sola falla vuelve falso todoBien.
    for (int i = 0; i < CANTIDAD_NODOS; i++) {
        if (!turnoCorrecto[i] || !recibioDatos[i] ||
            ahora - ultimaLectura[i] >= VIGENCIA_MS) todoBien = false;
    }
    // digitalWrite controla el nivel eléctrico del pin; ?: elige encendido/apagado.
    digitalWrite(LED_VERDE, conocidos && todoBien ? HIGH : LOW);
    digitalWrite(LED_ROJO, conocidos && !todoBien ? HIGH : LOW);
}

// Lee el pulsador sin delay(): el programa sigue recibiendo mensajes LoRa.
void leerBoton(uint32_t ahora) {
    // INPUT_PULLUP hace que HIGH sea suelto y LOW sea presionado.
    bool lectura = digitalRead(BOTON);
    // Cada cambio reinicia la cuenta de estabilidad, incluso un rebote mecánico.
    if (lectura != lecturaAnteriorBoton) ultimoCambioBoton = ahora;
    lecturaAnteriorBoton = lectura;

    // Se acepta el cambio solo si se mantuvo estable 40 ms.
    if (ahora - ultimoCambioBoton >= ANTIRREBOTE_MS &&
        lectura != estadoEstableBoton) {
        estadoEstableBoton = lectura;
        // Al aceptar una bajada comienza una pulsación; al aceptar una subida
        // se decide si fue corta y todavía no se había procesado como larga.
        if (estadoEstableBoton == LOW) {
            inicioPulsacion = ahora;
            pulsacionLargaProcesada = false;
        } else if (!pulsacionLargaProcesada) {
            // Pulsación corta: pasa a la siguiente pantalla (0,1,2,3,0...).
            // El resto (%) hace que después de la pantalla 3 vuelva a la 0.
            pantallaActual = (pantallaActual + 1) % 4;
            // Adelanta el próximo refresco para que la pantalla cambie enseguida.
            ultimoRefrescoLCD = ahora - REFRESCO_LCD_MS;
        }
    }

    // La pulsación larga vuelve a inicio y no suma una corta al soltar.
    if (estadoEstableBoton == LOW && !pulsacionLargaProcesada &&
        ahora - inicioPulsacion >= PULSACION_LARGA_MS) {
        pantallaActual = 0;
        // Esta bandera impide que soltar el botón también cuente como clic corto.
        pulsacionLargaProcesada = true;
        ultimoRefrescoLCD = ahora - REFRESCO_LCD_MS;
    }
}

// Solo se muestran lecturas de menos de 15 segundos.
// Evita anunciar una helada usando datos viejos de un nodo desconectado.
bool datoReciente(int i, uint32_t ahora) {
    // Se exige una lectura recibida y menos de 15 s desde su llegada.
    return recibioDatos[i] && ahora - ultimaLectura[i] < VIGENCIA_MS;
}

// Convierte una cifra en texto; un sensor que devolvió nan aparece como --.
String cifra(float valor, int decimales = 1) {
    // El parámetro = 1 se usa cuando el llamador no indica otra precisión.
    return isfinite(valor) ? String(valor, decimales) : String("--");
}

// Dibuja una de cuatro pantallas. filaLCD completa con espacios hasta 16.
void mostrarPantalla(uint32_t ahora) {
    // String es una cadena de Arduino. Los booleanos señalan datos recientes.
    String arriba, abajo;
    bool ok1 = datoReciente(0, ahora);
    bool ok2 = datoReciente(1, ahora);

    if (pantallaActual == 0) {
        // Pantalla 0: temperaturas BMP180 de ambos nodos.
        arriba = "N1:" + String(ok1 ? cifra(mediciones[0].getTemperaturaBmp()) : "--") +
                 " N2:" + String(ok2 ? cifra(mediciones[1].getTemperaturaBmp()) : "--");
        // El aviso local usa 2 °C fijos, independientes del límite enviado por la PC.
        // isfinite impide comparar un NAN procedente de un sensor ausente.
        bool helada1 = ok1 && isfinite(mediciones[0].getTemperaturaBmp()) &&
                       mediciones[0].getTemperaturaBmp() <= ALERTA_HELADA_C;
        bool helada2 = ok2 && isfinite(mediciones[1].getTemperaturaBmp()) &&
                       mediciones[1].getTemperaturaBmp() <= ALERTA_HELADA_C;
        if (helada1 || helada2) abajo = "ALERTA HELADA";
        // La condición de helada tiene prioridad sobre los mensajes de enlace.
        else if (!turnoEvaluado[0] || !turnoEvaluado[1]) abajo = "Esperando nodos";
        else if (turnoCorrecto[0] && turnoCorrecto[1] && ok1 && ok2) abajo = "LoRa OK 2/2";
        else abajo = "FALLA COMUNIC.";
    } else if (pantallaActual == 1 || pantallaActual == 2) {
        // Pantallas 1 y 2: DHT, humedad, BMP180 y presión del nodo elegido.
        int i = pantallaActual - 1;
        arriba = "Nodo " + String(i + 1);
        if (!datoReciente(i, ahora)) {
            // Se distingue un nodo que nunca respondió de una lectura vencida.
            abajo = recibioDatos[i] ? "Dato desactual." : "Sin datos";
        } else {
            // Dos vistas alternadas cada 4 segundos para no perder ninguna medida.
            // El cociente cambia cada 4 s y el resto alterna entre dos vistas.
            if ((ahora / 4000) % 2 == 0) {
                arriba = "N" + String(i + 1) + " D:" + cifra(mediciones[i].getTemperaturaDht()) + "C";
                abajo = "H:" + cifra(mediciones[i].getHumedad(), 0) + "%";
            } else {
                arriba = "N" + String(i + 1) + " B:" + cifra(mediciones[i].getTemperaturaBmp()) + "C";
                abajo = "P:" + cifra(mediciones[i].getPresion(), 1) + " hPa";
            }
        }
    } else {
        // Pantalla 3: alterna estado y errores con RSSI y un extremo térmico.
        if ((ahora / 4000) % 2 == 0) {
            // Los ?: anidados muestran -- sin turno, OK con cierre o ERR al fallar.
            arriba = "N1:" + String(turnoEvaluado[0] ? (turnoCorrecto[0] ? "OK" : "ERR") : "--") +
                     " N2:" + String(turnoEvaluado[1] ? (turnoCorrecto[1] ? "OK" : "ERR") : "--");
            abajo = "Errores:" + String(errores[0]) + "/" + String(errores[1]);
        } else {
            arriba = "RSSI " + String(ok1 ? String(mediciones[0].getRssi()) : "--") +
                     "/" + String(ok2 ? String(mediciones[1].getRssi()) : "--");
            abajo = "Min:" + cifra(temperaturaMin[0], 0) + "/" + cifra(temperaturaMin[1], 0);
            // En el ciclo de 16 s, se ve Min en un bloque y Max en el siguiente.
            if ((ahora / 4000) % 4 == 3)
                abajo = "Max:" + cifra(temperaturaMax[0], 0) + "/" + cifra(temperaturaMax[1], 0);
        }
    }
    filaLCD(0, arriba);
    filaLCD(1, abajo);
}

// Envía una trama completa y vuelve a escuchar por LoRa.
bool transmitir(const String &mensaje) {
    // La radio deja recepción, transmite y vuelve a escuchar; bool informa si salió.
    LoRa.idle();
    // Si ni siquiera se pudo abrir el paquete, se vuelve a recepción y se falla.
    if (!LoRa.beginPacket()) {
        LoRa.receive();
        return false;
    }
    LoRa.print(mensaje);
    bool enviado = LoRa.endPacket() == 1;
    LoRa.receive();
    return enviado;
}

// Solo se consulta a un nodo por vez. El próximo turno queda asignado al otro.
void iniciarTurno() {
    // El operador ?: elige volver al nodo 1 después del 2.
    nodoActual = proximoNodo;
    // proximoNodo queda listo antes de transmitir: un fallo no cancela el turno del otro.
    proximoNodo = nodoActual == CANTIDAD_NODOS ? 1 : nodoActual + 1;
    // Cada consulta lleva un número nuevo para descartar respuestas de otros turnos.
    ++secuencia;
    inicioTurno = millis();

    // Por ejemplo, BASE|POLL|1|42 consulta al Nodo 1 con la secuencia 42.
    String consulta = String("BASE|POLL|") + String(nodoActual) + "|" + String(secuencia);
    // Un fallo al emitir POLL se registra y termina este turno sin esperar DATOS.
    if (!transmitir(consulta)) {
        Serial.println("Error LoRa TX.");
        terminarTurno(false);
        return;
    }

    // La espera de DATOS empieza después de transmitir POLL.
    inicioEspera = millis();
    etapa = ESPERANDO_DATOS;
    Serial.print("TX ");
    Serial.println(consulta.c_str());
    // El LCD muestra la pantalla elegida incluso durante las consultas.
}

// Reutiliza el formateo de Medicion y agrega el ID para que Processing pueda
// mostrar las lecturas y el RSSI en el panel del nodo que corresponde.
void publicarMedicion(const Medicion &medicion) {
    // La referencia apunta al objeto existente. const evita cambiarlo al publicarlo.
    char linea[128];
    String numero = String(secuencia);
    // Si el texto excediera el arreglo, no se envía una línea incompleta.
    if (medicion.prepararLinea(numero.c_str(), linea, sizeof(linea))) {
        // Medicion devuelve SERENO,secuencia,... y conservamos todo desde
        // la secuencia en adelante. sizeof incluye el carácter nulo final.
        Serial.print("SERENO,");
        Serial.print(nodoActual);
        Serial.print(',');
        // Avanzar el puntero salta el prefijo que Medicion ya escribió: queda
        // la secuencia y los cuatro valores para formar SERENO,nodo,secuencia,...
        Serial.println(linea + sizeof("SERENO,") - 1);
    }
}

// Recibe una respuesta del nodo consultado. El RSSI se mide en el RA-02
// de la base para este paquete, antes de transmitir el ACK.
void procesarRespuesta(const String &mensaje, int rssi) {
    String identificador = String("NODO") + String(nodoActual);
    String numero = String(secuencia);

    if (etapa == ESPERANDO_DATOS) {
        // Solo se acepta la respuesta que coincide con el ID y la secuencia
        // del turno. La barra final separa la secuencia de las mediciones.
        String prefijo = identificador + "|DATOS|" + numero + "|";
        // Una respuesta de otro nodo, secuencia o tipo se ignora y sigue el plazo.
        if (!mensaje.startsWith(prefijo)) {
            Serial.print("Datos esperados con prefijo ");
            Serial.println(prefijo.c_str());
            return;
        }

        // La referencia permite actualizar el objeto que ya existe en el arreglo.
        // & declara una referencia al objeto del arreglo, sin crear otra medición.
        Medicion &medicion = mediciones[nodoActual - 1];
        // c_str da un puntero al texto C; se avanza hasta después del prefijo.
        const char *datos = mensaje.c_str() + prefijo.length();
        // cargar verifica los cuatro campos antes de sustituir la lectura previa.
        bool medicionValida = medicion.cargar(datos, rssi);

        // La pausa le da tiempo al nodo para pasar de transmisión a recepción.
        // El ACK lleva ambos datos para evitar confirmaciones de otro turno.
        delay(PAUSA_RADIO_MS);
        // El ACK confirma la recepción para este ID y secuencia incluso si
        // cargar rechazó las cifras; en ese caso no se publica una medición.
        String ack = String("BASE|ACK|") + String(nodoActual) + "|" + numero;
        bool ackEnviado = transmitir(ack);
        // El plazo de OK comienza después de transmitir el ACK, no al llegar DATOS.
        inicioEspera = millis();
        // Solo se espera OK cuando salió el ACK. Si falló, termina el turno.
        etapa = ackEnviado ? ESPERANDO_OK : ENTRE_TURNOS;
        if (!ackEnviado) terminarTurno(false);

        Serial.print(ackEnviado ? "TX " : "Error TX ");
        Serial.println(ack.c_str());
        if (medicionValida) {
            // Esta marca mide la antigüedad de la lectura en LCD y LEDs.
            ultimaLectura[nodoActual - 1] = millis();
            recibioDatos[nodoActual - 1] = true;
            // USB publica la medición antes de recibir el OK final; ese OK
            // determina el resultado del turno para LEDs y diagnóstico.
            publicarMedicion(medicion);
            // Mínimo y máximo de BMP180; solo si hay una lectura válida.
            float temp = medicion.getTemperaturaBmp();
            int i = nodoActual - 1;
            // Los extremos no cambian cuando BMP180 informó NAN.
            if (isfinite(temp)) {
                if (!isfinite(temperaturaMin[i]) || temp < temperaturaMin[i])
                    temperaturaMin[i] = temp;
                if (!isfinite(temperaturaMax[i]) || temp > temperaturaMax[i])
                    temperaturaMax[i] = temp;
            }
        } else {
            Serial.println("Medicion invalida. Se confirma solamente la recepcion.");
        }
        // El diagnóstico se consulta desde la cuarta pantalla.
    // En el segundo estado solo vale un OK con ID y secuencia exactos.
    } else if (etapa == ESPERANDO_OK && mensaje == identificador + "|OK|" + numero) {
        // Este OK confirma que el nodo recibió el ACK de la base.
        terminarTurno(true);
        Serial.println("Ida y vuelta OK.");
    }
}

// Aplica CONFIG,temperatura,humedad y devuelve los valores aceptados por USB.
void aplicarComando(const char *linea) {
    // Solo se interpreta el prefijo CONFIG,; otros diagnósticos se ignoran.
    const char prefijo[] = "CONFIG,";
    // strncmp compara solo los caracteres de CONFIG,; otras líneas se ignoran.
    if (strncmp(linea, prefijo, sizeof(prefijo) - 1) != 0) return;

    // sizeof incluye el '\0' final; -1 sitúa el puntero tras la coma.
    const char *inicio = linea + sizeof(prefijo) - 1;
    char *fin;
    // strtof lee un float y escribe en fin dónde terminó; & pasa su dirección.
    float frio = strtof(inicio, &fin);
    // Se exige un número, coma exacta, valor finito y rango permitido.
    if (fin == inicio || *fin != ',' || !isfinite(frio) || frio < -40 || frio > 85) {
        Serial.println("CONFIG_ERROR");
        return;
    }

    // La coma validada separa el segundo valor. No se admite texto sobrante.
    inicio = fin + 1;
    float humedad = strtof(inicio, &fin);
    if (fin == inicio || *fin != '\0' || !isfinite(humedad) || humedad < 0 || humedad > 100) {
        Serial.println("CONFIG_ERROR");
        return;
    }

    // Solo después de validar ambos valores se cambian juntos los límites.
    limiteFrio = frio;
    limiteHumedad = humedad;
    Serial.printf("CONFIG_OK,%.1f,%.1f\n", limiteFrio, limiteHumedad);
}

// Acumula una línea sin bloquear la radio si el comando llega en partes.
void leerUSB() {
    // El tope de bytes deja tiempo para atender las tramas LoRa en cada vuelta.
    for (int i = 0; i < 48 && Serial.available() > 0; i++) {
        char dato = static_cast<char>(Serial.read());
        // Se ignora retorno de carro; el salto de línea cierra el comando.
        if (dato == '\r') continue;
        if (dato == '\n') {
            // Terminar con '\0' convierte el arreglo de char en cadena C.
            comando[largoComando] = '\0';
            if (!comandoLargo) aplicarComando(comando);
            largoComando = 0;
            comandoLargo = false;
        } else if (largoComando < sizeof(comando) - 1) {
            // Se guarda el carácter y luego aumenta el índice para el siguiente.
            comando[largoComando++] = dato;
        } else {
            comandoLargo = true;
        }
    }
}

// Inicia USB, LEDs, LCD y radio.
void setup() {
    // Processing recibe las tramas por USB a 115200 baudios.
    Serial.begin(115200);
    pinMode(BOTON, INPUT_PULLUP);
    pinMode(LED_VERDE, OUTPUT);
    pinMode(LED_ROJO, OUTPUT);
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(LED_ROJO, LOW);
    Wire.begin(LCD_SDA, LCD_SCL);
    lcd.init();
    lcd.backlight();
    filaLCD(0, "EL SERENO BASE");
    filaLCD(1, "Iniciando LoRa");

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    if (!LoRa.begin(433000000)) {
        filaLCD(0, "ERROR LoRa");
        filaLCD(1, "Revisar conexion");
        Serial.println("ERROR: no se detecta el RA-02.");
        // Sin radio, la base queda detenida y conserva el mensaje de error.
        while (true) delay(1000);
    }

    // Los tres radios deben usar la misma configuración para comunicarse.
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125000);
    LoRa.setCodingRate4(5);
    LoRa.setSyncWord(0x12);
    LoRa.setPreambleLength(8);
    LoRa.setTxPower(10);
    // La misma configuración del nodo permite entenderse por radio y detectar errores.
    LoRa.enableCrc();
    LoRa.receive();

    Serial.println("BASE_LISTA");
    mostrarPantalla(millis());

    // Deja habilitado el primer turno inmediatamente. La resta se hace con
    // enteros sin signo, de la misma forma que los cálculos de tiempo de loop().
    inicioTurno = millis() - INTERVALO_TURNOS_MS;
}

// Lee las banderas del RA-02 por el mismo bus SPI que usa LoRa.
// El bit 7 de la dirección queda en 0 porque la operación es de lectura.
uint8_t leerBanderasLoRa() {
    // SPI selecciona el RA-02 bajando CS; transfer envía un byte mientras lee otro.
    SPI.beginTransaction(SPISettings(8000000, MSBFIRST, SPI_MODE0));
    digitalWrite(LORA_CS, LOW);
    SPI.transfer(REG_IRQ_FLAGS);
    uint8_t banderas = SPI.transfer(0x00);
    digitalWrite(LORA_CS, HIGH);
    SPI.endTransaction();
    return banderas;
}

// Lee una trama completa si el RA-02 avisó que la recibió.
void recibirRadio() {
    // parsePacket() cambia la radio a recepción de un solo paquete si se
    // llama sin un paquete listo. Consulta primero la bandera para que
    // el RA-02 permanezca en recepción continua mientras espera al nodo.
    uint8_t banderas = leerBanderasLoRa();
    // & entre enteros revisa un bit; si RX_DONE no está activo, no hay trama nueva.
    if ((banderas & IRQ_RX_DONE) == 0) return;

    // La biblioteca limpia las banderas y coloca el puntero al inicio
    // del paquete. Se vuelve a activar la escucha al terminar de leerlo.
    // parsePacket descarta paquetes con CRC incorrecto y da el tamaño del válido.
    int tamano = LoRa.parsePacket();
    if (tamano <= 0) {
        LoRa.receive();
        if (banderas & IRQ_CRC_ERROR) Serial.println("RX con CRC incorrecto.");
        return;
    }

    String mensaje;
    mensaje.reserve(tamano);
    while (LoRa.available()) {
        mensaje += static_cast<char>(LoRa.read());
    }
    // RSSI mide la fuerza de esta recepción en la radio de la base, en dBm.
    int rssi = LoRa.packetRssi();
    LoRa.receive();
    Serial.print("RX ");
    Serial.println(mensaje.c_str());
    procesarRespuesta(mensaje, rssi);
}

// Avanza la máquina de estados y atiende la PC entre consultas.
void loop() {
    // Las esperas usan diferencias de millis() para seguir leyendo la radio
    // mientras se espera la respuesta y manejar el desborde del contador.
    // millis() cuenta milisegundos desde el encendido. Restar marcas evita
    // bloquear loop() y funciona al dar la vuelta el contador sin signo.
    uint32_t ahora = millis();
    // Vencida la espera de 1,2 s, el turno se marca fallido. Una respuesta
    // posterior de esa secuencia ya no se acepta para publicarla.
    if (etapa == ESPERANDO_DATOS && ahora - inicioEspera >= ESPERA_DATOS_MS) {
        terminarTurno(false);
        Serial.println("Fin de espera sin DATOS validos para este turno.");
    // La ausencia de OK durante 0,8 s marca fallo aunque DATOS ya se publicara.
    } else if (etapa == ESPERANDO_OK && ahora - inicioEspera >= ESPERA_OK_MS) {
        terminarTurno(false);
        Serial.println("Fin de espera sin OK del nodo.");
    }

    // Cada pasada atiende radio, USB, botón, LEDs y pantalla antes de otro POLL.
    recibirRadio();
    leerUSB();
    leerBoton(millis());
    actualizarLeds(millis());

    // Se refresca la pantalla sin bloquear la recepción LoRa.
    if (millis() - ultimoRefrescoLCD >= REFRESCO_LCD_MS) {
        ultimoRefrescoLCD = millis();
        mostrarPantalla(ultimoRefrescoLCD);
    }

    // La PC puede distinguir una base conectada de dos nodos fuera de alcance.
    if (millis() - ultimoEstado >= INTERVALO_ESTADO_MS) {
        ultimoEstado = millis();
        Serial.println("ESTADO_BASE");
    }

    // No se consulta al siguiente nodo mientras haya una respuesta pendiente.
    if (etapa == ENTRE_TURNOS && millis() - inicioTurno >= INTERVALO_TURNOS_MS) {
        iniciarTurno();
    }
}
