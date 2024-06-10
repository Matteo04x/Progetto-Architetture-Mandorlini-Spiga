# Progetto Mandorlini-Spiga
============  
  
Come fare:  
1) Assemblate il circuito;
2) Scaricate la cartella aws;  
3) Caricate i file DetectLables.py e FaceAuth.py su due lambda function distinte;
4) Configurate tre REST API utilizzando Amazon API Gateway: una con metodo POST, una con metodo GET per l'ultima con metodo PUT;
5) Impostate le due API come trigger per le relative funzioni;
6) Create due bucket S3: uno per caricare manualmente le immagini degli utenti autorizzati e un altro per le immagini scattate contenenti persone;
7) Create due tabelle DynamoDB: una per salvare i risultati delle operazioni di rilevamento etichette e un'altra, da popolare manualmente, per memorizzare gli UUID associati alle immagini degli     utenti;
8) Installate la CLI di AWS e usato il comando "aws rekognition create-collection --collection-id MyFaceCollection" sostituendo MyFaceCollection con il nome che volete;
9) Scaricate il codice Master.ino e Slave.ino;  
10) Aprite il codice nell'Arduino IDE;  
11) Connettete le schede Arduino al pc;  
12) Caricate il codice;  
13) Installate il Broker MQTT sul Raspberry;  
14) Scaricate la cartella Raspberry;  
15) Caricate la cartella sul Raspberry;  
16) Eseguite il comando python3 mqtt_client.py;  
12) Se avete dubbi consultate la Wiki;  
13) Divertitevi.  
