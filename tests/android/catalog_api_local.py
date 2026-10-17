"""Probe Spotify's real paging/schema contract with the authorized, ignored local seed.
Only public catalog IDs, HTTP status and item counts enter the report.
"""
import json
from pathlib import Path
from urllib.request import Request, urlopen
from urllib.error import HTTPError
from urllib.parse import urlencode, urlparse

root=Path(__file__).resolve().parents[2]
seed=json.loads((root/'out/android/session-private/test-auth.json').read_text(encoding='utf-8-sig'))
token=seed['tokens']['access_token']
checks=[]
def get(path,expected=200):
    url=path if path.startswith('https://') else 'https://api.spotify.com/v1/'+path
    parsed=urlparse(url)
    assert parsed.scheme=='https' and parsed.netloc=='api.spotify.com' and parsed.path.startswith('/v1/')
    try:
        with urlopen(Request(url,headers={'Authorization':'Bearer '+token}),timeout=20) as response:
            status=response.status;data=json.load(response)
    except HTTPError as e:
        status=e.code;data=json.load(e)
    assert status==expected,(path,status)
    checks.append(dict(endpoint=parsed.path,status=status,passed=True))
    return data
def page(path,key=None,wrapper=None):
    data=get(path);group=data[key] if key else data
    values=[(item.get(wrapper) or item.get('item')) if wrapper else item for item in group.get('items',[]) if item]
    values=[v for v in values if v]
    checks[-1]['count']=len(values)
    if group.get('next'):
        more=get(group['next']);more=more[key] if key and key in more else more
        assert isinstance(more.get('items'),list)
        checks[-1]['count']=len(more['items']);checks[-1]['pagination']=True
    return values

get('search?'+urlencode(dict(q='Veruca Salt',type='track',limit=20)),expected=400)
search=get('search?'+urlencode(dict(q='Veruca Salt',type='track,artist,album,playlist',limit=10)))
assert all(search[k]['items'] for k in ['tracks','artists','albums','playlists'])
artist=next(a for a in search['artists']['items'] if a and a['name']=='Veruca Salt')
get('artists/'+artist['id'])
albums=page('artists/'+artist['id']+'/albums?include_groups=album&limit=10')
album=next(a for a in albums if a['name']=='American Thighs')
get('albums/'+album['id']);tracks=page('albums/'+album['id']+'/tracks?limit=10');assert tracks
page('search?'+urlencode(dict(q='Veruca Salt',type='track',limit=10)),'tracks')
playlists=page('me/playlists?limit=10');assert playlists
playlist=next((p for p in playlists if p['name']=='Girls Rock'),playlists[0])
get('playlists/'+playlist['id']);page('playlists/'+playlist['id']+'/items?limit=10',wrapper='item')
assert page('me/albums?limit=10',wrapper='album')
assert page('me/following?type=artist&limit=10','artists')
assert page('me/tracks?limit=10',wrapper='track')
assert get('me/player/recently-played?limit=10')['items']
empty=get('search?'+urlencode(dict(q='isrc:ZZZZZ0000000',type='track,artist,album,playlist',limit=10)))
assert not any(empty[k]['items'] for k in ['tracks','artists','albums','playlists'])
report=dict(pass_all=True,version='0.2.0-local',checks=checks)
(root/'out/android/catalog-api-0.2.0.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
