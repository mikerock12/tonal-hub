"""Whitelist-only public export of the completed 19 October runs.

Requires matplotlib for the static chart. Never copy private dumps wholesale.
"""
from pathlib import Path
from datetime import datetime
import csv,hashlib,json,re,statistics as st,sys,os,math

ROOT=Path(__file__).resolve().parents[2]
RAW=ROOT/'out/benchmark-2026-10-19'
OUT=ROOT/'docs/benchmarks/2026-10-19'

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def stats(v):
    v=sorted(v)
    return dict(median=st.median(v),mean=st.mean(v),minimum=v[0],maximum=v[-1],p95=v[math.ceil(.95*len(v))-1])

def state_title(state):
    return {'playing':'reproduzindo','paused':'pausado','playing-minimized':'reproduzindo','paused-minimized':'pausado','foreground':'tela inicial','background':'segundo plano','streaming':'streaming Tonalify'}[state]

def main():
    os.environ.setdefault('MPLCONFIGDIR',str(ROOT/'out/matplotlib'))
    import matplotlib,psutil
    OUT.mkdir(parents=True,exist_ok=True)
    w=read(RAW/'windows/summary.json');a=read(RAW/'android/summary.json')
    assert len(w['results']) in [6,8] and len(a['results'])==8
    records=[];groups=[]
    series=[('Windows exploratório',w['results'])]
    verification=[]
    minimized=RAW/'windows/minimized-summary.json'
    if minimized.exists():
        extra=read(minimized);assert len(extra['results'])==8
        assert len(extra['proofs'])==8
        for p in extra['proofs']:
            assert p['target_peak']>0 and p['state']['playing'] and p['state']['uri']==w['track']
            assert p['windows'] and all(win['minimized'] for win in p['windows'])
            verification.append(dict(app=p['app'],round=p['round'],phase=p['phase'],track_verified=True,playing=True,target_audio_peak=p['target_peak'],minimized=True))
        series.append(('Windows minimizado',extra['results']))
    for platform,results in series:
        for app in ['tonalify','spotify']:
            for state in sorted({r['scenario'] for r in results}):
                runs=[r for r in results if r['app']==app and r['scenario']==state]
                samples=[s for r in runs for s in r['samples']]
                assert len(samples)==(120 if platform=='Windows minimizado' else 60*len(runs))
                g=dict(platform=platform,app=app,scenario=state,memory_kind='USS',memory=stats([s['uss_mib'] for s in samples]),cpu=stats([s['cpu_one_core_percent'] for s in samples]),samples=len(samples),
                    rss=stats([s['rss_sum_mib'] for s in samples]),commit=stats([s['private_commit_mib'] for s in samples]),processes=stats([s['processes'] for s in samples]),host_cpu=stats([s['host_cpu_percent'] for s in samples]),host_available_ram=stats([s['host_available_ram_mib'] for s in samples]),churn=sum(s['process_churn'] for s in samples),read_bytes=sum(s['read_bytes'] for s in samples),write_bytes=sum(s['write_bytes'] for s in samples),windows=[r['windows'] for r in runs],rounds=[])
                for r in runs:
                    g['rounds'].append(dict(round=r['round'],memory_median=st.median(s['uss_mib'] for s in r['samples']),cpu_mean=st.mean(s['cpu_one_core_percent'] for s in r['samples'])))
                    for s in r['samples']:records.append(dict(platform=platform,app=app,scenario=state,round=r['round'],sample=s['sample'],memory_mib=s['uss_mib'],swap_mib='',cpu_one_core_percent=s['cpu_one_core_percent'],seconds=s['seconds'],processes=s['processes']))
                groups.append(g)
    for app in ['tonalify','spotify']:
        for state in ['foreground','background']:
            runs=[r for r in a['results'] if r['label'].startswith(app+'-') and r['label'].endswith('-'+state)]
            samples=[s for r in runs for s in r['samples']];assert len(samples)==12
            assert all(r['profile']['app_version']==('0.12.0-local' if app=='tonalify' else '9.1.90.2270') for r in runs)
            g=dict(platform='Android emulado, sem login',app=app,scenario=state,memory_kind='PSS',memory=stats([s['pss_kib']/1024 for s in samples]),cpu=stats([s['cpu_one_core_percent'] for s in samples]),swap=stats([s['swap_kib']/1024 for s in samples]),samples=len(samples),processes=stats([s['processes'] for s in samples]),profile=runs[0]['profile'],rounds=[])
            for r in runs:
                g['rounds'].append(dict(round=r['round'],memory_median=st.median(s['pss_kib']/1024 for s in r['samples']),cpu_mean=st.mean(s['cpu_one_core_percent'] for s in r['samples'])))
                for s in r['samples']:records.append(dict(platform=g['platform'],app=app,scenario=state,round=r['round'],sample=s['sample'],memory_mib=s['pss_kib']/1024,swap_mib=s['swap_kib']/1024,cpu_one_core_percent=s['cpu_one_core_percent'],seconds=s['interval_seconds'],processes=s['processes']))
            groups.append(g)
    supplement=None;stream=RAW/'android/tonalify-streaming/summary.json'
    if stream.exists():
        d=read(stream);assert len(d['results'])==2 and len(d['proofs'])==2
        samples=[s for r in d['results'] for s in r['samples']];assert len(samples)==12
        pcm=[]
        for r,p in zip(d['results'],d['proofs']):
            before,after=p['before'],p['after']
            assert after['frames']>before['frames'] and not after['paused'] and not before['paused']
            assert after['currentUri']==before['currentUri']==w['track']
            assert after['underruns']>=before['underruns']
            pcm.append(dict(frames_written=after['frames']-before['frames'],new_underruns=after['underruns']-before['underruns'],new_recoveries=after['recoveries']-before['recoveries'],dsp_us_per_block=(after['dspNanos']-before['dspNanos'])/(after['blocks']-before['blocks'])/1000))
            for s in r['samples']:records.append(dict(platform='Android emulado, streaming Tonalify',app='tonalify',scenario='streaming',round=len(pcm),sample=s['sample'],memory_mib=s['pss_kib']/1024,swap_mib=s['swap_kib']/1024,cpu_one_core_percent=s['cpu_one_core_percent'],seconds=s['interval_seconds'],processes=s['processes']))
        supplement=dict(memory=stats([s['pss_kib']/1024 for s in samples]),cpu=stats([s['cpu_one_core_percent'] for s in samples]),samples=len(samples),profile=d['results'][0]['profile'],configuration={k:d['proofs'][0]['before'][k] for k in ['rate','channels','bufferFrames','eq','enabled','hrtfRequested']},pcm=pcm,variant=d['variant'],official_spotify_equivalent=False)
        supplement['release_native_library_parity']=read(RAW/'android-benchmark-native-parity.json')
        assert len(supplement['release_native_library_parity'])==20 and all(supplement['release_native_library_parity'].values())
        supplement['apk_sha256']=hashlib.sha256((ROOT/'android/app/build/outputs/apk/benchmark/app-benchmark.apk').read_bytes()).hexdigest()
    h=w['hardware'];parity=read(RAW/'android-native-parity.json');assert len(parity)==20 and all(parity.values())
    end=datetime.fromtimestamp(max(p.stat().st_mtime for p in [RAW/'windows/minimized-summary.json',RAW/'android/summary.json']+([stream] if stream.exists() else []))).astimezone()
    date_label='19/10/2026' if end.date().isoformat()=='2026-10-19' else '19–'+end.strftime('%d/%m/%Y')
    public=dict(date='2026-10-19',collection_ended_at=end.isoformat(),timezone='America/Sao_Paulo',product_version='0.12.0',product_commit='d4acee3ca35147d289ea9b71dc80038a99950eab',spotify_windows_version='1.3.5.3058',spotify_android_version='9.1.90.2270',logical_cores=w['logical_cores'],ram_gib=w['ram_bytes']/2**30,
        hardware=dict(cpu=[dict(name=c['Name'],cores=c['NumberOfCores'],threads=c['NumberOfLogicalProcessors']) for c in h['cpu']],os=h['os'],gpu=[dict(name=c['Name'],driver=c['DriverVersion']) for c in h['gpu']]),
        android_avd=dict(requested_ram_mib=3072,guest_total_ram_kib=int(re.search(r'MemTotal:\s*(\d+)',a['meminfo'])[1]),tonal_hub_included=a['tonal_hub_included'],release_native_library_parity=parity),hashes=dict(tonal_exe=w['tonal_exe_sha256'],android_comparison=a['comparison_apk_sha256']),tooling=dict(python=sys.version.split()[0],psutil=psutil.__version__,matplotlib=matplotlib.__version__),windows_audio_settings=w['tonal_audio_settings'],spotify_quality_verified=False,android_spotify_streaming_measured=a['spotify_streaming_measured'],playback_verification=verification,tonal_streaming_supplement=supplement,groups=groups)
    auth=read(RAW/'android-official-auth.json')
    public['android_official_auth_attempt']={k:auth[k] for k in ['attempted','native_account_identifier_accepted','login_code_required','fresh_user_code_provided','fresh_code_submissions','authenticated_streaming_measured','observed_error','reason']}
    (OUT/'summary.json').write_text(json.dumps(public,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    with (OUT/'samples.csv').open('w',encoding='utf-8',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(records[0]),lineterminator='\n');writer.writeheader();writer.writerows(records)
    wt,ws=[next(g for g in groups if g['platform']=='Windows minimizado' and g['scenario']=='playing-minimized' and g['app']==app) for app in ['tonalify','spotify']]
    saving=100*(1-wt['memory']['median']/ws['memory']['median'])
    text=['# Resultados — Tonalify 0.12.0 vs Spotify','', f'Coleta de {date_label} (horário de São Paulo). [Método, condições e limitações](../../BENCHMARK_0120.md).','',
        f'Windows minimizado tocando a mesma faixa: **{saving:.2f}% menos RAM exclusiva no Tonalify nesta amostra**. A direção da diferença de CPU mudou entre rodadas; não houve vantagem consistente de CPU. DSP/qualidade não foram igualados.',
        'Android compara as telas sem login, com Hub no Tonalify. Isso não demonstra economia durante streaming; manter esse cenário sem login foi autorizado pelo usuário.','',
        '| Série / estado | Tonalify RAM mediana | Spotify RAM mediana | CPU média Tonalify / Spotify |','|---|---:|---:|---:|']
    for platform in dict.fromkeys(g['platform'] for g in groups if g['platform']!='Windows exploratório'):
        for state in dict.fromkeys(g['scenario'] for g in groups if g['platform']==platform):
            t,s=[next(g for g in groups if g['platform']==platform and g['scenario']==state and g['app']==app) for app in ['tonalify','spotify']]
            text.append(f"| {platform}, {state_title(state)} | {t['memory']['median']:.2f} MiB {t['memory_kind']} | {s['memory']['median']:.2f} MiB {s['memory_kind']} | {t['cpu']['mean']:.2f}% / {s['cpu']['mean']:.2f}% |")
    text+=['','100% de CPU = um núcleo. RAM USS Windows e PSS Android não são comparáveis entre si.',
        'Android sem login não demonstra vantagem durante streaming. A série exploratória Windows tem visibilidade desigual.',
        'A série exploratória é parcial: 60 amostras por estado do Tonalify e 120 do Spotify. A série minimizada tem 120 por aplicativo/estado; Android tem 12.',
        '', '![Comparação das medições](comparison.png)', '', '## Série exploratória parcial — visibilidade desigual', '',
        'Spotify em primeiro plano e Tonalify coberto. Estes números não sustentam uma comparação justa de CPU; ficam separados da série principal minimizada.', '',
        '| Estado | Tonalify RAM / CPU | Spotify RAM / CPU |','|---|---:|---:|']
    for state in ['playing','paused']:
        t,s=[next(g for g in groups if g['platform']=='Windows exploratório' and g['scenario']==state and g['app']==app) for app in ['tonalify','spotify']]
        text.append(f"| {state_title(state)} | {t['memory']['median']:.2f} MiB USS / {t['cpu']['mean']:.2f}% | {s['memory']['median']:.2f} MiB USS / {s['cpu']['mean']:.2f}% |")
    text+=['', '## Rodadas e dispersão', '',
        '| Série / app / estado | Rodada | RAM mediana (MiB) | CPU média | CPU p95 agregado |','|---|---:|---:|---:|---:|']
    for g in sorted(groups,key=lambda g:g['platform']=='Windows exploratório'):
        for r in g['rounds']:text.append(f"| {g['platform']} / {g['app']} / {state_title(g['scenario'])} | {r['round']} | {r['memory_median']:.2f} | {r['cpu_mean']:.2f}% | {g['cpu']['p95']:.2f}% |")
    if supplement:
        text+=['','## Streaming Tonalify Android, separado', '',f"Duas janelas, 12 amostras: **{supplement['memory']['median']:.2f} MiB PSS** e **{supplement['cpu']['mean']:.2f}% de um núcleo** em média. {sum(p['new_underruns'] for p in pcm)} novos underruns nos intervalos verificados.",
            'Player de teste sem Hub, variante depurável, bibliotecas nativas Release e polling diagnóstico de UI a cada 2,5 s. PCM aceito pelo AudioTrack cresceu; o emulador não tem saída acústica no host.',
            '**Sem amostra equivalente do Spotify oficial autenticado: não calcular economia de streaming a partir das telas sem login.**']
    text+=['','P95 usa nearest-rank: valor na posição ceil(0,95×n) da ordenação; não é intervalo de confiança.',
        'CPU da tabela principal é a média dos intervalos. Consulte JSON para mínimos/máximos, commit/RSS, I/O, swap, processos, carga do host e estado das janelas.',
        '', '## Integridade e reprodução', '', '[CSV](samples.csv) · [JSON](summary.json) · [Método e funcionalidades](../../BENCHMARK_0120.md).',
        'Os hashes identificam os executáveis efetivamente medidos; a variante Android de comparação tem identificador e perfil separados do APK pessoal.',
        'Dados brutos de contas e dispositivos não foram publicados. Nenhum instalável foi alterado por esta entrega.']
    (OUT/'RESULTADOS.md').write_text('\n'.join(text)+'\n',encoding='utf-8',newline='\n')
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    selected=[g for g in groups if g['platform']!='Windows exploratório']
    labels=list(dict.fromkeys((g['platform'],g['scenario']) for g in selected))
    fig,axes=plt.subplots(1,2,figsize=(14,6));colors=['#009bc9','#1db954']
    for index,(field,title) in enumerate([('memory','RAM mediana (MiB; USS no Windows, PSS no Android)'),('cpu','CPU média (% de um núcleo)')]):
        ax=axes[index]
        for ai,app in enumerate(['tonalify','spotify']):
            vals=[next(g for g in selected if (g['platform'],g['scenario'])==key and g['app']==app)[field]['median' if field=='memory' else 'mean'] for key in labels]
            bars=ax.barh([i+ai*.36 for i in range(len(labels))],vals,height=.34,color=colors[ai],label=app.title())
            ax.bar_label(bars,fmt='%.2f',padding=3,fontsize=9)
        ax.set_yticks([i+.18 for i in range(len(labels))],[key[0]+'\n'+state_title(key[1]) for key in labels]);ax.invert_yaxis();ax.set_title(title,fontsize=10);ax.margins(x=.23);ax.legend(loc='lower right');ax.grid(axis='x',alpha=.18)
    fig.suptitle(f'Tonalify 0.12.0 × Spotify — coleta local {date_label}',fontsize=14)
    fig.text(.5,.01,'Android: telas sem login, sem streaming equivalente. Emulador ≠ celular físico. Dois blocos por estado.',ha='center',fontsize=9)
    fig.tight_layout(rect=[0,.05,1,.94]);fig.savefig(OUT/'comparison.png',dpi=150);plt.close(fig)
    files=['summary.json','samples.csv','RESULTADOS.md','comparison.png']
    manifest=dict(product_commit=public['product_commit'],files={name:hashlib.sha256((OUT/name).read_bytes()).hexdigest() for name in files},collectors={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (ROOT/'tests/benchmark').glob('*0120.py')})
    (OUT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(dict(groups=len(groups),samples=len(records),files=[p.name for p in OUT.iterdir()])))

if __name__=='__main__':main()
