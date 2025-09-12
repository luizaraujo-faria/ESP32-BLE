// CHAMA AS BIBLIOTECAS
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const int ledPin = 16;
const int potPin = 34; // Pino do potenciômetro para controlar pisca do LED

// CRIA UUIDs PARA O SERVIÇO E CARACTERITICAS
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-1234-5678-1234567890ab"

BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;

bool deviceConnected = false;

// VARIÁVEIS DO LED
bool ledState = false;            // Se o LED deve piscar
unsigned long lastToggle = 0;     // Última mudança de estado do LED
int interval = 1000;              // Intervalo do pisca (ms)

LiquidCrystal_I2C lcd(0x27, 16, 2); // Cria objeto para o display (endereço 0x27, 16 colunas, 2 linhas)

// CALLBACK DO NIMBLE PARA CONEXÃO E DESCONEXÃO 
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("✅ Cliente conectado!");
    }

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("❌ Cliente desconectado!");
      BLEDevice::startAdvertising();
    }
};

// CALLBACK DO NIMBLE PARA ESCRITA E LEITURA DE DADOS
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue();
      String msg(value.c_str());

      Serial.print("📩 Recebido: ");
      Serial.println(msg);

      // CONTROLE DO LED
      if (msg == "1") {
        ledState = true;           // Ativa pisca
        digitalWrite(ledPin, HIGH);// Liga imediatamente
        lastToggle = millis();     // Reinicia temporizador
        Serial.println("💡 LED LIGADO");
        pCharacteristic->setValue("LED ligado");
      } 
      else if (msg == "0") {
        ledState = false;          // Para pisca
        digitalWrite(ledPin, LOW);
        Serial.println("💡 LED DESLIGADO");
        pCharacteristic->setValue("LED desligado");
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
      } 
      else {
        pCharacteristic->setValue("Comando desconhecido");
      }
    }
};

void setup() {
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(potPin, INPUT); // Potenciômetro

  // INICIALIZA O DISPLAY
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("BLE ESP32 Ready");

  Serial.println("🚀 Iniciando BLE...");

  BLEDevice::init("ESP32_BLE_Display"); // NOME DO DISPOSITIVO EM PUBLICIDADE
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

// DEFINE CARACTERISTICAS DE ESCRITA E LEITURA
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->setValue("ESP32 Pronto!");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); 
  BLEDevice::startAdvertising();

// EXIBE AS INFORMAÇÕES NO SERIAL AO INICIALIZAR DISPOSITIVO BLE
  Serial.println("📱 BLE Ativo! Aguardando conexão...");
  Serial.print("🔧 Serviço UUID: ");
  Serial.println(SERVICE_UUID);
  Serial.print("📨 Característica UUID: ");
  Serial.println(CHARACTERISTIC_UUID);
}

void loop() {
  // --- Lê potenciômetro e atualiza intervalo do LED ---
  int potValue = analogRead(potPin);
  interval = map(potValue, 0, 4095, 200, 2000);

  // --- Pisca LED se estiver ativado ---
  if (ledState) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastToggle >= interval) {
      lastToggle = currentMillis;
      digitalWrite(ledPin, !digitalRead(ledPin));
    }
  }

  delay(10); // Pequeno delay para estabilidade
}