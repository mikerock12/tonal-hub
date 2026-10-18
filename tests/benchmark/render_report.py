"""Build a sanitized, reproducible Markdown/HTML/CSV report from completed runs."""
from pathlib import Path
import json, statistics as st, csv, hashlib, html, re

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / 'out/benchmark-2026-10-17'
OUT = ROOT / 'docs/benchmarks/2026-10-17'
OUT.mkdir(parents=True, exist_ok=True)

def read(path): return json.loads(Path(path).read_text(encoding='utf-8-sig'))
def stats(values):
    return dict(median=st.median(values), mean=st.mean(values), minimum=min(values), maximum=max(values))
def fmt(n): return f'{n:.2f}'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()

w = read(RAW/'windows/summary.json')
a = read(RAW/'android/summary.json')
stream = read(RAW/'android/tonalify-real-streaming/result.json')
pcm = read(RAW/'android/tonalify-real-streaming/pcm-proof.json')
rows = []
summary = []
for app in ['tonalify', 'spotify']:
    for scenario in ['playing-background', 'paused-background']:
        samples = [s for r in w['results'] if r['app']==app and r['scenario']==scenario for s in r['samples']]
        assert len(samples)==60
        clean = [{k:v for k,v in s.items() if k!='pids'} for s in samples]
        group = dict(platform='Windows', app=app, scenario='Reproduzindo' if scenario.startswith('playing') else 'Pausado',
            memory_kind='USS', memory=stats([s['private_working_set_mib'] for s in samples]),
            cpu=stats([s['cpu_one_core_percent'] for s in samples]), samples=len(samples),
            commit=stats([s['private_commit_mib'] for s in samples]),
            rss=stats([s['summed_working_set_mib'] for s in samples]),
            processes=stats([s['process_count'] for s in samples]),
            read_bytes=sum(s['read_bytes'] for s in samples), write_bytes=sum(s['write_bytes'] for s in samples),
            churn_samples=sum(s['process_churn'] for s in samples))
        summary.append(group)
        for s in clean: rows.append(dict(platform='Windows',app=app,scenario=group['scenario'],memory_mib=s['private_working_set_mib'],cpu_one_core_percent=s['cpu_one_core_percent'],seconds=s['elapsed'],processes=s['process_count']))
for label, selected in [
    ('Tela inicial, sem login', [r for r in a['results'] if 'foreground' in r['label']]),
    ('Segundo plano, sem login', [r for r in a['results'] if 'background' in r['label']]),
    ('Tela inicial + Hub, sem login', [r for r in a['results'] if 'with-hub' in r['label']]),
    ('Streaming Tonalify, autenticado', [stream])]:
    for app in ['tonalify','spotify']:
        records=[r for r in selected if ('tonal' in r['package']) == (app=='tonalify')]
        samples=[s for r in records for s in r['samples']]
        if not samples: continue
        group=dict(platform='Android emulado',app=app,scenario=label,memory_kind='PSS',memory=stats([s['pss_kib']/1024 for s in samples]),cpu=stats([s['cpu_one_core_percent'] for s in samples]),samples=len(samples),processes=stats([s['processes'] for s in samples]),swap_mib=max(s['swap_kib']/1024 for s in samples))
        summary.append(group)
        for s in samples: rows.append(dict(platform='Android emulado',app=app,scenario=label,memory_mib=s['pss_kib']/1024,cpu_one_core_percent=s['cpu_one_core_percent'],seconds=s['interval_seconds'],processes=s['processes']))

proof=[]
for p in w['proof']:
    # Deliberately omit machine/device identifiers and process IDs.
    proof.append(dict(app=p['app'],round=p['round'],playing=p['state']['playing'],title=p['state']['title'],artist=p['state']['artist'],position_ms=p['state']['positionMs'],nonzero_process_audio=any(x['peak']>0 for x in p['audio']['sessions'] if x['pid']!=0),max_session_peak=max((x['peak'] for x in p['audio']['sessions']),default=0),gpu_engine_utilization_percent=[x['UtilizationPercentage'] for x in p.get('gpu_engines',[])]))
artifacts={}
for rel in ['out/release-local/Tonal-Hub-0.8.0.0-Windows-x64-Setup.exe','out/android/Tonal-Android-0.8.0-arm64-v8a.apk','out/android/Tonal-Android-0.8.0-local.apk','out/android/Tonal-Android-0.8.0-audio-sources.tar.gz']:
    path=ROOT/rel;artifacts[path.name]=dict(bytes=path.stat().st_size,sha256=sha(path))
def instrumentation(path):
    line=next(x for x in (ROOT/path).read_text(encoding='utf-8-sig').splitlines() if x.startswith('INSTRUMENTATION_RESULT: result='))
    return json.loads(line.split('result=',1)[1])
core=instrumentation('out/android/core-tests-080-final.log')
pipeline=instrumentation('out/android/lyric-pipeline-080-final.log')
installer=read(ROOT/'out/installer-smoke-0800-connect/result.json')
validation=dict(windows_ctest=dict(passed='100% tests passed, 0 tests failed out of 8' in (ROOT/'out/ctest-080-final.log').read_text(),executables=8),
    remote_transport_regression=dict(passed='PASS remote Connect pause/resume, foreign device and stale poll guards' in (ROOT/'out/build/release/Testing/Temporary/LastTest.log').read_text()),
    qml=dict(passed='6 passed, 0 failed' in (ROOT/'out/lyrics-ui-080-final.log').read_text(),checks=6),
    android_core=core,android_translation=pipeline,
    android_song_page=dict(passed=read(ROOT/'out/android/song-page-0.8.0/result.json')['passed']),
    android_catalog=dict(passed=read(ROOT/'out/android/catalog-polish-0.8.0/result.json')['passed']),
    android_release_upgrade={k:v for k,v in read(ROOT/'out/android/release-upgrade-0.8.0/result.json').items() if k not in ['before','after']},
    windows_installer={k:v for k,v in installer.items() if k!='installation'})
assert all(v['passed'] for v in validation.values()),validation
assert installer['installer_sha256']==artifacts['Tonal-Hub-0.8.0.0-Windows-x64-Setup.exe']['sha256']
(ROOT/'docs/player-080-tests.json').write_text(json.dumps(validation,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
report=dict(date='2026-10-17',version='0.8.0-local',summary=summary,hardware=w['hardware'],windows_audio_proof=proof,
    android_pcm_proof=dict(before_frames=pcm['before']['frames'],after_frames=pcm['after']['frames'],before_underruns=pcm['before']['underruns'],after_underruns=pcm['after']['underruns']),
    startup=dict(windows=w['startup'],android=a['startup_ms']),artifacts=artifacts,validation=validation,
    scope=dict(android_official_streaming=False,physical_bluetooth=False,quality_blind_test=False,window_visibility_controlled=False))
(OUT/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
with (OUT/'samples.csv').open('w',newline='',encoding='utf-8-sig') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)

def group(platform,app,scenario): return next(x for x in summary if x['platform']==platform and x['app']==app and x['scenario']==scenario)
tw=group('Windows','tonalify','Reproduzindo');sw=group('Windows','spotify','Reproduzindo')
reference_title=w['proof'][0]['state']['title'];reference_artist=w['proof'][0]['state']['artist']
ta=group('Android emulado','tonalify','Tela inicial, sem login');sa=group('Android emulado','spotify','Tela inicial, sem login')
ram_saving=100*(1-tw['memory']['median']/sw['memory']['median'])
cpu_saving=100*(1-tw['cpu']['mean']/sw['cpu']['mean']) if sw['cpu']['mean'] else 0
cpu_comparison=f"{fmt(abs(cpu_saving))}% {'menor' if cpu_saving>=0 else 'maior'} nesta amostra"
table='\n'.join(f"| {r['platform']} | {r['scenario']} | {'Tonalify' if r['app']=='tonalify' else 'Spotify'} | {fmt(r['memory']['median'])} ({r['memory_kind']}) | {fmt(r['cpu']['mean'])} / {fmt(r['cpu']['median'])} | {r['samples']} |" for r in summary)
detail='\n'.join(f"| {'Tonalify' if r['app']=='tonalify' else 'Spotify'} | {r['scenario']} | {fmt(r['memory']['minimum'])}–{fmt(r['memory']['maximum'])} | {fmt(r['commit']['median'])} | {fmt(r['rss']['median'])} | {int(r['processes']['minimum'])}–{int(r['processes']['maximum'])} | {r['read_bytes']} / {r['write_bytes']} |" for r in summary if r['platform']=='Windows')
rounds='\n'.join(f"| {r['round']} | {r['app']} | {'Reproduzindo' if r['scenario'].startswith('playing') else 'Pausado'} | {fmt(r['median_private_working_set_mib'])} | {fmt(r['mean_cpu_one_core_percent'])} |" for r in w['results'])
startup='\n'.join(f"| {app} | {', '.join(fmt(t) for t in v['process_visible_in_enumeration_ms'])} |" for app,v in w['startup'].items())
astart='\n'.join(f"| {'Tonalify' if 'tonal' in p else 'Spotify'} | {', '.join(str(t) for t in times)} | {fmt(st.median(times))} |" for p,times in a['startup_ms'].items())
hashes='\n'.join(f"| {name} | {v['bytes']:,} | `{v['sha256']}` |" for name,v in artifacts.items())
md=f'''# Tonal Hub 0.8 — desempenho e funcionalidades

Medição local de 17/10/2026. Projeto privado de Maicon Nunes, derivado de [tonal-player, de Matheus Nunes (natheus)](https://github.com/natheus/tonal-player), com crédito e licença MIT preservados.

## Resultado que os dados sustentam

No Windows, nesta sessão com streaming real da mesma faixa, o Tonalify usou **{fmt(tw['memory']['median'])} MiB de RAM privada**, contra **{fmt(sw['memory']['median'])} MiB** do Spotify: **{fmt(ram_saving)}% menos**. A CPU média foi **{fmt(tw['cpu']['mean'])}% contra {fmt(sw['cpu']['mean'])}% de um núcleo** ({cpu_comparison}). A comparação inclui subprocessos e tem duração curta; não é promessa para todas as bibliotecas, máquinas ou efeitos.

A CPU deve ser lida primeiro em valores absolutos. Com médias próximas de zero, diferenças relativas grandes podem corresponder a poucos milissegundos e ruído de amostragem. Estas rodadas não estabelecem uma vantagem universal de CPU; a evidência mais clara é o uso de memória neste cenário.

No Android emulado, a tela inicial **sem login em ambos** ficou em **{fmt(ta['memory']['median'])} contra {fmt(sa['memory']['median'])} MiB PSS**. Isso mede a interface inicial, não a eficiência relativa de streaming. O Tonalify autenticado, medido separadamente, ficou em **{fmt(stream['median_pss_mib'])} MiB PSS** e **{fmt(stream['median_cpu_one_core_percent'])}% de um núcleo**. **Não foi concluída uma amostra equivalente de streaming autenticado no Spotify Android. Não há conclusão de vantagem de reprodução no Android.**

## Hardware, versões e cenário

- PC: Windows 10 Pro, Intel Core i5-6500 3,2 GHz, 4 núcleos/4 threads, 16 GB instalados. Intel HD Graphics 530, driver 26.20.100.7262. Lexar NQ100 240 GB e WDC 1 TB; inventário exato no JSON.
- Tonalify Windows: build Release MSVC 2022/Qt 6.8.3, pacote 0.8.0. Spotify oficial Store: 1.3.5.3058, pacote 1.305.3058.0.
- Emulador dedicado: Android 16/API 36, x86_64, 2 vCPUs, páginas 4 KiB. Spotify 9.1.90.2270 com ABI x86_64; sem tradução de código ARM. Não é uma medição no POCO X7 Pro.
- Android comparável: variante Release `comparison`, sem depuração, perfil independente e sem login, contra a tela inicial sem login do Spotify. Teste de streaming Tonalify separado: variante `benchmark` com depuração, mesmas bibliotecas nativas Release.
- Faixa Windows: **{reference_title} — {reference_artist}**, identificada pela sessão real, URI registrada no coletor privado. Início solicitado em 30 s; há aquecimento e consultas antes da coleta, portanto os trechos medidos não são exatamente os mesmos milissegundos.
- Configuração Windows Tonalify existente: efeitos ligados, preset Rock, EQ ativo, graves +8 dB, compressor desligado, espacial original, normalização ligada, alvo local −16 LUFS e teto −1 dBFS. Configurações de qualidade/normalização do cliente oficial não foram igualadas/confirmadas. Não é um teste de fidelidade sonora ou volume.
- Sem compilação, compressão ou emulador durante a coleta Windows. A posição/visibilidade das janelas não foi imposta pelo coletor; os rótulos brutos com “background” não comprovam minimização equivalente. Essa limitação impede atribuir toda a diferença apenas ao motor de áudio.

## CPU e memória

CPU: **100% = um núcleo totalmente ocupado**. Divida os valores por 4 para a capacidade total deste PC e por 2 para a do emulador. RAM Windows é **USS**, páginas residentes exclusivas. Android é **PSS**, com páginas compartilhadas rateadas. Não compare diretamente os números absolutos entre sistemas.

| Plataforma | Cenário | Aplicativo | RAM mediana MiB | CPU média / mediana % de 1 núcleo | Amostras |
|---|---|---|---:|---:|---:|
{table}

### Rodadas individuais no Windows

| Rodada | Aplicativo | Estado | RAM privada mediana MiB | CPU média % de 1 núcleo |
|---|---|---|---:|---:|
{rounds}

Variação entre rodadas também faz parte do resultado. As médias agregadas não ocultam picos, atualização do catálogo ou trabalho de interface. Consulte os intervalos por amostra no CSV antes de atribuir uma diferença pequena ao decoder.

### Memória adicional e I/O no Windows

| Aplicativo | Cenário | RAM privada mín.–máx. MiB | Commit privado mediano MiB | Soma RSS mediana MiB | Processos | Bytes lidos / escritos |
|---|---|---:|---:|---:|---:|---:|
{detail}

RSS somado conta páginas compartilhadas mais de uma vez. Commit não é RAM residente. I/O é lógico por processo, incluindo cache, arquivos e outras operações; não é medição isolada da internet nem do disco físico. Intervalos e dados completos estão em `samples.csv` e `summary.json`. Contadores de memória ausentes abortam o coletor; processos de Chromium com caminho protegido continuam incluídos pelo nome. Nenhuma árvore incompleta foi aceita como medição final.

## Protocolo e confirmação de áudio

Windows: duas rodadas com ordem invertida; 30 amostras de aproximadamente 1 s por aplicativo e estado em cada rodada; aquecimento de 8 s tocando e 6 s pausado. Os dois clientes permanecem carregados, um deles pausado, e medem-se apenas os processos pertencentes ao aplicativo alvo. Tonalify inclui librespot e seus filhos; Spotify inclui seus processos de interface, GPU e serviços.

A reprodução foi confirmada pela API da sessão e pelo pico não nulo da sessão Core Audio pertencente ao processo alvo, antes de cada rodada. Contextos artificiais de play por URI não foram tratados consistentemente pelos clientes na preparação; tentativas sem áudio foram descartadas. A faixa foi inicialmente aberta pelo protocolo do cliente oficial. Nas rodadas válidas, o contexto já carregado foi transferido para cada dispositivo com reprodução ativa e ajustado a 30 s pela API da sessão autorizada. Não se mede um aplicativo parado como se estivesse tocando.

Android: duas rodadas em ordem invertida, 6 amostras de 5 s por aplicativo em primeiro plano; 3 amostras no cenário de segundo plano; 6 amostras com o Hub. Aquecimento de 10 s. Memória e CPU somam todos os PIDs, incluindo o Hub. A coleta de streaming Tonalify teve avanço de PCM de **{pcm['before']['frames']:,} para {pcm['after']['frames']:,} frames**, sem aumento dos underruns (**{pcm['before']['underruns']} → {pcm['after']['underruns']}**). Isso não mede estalos acústicos, uma troca de rede ou Bluetooth físico.

As amostras sucessivas não são observações independentes de uma população. Os intervalos mín./máx. são descritivos; não são intervalos de confiança. RAM/CPU podem mudar com cache, tamanho da biblioteca, tela, letras, efeitos, atualizações e tarefas do sistema.

## Abertura, GPU, armazenamento e lacunas

No Windows, estes números são **tempo até o processo aparecer**, com mecanismos de abertura diferentes (executável direto e protocolo Store), sem medir prontidão da interface e sem limpar cache do sistema. Não sustentam um vencedor em abertura.

| Aplicativo Windows | Três tentativas, ms |
|---|---|
{startup}

Android: `am start -S -W`, processo encerrado antes de cada tentativa, cache do sistema preservado. `TotalTime` mede abertura da Activity inicial sem login, não prontidão do catálogo ou do áudio.

| Aplicativo Android | Três tentativas, ms | Mediana ms |
|---|---|---:|
{astart}

GPU: snapshots dos engines atribuídos aos PIDs antes da coleta, no JSON; não há série contínua nem energia/VRAM comparável. Contadores zerados não provam ausência de uso. Tamanho do APK/instalador abaixo é o arquivo distribuído, não o espaço total instalado/cache. Bateria, temperatura, dados móveis, latência acústica, Bluetooth do carro, estabilidade por horas e transições Wi-Fi/4G no aparelho físico permanecem sem medição nesta rodada.

## Funcionalidades e qualidade

| Recurso | Tonal Hub 0.8 | Spotify oficial |
|---|---|---|
| Busca de artistas, álbuns e faixas Spotify | Tonalify, conta autorizada; streaming via librespot depende de Premium | Catálogo e integração oficiais |
| Outras fontes e raridades | IndieTonal: Audius, Archive/LMA/Aadam Jacobs, MusicBrainz; Jamendo com credencial e OpenSubsonic configurados; disponibilidade varia | Não agrega essas fontes externas; conteúdos do próprio catálogo variam |
| Playlists e favoritos | Biblioteca pessoal local e integração já existente com Spotify; coração preenchido; exportação pessoal | Playlists, favoritos e sincronização da conta |
| Histórico e estatísticas | Buscas locais; tempo, escutas qualificadas, músicas, álbuns e artistas; opção nas listas; gráficos clicáveis | Recursos oficiais de atividade e retrospectiva; não foi auditada equivalência de cada estatística |
| Clima do repertório | TonalMuse Tiny C++ local, 32 KiB, corpus sintético; temas experimentais de letras/estilos, fontes e cobertura | Sem comparação equivalente validada para este classificador |
| Letras e tradução | Busca automática, tradução pt-BR com detecção automática, pares por linha, retomada de pendências | Letras/traduções conforme faixa, dispositivo e disponibilidade; traduções móveis anunciadas globalmente [1] |
| Sincronia e aparência | Desligar destaque, alinhar linha ao áudio, salvar por versão, exportar ajuste; fontes/cores/contorno/neon/brilho/piscante e tela cheia | Não foi encontrada/documentada equivalência para os ajustes locais desta versão; não é prova de ausência em versões futuras |
| EQ e efeitos | 10 bandas, presets, graves, compressor, limiter e perfis de saída; C++ nativo no DSP | Equalizador documentado no desktop e Android [2] |
| Espacial | HRTF OpenAL Soft; IAMF apenas se existir no arquivo compatível | Recursos variam com plataforma/serviço; não auditados aqui |
| Qualidade Spotify | 96/160/320 kbps; não há implementação Spotify lossless nesta versão | Automática e níveis conforme plano [3]; lossless até 24-bit/44,1 kHz documentado para clientes/dispositivos compatíveis [5]; disponibilidade e ativação nesta conta não verificadas |
| Uso sem rede | Buffer e cache temporários de streaming; arquivos locais; não equivale a downloads permanentes de álbuns Spotify | Downloads offline Premium e regras oficiais de uso [4] |
| Bluetooth | PCM via saída do Android; perfis de efeitos e buffer; o sistema/receptor negociam o codec | Integração do cliente oficial; codec também depende do sistema/receptor |
| Backup | Exportação/importação pessoal e cópias de segurança; conta e senha ficam fora | Biblioteca sincronizada com a conta; downloads não equivalem a backup pessoal |

Não existe conversão que recupere informação perdida de uma fonte com compressão. EQ, HRTF e mais volume não demonstram maior fidelidade; ganhos altos podem reduzir headroom e dinâmica. Bluetooth recodifica conforme aparelho/codec/receptor. Nenhuma medição feita aqui comprova “som superior”, baixa latência garantida ou ausência absoluta de falhas.

[1] [Anúncio oficial de traduções e letras offline](https://newsroom.spotify.com/2026-02-04/lyric-translations-offline-previews/), consultado em 17/10/2026.

[2] [Equalizador Spotify](https://support.spotify.com/br-pt/article/equalizer/), consultado em 17/10/2026.

[3] [Qualidade do áudio Spotify](https://support.spotify.com/br-pt/article/audio-quality/), consultado em 17/10/2026. Recursos adicionais dependem do plano, região e dispositivo; este relatório não verifica lossless na conta.

[4] [Ouça offline — Spotify](https://support.spotify.com/br-pt/article/listen-offline/), consultado em 17/10/2026.

[5] [Qualidade lossless — documentação oficial](https://support.spotify.com/il-en/article/lossless-audio-quality/), consultada em 17/10/2026. Não há prova de uso de lossless nesta medição, nem de transporte lossless pelo Bluetooth do usuário.

## Correções entregues e validação

Capas: metadados do álbum acompanham as faixas da fila; atualizações parciais preservam imagens conhecidas; cache compartilhado e tentativas limitadas. Ícones mantêm proporção e alinhamento; álbum/artista têm hierarquia e metadados legíveis. Estatísticas resumidas podem ser mostradas/ocultadas. Cards do repertório separam temas, artistas, álbuns e estilos, com consulta a Spotify/MusicBrainz e identificação de ambiguidades; dados ausentes ficam pendentes, sem inventar gêneros ou ranquear “Não informado”.

Sincronia pode ser desativada ou alinhada clicando na linha atual, sem pular o áudio. Ajustes são por gravação/versão da letra. Exportar ajuste gera arquivo local; não envia automaticamente ao LRCLIB. Paletas e seletor atualizam o hexadecimal imediatamente. Estilos originais/tradução independentes incluem contorno, brilho, neon, piscante, negrito, itálico e sublinhado. Uma falha de tradução não interrompe as linhas seguintes, e o botão retoma pendências.

Durante a preparação, foi corrigida também a reconciliação da pausa física no Windows quando o Spotify Connect retoma sem o evento de transporte esperado. O estado confirmado da sessão serve de apoio; comandos locais recentes são protegidos e consultas antigas são descartadas. A regressão cobre retomada remota, pausa, outro dispositivo e consulta em voo durante a pausa.

- Windows: 8/8 testes CTest e 6/6 testes da tela QML real; instalador final validado em instalação isolada, atualização preservando arquivo pessoal, abertura sem SDK e desinstalação.
- Android: 34 verificações de núcleo e 4 cenários de tradução (retry, falha isolada, retomada e final em cache). Testes de página de música/letras/tela cheia, artista/álbum com Spotify real, capas, estatísticas e avanço de PCM passaram.
- APK Release atualizado sem desinstalar, com UID, pasta de dados e data da primeira instalação preservados. ARM64 e universal verificados quanto às bibliotecas/empacotamento e alinhamento de 16 KiB. APK para POCO X7 Pro: ARM64.
- Nenhum teste emulado substitui a validação de Bluetooth/rede no carro ou telefone físico. A página de letras depende da cobertura e disponibilidade dos provedores.

## Arquivos e integridade

Instalador Windows instala Tonal Hub, Tonalify e IndieTonal. APKs usam a mesma identidade de assinatura local da atualização testada. Código correspondente das bibliotecas de áudio e avisos acompanham o backup; as versões são experimentais privadas.

| Arquivo | Bytes | SHA-256 |
|---|---:|---|
{hashes}

Dados revisados: `benchmarks/2026-10-17/summary.json`, `samples.csv` e `report.html`. Coletores: `tests/benchmark/`. Dados brutos em `out/benchmark-2026-10-17`, fora do Git. Resumo publicado não contém tokens, IDs de dispositivo nem nome da máquina. Os valores iniciais com coleta incompleta do Hub Android e a tentativa Windows sem áudio foram descartados.
'''
(ROOT/'docs/BENCHMARK_080.md').write_text(md,encoding='utf-8')

def inline(s):
    s=html.escape(s)
    s=re.sub(r'`([^`]+)`',r'<code>\1</code>',s)
    s=re.sub(r'\*\*([^*]+)\*\*',r'<strong>\1</strong>',s)
    return re.sub(r'\[([^\]]+)\]\((https://[^)]+)\)',r'<a href="\2">\1</a>',s)
def render_md(source):
    result=[];table=False;listing=False
    for line in source.splitlines():
        if not line:
            if table:result.append('</tbody></table></div>');table=False
            if listing:result.append('</ul>');listing=False
            continue
        if line.startswith('|'):
            cells=[x.strip() for x in line.strip('|').split('|')]
            if all(re.fullmatch(r'[-: ]+',x) for x in cells):continue
            tag='td' if table else 'th'
            if not table:result.append('<div class="table"><table><tbody>');table=True
            result.append('<tr>'+''.join(f'<{tag}>{inline(x)}</{tag}>' for x in cells)+'</tr>');continue
        if line.startswith('#'):
            level=len(line)-len(line.lstrip('#'));result.append(f'<h{level}>{inline(line[level:].strip())}</h{level}>')
        elif line.startswith('- '):
            if not listing:result.append('<ul>');listing=True
            result.append('<li>'+inline(line[2:])+'</li>')
        else:result.append('<p>'+inline(line)+'</p>')
    return '\n'.join(result)

template='''<!doctype html><html lang="pt-BR"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Tonal 0.8 · relatório de desempenho</title>
<style>*{box-sizing:border-box}body{margin:0;background:#10151c;color:#e6edf4;font:16px/1.65 system-ui,sans-serif}main{max-width:1160px;margin:auto;padding:48px 24px 90px}h1{font-size:clamp(28px,4vw,46px);line-height:1.18}h2{margin-top:50px;color:#82dcf2;font-size:26px}h3{font-size:20px}p,li{max-width:100ch;color:#c4d0df}strong{color:#fff}a{color:#76d5f2}code{font-size:.83em;overflow-wrap:anywhere}.table{overflow-x:auto;border:1px solid #2d3b4b;border-radius:14px;margin:22px 0}table{border-collapse:collapse;width:100%;font-size:14px}td,th{padding:13px 15px;text-align:left;border-bottom:1px solid #293442;vertical-align:top}th{color:#fff;background:#202e3c}tr:nth-child(even){background:#17212b}.dashboard{padding:24px;background:#1a2633;border:1px solid #334a60;border-radius:22px;margin:30px 0}label{margin-right:20px;display:inline-block}select{font:inherit;padding:8px;border-radius:9px;background:#0f1923;color:#fff;border:1px solid #52697c}.barrow{display:grid;grid-template-columns:130px 1fr 120px;gap:14px;align-items:center;margin:20px 0}.bar{height:30px;min-width:2px;border-radius:7px;background:#25b4dc}.bar.spotify{background:#33ca80}.note{font-size:14px;color:#b3c5d6}.metric{font-weight:700;font-variant-numeric:tabular-nums}#scope{padding:10px 0;border-bottom:1px solid #3a4d60}.kicker{color:#84d7ea;letter-spacing:.13em;font-size:13px;text-transform:uppercase}@media(max-width:600px){main{padding:24px 16px}.barrow{grid-template-columns:80px 1fr 78px;gap:8px;font-size:13px}label{margin-bottom:10px}td,th{min-width:110px}}</style>
<main><div class="kicker">Tonal Hub · estudo local · 17 outubro 2026</div><section class="dashboard"><h2 style="margin-top:0">Explore as medições</h2><label>Cenário <select id="scenario"></select></label><label>Métrica <select id="metric"><option value="memory">RAM mediana (MiB)</option><option value="cpu">CPU média (% de 1 núcleo)</option></select></label><p id="scope" class="note"></p><div id="bars"></div><p class="note">Barras com escala relativa ao cenário selecionado. Android sem login não é uma comparação de streaming. USS e PSS não são equivalentes.</p></section>__BODY__<footer class="note">Relatório autônomo, sem bibliotecas remotas, telemetria ou chamadas de rede.</footer></main>
<script>const data=__DATA__;const scenario=document.querySelector('#scenario'),metric=document.querySelector('#metric');const keys=[...new Set(data.map(x=>x.platform+' · '+x.scenario))];for(const k of keys){const o=document.createElement('option');o.value=k;o.textContent=k;scenario.append(o)}function draw(){const records=data.filter(x=>x.platform+' · '+x.scenario===scenario.value);const mem=metric.value==='memory';const vals=records.map(x=>mem?x.memory.median:x.cpu.mean);const max=Math.max(...vals,.001);document.querySelector('#scope').textContent=records[0].platform+' · '+records[0].scenario+' · '+records[0].memory_kind+' · '+records[0].samples+' amostras por aplicativo'+(records.length===1?' · Sem par oficial equivalente':'');const bars=document.querySelector('#bars');bars.replaceChildren();records.forEach((r,i)=>{const row=document.createElement('div');row.className='barrow';const name=document.createElement('span');name.textContent=r.app==='spotify'?'Spotify':'Tonalify';const track=document.createElement('div');const bar=document.createElement('div');bar.className='bar '+r.app;bar.style.width=(100*vals[i]/max)+'%';bar.title='Mínimo '+(mem?r.memory.minimum:r.cpu.minimum).toFixed(2)+' / máximo '+(mem?r.memory.maximum:r.cpu.maximum).toFixed(2);track.append(bar);const v=document.createElement('span');v.className='metric';v.textContent=vals[i].toFixed(2)+(mem?' MiB':'%');row.append(name,track,v);bars.append(row)})}scenario.onchange=metric.onchange=draw;draw();</script></html>'''
page=template.replace('__BODY__',render_md(md)).replace('__DATA__',json.dumps(summary,ensure_ascii=False).replace('<','\\u003c'))
(OUT/'report.html').write_text(page,encoding='utf-8')
print(json.dumps(dict(windows_ram_saving_percent=ram_saving,windows_cpu_mean_saving_percent=cpu_saving,output=str(OUT)),indent=2))
