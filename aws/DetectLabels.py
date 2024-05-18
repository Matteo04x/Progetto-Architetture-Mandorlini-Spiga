import json
import base64
import boto3
import uuid
from datetime import datetime

#connessione ai servizi AWS
rekognition = boto3.client('rekognition', region_name='eu-west-2')
dynamodb = boto3.resource('dynamodb', region_name='eu-west-2')
table_name = 'DetectLabelsTable'
table = dynamodb.Table(table_name)

def lambda_handler(event, context):

    #ricevo l'immagine in base64 e la passo alla funzione detect_labels per l'analisi
    response = detect_labels(event['body']) 
    
    #salvo la risposta dell'analisi su dynamodb
    status_code, image_uuid = save_detected_labels(response)
    
    
    if(status_code == 200):
        #se è andato tutto a buon fine ricavo i nomi e i gradi di affidabilità delle labels dell'ultima immagine analizzata e le invio al raspberry
        latest_names, latest_confidences = get_all_names_confidences(image_uuid)
        
        if not latest_names and not latest_confidences:
            body = "Data not found"
        else:  
            body = {
                'names': latest_names,
                'confidences': latest_confidences
            }
    else:
        body = "Some errors occurred"
    
    #risposta da inviare al raspberry
    return {
        'statusCode': status_code,
        'body': json.dumps(body)
    }
    
    
def detect_labels(image_base64):
    #decodifico l'immagine nel formato base64-encoded bytes e ritorno l'analisi di rekognition 
    image_bytes = base64.b64decode(image_base64)
    return rekognition.detect_labels(Image={'Bytes': image_bytes})


def save_detected_labels(data):
    
    #genero un id univoco per l'analisi dell'immagine
    image_uuid = uuid.uuid4()
    
    #traformo la data in cui è stata fatta la richiesta dal raspberry in timestap
    date = data["ResponseMetadata"]["HTTPHeaders"]["date"]
    date_format = "%a, %d %b %Y %H:%M:%S %Z"
    datetime_object = datetime.strptime(date, date_format)
    timestamp = datetime_object.timestamp()
    
        
    name = []
    confidence = []
    categories = []
    parents = []
    
    #popolo le liste con i dati da salvare
    for item in data["Labels"]:
        name.append(item["Name"])
        confidence.append(str(item["Confidence"]))
        categories.append(item["Categories"])
        parents.append(item["Parents"])
            
    #salvo i dati su un'apposita tabella dynamodb   
    res = table.put_item(
        TableName=table_name,
        Item={
            'detection_id':str(image_uuid),
            'nome': name,
            'accuratezza': confidence,
            'categorie': categories,
            'parents_label': parents,
            'timestamp': str(timestamp)
        }
        
    )
    
    #ritorno anche l'image_uuid per poi poter ricavare i dati dell'ultima analisi effettuata
    return res["ResponseMetadata"]["HTTPStatusCode"], image_uuid
      
     
def get_all_names_confidences(image_uuid):
    
    #prendo i dati dell'ultima analisi
    response = table.scan(
        TableName=table_name,
        FilterExpression='detection_id= :image_uuid',
        ExpressionAttributeValues={
            ':image_uuid': str(image_uuid),
        },
    )
    
    #filtro le informazioni che mi servono, ovvero nome e accuratezza e le restituisco
    if 'Items' in response and len(response['Items']) > 0:
        latest_item = response['Items'][0]
        return latest_item['nome'], latest_item['accuratezza']
    else:
        return [], []
