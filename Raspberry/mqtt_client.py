import re
import cv2
import uuid
import json
import base64
import requests
from io import StringIO
from time import sleep
import board
from digitalio import DigitalInOut, Direction
import microcontroller
import datetime
import adafruit_logging as logger
import adafruit_minimqtt.adafruit_minimqtt as MQTT

try:
    IS_RASPBERRY_PI_SBC = True
except:
    IS_RASPBERRY_PI_SBC = False
if IS_RASPBERRY_PI_SBC:
    import socket
elif board.board_id == "raspberry_pi_pico_w":
    import socketpool
    import wifi

try:
    import segreti
except ImportError:
    print("WiFi and MQTT secrets are stored in segreti.py.")
    raise


DEBUG: int = 1

mqtt_client: MQTT = None


def mqtt_connected(client: MQTT, userdata: any, flags: int, rc: int) -> None:
    if DEBUG:
        print(f"\nBroker connected: {client.broker}")

def mqtt_disconnected(client: MQTT, userdata: any, rc: int) -> None:
    if DEBUG:
        print(f"Broker disconnected: {client.broker}")

def mqtt_message_received(client: MQTT, topic: str, message: str) -> None:
    if DEBUG:
        print(f"Command received: {topic} {message}")
        print("ERROR: Unknown command.")

def mqtt_subscribed(client: MQTT, userdata: any, topic: str, granted_qos: int) -> None:
    if DEBUG:
        print(f"Subscribed to topic: {topic}")

def mqtt_unsubscribed(client: MQTT, userdata: any, topic: str, pid: int) -> None:
    if DEBUG:
        print(f"Unsubscribed from topic: {topic}")

def configure_mqtt_client() -> None:
    global mqtt_client
    if IS_RASPBERRY_PI_SBC:
        socket_pool = socket
    if board.board_id == "raspberry_pi_pico_w":
        wifi.radio.connect(segreti.wifi["ssid"], segreti.wifi["password"])
        socket_pool = socketpool.SocketPool(wifi.radio)
    mqtt_client = MQTT.MQTT(
        broker=segreti.mqtt["broker_url"],
        username=segreti.mqtt["broker_username"],
        password=segreti.mqtt["broker_password"],
        client_id=segreti.mqtt["client_id"],
        socket_pool=socket_pool
    )
    if DEBUG == 2:
        mqtt_client.enable_logger(logger, log_level=logger.DEBUG)
    mqtt_client.on_connect     = mqtt_connected
    mqtt_client.on_disconnect  = mqtt_disconnected
    mqtt_client.on_message     = mqtt_message_received
    mqtt_client.on_subscribe   = mqtt_subscribed
    mqtt_client.on_unsubscribe = mqtt_unsubscribed

    mqtt_client.add_topic_callback(
        f"Arduino/command/photo",
        mqtt_command_photo_received
    )

def connect_mqtt_broker() -> None:
    if DEBUG:
        print("Connecting to MQTT broker...", end="")
    while mqtt_client.connect():
        if DEBUG:
            print(".", end="")
        sleep(1)

def scattaFoto():
    
    #inizializzo la webcam
    cam = cv2.VideoCapture(0)

    #scatto la foto
    ret, frame = cam.read()
    if not ret:
        print('failed to grab frame')
    
    #rilascio la webcam
    cam.release()

    #il nome dell'immagine è la data e l'ora al momento dello scatto
    data_ora =datetime.datetime.now()  
    img_name = '{}.jpg'.format(data_ora)
    
    #salvo la foto nella cartella images
    cv2.imwrite('images/' + img_name, frame)


    if DEBUG:#se il debug è attivo segnalo di aver salvato l'immagine correttamente
        print('{} saved!'.format(img_name))

    #leggo l'immagine appena salvata
    with open('images/' + img_name, 'rb') as f:
        img_data = f.read()
    
    
    #converte l'immagine in una stringa base64
    image_base64 = base64.b64encode(img_data).decode("utf8")


    url = 'https://oy09g7odf7.execute-api.eu-west-2.amazonaws.com/dev/detect_labels'        

    #eseguo la prima richiesta al servizio Amazon Rekognition
    response = requests.post(url, data=image_base64)

    #se la richiesta ha esito positivo
    if int(response.status_code) == 200:
        
        #estraggo l'elaborazione 
        contenuto = response._content

        #ottengo tutte i nomi degli oggetti trovati e il rispettivo grado di confidenza
        names_confidences = re.findall(r'"(.*?)"', str(contenuto))
        
        #se nella foto è ritratta una persona
        if "Person" in names_confidences: 
            
            base64WithoutPrefix = image_base64.replace('data:image/jpeg;base64,', '')
            
            #decodifico l'immagine che ho codificato in base64 in precedenza
            image_bytes = base64.b64decode(base64WithoutPrefix)
            
            #genero un'id univoco per l'immagine
            user_image_name = str(uuid.uuid4())

            url = f'https://7zkgxx1i53.execute-api.eu-west-2.amazonaws.com/dev/input-faces/{user_image_name}.jpg'
            
            #eseguo la richiesta per salvare l'immagine
            requests.put(url, headers={'Content-Type': 'image/jpeg'}, data=image_bytes)
    
            params = {'objectKey': f'{user_image_name}.jpg'}
            
            request_url = f'https://7zkgxx1i53.execute-api.eu-west-2.amazonaws.com/dev/inputface'

            #eseguo la richiesta per elaborare l'immagine appena salvata, per determinare se la persona ritratta è autorizzata o meno
            response = requests.get(request_url, headers={'Accept': 'application/json', 'Content-Type': 'application/json'}, params = params)
           
            #ottengo la risposta del servizio
            message = response._content
            
            #la converto in un dictionary
            message = json.loads(message.decode('utf-8'))
            
            #se la risposta dell'elaborazione è presente
            if 'Message' in message:
                #sostituisco nella stringa di nomi e gradi di confidenza alla sottostringa Person il grado di autorizzazione
                names_confidences = str(names_confidences).replace("Person", message['Message'])
        
        #pubblico il risultato dell'elaborazione sullo stream data del raspberry
        mqtt_publish_data_photo(names_confidences)

#funzione per pubblicare il risultato dell'elaborazione della foto sullo stream data del raspberry
def mqtt_publish_data_photo(names_confidences) -> None:
    mqtt_client.publish(
        f"{segreti.mqtt['client_id']}/data/photo",
        str(names_confidences),
        retain=True
    )
    if DEBUG:
        print(f"Status published: {segreti.mqtt['client_id']}/data/photo {str(names_confidences)}")

#funzione per far sì che quando viene premuto il bottone sull'arduino, venga scattata una foto sul raspberry, elaborata dai servizi amazon e poi pubblicato il risultato dell'elaborazione sullo stream data del raspberry 
def mqtt_command_photo_received(client: MQTT, topic: str, message: str) -> None:
    if DEBUG:
        print(f"Command received: {topic} {message}")
    
    #se il bottone è stato premuto, scatta una foto e inviala ai servizi AWS
    if message in ("on", "off"):
        if message == "on":
            scattaFoto()
    else:
        if DEBUG:
            print("ERROR: Unknown command.")

def main() -> None:
    def loop() -> None:
        if not mqtt_client.is_connected():
            mqtt_client.reconnect()
        mqtt_client.loop(1)

    if DEBUG:
        print("Running in DEBUG mode.  Turn off for normal operation.")
    if IS_RASPBERRY_PI_SBC:
        print("Press CTRL-C to exit.")
    configure_mqtt_client()
    connect_mqtt_broker()
    
    #mi iscrivo allo stream command dell'arduino
    mqtt_client.subscribe(f"Arduino/command/#", qos=1)

    if IS_RASPBERRY_PI_SBC:
        try:
            while True:
                loop()
        except KeyboardInterrupt:
            print()
        finally:
            mqtt_client.unsubscribe(f"Arduino/command/#")
            mqtt_client.disconnect()
    elif board.board_id == "raspberry_pi_pico_w":
        while True:
            loop()
    else:
        print(f"ERROR: The {board.board_id} board is not supported.")

if __name__ == "__main__":
    main()



