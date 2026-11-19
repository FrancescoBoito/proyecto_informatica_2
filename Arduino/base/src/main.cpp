#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "Medicion.h"

// El RA-02 usa SPI. El LCD 1602 usa I²C y conserva la dirección 0x27.
constexpr int LORA_SCK = 18, LORA_MISO = 19, LORA_MOSI = 23;
constexpr int LORA_CS = 5, LORA_RST = 27, LORA_DIO0 = 33;
constexpr int LCD_SDA = 21, LCD_SCL = 22;
constexpr int LED_ENLACE = 25, LED_ALARMA = 26;
constexpr uint32_t VIGENCIA_MS = 15000;
constexpr uint32_t INTERVALO_ESTADO_MS = 3000;

// El registro del SX1278 indica si recibió una trama y si pasó el CRC.
constexpr uint8_t REG_IRQ_FLAGS = 0x12;
constexpr uint8_t IRQ_RX_DONE = 0x40;
constexpr uint8_t IRQ_CRC_ERROR = 0x20;

// La base inicia un turno cada 2,5 s. Cada nodo tiene un turno cada 5 s.
// Si un nodo no responde, se informa en el LCD y el otro conserva su turno.
constexpr uint32_t INTERVALO_TURNOS_MS = 2500;
constexpr uint32_t ESPERA_DATOS_MS = 1200;
constexpr uint32_t ESPERA_OK_MS = 800;
constexpr uint32_t PAUSA_RADIO_MS = 30;

constexpr uint8_t CANTIDAD_NODOS = 2;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Hay una sola clase Medicion y un objeto por nodo.
// El índice 0 guarda el Nodo 1 y el índice 1 guarda el Nodo 2.
Medicion mediciones[CANTIDAD_NODOS];
uint32_t ultimaLectura[CANTIDAD_NODOS] = {};
bool recibioDatos[CANTIDAD_NODOS] = {};

// Los límites iniciales coinciden con Processing. El comando CONFIG los actualiza.
float limiteFrio = 0.0f;
float limiteHumedad = 90.0f;
uint32_t ultimoEstado = 0;

char comando[48];
size_t largoComando = 0;
bool comandoLargo = false;

// El estado indica qué puede aceptar la base en el turno actual.
enum Etapa { ENTRE_TURNOS, ESPERANDO_DATOS, ESPERANDO_OK };
Etapa etapa = ENTRE_TURNOS;

uint8_t proximoNodo = 1;
uint8_t nodoActual = 1;
uint32_t secuencia = 0;      // La base asigna un número nuevo a cada consulta.
uint32_t inicioTurno = 0;   // Se usa para mantener el intervalo entre consultas.
uint32_t inicioEspera = 0; // Se cuenta desde que terminó de transmitir la base.

// Escribe los 16 caracteres de una fila. Los espacios borran el texto anterior.
void filaLCD(uint8_t fila, const String &texto) {
    lcd.setCursor(0, fila);
    for (uint8_t columna = 0; columna < 16; columna++) {
        lcd.print(columna < texto.length() ? texto[columna] : ' ');
    }
}

// Arma el título que se muestra en el LCD durante el turno.
String nombreNodoActual() {
    return String("Nodo ") + String(nodoActual);
}

// Envía una trama completa y vuelve a escuchar por LoRa.
bool transmitir(const String &mensaje) {
    LoRa.idle();
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
    nodoActual = proximoNodo;
    proximoNodo = nodoActual == CANTIDAD_NODOS ? 1 : nodoActual + 1;
    ++secuencia;
    inicioTurno = millis();

    // Por ejemplo, BASE|POLL|1|42 consulta al Nodo 1 con la secuencia 42.
    String consulta = String("BASE|POLL|") + String(nodoActual) + "|" + String(secuencia);
    if (!transmitir(consulta)) {
        filaLCD(0, nombreNodoActual());
        filaLCD(1, "Error LoRa TX");
        return;
    }

    inicioEspera = millis();
    etapa = ESPERANDO_DATOS;
    Serial.print("TX ");
    Serial.println(consulta.c_str());
    filaLCD(0, nombreNodoActual());
    filaLCD(1, "Consultando...");
}

// Reutiliza el formateo de Medicion y agrega el ID para que Processing pueda
// mostrar las lecturas y el RSSI en el panel del nodo que corresponde.
void publicarMedicion(const Medicion &medicion) {
    char linea[128];
    String numero = String(secuencia);
    if (medicion.prepararLinea(numero.c_str(), linea, sizeof(linea))) {
        // Medicion devuelve SERENO,secuencia,... y conservamos todo desde
        // la secuencia en adelante. sizeof incluye el carácter nulo final.
        Serial.print("SERENO,");
        Serial.print(nodoActual);
        Serial.print(',');
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
        if (!mensaje.startsWith(prefijo)) {
            Serial.print("Datos esperados con prefijo ");
            Serial.println(prefijo.c_str());
            return;
        }

        // La referencia permite actualizar el objeto que ya existe en el arreglo.
        Medicion &medicion = mediciones[nodoActual - 1];
        const char *datos = mensaje.c_str() + prefijo.length();
        bool medicionValida = medicion.cargar(datos, rssi);

        // La pausa le da tiempo al nodo para pasar de transmisión a recepción.
        // El ACK lleva ambos datos para evitar confirmaciones de otro turno.
        delay(PAUSA_RADIO_MS);
        // El ACK confirma el enlace para este ID y secuencia. La validez de
        // los sensores solo determina qué lecturas se publican por USB.
        String ack = String("BASE|ACK|") + String(nodoActual) + "|" + numero;
        bool ackEnviado = transmitir(ack);
        inicioEspera = millis();
        etapa = ackEnviado ? ESPERANDO_OK : ENTRE_TURNOS;

        Serial.print(ackEnviado ? "TX " : "Error TX ");
        Serial.println(ack.c_str());
        if (medicionValida) {
            ultimaLectura[nodoActual - 1] = millis();
            recibioDatos[nodoActual - 1] = true;
            publicarMedicion(medicion);
        } else {
            Serial.println("Medicion invalida. Se confirma solamente la recepcion.");
        }
        filaLCD(1, ackEnviado ? String("R:") + String(rssi) + " dBm" : String("Error ACK TX"));
    } else if (etapa == ESPERANDO_OK && mensaje == identificador + "|OK|" + numero) {
        // Este OK confirma que el nodo recibió el ACK de la base.
        etapa = ENTRE_TURNOS;
        filaLCD(1, "Ida y vuelta OK");
        Serial.println("Ida y vuelta OK.");
    }
}

// Aplica CONFIG,temperatura,humedad y devuelve los valores aceptados por USB.
void aplicarComando(const char *linea) {
    const char prefijo[] = "CONFIG,";
    if (strncmp(linea, prefijo, sizeof(prefijo) - 1) != 0) return;

    const char *inicio = linea + sizeof(prefijo) - 1;
    char *fin;
    float frio = strtof(inicio, &fin);
    if (fin == inicio || *fin != ',' || !isfinite(frio) || frio < -40 || frio > 85) {
        Serial.println("CONFIG_ERROR");
        return;
    }

    inicio = fin + 1;
    float humedad = strtof(inicio, &fin);
    if (fin == inicio || *fin != '\0' || !isfinite(humedad) || humedad < 0 || humedad > 100) {
        Serial.println("CONFIG_ERROR");
        return;
    }

    limiteFrio = frio;
    limiteHumedad = humedad;
    Serial.printf("CONFIG_OK,%.1f,%.1f\n", limiteFrio, limiteHumedad);
}

// Acumula una línea sin bloquear la radio si el comando llega en partes.
void leerUSB() {
    // El tope de bytes deja tiempo para atender las tramas LoRa en cada vuelta.
    for (int i = 0; i < 48 && Serial.available() > 0; i++) {
        char dato = static_cast<char>(Serial.read());
        if (dato == '\r') continue;
        if (dato == '\n') {
            comando[largoComando] = '\0';
            if (!comandoLargo) aplicarComando(comando);
            largoComando = 0;
            comandoLargo = false;
        } else if (largoComando < sizeof(comando) - 1) {
            comando[largoComando++] = dato;
        } else {
            comandoLargo = true;
        }
    }
}

// Los LEDs muestran si hay algún nodo en línea y si hay una alarma vigente.
void actualizarLeds(uint32_t ahora) {
    bool enlace = false;
    bool alarma = false;
    for (int i = 0; i < CANTIDAD_NODOS; i++) {
        if (recibioDatos[i] && ahora - ultimaLectura[i] < VIGENCIA_MS) {
            enlace = true;
            if (mediciones[i].hayAlarma(limiteFrio, limiteHumedad)) alarma = true;
        }
    }
    digitalWrite(LED_ENLACE, enlace ? HIGH : LOW);
    digitalWrite(LED_ALARMA, alarma ? HIGH : LOW);
}

// Inicia USB, LEDs, LCD y radio.
void setup() {
    // Processing recibe las tramas por USB a 115200 baudios.
    Serial.begin(115200);
    pinMode(LED_ENLACE, OUTPUT);
    pinMode(LED_ALARMA, OUTPUT);
    actualizarLeds(millis());
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
        while (true) delay(1000);
    }

    // Los tres radios deben usar la misma configuración para comunicarse.
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125000);
    LoRa.setCodingRate4(5);
    LoRa.setSyncWord(0x12);
    LoRa.setPreambleLength(8);
    LoRa.setTxPower(10);
    LoRa.enableCrc();
    LoRa.receive();

    filaLCD(0, "BASE LISTA");
    filaLCD(1, "Nodos 1 y 2");
    Serial.println("BASE_LISTA");

    // Deja habilitado el primer turno inmediatamente. La resta se hace con
    // enteros sin signo, de la misma forma que los cálculos de tiempo de loop().
    inicioTurno = millis() - INTERVALO_TURNOS_MS;
}

// Lee las banderas del RA-02 por el mismo bus SPI que usa LoRa.
// El bit 7 de la dirección queda en 0 porque la operación es de lectura.
uint8_t leerBanderasLoRa() {
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
    if ((banderas & IRQ_RX_DONE) == 0) return;

    // La biblioteca limpia las banderas y coloca el puntero al inicio
    // del paquete. Se vuelve a activar la escucha al terminar de leerlo.
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
    uint32_t ahora = millis();
    if (etapa == ESPERANDO_DATOS && ahora - inicioEspera >= ESPERA_DATOS_MS) {
        etapa = ENTRE_TURNOS;
        filaLCD(1, "Sin respuesta");
        Serial.println("Fin de espera sin DATOS validos para este turno.");
    } else if (etapa == ESPERANDO_OK && ahora - inicioEspera >= ESPERA_OK_MS) {
        etapa = ENTRE_TURNOS;
        filaLCD(1, "Sin OK del nodo");
        Serial.println("Fin de espera sin OK del nodo.");
    }

    recibirRadio();
    leerUSB();
    actualizarLeds(millis());

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
