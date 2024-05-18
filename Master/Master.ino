#include <Wire.h>
#include <WiFiNINA.h>
#include <PubSubClient.h>
#include "secrets.h"
#include "pitches.h"
#include "melodie.h"

//ridefinisco la macro per far sì che l'arduino riesca a leggere i lunghi messaggi pubblicati dal raspberry 
#undef  MQTT_MAX_PACKET_SIZE // un-define max packet size
#define MQTT_MAX_PACKET_SIZE 1500  // fix for MQTT client dropping messages over 128B

#define DEBUG 0

//definisco le macro per i 4 LED, per il bottone e per il pin
#define LED_BLU    4
#define LED_ROSSO  5
#define LED_GIALLO 6
#define LED_VERDE  7
#define BUTTON_PIN 8
#define BUZZER_PIN 13

//inizializzo le costanti per far sì che il mosquitto funzioni correttamente, i valori vengono presi dal file secrets.h
const char WiFiSSID[]           = WIFI_SSID;
const char WiFiPassword[]       = WIFI_PASSWORD;
const char MQTTBrokerIP[]       = MQTT_BROKER_IP;
const uint16_t MQTTBrokerPort   = MQTT_BROKER_PORT;
const char MQTTBrokerUsername[] = MQTT_BROKER_USERNAME;
const char MQTTBrokerPassword[] = MQTT_BROKER_PASSWORD;
const char MQTTClientID[]       = MQTT_CLIENT_ID;

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

byte lastButtonState = LOW;

//flag utilizzati per assicurare il corretto funzionamento del programma
bool flagRichiesta = false;
bool flagAttesa    = false;
String messaggio   = "";


//funzione per inzializzare il monitor seriale se il debug è attivo
void configureSerialMonitor() {
  Serial.begin(9600);
  while (!Serial);
}

//funzione per configurare il mosquitto client
void configureMQTTClient() {
  mqttClient.setServer(MQTTBrokerIP, MQTTBrokerPort);
  mqttClient.setCallback(mqttCommandReceived);
}

//funzione per connettersi al mosquitto broker
void connectMQTTBroker() {
  if (DEBUG) {
    Serial.print("Connecting to MQTT broker (");
    Serial.print(MQTTBrokerIP);
    Serial.print(")...");
  }
  while (!mqttClient.connected()) {
    if (mqttClient.connect(MQTTClientID, MQTTBrokerUsername, MQTTBrokerPassword)) {
        mqttClient.setBufferSize(MQTT_MAX_PACKET_SIZE);
        if (DEBUG) Serial.println("connected."); 
    } else {
        delay(1000);
        if (DEBUG) {
          Serial.print(".");
        }
    }
  }
}

//funzione per far connettere l'arduino al WiFi
void connectWiFiNetwork() {
  if (WiFi.status() == WL_NO_MODULE) {
    if (DEBUG) Serial.println("ERROR: Communication with WiFi module failed.");
    while (true);
  } else {
    if (DEBUG) Serial.println("WiFi module found.");
  }
  if (DEBUG) {
    String version = WiFi.firmwareVersion();
    if (version < WIFI_FIRMWARE_LATEST_VERSION) {
        Serial.println("Please upgrade the WiFi module firmware.");
    }
  }
  if (DEBUG) {
    Serial.print("Connecting to WiFi network (");
    Serial.print(WiFiSSID);
    Serial.print(")...");
  }
  WiFi.begin(WiFiSSID, WiFiPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    if (DEBUG) Serial.print(".");
  }
  if (DEBUG) Serial.println("connected.");
}

//funzione che prende una stringa nel seguente formato "['names', 'Electronics', 'Phone', 'confidences', '99.97299194335938', '97.9323764335939']"
// e popola l'array substrings con tutte le stringhe comprese tra due apici
void ottieniSubstringsTraApici(const char* s, char* substrings[], int& count) {
  count = 0;
  bool in_quotes = false;
  const char* start = nullptr;

  while (*s != '\0') {
    if (*s == '\'') {
      if (!in_quotes) {
        in_quotes = true;
        start = s + 1;
      }
      else {
        int len = s - start;
        if (len > 0) {
          substrings[count] = new char[len + 1];
          strncpy(substrings[count], start, len);
          substrings[count][len] = '\0';
          count++;
        }
        in_quotes = false;
      }
    }
    s++;
  }
}

//funzione per riprodurre una canzone attraverso il piezo
void musica(int melody[], int durations[], int sizeDurations) {
  int size = sizeDurations / sizeof(int);

  for (int note = 0; note < size; note++) {
    //to calculate the note duration, take one second divided by the note type.
    //e.g. quarter note = 1000 / 4, eighth note = 1000/8, etc.
    int duration = 1000 / durations[note];
    tone(BUZZER_PIN, melody[note], duration);

    //to distinguish the notes, set a minimum time between them.
    //the note's duration + 30% seems to work well:
    int pauseBetweenNotes = duration * 1.30;
    delay(pauseBetweenNotes);
    
    //stop the tone playing:
    noTone(BUZZER_PIN);
  }
}

//funzione per trasmettere le informazioni allo slave in modo da mostrarle sullo schermo LCD
void trasmetti(String testo){
  Wire.beginTransmission(9);
  Wire.print(testo);
  Wire.endTransmission();
}

//quando il raspberry pubblica sulla propria "newsletter" il risultato dell'elaborazione dell'immagine, l'arduino riceve il comando e esegue la seguente funzione
void mqttCommandReceived(char* topic, byte* payload, unsigned int length) {
  
  //questo controllo serve per far sì che l'arduino all'accensione non stampi il risultato dell'ultima elaborazione
  if(flagRichiesta == true){
    
    //leggo il messaggio
    char message[length + 1];
    for (unsigned int i = 0; i < length; i++) {
      message[i] = (char)payload[i];
    }
    message[length] = '\0';
    //il messaggio è una stringa del tipo "['names', 'Electronics', 'Phone', 'confidences', '99.97299194335938', '97.9323764335939']"

    //questo controllo serve per far sì che l'arduino non stampi due volte il risultato dell'elaborazione
    if(messaggio != message){
      //salvo il nuovo messaggio per confrontarlo con il prossimo
      messaggio = message;
      //accendo il LED GIALLO e spengo il LED VERDE per segnalare che lo slave sta mostrando il risultato dell'elaborazione sullo schermo LCD
      digitalWrite(LED_VERDE, LOW);
      digitalWrite(LED_GIALLO, HIGH);

      char* substrings[120];

      //count conterrà il numero di substrings trovate
      int count = 0;

      //richiamo la funzione per popolare l'array substrings con il risultato dell'elaborazione della foto
      ottieniSubstringsTraApici(message, substrings, count);

      //trovo il numero di coppie, per come è strutturato il messaggio se ho 16 substrings significa che ho 8 coppie,
      // dove le prime 8 sotto stringhe rappresentano i nomi e le rimanenti 8 rappresentano il grado di confidenza
      //effetive ho 7 coppie in quanto la prima coppia è data dalle stringhe 'names' e 'confidences'
      int numero_coppie = (count/2);

      //variabile in cui viene salvata, se trovato, l'indice della stringa 'Autorizzato' o 'Non autorizzato'
      int indice = 0;
      //variabile in cui viene salvata il numero di coppie da mostrare meno 1
      int indice_max = 11;

      //scorro le prime 10 coppie, in modo da far sì che se nella foto è stata ritratta una persona,  
      //la prima stringa stampata dallo slave, indichi se la persona è autorizzata o meno, e poi riproduco una melodia sul piezo a seconda dei permessi della persona
      //salto la coppia con indice 0 poichè sarebbe la coppia di stringhe 'names' e 'confidences'
      for (unsigned int i = 1; i < 11; i++) {
        //se la foto ritraeva una persona autorizzata
        if (String(substrings[i]).equals("Autorizzato")) {
          digitalWrite(LED_BLU, HIGH);//accendo il LED BLU
          trasmetti("Authorized");
          musica(melodyStarWars, durationsStarWars, sizeof(durationsStarWars));//riproduco la melodia per gli utenti autorizzati
          trasmetti("");
          indice = i;//salvo l'indice in modo da non ristampare i permessi della persona nel secondo ciclo
        } else if (String(substrings[i]).equals("Non autorizzato")) {//se la foto ritraeva una persona non autorizzata 
          digitalWrite(LED_ROSSO, HIGH);//accendo il LED ROSSO
          trasmetti("Not Authorized");
          musica(melodyDoom, durationsDoom, sizeof(durationsDoom));//riproduco la melodia per gli utenti non autorizzati
          trasmetti("");
          indice = i;//salvo l'indice in modo da non ristampare i permessi della persona nel secondo ciclo
        }

        //nel raro caso in cui il servizio restituisce meno di 10 coppie
        if (String(substrings[i]).equals("confidences")) {
          indice_max = i;//salvo l'indice fino della stringa 'confidences' in modo da non stamparla e stampare un numero di coppie pari a indice_max meno 1
        }
      }

      //trasmetto allo slave il risultato dell'elaborazione della foto, in particolare i primi 10 oggetti individuati e il loro grado di confidenza, i parte da 0 per evitare di mostrare la coppia 'names' e 'confidences'
      //se il numero di coppie è minore di 10, verranno stampate le prime indice_max meno uno coppie
      for (unsigned int i = 1; i < indice_max; i++) {
        if(i != indice) {//se la foto non ritraeva una persona, stampo tutte e 10 le coppie, altrimenti ne stampo 9 evitando di stampare il grado di autorizzazione della persona, in quanto stampato in precedenza
          trasmetti(substrings[i]);//trasmetto il nome dell'oggetto individuato
          delay(500);
          trasmetti(substrings[i + numero_coppie]);//trasmetto il grado di confidenza dell'oggetto di cui ho appena stampato il nome
          delay(2000);
        }
      }

      delay(3000);
      trasmetti("Waiting...");
      flagAttesa = true;
      //spengo il LED BLU o il LED ROSSO se la foto ritraeva una persona
      digitalWrite(LED_BLU,    LOW);
      digitalWrite(LED_ROSSO,  LOW);
      //spengo il LED GIALLO per segnalare la fine della stampa dell'elaborazione della foto
      digitalWrite(LED_GIALLO, LOW);
      //accendo il LED VERDE per indicare posso procedere con una nuova richiesta
      digitalWrite(LED_VERDE,  HIGH);

      if (DEBUG) {
        Serial.print("Command received: ");
        Serial.print(topic);
        Serial.print(" ");
        Serial.println(message);
      }
    }
  }
  else{
    flagRichiesta = true;
  }
}

//per iscriversi allo stream data del raspberry
void mqttSubscribeToCommands() {
  char topic[sizeof("RaspberryPi") + sizeof("/data/photo")] = "";
  strcat(topic, "RaspberryPi");
  strcat(topic, "/data/photo");
  if (mqttClient.subscribe(topic, 1)) {
    if (DEBUG) {
      Serial.print("Subscribed to topic: ");
      Serial.println(topic);
    }
  }
  else {
    if (DEBUG) {
      Serial.print("ERROR: Unable to subscribe to topic: ");
      Serial.println(topic);
    }
  }
}

//funzione che pubblica lo stato del bottone nella "newsletter" dell'arduino
void mqttPublishScattaFoto() {
  char topic[sizeof(MQTTClientID) + sizeof("/command/photo")] = "";
  strcat(topic, MQTTClientID);
  strcat(topic, "/command/photo");
  char status[4] = "";
  strcpy(status, lastButtonState == HIGH ? "on" : "off");
  
  //pubblico lo stato del bottone solo quando viene premuto
  if(strcmp(status, "on") == 0){
    if (mqttClient.publish(topic, status, true)) {

      //controllo per far sì che le informazioni vengano stampate correttamente,
      //poichè se il flagAttesa è settato a true significa che il messaggio 'In attesa...', viene stampato alla fine di ogni elaborazione
      if(flagAttesa == true){
        trasmetti("");//allora trasmetto "" per resettare lo schermo
      }

      //per resettare lo schermo
      trasmetti("");
      trasmetti("");
      
      //stampo il messaggio 'Photo sent!'
      trasmetti("Photo sent!");
      trasmetti("");

      if (DEBUG) {
        Serial.print("Status published: ");
        Serial.print(topic);
        Serial.print(" ");
        Serial.println(status);
      }
    }
    else {
      if (DEBUG) Serial.println("ERROR: Unable to publish command take photo.");
    }
  }
}

//funzione nel caso in cui il bottone venga premuto, per far sì che l'arduino pubblichi nella propria "newsletter" lo stato del bottone
void controllaStatoBottone() {
  byte buttonState = digitalRead(BUTTON_PIN);
  if (buttonState != lastButtonState) {
    lastButtonState = buttonState;
    mqttPublishScattaFoto();
  }
}

void setup() {
  Wire.begin(); 
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_GIALLO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_BLU,   OUTPUT);
  pinMode(LED_ROSSO, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_VERDE, HIGH);
  if (DEBUG) configureSerialMonitor();
  configureMQTTClient();
  connectWiFiNetwork();
  connectMQTTBroker();
  mqttSubscribeToCommands();
}

void loop() {
  if (!mqttClient.connected()) {
    connectMQTTBroker();
    mqttSubscribeToCommands();
  }
  //controllo continuamente se il bottone viene premuto o meno
  controllaStatoBottone();
  mqttClient.loop();
}

