#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include "Sensores.h"

#if ID_NODO != 1 && ID_NODO != 2
#error "Compilar para nodo1 o nodo2 desde PlatformIO."
#endif

// Pines del RA-02 en el ESP32-C3 SuperMini.
constexpr int PIN_SCK = 6, PIN_MISO = 5, PIN_MOSI = 7;
constexpr int PIN_CS = 10, PIN_RST = 0, PIN_DIO0 = 1;
// Los pines son iguales en ambos ESP32-C3. Solo cambia el tipo de DHT.
constexpr int PIN_DHT = 4, PIN_BMP_SDA = 3, PIN_BMP_SCL = 20;
constexpr int TIPO_DHT = ID_NODO == 1 ? DHT11 : DHT22;
Sensores sensores(PIN_DHT, TIPO_DHT);

constexpr uint32_t PAUSA_RADIO_MS = 30;
constexpr uint32_t ESPERA_ACK_MS = 1000;

// El ID se configura al elegir nodo1 o nodo2 en PlatformIO.
const String id = String(ID_NODO);
const String nombre = String("NODO") + id;
const String prefijoConsulta = String("BASE|POLL|") + id + "|";
const String prefijoAck = String("BASE|ACK|") + id + "|";

// Una secuencia pendiente vacía significa que no estamos esperando un ACK.
String secuenciaPendiente;
uint32_t inicioEspera = 0;

// La secuencia se conserva como texto para devolverla tal como llegó.
bool secuenciaValida(const String &texto) {
    if (texto.length() == 0 || texto.length() > 10) return false;
    for (unsigned int i = 0; i < texto.length(); i++) {
        if (texto[i] < '0' || texto[i] > '9') return false;
    }
    return true;
}

// Da tiempo a que la base termine de transmitir y pase a recepción.
// Después de transmitir, el nodo vuelve a escuchar el canal.
bool transmitir(const String &trama) {
    delay(PAUSA_RADIO_MS);
    LoRa.idle();
    if (!LoRa.beginPacket()) {
        LoRa.receive();
        return false;
    }
    LoRa.print(trama);
    bool enviado = LoRa.endPacket() == 1;
    LoRa.receive();
    return enviado;
}

// Interpreta las consultas y los ACK dirigidos a esta placa.
void procesarTrama(const String &trama) {
    if (trama.startsWith(prefijoConsulta)) {
        // Solo se entra acá cuando la consulta está dirigida a este nodo.
        String secuencia = trama.substring(prefijoConsulta.length());
        if (!secuenciaValida(secuencia)) return;

        // La base espera cuatro valores. snprintf escribe nan por cada lectura faltante.
        Lecturas datos = sensores.leer();
        char valores[96];
        snprintf(valores, sizeof(valores), "%.1f|%.1f|%.1f|%.1f",
                 datos.temperaturaDht, datos.humedad, datos.temperaturaBmp, datos.presionHpa);
        String respuesta = nombre + "|DATOS|" + secuencia + "|" + valores;
        secuenciaPendiente = "";
        if (transmitir(respuesta)) {
            secuenciaPendiente = secuencia;
            inicioEspera = millis();
            Serial.println("Respuesta enviada. Esperando ACK de la base.");
        } else {
            Serial.println("Error al enviar la respuesta.");
        }
    } else if (secuenciaPendiente.length() > 0 && trama == prefijoAck + secuenciaPendiente) {
        // Si coinciden ID y secuencia, el ACK corresponde a nuestra respuesta.
        // El OK final permite que la base indique «Ida y vuelta OK» en el LCD.
        String confirmacion = nombre + "|OK|" + secuenciaPendiente;
        secuenciaPendiente = "";
        bool okEnviado = transmitir(confirmacion);
        Serial.println(okEnviado ? "ACK recibido. OK enviado a la base."
                                 : "ACK recibido. Error al enviar OK.");
    }
}

// Inicializa sensores y radio al encender el nodo.
void setup() {
    Serial.begin(115200);
    sensores.iniciar(PIN_BMP_SDA, PIN_BMP_SCL);
    SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_CS);
    LoRa.setPins(PIN_CS, PIN_RST, PIN_DIO0);
    if (!LoRa.begin(433000000)) {
        // Repetir el aviso permite leerlo si el monitor serie se abre más tarde.
        while (true) {
            Serial.println("Error: RA-02 no detectado.");
            delay(1500);
        }
    }

    // Estos parámetros coinciden con los del firmware de la base para dos nodos.
    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125000);
    LoRa.setCodingRate4(5);
    LoRa.setSyncWord(0x12);
    LoRa.setPreambleLength(8);
    LoRa.setTxPower(10);
    LoRa.enableCrc();
    LoRa.receive();
    Serial.print(nombre);
    Serial.println(" listo. Esperando consultas de la base.");
}

// Escucha LoRa y cancela una espera si vence el plazo del ACK.
void loop() {
    // Si el ACK no llega a tiempo, se descarta la espera y se sigue escuchando.
    if (secuenciaPendiente.length() > 0 && millis() - inicioEspera >= ESPERA_ACK_MS) {
        secuenciaPendiente = "";
        Serial.println("La base no confirmo a tiempo.");
    }

    int tamano = LoRa.parsePacket();
    if (tamano > 0) {
        String trama;
        trama.reserve(tamano);
        while (LoRa.available()) {
            trama += static_cast<char>(LoRa.read());
        }
        LoRa.receive();
        procesarTrama(trama);
    }
}
