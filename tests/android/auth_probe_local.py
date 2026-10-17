"""Small interoperability probe using librespot's protobuf schema. Never prints tokens."""
import base64
import json
from pathlib import Path
import urllib.request
import urllib.error
import uuid

KEYMASTER='65b708073fc0480ea92a077233ca87bd'
ANDROID='9a8d2f0ce77a4e248bb71fefcb557637'
def varint(value):
    result=bytearray()
    while value>127: result.append((value&127)|128);value>>=7
    result.append(value);return bytes(result)
def integer(number,value):return varint(number<<3)+varint(value)
def field(number,value):
    if isinstance(value,str):value=value.encode()
    return varint((number<<3)|2)+varint(len(value))+value
def decode(data):
    def read(position):
        value=shift=0
        while True:
            byte=data[position];position+=1;value|=(byte&127)<<shift
            if byte<128:return value,position
            shift+=7
    result={};position=0
    while position<len(data):
        key,position=read(position);number,wire=key>>3,key&7
        value,position=read(position)
        if wire==2:value,position=data[position:position+value],position+value
        elif wire!=0:raise ValueError('Unsupported wire type')
        result.setdefault(number,[]).append(value)
    return result
def post(url,body,ua,headers=None):
    headers={'Accept':'application/x-protobuf','Content-Type':'application/x-protobuf','User-Agent':ua,**(headers or {})}
    try:
        with urllib.request.urlopen(urllib.request.Request(url,body,headers),timeout=20) as response:return response.status,response.read()
    except urllib.error.HTTPError as error:return error.code,error.read()

def probe(label,client,platform):
    device=str(uuid.uuid4())
    if platform=='Android':
        details=field(1,field(2,'16')+integer(3,31)+field(4,'Pixel')+field(5,'GF5KQ')+field(6,'Google'))
        version='8.9.82.620';ua='Spotify/8.9.82.620 Android/16 (librespot)'
    else:
        details=field(5,field(1,'Linux')+field(2,'6.12.0')+field(3,'0')+field(4,'x86_64'))
        version='1.2.52.442';ua='Spotify/124200290 Linux/0 (librespot)'
    body=integer(1,1)+field(2,field(1,version)+field(2,client)+field(3,field(1,details)+field(2,device)))
    code,response=post('https://clienttoken.spotify.com/v1/clienttoken',body,ua)
    result={'profile':label,'client_token_http':code}
    if code==200:
        parsed=decode(response);result['client_token_response_type']=parsed.get(1,[0])[0]
        if result['client_token_response_type']==1:
            client_token=decode(parsed[2][0])[1][0].decode()
            private=Path('out/android/session-private/test-auth.json')
            if private.exists():
                credentials=json.loads(private.read_text(encoding='utf-8-sig'))['credentials']
                login=field(1,field(1,KEYMASTER)+field(2,device))+field(100,field(1,credentials['username'])+field(2,base64.b64decode(credentials['auth_data'])))
                status,reply=post('https://login5.spotify.com/v3/login',login,ua,{'Client-Token':client_token})
                result['login5_http']=status
                if status==200:
                    parsed_login=decode(reply)
                    result['login5_ok']=1 in parsed_login
                    result['login5_error']=parsed_login.get(2,[None])[0]
                    result['login5_challenge']=3 in parsed_login
    else:
        # This request carries no user secrets. Return only the protobuf error's
        # human-readable message, never success payloads or token strings.
        try:result['client_error']=decode(response)[1][0].decode()[:200]
        except Exception:result['client_error_format']='unrecognized'
    print(json.dumps(result),flush=True)

if __name__=='__main__':
    probe('current-apk-Android-Keymaster',KEYMASTER,'Android')
    probe('upstream-Android-client',ANDROID,'Android')
    probe('desktop-OAuth-compatibility',KEYMASTER,'Linux')
