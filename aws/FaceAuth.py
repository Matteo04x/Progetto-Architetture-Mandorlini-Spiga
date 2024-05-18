import boto3
import json

#connessione con i servizi aws
s3 = boto3.client('s3')
rekognition = boto3.client('rekognition', region_name='eu-west-2')
dynamodb = boto3.resource('dynamodb', region_name='eu-west-2')

#tabella dynamodb in cui sono salvati gli uuid associati alle immagini dei nostri volti
dynamodbTableName = 'sourceFaces'
sourceFacesTable = dynamodb.Table(dynamodbTableName)

#bucket s3 in cui vengono salvate le foto scattate
bucket_name = 'input-faces'

#lambda function che viene eseguita appena viene scattata una foto
def lambda_handler(event,context):
    
    #salvo l'immagine arrivata tramite url params
    objectKey = event['queryStringParameters']['objectKey']
    
    #trasformo immagine in bytes per essere usata da rekognition
    image_bytes = s3.get_object(Bucket=bucket_name,Key=objectKey)['Body'].read() 
    
    #uso rekognition per effettuare il riconoscimento facciale
    response = rekognition.search_faces_by_image(
        CollectionId='faces',
        Image={'Bytes':image_bytes}
    )

    #per ogni possibile match effettuato vedo se c'è una corrispondenza dell'indice dell'immagine nella tabella sourceFacesTable
    for match in response['FaceMatches']:
        face = sourceFacesTable.get_item(
            Key={
                'rekognitionid': match['Face']['FaceId']
            }
        )
        # se c'è vuoldire che il riconoscimento è avvenuto con successo
        if 'Item' in face: 
            return build_response(200, {'Message':'Autorizzato'} )
        
    return build_response(401,{'Message':'Non autorizzato'})
        
def build_response(status_code,body=None):
    response = {
        'statusCode':status_code,
        'headers':{
            'Content-Type':'application/json',
            'Access-Control-Allow-Origin':'*',
        }
    }
    
    if body is not None:
        response['body'] = json.dumps(body)
        
    return response
