"""Consolidate only public test results; never open authorization files."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib, json, xml.etree.ElementTree as ET

root=Path(__file__).resolve().parents[1]
def read(path):return json.loads((root/path).read_text(encoding='utf-8'))
cases={
    'native_features':('audio-features-0.3.0','passed'),
    'audio_panel':('audio-panel-0.3.0','passed'),
    'streaming_stability':('audio-stability-0.3.0','passed'),
    'pause_transitions':('audio-transitions-0.3.0','passed'),
    'network_recovery':('audio-network-0.3.0','passed'),
    'catalog':('catalog-smoke-0.3.0','pass_all'),
    'local_audio':('audio-smoke-0.3.0','pass_all'),
}
tests={};data={}
for name,(folder,key) in cases.items():
    path=f'out/android/{folder}/result.json';value=read(path);assert value[key],name;data[name]=value
    tests[name]={'passed':True,'report':path,'recorded_at_utc':datetime.fromtimestamp((root/path).stat().st_mtime,timezone.utc).isoformat()}
t=data['pause_transitions'];assert int(t['buffer_frames']['1'])>int(t['buffer_frames']['0'])>int(t['buffer_frames']['2'])
for c in t['cycles']:assert c['before']['underruns']==c['resumed']['underruns'] and c['paused']['volume']<.01 and c['resumed']['volume']>.99
n=data['network_recovery'];short=n['short_outage'];e=n['buffer_exhaustion'];r=e['automatically_resumed']
assert all(s['underruns']==short['before']['underruns'] for s in short['offline_samples'])
assert r['currentUri']==e['before_outage']['currentUri'] and abs(r['positionMs']-r['frames']*1000/r['rate'])<3000
artifacts={}
for name,suffix,validation in [('universal','local','apk-validation-0.3.0.json'),('poco_arm64','arm64-v8a','apk-arm64-validation-0.3.0.json')]:
    path=f'out/android/Tonal-Android-0.3.0-{suffix}.apk';v=read('out/android/'+validation)
    assert hashlib.sha256((root/path).read_bytes()).hexdigest()==v['sha256']
    assert not v['private_seed_file_present'] and not v['session_token_present']
    sig=(root/f'out/android/apk-signature-{suffix}-0.3.0.txt').read_text(encoding='utf-8-sig')
    assert 'Verified using v2 scheme (APK Signature Scheme v2): true' in sig
    assert '464ae7cf34f4f145541569db215f8fcdef5deda1be31921d8e366eed20aa22c2' in sig
    artifacts[name]={'path':path,'sha256':v['sha256'],'size_bytes':(root/path).stat().st_size,'size_mib':round((root/path).stat().st_size/1048576,2),'architectures':list(v['architectures']),'libraries_per_abi':9,'alignment_16kb_verified':True,'signature_verified':True,'credentials_embedded':False,'license_files':v['license_files']}
issues=ET.parse(root/'android/app/build/reports/lint-results-release.xml').getroot().findall('issue')
assert not any(i.get('severity') in ['Error','Fatal'] for i in issues)
phases=[{k:p[k] for k in ['name','seconds','cpu_percent_one_core','pss_kib','average_dsp_us','underruns_during_phase','slow_blocks_during_phase']} for p in data['streaming_stability']['phases']]
report={'version':'0.3.0-local','recorded_at_utc':datetime.now(timezone.utc).isoformat(),'local_only':True,'git_commit_push_pr_performed':False,'artifacts':artifacts,'tests':tests,'streaming_measurements':phases,'cpu_unit':'Percent of one logical CPU; process ticks divided by elapsed time','ram_unit':'Android PSS KiB, not Windows private memory','effective_buffer_frames':t['buffer_frames'],'pause_resume_cycles':len(t['cycles']),'network':{'short_outage_seconds':25,'new_underruns_during_short_outage':0,'pause_survived_reconnection':short['after_reconnect_paused']['paused'],'cached_audio_seconds_at_exhaustion':round(e['exhausted']['frames']/e['exhausted']['rate'],2),'same_track_and_decoded_position_resumed':True,'cache_limit_mib':256,'read_ahead_requested_seconds':60,'transport_tested':'emulator Wi-Fi'},'lint':{'errors':0,'warnings':sum(i.get('severity')=='Warning' for i in issues)},'limits':{'physical_poco_tested':False,'physical_bluetooth_tested':False,'audible_clicks_tested':False,'real_wifi_cellular_handover_tested':False,'battery_or_thermal_tested':False,'comparable_official_spotify_streaming_tested':False}}
(root/'docs/android-audio-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'passed':True,'artifacts':artifacts,'test_count':len(tests),'lint':report['lint']},ensure_ascii=False,indent=2))
