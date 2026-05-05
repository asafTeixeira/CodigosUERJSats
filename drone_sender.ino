// ============================================================
//  DRONE — Emissor de Imagem (Push TCP, lean memory)
//  
//  Arquitetura:
//    - SEM WebServer (economiza ~20 KB de heap)
//    - Push ativo: drone envia, receptor escuta
//    - Imagem lida do LittleFS em chunks (sem carregar no heap)
//    - Arquivo deletado logo após envio confirmado
//    - Se envio falhar, tenta reenviar antes de aceitar nova imagem
//    - Delay configurável entre envios
// ============================================================

#include <WiFi.h>
#include <LittleFS.h>
#include <Arduino.h>

// ── Wi-Fi ─────────────────────────────────────────────────── 
const char* ssid   = "LYM_Teste";
const char* senha  = "123456789";

// ── Receptor (computador / ground station) ────────────────── 
const char* RECEIVER_IP   = "10.208.194.35"; // ← IP do receptor
const uint16_t RECEIVER_PORT = 5000;          // Porta TCP do receptor

// ── Tunáveis ──────────────────────────────────────────────── 
#define SEND_INTERVAL_MS   3000   // Intervalo mínimo entre envios (ms)
#define MAX_RETRIES        3      // Tentativas antes de descartar imagem
#define TCP_TIMEOUT_MS     5000   // Timeout de conexão/resposta TCP
#define CHUNK_SIZE         512    // Bytes por leitura do LittleFS (heap mínimo)

// ── Arquivo pendente (única imagem em espera) ─────────────── 
#define PENDING_PATH "/pending.jpg"

// ── ADICIONADO: Lote de 4 Imagens ───────────────────────────
const char* imageFiles[] = {
  "/teste1.jpg",
  "/teste2.jpg",
  "/teste3.jpg",
  "/teste4.jpg"
};
const uint8_t NUM_IMAGES = 4;
static uint8_t currentTestImage = 0;
static bool testBatchFinished = false;

// ── Estado global ─────────────────────────────────────────── 
static bool     hasPending    = false;
static uint8_t  retryCount    = 0;
static uint32_t lastSendTime  = 0;

WiFiClient client;

// ────────────────────────────────────────────────────────────
//  ADICIONADO: Envia imagens do lote de teste (Mantém o arquivo)
// ────────────────────────────────────────────────────────────
bool enviarImagemDoLote(const char* filepath) {
  File f = LittleFS.open(filepath, "r");
  if (!f) {
    Serial.printf("[TX Lote] Arquivo %s nao encontrado.\n", filepath);
    return false;
  }

  uint32_t fileSize = f.size();
  Serial.printf("[TX Lote] Enviando %s (%u bytes)...\n", filepath, fileSize);

  client.setTimeout(TCP_TIMEOUT_MS / 1000);
  if (!client.connect(RECEIVER_IP, RECEIVER_PORT)) {
    Serial.println("[TX Lote] Falha ao conectar.");
    f.close();
    return false;
  }

  uint8_t header[4] = {
    (uint8_t)(fileSize >> 24),
    (uint8_t)(fileSize >> 16),
    (uint8_t)(fileSize >>  8),
    (uint8_t)(fileSize      )
  };
  client.write(header, 4);

  uint8_t  buf[CHUNK_SIZE];
  uint32_t totalEnviado = 0;

  while (f.available()) {
    int lido = f.read(buf, CHUNK_SIZE);
    if (lido <= 0) break;

    int escrito = client.write(buf, lido);
    if (escrito <= 0) {
      Serial.println("[TX Lote] Erro de transmissao.");
      f.close();
      client.stop();
      return false;
    }
    totalEnviado += escrito;
  }
  f.close();

  uint32_t t0 = millis();
  while (!client.available()) {
    if (millis() - t0 > TCP_TIMEOUT_MS) {
      Serial.println("[TX Lote] Timeout aguardando ACK.");
      client.stop();
      return false;
    }
    delay(10);
  }

  uint8_t ack = client.read();
  client.stop();

  if (ack != 0xAC) {
    Serial.println("[TX Lote] ACK invalido.");
    return false;
  }

  // NOTA: Como você pediu, não removemos as imagens de teste
  Serial.printf("[TX Lote] Sucesso! %u bytes confirmados.\n", totalEnviado);
  return true;
}

// ────────────────────────────────────────────────────────────
//  ORIGINAL: Envia a imagem pendente ao receptor via TCP raw
// ────────────────────────────────────────────────────────────
bool enviarImagem() {
  File f = LittleFS.open(PENDING_PATH, "r");
  if (!f) {
    Serial.println("[TX] Arquivo pendente nao encontrado.");
    hasPending = false;
    return false;
  }

  uint32_t fileSize = f.size();
  Serial.printf("[TX] Enviando %u bytes para %s:%u\n", fileSize, RECEIVER_IP, RECEIVER_PORT);

  client.setTimeout(TCP_TIMEOUT_MS / 1000);
  if (!client.connect(RECEIVER_IP, RECEIVER_PORT)) {
    Serial.println("[TX] Falha ao conectar. Sera tentado novamente.");
    f.close();
    return false;
  }

  uint8_t header[4] = {
    (uint8_t)(fileSize >> 24),
    (uint8_t)(fileSize >> 16),
    (uint8_t)(fileSize >>  8),
    (uint8_t)(fileSize      )
  };
  client.write(header, 4);

  uint8_t  buf[CHUNK_SIZE];
  uint32_t totalEnviado = 0;

  while (f.available()) {
    int lido = f.read(buf, CHUNK_SIZE);
    if (lido <= 0) break;

    int escrito = client.write(buf, lido);
    if (escrito <= 0) {
      Serial.println("[TX] Erro durante transmissao.");
      f.close();
      client.stop();
      return false;
    }
    totalEnviado += escrito;
  }
  f.close();

  uint32_t t0 = millis();
  while (!client.available()) {
    if (millis() - t0 > TCP_TIMEOUT_MS) {
      Serial.println("[TX] Timeout aguardando ACK.");
      client.stop();
      return false;
    }
    delay(10);
  }

  uint8_t ack = client.read();
  client.stop();

  if (ack != 0xAC) {
    Serial.printf("[TX] ACK invalido recebido: 0x%02X\n", ack);
    return false;
  }

  LittleFS.remove(PENDING_PATH);
  hasPending   = false;
  retryCount   = 0;
  lastSendTime = millis();

  Serial.printf("[TX] OK — %u bytes enviados e confirmados.\n", totalEnviado);
  return true;
}

// ────────────────────────────────────────────────────────────
//  ORIGINAL: Salva nova imagem no LittleFS como "pendente"
// ────────────────────────────────────────────────────────────
bool armazenarImagem(const uint8_t* dados, size_t tamanho) {
  if (hasPending && retryCount < MAX_RETRIES) {
    Serial.println("[CAM] Nova imagem: ainda ha pendente, tentando reenviar primeiro...");
    enviarImagem();
    if (hasPending) {
      Serial.println("[CAM] Reenvio falhou. Nova imagem DESCARTADA para proteger pendente.");
      return false;
    }
  }

  if (hasPending && retryCount >= MAX_RETRIES) {
    Serial.println("[CAM] Max retries atingido. Substituindo imagem antiga pela nova.");
    LittleFS.remove(PENDING_PATH);
    retryCount = 0;
  }

  File f = LittleFS.open(PENDING_PATH, "w");
  if (!f) {
    Serial.println("[CAM] Erro ao abrir arquivo para escrita.");
    return false;
  }

  size_t escritos = f.write(dados, tamanho);
  f.close();

  if (escritos != tamanho) {
    Serial.println("[CAM] Escrita incompleta no LittleFS.");
    LittleFS.remove(PENDING_PATH);
    return false;
  }

  hasPending = true;
  Serial.printf("[CAM] Imagem armazenada: %u bytes\n", tamanho);
  return true;
}

// ────────────────────────────────────────────────────────────
//  Setup
// ────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("=== DRONE TX — Inicializando ===");

  if (!LittleFS.begin(true)) {
    Serial.println("Erro ao montar LittleFS. Abortando.");
    return;
  }

  if (LittleFS.exists(PENDING_PATH)) {
    LittleFS.remove(PENDING_PATH);
    Serial.println("Arquivo pendente anterior removido.");
  }

  WiFi.begin(ssid, senha);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado! IP: " + WiFi.localIP().toString());
  Serial.println("=== Pronto para transmitir ===\n");
}

// ────────────────────────────────────────────────────────────
//  Loop
// ────────────────────────────────────────────────────────────
void loop() {
  
  // ── 1. Primeiro envia as 4 imagens de teste que você pediu ──
  if (!testBatchFinished) {
    if (currentTestImage < NUM_IMAGES && (millis() - lastSendTime >= SEND_INTERVAL_MS)) {
      
      const char* currentFile = imageFiles[currentTestImage];
      Serial.printf("\n[TX] Testando Imagem %d/4...\n", currentTestImage + 1);

      if (enviarImagemDoLote(currentFile)) {
        currentTestImage++;
        retryCount = 0;
        lastSendTime = millis();
      } else {
        retryCount++;
        lastSendTime = millis();
        if (retryCount >= MAX_RETRIES) {
          Serial.println("[TX] Pulando imagem de teste após falhas.");
          currentTestImage++;
          retryCount = 0;
        }
      }
    } else if (currentTestImage >= NUM_IMAGES) {
      Serial.println("\n[TX] === As 4 imagens de teste foram enviadas com sucesso! ===");
      Serial.println("[TX] Retornando à rotina original da câmera...\n");
      testBatchFinished = true;
      retryCount = 0; // Zera para a rotina normal
    }
    return; // Impede que o resto do loop rode até terminar os testes
  }

  // ── 2. Sua lógica original (Roda após os testes terminarem) ──
  
  // ── Sua lógica de câmera aqui ─────────────────────────────
  //
  // Exemplo de integração com ESP32-CAM:
  //
  //   camera_fb_t* fb = esp_camera_fb_get();
  //   if (fb) {
  //     armazenarImagem(fb->buf, fb->len);
  //     esp_camera_fb_return(fb);   // libera heap da câmera imediatamente
  //   }
  //
  // ─────────────────────────────────────────────────────────

  if (hasPending && (millis() - lastSendTime >= SEND_INTERVAL_MS)) {

    Serial.printf("[TX] Tentativa %u/%u...\n", retryCount + 1, MAX_RETRIES);

    if (!enviarImagem()) {
      retryCount++;
      lastSendTime = millis();

      if (retryCount >= MAX_RETRIES) {
        Serial.println("[TX] Max retries atingido. Imagem sera descartada na proxima captura.");
      }
    }
  }
}