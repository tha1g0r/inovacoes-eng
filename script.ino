// ESP32 — microfone simulado (random) → painel do condomínio via MQTT
#include <WiFi.h>
#include <PubSubClient.h>

// ================== IDENTIFICAÇÃO ==================
const char* BLOCO = "A";
const int   ANDAR = 1;
const int   APTO  = 101;

// ================== AMBIENTE ==================
#define USANDO_WOKWI 1        // 1 = simulador Wokwi | 0 = placa física

#if USANDO_WOKWI
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASS = "";
#else
const char* WIFI_SSID = "SUA_REDE";     // rede 2,4 GHz
const char* WIFI_PASS = "SUA_SENHA";
#endif

// ================== CONFIGURAÇÃO ==================
#define PIN_LED 2

const int      LIMITE_PCT  = 30;
const int      HISTERESE   = 5;
const uint32_t JANELA_MS   = 50;
const uint32_t COOLDOWN_MS = 3000;
const uint32_t HOLD_MS     = 500;
const uint32_t ENVIO_MS    = 250;

const char* MQTT_BROKER  = "broker.hivemq.com";
const uint16_t MQTT_PORTA = 1883;
const char* MQTT_PREFIXO = "mova-pedro/condominio";

WiFiClient   net;
PubSubClient mqtt(net);
char topicoNivel[96], topicoEvento[96];
bool     alarme = false;
uint32_t tUltimoAlerta = 0, tUltimoSom = 0, tUltimaTentativa = 0, tUltimoEnvio = 0;

// ---------- Microfone simulado ----------
int      nivelManual = -1;        // valor digitado no Serial (-1 = automático)
int      nivelAtual  = 5;
bool     emBarulho   = false;
int      alvoBarulho = 0;
uint32_t tFimBarulho = 0, tProxBarulho = 0;

int lerNivelPct() {
  // Permite forçar um valor pelo Monitor Serial: 0–100, ou -1 para voltar ao automático
  if (Serial.available()) {
    int v = Serial.parseInt();
    while (Serial.available()) Serial.read();
    nivelManual = constrain(v, -1, 100);
    Serial.printf("[SIM] nivel manual = %d\n", nivelManual);
  }
  delay(JANELA_MS);
  if (nivelManual >= 0) return nivelManual;

  uint32_t agora = millis();
  if (!emBarulho && agora >= tProxBarulho) {            // começa um "barulho"
    emBarulho   = true;
    alvoBarulho = random(40, 91);                       // 40–90%
    tFimBarulho = agora + random(1000, 4001);           // dura 1–4 s
  }
  if (emBarulho && agora >= tFimBarulho) {              // termina o barulho
    emBarulho    = false;
    tProxBarulho = agora + random(5000, 15001);         // próximo em 5–15 s
  }

  int alvo = emBarulho ? alvoBarulho : random(3, 13);   // ruído de fundo 3–12%
  nivelAtual = (nivelAtual * 6 + alvo * 4) / 10 + random(-3, 4);   // suaviza
  return constrain(nivelAtual, 0, 100);
}

// ---------- Rede ----------
void conectarWiFi() {
  WiFi.mode(WIFI_STA);
#if USANDO_WOKWI
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
#else
  WiFi.begin(WIFI_SSID, WIFI_PASS);
#endif
  Serial.print("[WiFi] conectando");
  while (WiFi.status() != WL_CONNECTED) { delay(250); Serial.print("."); }
  Serial.print(" ok — IP: "); Serial.println(WiFi.localIP());
}

void manterMQTT() {
  if (mqtt.connected() || millis() - tUltimaTentativa < 3000) return;
  tUltimaTentativa = millis();
  String id = "esp32-" + String(BLOCO) + "-" + String(APTO) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqtt.connect(id.c_str())) Serial.println("[MQTT] conectado");
  else Serial.printf("[MQTT] falha, rc=%d\n", mqtt.state());
}

void enviarEvento(const char* evento, int nivel) {
  char msg[192];
  snprintf(msg, sizeof(msg),
    "{\"evento\":\"%s\",\"bloco\":\"%s\",\"andar\":%d,\"apto\":%d,\"nivel_pct\":%d,\"limite_pct\":%d}",
    evento, BLOCO, ANDAR, APTO, nivel, LIMITE_PCT);
  Serial.println(msg);
  if (mqtt.connected()) mqtt.publish(topicoEvento, msg);
}

void enviarNivel(int nivel) {
  char msg[96];
  snprintf(msg, sizeof(msg), "{\"nivel\":%d,\"alarme\":%s,\"andar\":%d}",
           nivel, alarme ? "true" : "false", ANDAR);
  mqtt.publish(topicoNivel, msg);
}

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(100);
  pinMode(PIN_LED, OUTPUT);
  randomSeed(esp_random());
  tProxBarulho = millis() + 3000;
  snprintf(topicoNivel,  sizeof(topicoNivel),  "%s/%s/%d/nivel",  MQTT_PREFIXO, BLOCO, APTO);
  snprintf(topicoEvento, sizeof(topicoEvento), "%s/%s/%d/evento", MQTT_PREFIXO, BLOCO, APTO);
  Serial.printf("\n=== Bloco %s, %dº andar, apto %d — microfone simulado ===\n", BLOCO, ANDAR, APTO);
  conectarWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORTA);
}

void loop() {
  manterMQTT();
  mqtt.loop();

  int nivel = lerNivelPct();
  uint32_t agora = millis();

  if (nivel >= LIMITE_PCT - HISTERESE) tUltimoSom = agora;

  if (!alarme && nivel >= LIMITE_PCT && agora - tUltimoAlerta > COOLDOWN_MS) {
    alarme = true;
    tUltimoAlerta = agora;
    enviarEvento("LIMITE_ULTRAPASSADO", nivel);
  } else if (alarme && agora - tUltimoSom > HOLD_MS) {
    alarme = false;
    enviarEvento("NIVEL_NORMALIZADO", nivel);
  }

  if (mqtt.connected() && agora - tUltimoEnvio >= ENVIO_MS) {
    tUltimoEnvio = agora;
    enviarNivel(nivel);
  }

  digitalWrite(PIN_LED, alarme);
}
