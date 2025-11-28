// BIBLIOTECAS BLE
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// BIBLIOTECAS RFID
#include <SPI.h>
#include <MFRC522.h>

// BIBLIOTECA DISPLAY
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// --------- CONFIGURAÇÕES RFID ----------
#define SS_PIN 5
#define RST_PIN 4
MFRC522::MIFARE_Key key;
MFRC522 mfrc522(SS_PIN, RST_PIN);

// --------- CONFIGURAÇÕES BLE ----------
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-1234567890ab"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// --------- PINOS ----------
const int ledPin = 16;
const int buzzer = 17;
const int pinVerde = 12;
const int pinVermelho = 13;

// --------- DISPLAY ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --------- VARIÁVEIS GLOBAIS ----------
bool ledState = false;
unsigned long lastToggle = 0;
int interval = 1000;

// --------- CALLBACKS BLE ----------
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("✅ Cliente BLE conectado!");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("BLE Conectado");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("❌ Cliente BLE desconectado!");
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Aguardando...");
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();
      String msg(value.c_str());

      Serial.print("📩 Recebido via BLE: ");
      Serial.println(msg);

      // CONTROLE DO LED
      if (msg == "1") {
        ledState = true;
        digitalWrite(ledPin, HIGH);
        lastToggle = millis();
        Serial.println("💡 LED LIGADO");

        pCharacteristic->setValue("LED ligado");
        pCharacteristic->notify();   // ENVIA AO APP
      } 
      else if (msg == "0") {
        ledState = false;
        digitalWrite(ledPin, LOW);
        Serial.println("💡 LED DESLIGADO");

        pCharacteristic->setValue("LED desligado");
        pCharacteristic->notify();
      } 
      // CONTROLE DE EXIBIÇÃO DO DISPLAY
      else if (msg.startsWith("display:")) {
        String content = msg.substring(8);
        Serial.print("🖥️ Display: ");
        Serial.println(content);

        if (content.equalsIgnoreCase("clear")) {
          lcd.clear();
        } else {
          lcd.clear();
          lcd.setCursor(0,0);
          lcd.print(content);
        }

        pCharacteristic->setValue(("Display: " + content).c_str());
        pCharacteristic->notify();
      } 
      else {
        pCharacteristic->setValue("Comando desconhecido");
        pCharacteristic->notify();
      }
    }
};

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  
  // INICIALIZA PINOS
  pinMode(ledPin, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(pinVerde, OUTPUT);
  pinMode(pinVermelho, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzer, LOW);
  digitalWrite(pinVerde, LOW);
  digitalWrite(pinVermelho, LOW);

  // INICIALIZA DISPLAY
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Iniciando...");

  // INICIALIZA RFID
  SPI.begin();
  mfrc522.PCD_Init();
  for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

  // INICIALIZA BLE
  Serial.println("🚀 Iniciando BLE...");
  BLEDevice::init("ESP32_FRID");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY 
  );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("ESP32 RFID Pronto!");

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);

  BLEDevice::startAdvertising();

  Serial.println("📱 BLE Ativo! Aguardando conexão...");
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Aguardando...");
}

// =====================================================
// LEITURA RFID SIMPLIFICADA
// =====================================================
String lerCartaoRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return "";
  }

  byte sector = 0;
  byte block = 1;
  byte blockAddr = sector * 4 + block;

  // Autentica e lê o bloco
  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(
      MFRC522::PICC_CMD_MF_AUTH_KEY_A, blockAddr, &key, &(mfrc522.uid));

  if (status != MFRC522::STATUS_OK) {
    mfrc522.PICC_HaltA();
    return "";
  }

  byte buffer[18];
  byte size = sizeof(buffer);
  status = mfrc522.MIFARE_Read(blockAddr, buffer, &size);

  if (status != MFRC522::STATUS_OK) {
    mfrc522.PICC_HaltA();
    return "";
  }

  // Processa dados lidos
  String rawData = "";
  for (uint8_t i = 0; i < 6; i++) {
    char c = (char)buffer[i];
    rawData += c;
  }

  // Verifica se é "Visita"
  if (rawData.indexOf("Visita") != -1) {
    return "Visita";
  }

  // Extrai apenas números
  String uidString = "";
  for (uint8_t i = 0; i < 6; i++) {
    char c = (char)buffer[i];
    if (isDigit(c)) {
      uidString += c;
    }
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  
  return uidString;
}

// =====================================================
// LOOP PRINCIPAL
// =====================================================
void loop() {
  // --- Controle do LED piscante ---
  if (ledState) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastToggle >= interval) {
      lastToggle = currentMillis;
      digitalWrite(ledPin, !digitalRead(ledPin));
    }
  }

  // --- Leitura do RFID ---
  String idCartao = lerCartaoRFID();
  if (idCartao != "") {
    Serial.print("🎫 Cartão lido: ");
    Serial.println(idCartao);

    // Feedback visual e sonoro
    digitalWrite(buzzer, HIGH);
    digitalWrite(pinVerde, HIGH);
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Cartao Lido!");
    lcd.setCursor(0,1);
    lcd.print("ID: " + idCartao);

    // Envia via BLE se conectado
    if (deviceConnected) {
      pCharacteristic->setValue(idCartao.c_str());
      pCharacteristic->notify();   // ENVIA AO APP
      Serial.println("📤 ID enviado via BLE");
    }

    delay(1000);
    
    // Limpa feedback
    digitalWrite(buzzer, LOW);
    digitalWrite(pinVerde, LOW);
    
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Aguardando...");
  }

  delay(100);
}
// // CHAMA AS BIBLIOTECAS
// #include <BLEDevice.h>
// #include <BLEUtils.h>
// #include <BLEServer.h>
// #include <Wire.h>
// #include <LiquidCrystal_I2C.h>

// const int ledPin = 16;
// const int potPin = 34; // Pino do potenciômetro para controlar pisca do LED

// // CRIA UUIDs PARA O SERVIÇO E CARACTERITICAS
// #define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
// #define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-1234567890ab"

// BLEServer* pServer = nullptr;
// BLECharacteristic* pCharacteristic = nullptr;

// bool deviceConnected = false;

// // VARIÁVEIS DO LED
// bool ledState = false;            // Se o LED deve piscar
// unsigned long lastToggle = 0;     // Última mudança de estado do LED
// int interval = 1000;              // Intervalo do pisca (ms)

// LiquidCrystal_I2C lcd(0x27, 16, 2); // Cria objeto para o display (endereço 0x27, 16 colunas, 2 linhas)

// // CALLBACK DO NIMBLE PARA CONEXÃO E DESCONEXÃO 
// class MyServerCallbacks: public BLEServerCallbacks {
//     void onConnect(BLEServer* pServer) {
//       deviceConnected = true;
//       Serial.println("✅ Cliente conectado!");
//     }

//     void onDisconnect(BLEServer* pServer) {
//       deviceConnected = false;
//       Serial.println("❌ Cliente desconectado!");
//       BLEDevice::startAdvertising();
//     }
// };

// // CALLBACK DO NIMBLE PARA ESCRITA E LEITURA DE DADOS
// class MyCallbacks: public BLECharacteristicCallbacks {
//     void onWrite(BLECharacteristic *pCharacteristic) {
//       String value = pCharacteristic->getValue();
//       String msg(value.c_str());

//       Serial.print("📩 Recebido: ");
//       Serial.println(msg);

//       // CONTROLE DO LED
//       if (msg == "1") {
//         ledState = true;           // Ativa pisca
//         digitalWrite(ledPin, HIGH);// Liga imediatamente
//         lastToggle = millis();     // Reinicia temporizador
//         Serial.println("💡 LED LIGADO");
//         pCharacteristic->setValue("LED ligado");
//       } 
//       else if (msg == "0") {
//         ledState = false;          // Para pisca
//         digitalWrite(ledPin, LOW);
//         Serial.println("💡 LED DESLIGADO");
//         pCharacteristic->setValue("LED desligado");
//       } 
//       // CONTROLE DE EXIBIÇÃO DO DISPLAY
//       else if (msg.startsWith("display:")) {
//         String content = msg.substring(8);
//         Serial.print("🖥️ Display: ");
//         Serial.println(content);

//         if (content.equalsIgnoreCase("clear")) {
//           lcd.clear();
//         } else {
//           lcd.clear();
//           lcd.setCursor(0,0);
//           lcd.print(content);
//         }

//         pCharacteristic->setValue(("Display: " + content).c_str());
//       } 
//       else {
//         pCharacteristic->setValue("Comando desconhecido");
//       }
//     }
// };

// void setup() {
//   Serial.begin(115200);
//   pinMode(ledPin, OUTPUT);
//   digitalWrite(ledPin, LOW);

//   pinMode(potPin, INPUT); // Potenciômetro

//   // INICIALIZA O DISPLAY
//   lcd.init();
//   lcd.backlight();
//   lcd.clear();
//   lcd.setCursor(0,0);
//   lcd.print("BLE ESP32 Ready");

//   Serial.println("🚀 Iniciando BLE...");

//   BLEDevice::init("ESP32_BLE_Display"); // NOME DO DISPOSITIVO EM PUBLICIDADE
//   pServer = BLEDevice::createServer();
//   pServer->setCallbacks(new MyServerCallbacks());

//   BLEService *pService = pServer->createService(SERVICE_UUID);

// // DEFINE CARACTERISTICAS DE ESCRITA E LEITURA
//   pCharacteristic = pService->createCharacteristic(
//                       CHARACTERISTIC_UUID,
//                       BLECharacteristic::PROPERTY_READ |
//                       BLECharacteristic::PROPERTY_WRITE
//                     );

//   pCharacteristic->setCallbacks(new MyCallbacks());
//   pCharacteristic->setValue("ESP32 Pronto!");
//   pService->start();

//   BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
//   pAdvertising->addServiceUUID(SERVICE_UUID);
//   pAdvertising->setScanResponse(true);
//   pAdvertising->setMinPreferred(0x06); 
//   BLEDevice::startAdvertising();

// // EXIBE AS INFORMAÇÕES NO SERIAL AO INICIALIZAR DISPOSITIVO BLE
//   Serial.println("📱 BLE Ativo! Aguardando conexão...");
//   Serial.print("🔧 Serviço UUID: ");
//   Serial.println(SERVICE_UUID);
//   Serial.print("📨 Característica UUID: ");
//   Serial.println(CHARACTERISTIC_UUID);
// }

// void loop() {
//   // --- Lê potenciômetro e atualiza intervalo do LED ---
//   int potValue = analogRead(potPin);
//   interval = map(potValue, 0, 4095, 200, 2000);

//   // --- Pisca LED se estiver ativado ---
//   if (ledState) {
//     unsigned long currentMillis = millis();
//     if (currentMillis - lastToggle >= interval) {
//       lastToggle = currentMillis;
//       digitalWrite(ledPin, !digitalRead(ledPin));
//     }
//   }

//   delay(10); // Pequeno delay para estabilidade
// }