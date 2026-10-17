# Correção Spotify Android — 0.1.1-local

O HTTP 400 mostrado no telefone vinha da identificação inconsistente do motor:
o APK enviava dados/versão de cliente Android junto da identidade OAuth do cliente
desktop. A chamada de client-token era rejeitada antes da reprodução. Além disso,
o fallback utilizava o token do aplicativo Web API para autenticar áudio; esses
tokens têm finalidades diferentes e não devem ser intercambiados.

## Alterações

- O motor usa um perfil de protocolo desktop OAuth completo e coerente, aceito
  pelo serviço: identificação, versão, User-Agent e dados da chamada client-token.
  A saída de áudio, interface e execução continuam nativas Android.
- Biblioteca e reprodução têm clientes OAuth e armazenamento criptografado
  separados. A autorização do áudio abre o navegador automaticamente quando
  necessária, com PKCE/validação de state e callback local 5588/login.
- A autorização da biblioteca e seu Client ID permanecem disponíveis ao atualizar.
  O cache novo `librespot-oauth-v2` evita reutilizar as credenciais incompatíveis
  deixadas pela 0.1.0. Há uma ação para refazer somente a autorização do áudio.
- O serviço permanece ativo durante o fluxo no navegador para que o Android não
  congele o processo que espera o callback. Sair limpa as duas autorizações e as
  credenciais dos caches antigo/novo. Tokens de teste não são incluídos no APK.

## Validação realizada

No Android 16/x86_64 da AVD isolada, com a sessão Premium do Windows transferida
somente para o sandbox do APK benchmark:

- Streaming real de **Wolf — Veruca Salt**, URI
  `spotify:track:65cOtEO1eVrTezvIMDsoDr`, produzindo PCM e faixa AudioFlinger ativa
  de 44,1 kHz. O erro 400 desapareceu.
- Pausa: contagem de frames ficou estável em 4.166.464 entre as verificações.
  Retomada: avançou para 4.403.968; ao retornar do segundo plano, para 4.768.320.
- Reconexão após reiniciar o processo reutilizou as credenciais corretas e voltou
  a produzir PCM. Zero underruns e nenhum erro de conexão nas verificações.
- Reprodução local, teste DSP, pausa/retomada e segundo plano do APK release
  também passaram, com zero underruns no smoke final.
- Compilação ARM64 e x86_64, lint com zero erros, assinatura local, alinhamento
  ELF/ZIP de 16 KB, correspondência das bibliotecas e ausência de tokens no APK
  verificados. A assinatura é a mesma da 0.1.0 e o versionCode sobe para 2.

O streaming foi testado usando credenciais de motor já autorizadas. Uma nova
autorização completa no navegador do emulador não foi concluída neste teste;
essa etapa utiliza o mesmo fluxo OAuth desktop do librespot, separado da Web API.
O teste físico ARM64 e o login no navegador do POCO devem ser confirmados no
telefone. Não houve avaliação auditiva pelo emulador sem som.

Evidências: `out/android/auth-probe-0.1.1.jsonl`,
`out/android/streaming-smoke-0.1.1/result.json`,
`out/android/audio-smoke-0.1.1/result.json` e
`out/android/apk-validation-0.1.1.json`. Resumo preservado em
`docs/android-spotify-fix-tests.json`.

## Instalação

Instale `out/android/Tonal-Android-0.1.1-local.apk` por cima da versão anterior.
Toque em **Conectar motor Spotify**, autorize a reprodução no navegador com a
mesma conta Premium e volte ao Tonal. Se necessário, toque em **Refazer autorização
do áudio**. Não é preciso apagar os dados do aplicativo ou repetir o Client ID
da biblioteca.

SHA-256: `3be95ba56b48746977efa4586af1744ec24ab799ea9fc9f5717fab35172ed425`.
As medições anteriores de CPU/RAM são históricas da 0.1.0, sem streaming, e não
foram reinterpretadas como comparação de streaming após esta correção.

Implementação mantida localmente, sem commit, push, PR ou publicação.
