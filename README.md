# Progetto Mandorlini-Spiga
============  
  
Come fare:  
1) Assemblate il circuito;  
2) Scaricate il codice Master.ino e Slave.ino;  
3) Aprite il codice nell'Arduino IDE;  
4) Connettete le schede Arduino al pc;  
5) Caricate il codice;  
6) Installate il Broker MQTT sul Raspberry;  
7) Scaricate la cartella Raspberry;  
8) Caricate la cartella sul Raspberry;  
9) Eseguite il comando python3 mqtt_client.py;  
10) Scaricate la cartella aws;  
11) Caricate i file DetectLables.py e FaceAuth.py su due lambda function distinte;
12) Configurate due REST API utilizzando Amazon API Gateway e create un metodo POST per la prima funzione e un metodo GET per l'altra.
13) Impostate le due API come trigger per le relative funzioni
14) Create un bucket S3 e caricate le immagini degli utenti autorizzati
15) Create due tabelle DynamoDB, una per salvare i risultati delle operazioni di labels detecting e l'altra, da popolare manualmente, per salvare gli UUID associati alle immagini degli utenti
16) Installate la CLI di AWS e usato il comando "aws rekognition create-collection --collection-id MyFaceCollection" sostituendo MyFaceCollection con il nome che volete 
12) Se avete dubbi consultate la Wiki;  
13) Divertitevi.  
