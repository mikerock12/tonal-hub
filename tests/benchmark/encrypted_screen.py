"""Capture a screen into an encrypted envelope. No plaintext image is written."""
import base64,io,json,os,sys
from pathlib import Path
from PIL import ImageGrab
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
public=serialization.load_pem_public_key(Path('tests/benchmark/live-spotify-public.pem').read_bytes())
buffer=io.BytesIO();ImageGrab.grab().save(buffer,format='PNG')
key=AESGCM.generate_key(bit_length=256);nonce=os.urandom(12)
cipher=AESGCM(key).encrypt(nonce,buffer.getvalue(),None)
wrapped=public.encrypt(key,padding.OAEP(mgf=padding.MGF1(algorithm=hashes.SHA256()),algorithm=hashes.SHA256(),label=None))
encode=lambda b:base64.b64encode(b).decode('ascii')
Path(sys.argv[1]).write_text(json.dumps({'key':encode(wrapped),'nonce':encode(nonce),'cipher':encode(cipher)}))
print('Encrypted screen ready')
