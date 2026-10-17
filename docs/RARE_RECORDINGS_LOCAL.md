# Shows de fãs, demos e gravações raras

O filtro antigo exigia `collection:etree AND mediatype:etree`. Isso excluía
gravações reais catalogadas como `mediatype:audio`, inclusive a coleção Aadam Jacobs.

Agora, **Todas as fontes** consulta o catálogo de áudio ampliado do Internet
Archive. Também há filtros **Internet Archive**, **Aadam Jacobs** e o filtro
mais restrito **Live Music Archive**. Não são três serviços de áudio diferentes:
são recortes do acervo do Internet Archive, com cobertura diferente.

Exemplos públicos confirmados em 17/10/2026:

- [Nirvana — Dreamerz, 08/07/1989](https://archive.org/details/ajc00795_nirvana-1989-07-08).
- [Nirvana — Cabaret Metro, 30/09/1989](https://archive.org/details/ajc03187_nirvana-1989-09-30).
- [L7 — Dreamerz, 14/09/1989](https://archive.org/details/ajc02607_l7-1989-09-14).
- [L7 — Detroit, 1990](https://archive.org/details/l7-femmchismo).
- [L7 / The Breeders — The Gorge, 31/08/1994](https://archive.org/details/l-7-08-31-1994-the-gorge-xlii-from-dat-second-gen).

Na interface, procure **L7** ou **Nirvana**, escolha **Aadam Jacobs** para esse
acervo, abra o show e escolha a faixa. Para outras gravações e demos, experimente
**Internet Archive** e os filtros **Shows**, **Demos** e **Gravações**.

Nome exato do artista tem prioridade sobre título de remix/tributo e menções.
MusicBrainz conserva seu score para distinguir artistas homônimos. Shows da
coleção Aadam ganham indicação de procedência. A classificação show/demo é uma
heurística pelo título/metadado, não uma certificação da gravação ou raridade.

Cada faixa usa uma representação: MP3 primeiro, Ogg depois, FLAC como alternativa.
Isso evita duplicar uma música para cada formato e reduz tráfego/buffer no celular.
Arquivos privados, acervos restritos, metadados, checksums `.ffp` e caminhos inválidos
não são tratados como áudio. A página original permanece identificada. A oferta
de um item pode mudar e o app respeita a indisponibilidade retornada pela fonte.

## Outras integrações possíveis

| Fonte | Utilidade | Situação prática |
|---|---|---|
| Internet Archive / Aadam Jacobs | Shows gravados por fãs, fitas, demos e coleções históricas | Implementado, consultas públicas sem chave |
| Live Music Archive | Shows das bandas presentes no acervo etree | Implementado; não cobre todas as bandas |
| Audius | Artistas independentes, publicações e versões alternativas | Implementado; não é um substituto do catálogo rock comercial |
| OpenSubsonic / Navidrome | Sua coleção de shows, demos e compras Bandcamp | Implementado; requer seu servidor e credenciais |
| Jamendo | Música independente com API | Implementado; exige client ID próprio ativo |
| SoundCloud | Publicações de artistas, demos e sessões | Integração futura; API exige cadastro/credenciais de aplicativo, com faixas bloqueadas ou prévias em alguns casos |
| YouTube | Grande cobertura de apresentações e gravações de fãs | Integração futura por player oficial de vídeo; Premium não fornece uma API de áudio bruto para este player |
| MusicBrainz | Discografia, identificação de versões e metadados | Implementado; não transmite áudio |

As alternativas imediatamente úteis para Nirvana/L7 são o Archive ampliado e
sua própria biblioteca OpenSubsonic. Não há chave SoundCloud/Jamendo ou servidor
Subsonic pessoal embutido nem acesso inventado a bibliotecas externas.

Referências primárias: [coleção Aadam Jacobs](https://archive.org/details/aadamjacobs),
[SoundCloud API Guide](https://developers.soundcloud.com/docs/api/guide),
[YouTube IFrame API](https://developers.google.com/youtube/iframe_api_reference),
[OpenSubsonic](https://opensubsonic.netlify.app/),
[Jamendo API](https://developer.jamendo.com/v3.0).
