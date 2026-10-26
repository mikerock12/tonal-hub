# Proveniência do leitor local

- **Qwen Team / Alibaba Cloud**: Qwen3.5-2B, quantização
  Q4_K_M publicada por **Unsloth AI** em unsloth/Qwen3.5-2B-GGUF. Apache License 2.0.
  Modelo pré-treinado e quantizado, sem treinamento ou fine-tuning do Tonal. Download opcional pelo usuário.
- **ggml-org e contribuidores de llama.cpp/ggml**: runtime de inferência C/C++,
  MIT. Fonte exata e checksum em `resources/local-stories-model.json`.
- O código de integração do Tonal mantém C++20. O subprojeto externo usa
  seu padrão C++17 para compatibilidade com seus literais UTF-8.

Licenças completas: `packaging/licenses/Qwen-Apache-2.0.txt` e
`packaging/licenses/llama.cpp-MIT.txt`, copiadas para os pacotes Windows e assets
Android. As revisões são fixas: não buscar automaticamente `master`/`main`
durante builds ou downloads. `packaging/fetch-story-runtime.py` verifica o
arquivo-fonte antes de extraí-lo; não baixa pesos. `build-stories-local.ps1`
compila o helper Windows ou as bibliotecas JNI ARM64/x86_64.

O Tonal original continua sendo de **Matheus Nunes (natheus)**,
https://github.com/natheus/tonal-player, MIT. Esta integração não altera essa
autoria nem representa parceria com Qwen, ggml-org ou Spotify.

A versão 0.15.0 usou Qwen2.5-0.5B-Instruct Q4_K_M (Apache 2.0). Seu resumo
com oito letras falhou na aceitação do usuário. A troca na 0.15.1 é opcional,
com tamanho explícito; modelos não fazem parte do backup JSON da biblioteca.
