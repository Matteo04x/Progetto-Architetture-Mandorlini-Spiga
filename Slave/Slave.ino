//includo la libreria Wire per la comunicazione con il master, e la libreria LiquidCrystal per stampare i messaggi sullo schermo LCD
#include <Wire.h>
#include <LiquidCrystal.h>

String  wire_in;
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

//flag utilizzato per tenere traccia del numero di messaggi inviato, in modo da stampare il primo messaggio sulla riga superiore dello schermo LCD e il secondo messaggio sulla riga inferiore
bool flag = true;

void loop() {}

void receiveEvent(int howMany) {
  while(Wire.available() > 0) {
    char c = Wire.read();
    wire_in.concat(c);
    
    lcd.print(c);
    delay(10);
  }
  delay(100000);

  if(flag == true){//dopo il primo messaggio posiziono il cursore all'inizio della seconda riga
    lcd.setCursor(0, 1);
    flag = false;
  }
  else{//dopo il secondo messaggio resetto lo schermo LCD
    lcd.clear();
    flag = true;
  } 
}

void setup() {
  //inizializzo le righe e le colonne dello schermo LCD
  lcd.begin(16, 2);
  //stampo una scritta di default sullo schermo LCD, mentre attendo che l'utente inivii una richiesta
  lcd.print("Project:");
  lcd.setCursor(0, 1);
  lcd.print("Mandorlini-Spiga");
  lcd.setCursor(0, 0);
  Wire.begin(9); 
  //quando viene ricevuto qualcosa viene esaguita la funzione receiveEvent
  Wire.onReceive(receiveEvent);
  Serial.begin(9600);
}