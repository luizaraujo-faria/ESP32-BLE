// Importações
#include <NimBLEDevice.h>       // Biblioteca NimBLE para BLE no ESP32
#include <Wire.h>                 // Biblioteca para comunicação I2C
#include <LiquidCrystal_I2C.h>    // Biblioteca para usar o display LCD com I2C

// Definição dos pinos
const int ledPin = 16;            // LED conectado no pino 16
const int buttonPin = 17;         // Botão conectado no pino 17
const int potPin = 34;            // Potenciômetro no pino 34

// Variáveis de controle
bool ledState = false;            // Guarda se o LED deve piscar ou não
unsigned long lastToggle = 0;     // Guarda a última vez que o LED mudou de estado
int interval = 1000;              // Tempo de intervalo do pisca (em ms, começa com 1s)

// Objeto LCD (endereço 0x27, 16x2)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Ponteiros para BLE (servidor, serviço e característica)
NimBLEServer* pServer = nullptr;
NimBLEService* pService = nullptr;
NimBLECharacteristic* pCharacteristic = nullptr;

// UUIDs únicos para serviço e característica BLE
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890AB"
#define CHARACTERISTIC_UUID "ABCD1234-5678-1234-5678-1234567890AB"

class MyCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* pChar) {

    std::string value = pChar->getValue(); // Recebe dados do app
    String msg = String(value.c_str());

    Serial.println("Recebido via BLE: " + msg); // Log no monitor serial

    if(msg == "1"){
      ledState = true;
      digitalWrite(ledPin, HIGH);                  // Liga LED imediatamente
      lastToggle = millis();                       // Atualiza relógio
    }
    else if(msg == "0"){
      ledState = false;
      digitalWrite(ledPin, LOW);
    }
    else if (msg.startsWith("display:")) {
      String content = msg.substring(8);

      if(content.equalsIgnoreCase("clear")){
        lcd.clear();                      // Limpa LCD
      } 
      else{
        lcd.clear();                      
        lcd.setCursor(0,0);
        lcd.print(content);                // Mostra mensagem no LCD
      }
    }
  }
};

void setup() {

  pinMode(ledPin, OUTPUT);        // Define pino do LED como saída
  pinMode(buttonPin, INPUT_PULLUP); // Define pino do botão como entrada com resistor interno
  digitalWrite(ledPin, LOW);      // Garante que o LED começa desligado

  // Inicializa o display LCD
  lcd.init();                     
  lcd.backlight();                // Liga luz de fundo
  lcd.clear();                    // Limpa tela
  lcd.setCursor(0,0);             // Coloca cursor no canto superior esquerdo
  lcd.print("Bluetooth BLE");  // Escreve mensagem inicial

  Serial.begin(115200);           // Inicia comunicação serial (para monitorar pelo PC)

  // Inicializa BLE
  NimBLEDevice::init("ESP32_BLE"); // Nome do dispositivo
  pServer = NimBLEDevice::createServer(); // Cria servidor BLE
  pService = pServer->createService(SERVICE_UUID); // Cria serviço BLE
  
  // Cria característica com permissão de leitura/escrita
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      NIMBLE_PROPERTY::READ |
      NIMBLE_PROPERTY::WRITE
  );

  
  // Associa callback para receber dados do app
  pCharacteristic->setCallbacks(new MyCallbacks());

  // Inicia o serviço BLE
  pService->start();

  // Começa a publicidade para que o app descubra o ESP32
  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
}

void loop() {
  // --- Lê o potenciômetro e transforma em intervalo de pisca ---
  int potValue = analogRead(potPin);               // Lê valor do potenciômetro (0 a 4095)
  interval = map(potValue, 0, 4095, 200, 2000);    // Converte para intervalo entre 200ms e 2s

  // --- Controle do pisca do LED ---
  if (ledState) {                                  // Só entra se LED estiver habilitado
    unsigned long currentMillis = millis();        // Pega o tempo atual
    if (currentMillis - lastToggle >= interval) {  // Se já passou o tempo do intervalo...
      lastToggle = currentMillis;                  // Atualiza o "relógio"
      digitalWrite(ledPin, !digitalRead(ledPin));  // Troca estado do LED (liga/desliga)
    }
  }

  // --- Verifica botão ---
  if (digitalRead(buttonPin) == LOW) {             // Se botão for pressionado
    ledState = false;                              // Para o pisca do LED
    digitalWrite(ledPin, LOW);                     // Garante que LED fica desligado
    pCharacteristic->setValue("Botao pressionado - LED desligado"); // Envia mensagem para o servidor
    delay(500);                                    // Pequena pausa (evita múltiplos cliques)
  }
}