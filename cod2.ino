#include <LiquidCrystal_I2C.h> // Display com i2c
#include <EEPROM.h>     // memoria eprom
#include <SPI.h>        // RC522 Module uses SPI protocol
#include <MFRC522.h>  // Library for Mifare RC522 Devices

#include <WiFi.h>
#include <Crescer.h> // biblioteca para o milis tempo do programa


#define LED 15
#define Contador 16
#define COMMON_ANODE
#ifdef COMMON_ANODE
#define LED_ON LOW
#define LED_OFF HIGH
#else
#define LED_ON HIGH
#define LED_OFF LOW
#endif

////////    VARIÁVEIS     ////////
int acumula0 = 0;
Tempora temp1;  //  precisa adicionar a biblioteca <Crescer.h> por causa desse comando.
int analogica;
int refr = 1;
bool s_high = 0;
int L = 0;
const char* ssid = "fabi";
const char* password = "fabi2012";

WiFiServer server(80);
LiquidCrystal_I2C lcd(0x27,16,2); //define I2C LCD
constexpr uint8_t redLed = 27;   // Set Led Pins
constexpr uint8_t greenLed = 26;
constexpr uint8_t blueLed = 25;

constexpr uint8_t relay = 15;     // Set Relay Pin
constexpr uint8_t wipeB = 33;     // Button pin for WipeMode

boolean match = false;          // initialize card match to false
boolean programMode = false;  // initialize programming mode to false
boolean replaceMaster = false;

uint8_t successRead;    // Variable integer to keep if we have Successful Read from Reader

byte storedCard[4];   // Stores an ID read from EEPROM
byte readCard[4];   // Stores scanned ID read from RFID Module
byte masterCard[4];   // Stores master card's ID read from EEPROM

// Create MFRC522 instance.
constexpr uint8_t RST_PIN = 0;     // Configurable, see typical pin layout above
constexpr uint8_t SS_PIN = 5;     // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);

///////////////////////////////////////// Setup ///////////////////////////////////
void setup() {
   Serial.begin(115200);
  pinMode(15, OUTPUT);  // set the LED pin mode
  pinMode(13, OUTPUT);
  pinMode(16, INPUT_PULLUP);
  pinMode(14, INPUT);
  delay(10);
  temp1.defiSP(500);
  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Conectado a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }


  IPAddress ip(192, 168, 2, 123);
  IPAddress gateway(192, 168, 2, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.config(ip, gateway, subnet);

  Serial.println("");
  Serial.println("WiFi connected.");
  digitalWrite(13, HIGH);
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  server.begin();

  lcd.init(); // ligando lcd
  lcd.clear();// ligando lcd
  lcd.backlight();// ligando lcd


    EEPROM.begin(1024);  // Inicializa a EEPROM com 1024 bytes

    // Arduino Pin Configuration
    pinMode(redLed, OUTPUT);
    pinMode(greenLed, OUTPUT);
    pinMode(blueLed, OUTPUT);
    pinMode(wipeB, INPUT_PULLUP);   // Habilita o resistor pull-up do botão
    pinMode(relay, OUTPUT);

    // Configuração do Relé e LEDs
    digitalWrite(relay, LOW);    // Garante que a porta está trancada
    digitalWrite(redLed, LED_OFF);  // Garante que o LED vermelho está desligado
    digitalWrite(greenLed, LED_OFF);  // Garante que o LED verde está desligado
    digitalWrite(blueLed, LED_OFF); // Garante que o LED azul está desligado

    // Configuração do Protocolo
    Serial.begin(115200);  // Inicializa a comunicação serial
    SPI.begin();         // Inicializa o SPI para o MFRC522
    mfrc522.PCD_Init();  // Inicializa o hardware do MFRC522

    Serial.println(F("Controle de Acesso v0.1"));   // Mensagem de inicialização
    ShowReaderDetails();  // Exibe detalhes do leitor

    // Wipe Code - Se o botão (wipeB) estiver pressionado, limpa a EEPROM
    if (digitalRead(wipeB) == LOW) {  // Botão pressionado
        digitalWrite(redLed, LED_ON); // Indica que a EEPROM será limpa
        Serial.println(F("Botao de formatacao apertado"));
        Serial.println(F("Voce tem 10 segundos para cancelar"));
        Serial.println(F("Isso vai apagar todos os seus registros e nao tem como desfazer"));
         lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("formatacao em ");
        lcd.setCursor(0,1);
        lcd.print(" 10s");
        bool buttonState = monitorWipeButton(10000); // Tempo para cancelar operação
        if (buttonState == true && digitalRead(wipeB) == LOW) { // Se o botão ainda estiver pressionado
            Serial.println(F("Inicio da formatacao da EEPROM"));
            for (uint16_t x = 0; x < EEPROM.length(); x++) { // Percorre todos os endereços da EEPROM
                EEPROM.write(x, 0);  // Limpa a EEPROM escrevendo 0
            }
            EEPROM.write(0, 0);   // Zera o contador de IDs
            EEPROM.commit();      // Salva as alterações
            Serial.println(F("EEPROM formatada com sucesso"));
             lcd.clear();
            lcd.setCursor(0,0);
           lcd.print("formatacao foi ");
           lcd.setCursor(0,1);
           lcd.print(" um sucesso");


            // Feedback visual da formatação
            for (int i = 0; i < 3; i++) {
                digitalWrite(redLed, LED_ON);
                delay(200);
                digitalWrite(redLed, LED_OFF);
                delay(200);
            }
        } else {
            Serial.println(F("Formatacao cancelada")); // Indica que a formatação foi cancelada
            digitalWrite(redLed, LED_OFF);
             lcd.clear();
            lcd.setCursor(0,0);
           lcd.print("formatacao  ");
           lcd.setCursor(0,1);
           lcd.print("cancelada");
        }
    }

    // Verifica se o cartão mestre está definido
    if (EEPROM.read(1) != 143) {
        Serial.println(F("Cartao Mestre nao definido"));
        Serial.println(F("Leia um chip para definir cartao Mestre"));
         lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("mestre nao ");
        lcd.setCursor(0,1);
        lcd.print("definido leia ");


        do {
            successRead = getID(); // Lê o cartão RFID
            digitalWrite(blueLed, LED_ON); // Indica que o cartão mestre precisa ser definido
            delay(200);
            digitalWrite(blueLed, LED_OFF);
            delay(200);
        } while (!successRead); // Continua até que um cartão seja lido

        for (uint8_t j = 0; j < 4; j++) { // Grava o UID do cartão mestre
            EEPROM.write(2 + j, readCard[j]); // Escreve o UID a partir do endereço 2
        }
        EEPROM.write(1, 143); // Marca que o cartão mestre foi definido
        Serial.println(F("Cartao Mestre definido"));
         lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("mestre definido  ");

    }

    // Lê o UID do cartão mestre e o exibe
    Serial.println(F("-------------------"));
    Serial.println(F("UID do cartao Mestre"));
     lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("UID do cartao  ");
    lcd.setCursor(0,1);
    lcd.print("Mestre");
    for (uint8_t i = 0; i < 4; i++) { // Lê o UID do cartão mestre da EEPROM
        masterCard[i] = EEPROM.read(2 + i); // Armazena o UID
        Serial.print(masterCard[i], HEX); // Exibe o UID em formato hexadecimal
    }
    Serial.println("");
    Serial.println(F("-------------------"));
    Serial.println(F("Tudo esta pronto"));
    Serial.println(F("Aguardando pelos chips para serem lidos"));
    cycleLeds(); // Feedback visual de que tudo está pronto
     lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Tudo esta pronto");
    lcd.setCursor(0,1);
    lcd.print("Aguardando chips");

    EEPROM.commit(); // Garante que todas as alterações na EEPROM sejam salvas

   
 
}

///////////////////////////////////////// Main Loop ///////////////////////////////////
void loop () {
                 
    do {
              WiFiClient client = server.available();  // listen for incoming clients



        if (!digitalRead (14)) {
                 digitalWrite(LED, LOW);
                Serial.println ("FECHADURA TRAVADA");
                  delay(100);
                  if (!digitalRead(Contador)) {            // contagem de pessoas
                 L++;
                  delay(1000);
                  Serial.println("numero de pessoas");
                  Serial.println(L);

                 }
                }

         if (client) {                     // if you get a client,
           Serial.println("New Client.");  // print a message out the serial port
           String currentLine = "";        // make a String to hold incoming data from the client
           while (client.connected()) {    // loop while the client's connected
             if (client.available()) {     // if there's bytes to read from the client,
              char c = client.read();     // read a byte, then
          
               Serial.write(c);  // print it out the serial monitor
              if (c == '\n') {  // if the byte is a newline character

                  // if the current line is blank, you got two newline characters in a row.
                  // that's the end of the client HTTP request, so send a response:
                  if (currentLine.length() == 0) {
                    // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
                  // and a content-type so the client knows what's coming, then a blank line:
                   client.println("HTTP/1.1 200 OK");
                   client.println("Content-type:text/html");
                  client.println();

                 
                  client.println("<html>");
                  client.println("<head><meta content=\"width=device-width, initial-scale=1\">");
                  client.println("<style>htal (margin: 0px auto; text-align: center:)"); client.println(".botao_liga (background-color: #00FF00; color: white; padding: 15px 40px; border-radius: 25px;)");
                  client.println(".botao_desliga (background-color: #FF0000; color: white; padding: 15px 40px; border-radius: 25px;}</style></head>");
                   client.println("<center>");
                  client.println("<br>");
                  client.println("<body> <hi>FECHADURA ELETRONICA</hi>");
                   client.println("<center>");
                  client.println("<br>");
                   client.println("<a href=\"/bot\"\"><button>ABRIR FECHADURA</button></a>");
                   client.println("<br>");
                    client.println("<center>");
                   client.println("Numero de pessoas:");
                   client.print(L);


                  // The HTTP response ends with another blank line:
                  client.println();
                  // break out of the while loop:
                   break;
                  } else {  // if you got a newline, then clear currentLine:
                    currentLine = "";
                  }
              } else if (c != '\r') {  // if you got anything else but a carriage return character,
                currentLine += c;      // add it to the end of the currentLine
              }

              // manda o comando pra web de abrir a fechadura
               if (currentLine.endsWith("GET /bot")) {
                digitalWrite(LED, !digitalRead(LED));  //ligar e desligar / abrir fechadura conforme o que le anteriormente
                 delay(2000);
               }

      
             }
            }
          // close the connection:
           client.stop();
           Serial.println("Client Disconnected.");
          }
         delay(10);
        








        successRead = getID();  // Lê o ID do cartão        

        // Quando o botão de wipe é pressionado por 10 segundos, inicializa a remoção do cartão mestre
        if (digitalRead(wipeB) == LOW) { // Verifica se o botão está pressionado
            // Visualiza que a operação normal foi interrompida ao pressionar o botão
            digitalWrite(redLed, LED_ON);  // LED vermelho indica alerta
            digitalWrite(greenLed, LED_OFF);
            digitalWrite(blueLed, LED_OFF);

            // Feedback para o usuário
            Serial.println(F("Botao de formatacao apertado"));
            Serial.println(F("O cartao Mestre sera apagado! em 10 segundos"));
             lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("formatacao em ");
            lcd.setCursor(0,1);
            lcd.print(" 10s");
            bool buttonState = monitorWipeButton(10000); // Tempo para cancelar a operação
            

            if (buttonState == true && digitalRead(wipeB) == LOW) { // Se o botão ainda estiver pressionado, limpa a EEPROM
                EEPROM.write(1, 0); // Reseta o número mágico.
                EEPROM.commit(); // Salva as alterações na EEPROM
                Serial.println(F("Cartao Mestre desvinculado do dispositivo"));
                Serial.println(F("Aperte o reset da placa para reprogramar o cartao Mestre"));
                  lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Mestre desvincul");
                lcd.setCursor(0,1);
                lcd.print("Aperte reset");
                // while (1); // Loop infinito para parar a execução
                ESP.restart();
               
            }
            Serial.println(F("Desvinculo do cartao Mestre cancelado"));
             lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Desvinculo chip");
            lcd.setCursor(0,1);
            lcd.print("Mestre cancelado");
        }

        // Modo de Programação
        if (programMode) {
            cycleLeds(); // Cicla LEDs enquanto espera ler um novo cartão
        } else {
            normalModeOn(); // Modo normal, LED azul ligado, outros desligados
        }
    } while (!successRead); // Continua até obter uma leitura bem-sucedida
    Serial.println("if antes da saida do mestre");

    // Processa a leitura bem-sucedida do cartão
    if (programMode) {
        if (isMaster(readCard)) { // Verifica se o cartão lido é o mestre
            Serial.println(F("Leitura do cartao Mestre"));
            Serial.println(F("Saindo do modo de programacao"));
            Serial.println(F("-----------------------------"));
             lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("Saindo do modo");
            lcd.setCursor(0,1);
            lcd.print("de programacao");
            delay( 1500);
             lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("FECHADURA");
            lcd.setCursor(0,1);
            lcd.print("Leia chip");
            
            programMode = false; // Sai do modo de programação
            
            return;
            

        } else {            
            if (findID(readCard)) { // Se o cartão já é conhecido, remove-o
                Serial.println(F("Conheco este chip, removendo..."));
                deleteID(readCard);
                Serial.println("-----------------------------");
                Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
                 lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("chip removido");
                lcd.setCursor(0,1);
                lcd.print("Leia outro chip");
               
            } else { // Se o cartão não é conhecido, adiciona-o
                Serial.println(F("Nao conheco este chip, incluindo..."));
                writeID(readCard);
                Serial.println(F("-----------------------------"));
                Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
                 lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("chip adicionado");
                lcd.setCursor(0,1);
                lcd.print("Leia outro chip");
               
            }
        }

        
    } else {
      Serial.println("if antes do mestre");

        // Modo normal
        if (isMaster(readCard)) { // Se o cartão lido é o mestre, entra no modo de programação
            programMode = true;
            Serial.println(F("Ola Mestre LORENZO - Modo de programacao iniciado"));
            uint8_t count = EEPROM.read(0) - 5; // Lê o número de IDs armazenados
            Serial.print(F("Existem "));
            Serial.print(count);
            Serial.print(F(" registro(s) na EEPROM"));
            Serial.println("");
            Serial.println(F("Leia um chip para adicionar ou remover da EEPROM"));
            Serial.println(F("Leia o cartao Mestre novamente para sair do modo de programacao"));
            Serial.println(F("-----------------------------"));
             lcd.clear();
            lcd.setCursor(0,0);
            lcd.print("modo programacao");
            lcd.setCursor(0,1);
            lcd.print("Leia chip");
        } else {

                

            // Verifica se o cartão está na EEPROM
            if (findID(readCard)) {
                Serial.println(F("Bem-vindo, voce pode passar"));
                 lcd.clear();
                 lcd.setCursor(0,0);
                 lcd.print("Bem-vindo,");
                 lcd.setCursor(0,1);
                 lcd.print("Entre ");
                granted(800); // Abre a fechadura por 800 ms
            } else { // Cartão não válido
                Serial.println(F("Voce nao pode passar"));
                denied();
                 lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Acesso negado,");
                lcd.setCursor(0,1);
                lcd.print("Leia outro chip");
            }
        }
    }
}

/////////////////////////////////////////  Access Granted    ///////////////////////////////////
void granted ( uint16_t setDelay) {
  digitalWrite(blueLed, LED_OFF);   // Turn off blue LED
  digitalWrite(redLed, LED_OFF);  // Turn off red LED
  digitalWrite(greenLed, LED_ON);   // Turn on green LED
  digitalWrite(relay, HIGH);     // Unlock door!
  delay(setDelay);          // Hold door lock open for given seconds
  digitalWrite(relay, LOW);    // Relock door
  delay(1000);            // Hold green LED on for a second
}

///////////////////////////////////////// Access Denied  ///////////////////////////////////
void denied() {
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_ON);   // Turn on red LED
  delay(1000);
}


///////////////////////////////////////// Get PICC's UID ///////////////////////////////////
uint8_t getID() {
  // Getting ready for Reading PICCs
  if ( ! mfrc522.PICC_IsNewCardPresent()) { //If a new PICC placed to RFID reader continue
    return 0;
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {   //Since a PICC placed get Serial and continue
    return 0;
  }
  // There are Mifare PICCs which have 4 byte or 7 byte UID care if you use 7 byte PICC
  // I think we should assume every PICC as they have 4 byte UID
  // Until we support 7 byte PICCs
  Serial.println(F("UID do chip lido:"));
  for ( uint8_t i = 0; i < 4; i++) {  //
    readCard[i] = mfrc522.uid.uidByte[i];
    Serial.print(readCard[i], HEX);
  }
  Serial.println("");
  mfrc522.PICC_HaltA(); // Stop reading
  return 1;
}

void ShowReaderDetails() {
  // Get the MFRC522 software version
  byte v = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print(F("Versao do software MFRC522: 0x"));
  Serial.print(v, HEX);
  if (v == 0x91)
    Serial.print(F(" = v1.0"));
  else if (v == 0x92)
    Serial.print(F(" = v2.0"));
  else
    Serial.print(F(" (desconhecido),provavelmente um clone chines?"));
  Serial.println("");
  // When 0x00 or 0xFF is returned, communication probably failed
  if ((v == 0x00) || (v == 0xFF)) {
    Serial.println(F("ALERTA: Falha na comunicacao, o modulo MFRC522 esta conectado corretamente?"));
    Serial.println(F("SISTEMA ABORTADO: Verifique as conexoes."));
     lcd.clear();
                lcd.setCursor(0,0);
                lcd.print("Erro no");
                lcd.setCursor(0,1);
                lcd.print("sistema");
    // Visualize system is halted
    digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
    digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
    digitalWrite(redLed, LED_ON);   // Turn on red LED
    while (true); // do not go further
  }
}

///////////////////////////////////////// Cycle Leds (Program Mode) ///////////////////////////////////
void cycleLeds() {
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(blueLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
}

//////////////////////////////////////// Normal Mode Led  ///////////////////////////////////
void normalModeOn () {
  digitalWrite(blueLed, LED_ON);  // Blue LED ON and ready to read card
  digitalWrite(redLed, LED_OFF);  // Make sure Red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure Green LED is off
  digitalWrite(relay, LOW);    // Make sure Door is Locked
}

//////////////////////////////////////// Read an ID from EEPROM //////////////////////////////
void readID( uint8_t number ) {
  uint8_t start = (number * 4 ) + 2;    // Figure out starting position
  for ( uint8_t i = 0; i < 4; i++ ) {     // Loop 4 times to get the 4 Bytes
    storedCard[i] = EEPROM.read(start + i);   // Assign values read from EEPROM to array
  }
}

///////////////////////////////////////// Add ID to EEPROM   ///////////////////////////////////
void writeID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we write to the EEPROM, check to see if we have seen this card before!
    uint8_t num = EEPROM.read(0);     // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t start = ( num * 4 ) + 6;  // Figure out where the next slot starts
    num++;                // Increment the counter by one
    EEPROM.write( 0, num );     // Write the new count to the counter
    for ( uint8_t j = 0; j < 4; j++ ) {   // Loop 4 times
      EEPROM.write( start + j, a[j] );  // Write the array values to EEPROM in the right position
    }
    EEPROM.commit();
    successWrite();
    Serial.println(F("ID adicionado na EEPROM com sucesso"));
     lcd.clear();
              lcd.setCursor(0,0);
                lcd.print("ID adicionado");
                lcd.setCursor(0,1);
                lcd.print("na EEPROM");
  }
  else {
    failedWrite();
    Serial.println(F("Erro! Tem alguma coisa errada com o ID do chip ou problema na EEPROM"));
     lcd.clear();
    lcd.setCursor(0,0);
                lcd.print("Erro! ");
                lcd.setCursor(0,1);
                lcd.print("algo errado");
  }
}

///////////////////////////////////////// Remove ID from EEPROM   ///////////////////////////////////
void deleteID( byte a[] ) {
  if ( !findID( a ) ) {     // Before we delete from the EEPROM, check to see if we have this card!
    failedWrite();      // If not
    Serial.println(F("Erro! Tem alguma coisa errada com o ID do chip ou problema na EEPROM"));
     lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Erro! ");
    lcd.setCursor(0,1);
    lcd.print("algo errado");
  }
  else {
    uint8_t num = EEPROM.read(0);   // Get the numer of used spaces, position 0 stores the number of ID cards
    uint8_t slot;       // Figure out the slot number of the card
    uint8_t start;      // = ( num * 4 ) + 6; // Figure out where the next slot starts
    uint8_t looping;    // The number of times the loop repeats
    uint8_t j;
    uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores number of cards
    slot = findIDSLOT( a );   // Figure out the slot number of the card to delete
    
    start = (slot * 4) + 2;   // ...
    
    looping = ((num - slot) * 4);
    num--;      // Decrement the counter by one
    EEPROM.write( 0, num );   // Write the new count to the counter
    for ( j = 0; j < looping; j++ ) {         // Loop the card shift times
      EEPROM.write( start + j, EEPROM.read(start + 4 + j));   // Shift the array values to 4 places earlier in the EEPROM
    }
    for ( uint8_t k = 0; k < 4; k++ ) {         // Shifting loop
      EEPROM.write( start + j + k, 0);
    }
    EEPROM.commit();
    successDelete();
    Serial.println(F("ID removido da EEPROM com sucesso"));
     lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ID");
    lcd.setCursor(0,1);
    lcd.print("REMOVIDO");
  }
}

///////////////////////////////////////// Check Bytes   ///////////////////////////////////
boolean checkTwo ( byte a[], byte b[] ) {
  if ( a[0] == 0 ) return false; // Return false if the first byte is 0
  for ( uint8_t k = 0; k < 4; k++ ) {
    if ( a[k] != b[k] ) return false; // Return false if any byte does not match
  }
  return true; // All bytes matched
}

///////////////////////////////////////// Find Slot   ///////////////////////////////////
uint8_t findIDSLOT( byte find[] ) {
  uint8_t count = EEPROM.read(0);       // Read the first Byte of EEPROM that
  for ( uint8_t i = 0; i <= count; i++ ) {    // Loop once for each EEPROM entry
    readID(i+6);                // Read an ID from EEPROM, it is stored in storedCard[4]
    if ( checkTwo( find, storedCard ) ) {   // Check to see if the storedCard read from EEPROM
      // is the same as the find[] ID card passed
      return i;         // The slot number of the card
      break;          // Stop looking we found it
    }
  }
}

///////////////////////////////////////// Find ID From EEPROM   ///////////////////////////////////
boolean findID(byte find[]) {
  uint8_t count = EEPROM.read(0); // Read the first Byte of EEPROM that stores the number of IDs
  for (uint8_t i = 0; i < count; i++) { // Start from 0 to count - 1
    readID(i+6); // Read an ID from EEPROM, assuming tags start at address 6
    if (checkTwo(find, storedCard)) { // Check if the storedCard matches the one we're looking for
      return true; // Card found
    }
  }
  return false; // Card not found
}

///////////////////////////////////////// Write Success to EEPROM   ///////////////////////////////////
// Flashes the green LED 3 times to indicate a successful write to EEPROM
void successWrite() {
  digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200);
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(greenLed, LED_ON);   // Make sure green LED is on
  delay(200);
}

///////////////////////////////////////// Write Failed to EEPROM   ///////////////////////////////////
// Flashes the red LED 3 times to indicate a failed write to EEPROM
void failedWrite() {
  digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  delay(200);
  digitalWrite(redLed, LED_ON);   // Make sure red LED is on
  delay(200);
}

///////////////////////////////////////// Success Remove UID From EEPROM  ///////////////////////////////////
// Flashes the blue LED 3 times to indicate a success delete to EEPROM
void successDelete() {
  digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
  digitalWrite(redLed, LED_OFF);  // Make sure red LED is off
  digitalWrite(greenLed, LED_OFF);  // Make sure green LED is off
  delay(200);
  digitalWrite(blueLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(blueLed, LED_ON);  // Make sure blue LED is on
  delay(200);
  digitalWrite(blueLed, LED_OFF);   // Make sure blue LED is off
  delay(200);
  digitalWrite(blueLed, LED_ON);  // Make sure blue LED is on
  delay(200);
}

////////////////////// Check readCard IF is masterCard   ///////////////////////////////////
// Check to see if the ID passed is the master programing card
boolean isMaster( byte test[] ) {
  if ( checkTwo( test, masterCard ) )
    return true;
  else
    return false;
}

bool monitorWipeButton(uint32_t interval) {
  uint32_t now = (uint32_t)millis();
  while ((uint32_t)millis() - now < interval)  {
    // check on every half a second
    if (((uint32_t)millis() % 500) == 0) {
      if (digitalRead(wipeB) != LOW)
        return false;
    }
  }
  return true;
}
