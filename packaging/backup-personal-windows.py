"""Consistent private Windows library backup, deliberately excluding credentials."""
import argparse, hashlib, json, os, sqlite3, winreg
from pathlib import Path

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    assert os.name=='nt','This helper uses Windows app data paths'
    destination=args.output.resolve()
    destination.mkdir(parents=True,exist_ok=False)
    local=Path(os.environ['LOCALAPPDATA'])
    entries=[]
    for app in ['Tonal','IndieTonal','TonalHub']:
        root=local/'Tonal'/app
        db=root/'personal/music-library.sqlite'
        if db.is_file():
            target=destination/app/'personal/music-library.sqlite'
            target.parent.mkdir(parents=True,exist_ok=True)
            source=sqlite3.connect(db.as_uri()+'?mode=ro',uri=True)
            output=sqlite3.connect(target)
            try:
                source.backup(output)
                assert output.execute('PRAGMA integrity_check').fetchone()[0]=='ok'
            finally:output.close();source.close()
            entries.append(str(target.relative_to(destination)))
        preferences={}
        for group,names in {'player':['volume'],'librespot':['bitrate'],'ui':['language']}.items():
            try:
                with winreg.OpenKey(winreg.HKEY_CURRENT_USER,'Software\\Tonal\\'+app+'\\'+group) as key:
                    for name in names:
                        try:
                            value,kind=winreg.QueryValueEx(key,name)
                            if kind in (winreg.REG_SZ,winreg.REG_DWORD):preferences[group+'/'+name]=value
                        except FileNotFoundError:pass
            except FileNotFoundError:pass
        if preferences:
            target=destination/app/'preferences.json';target.parent.mkdir(parents=True,exist_ok=True)
            target.write_text(json.dumps(preferences,ensure_ascii=False,indent=2),encoding='utf-8')
            entries.append(str(target.relative_to(destination)))
        settings=root/'audio.json'
        if settings.is_file():
            target=destination/app/'audio.json';target.parent.mkdir(parents=True,exist_ok=True)
            # Serialize only DSP settings, never arbitrary account/config files.
            allowed={'schemaVersion','enabled','eq','eqEnabled','preset','bass','compressor','normalization','targetLufs','ceilingDb','spatial'}
            data=json.loads(settings.read_text(encoding='utf-8-sig'))
            target.write_text(json.dumps({k:v for k,v in data.items() if k in allowed},ensure_ascii=False,indent=2),encoding='utf-8')
            entries.append(str(target.relative_to(destination)))
    manifest=dict(format='tonal-windows-personal-backup-v1',files={name:hashlib.sha256((destination/name).read_bytes()).hexdigest() for name in entries},
        restore='Feche os players. Preserve os dados atuais. Restaure o SQLite e audio.json no mesmo caminho relativo sob %LOCALAPPDATA%/Tonal. preferences.json documenta valores do registro: reaplique idioma/volume/bitrate nas opções do app, não basta copiar esse JSON. Prefira Exportar/Importar no app para troca entre plataformas.',
        scope='SQLite pessoal consistente, configuração DSP e preferências permitidas existentes neste PC. Não inclui arquivos de música externos, fotos referenciadas, dados do telefone físico, Windows Vault, tokens, cache de login, keystores ou chave de assinatura. Exporte separadamente a biblioteca pelo app no telefone.')
    (destination/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(dict(files=len(entries),directory=str(destination)),ensure_ascii=False))

if __name__=='__main__':main()
