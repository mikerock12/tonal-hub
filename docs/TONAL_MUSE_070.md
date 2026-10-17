# Tonal 0.7.0 — repertório, apresentação e modelo local

## Como usar

- O coração fica azul preenchido quando a faixa está favorita, no miniplayer e na página da música.
- Buscar mostra até 20 buscas recentes; é possível repetir, remover e limpar o histórico.
- Sua música → Estatísticas: escolha artistas, músicas, álbuns, estilos ou dias. Clique/toque numa barra para ver escutas qualificadas, tempo realmente ouvido, faixas distintas e detalhes dos álbuns.
- O card Clima do repertório abre temas, cobertura da análise, artistas, álbuns e estilos. Sugestões vêm das relações editoriais por gênero em `resources/music-rules.json`.
- Na página de letras, os ícones permitem atualizar, traduzir, personalizar fontes/cores e expandir para tela cheia. Na tela cheia, toque/clique para revelar ou recolher os controles; eles se recolhem após 3,5 segundos. Escape/Voltar sai da apresentação.
- Existem 11 opções de cenário: Aurora, Ondas, Espectro, Capa, Foto, Estrelas, Chuva, Orbitas, Pulso, Grade e Sem animação. Ondas/Espectro usam PCM; os demais são efeitos decorativos.
- Letra e tradução têm fonte, cor e tamanho independentes. Tudo fica no SQLite pessoal e no backup exportável, junto com favoritos, playlists, histórico e buscas. Letras completas não são gravadas nesse banco.

## TonalMuse Tiny v1

Classificador multilabel linear com características de palavras, pares e fragmentos de palavras. **Não é LLM nem chat**. Os oito temas são amor, saudade, perda, rebeldia, esperança, celebração, tensão e introspecção. Usa pesos int8, oito escalas e vieses: **32.832 bytes**, sem runtime de aprendizado de máquina ou modelo externo.

O mesmo código C++ executa no Windows e no Android via JNI. A inferência ocorre ao carregar/importar letras, sem rede e fora dos callbacks de áudio. Posições, tempos, bandas e álbuns vêm dos metadados e do diário de escutas; o modelo não inventa essas informações. Os resumos combinam evidência temática com estatísticas verificáveis. Classificações abaixo do limiar 0,60 não destacam tema. Esses escores não são probabilidades calibradas.

Treino reprodutível, apenas Python padrão:

```powershell
python training/train_muse.py
```

Dados sintéticos originais PT/EN/ES em `resources/muse-training.json`, treino em `training/train_muse.py`, pesos em `src/insights/MuseWeights.inc`, inferência em `src/insights/TinyMusicModel.cpp`. Código, pesos e dados sob MIT, preservando a licença e os créditos do Tonal original de **Matheus Nunes (natheus)**.

O conjunto interno tem 2.290 exemplos de treino e 630 de validação por famílias de templates separadas, mas vocabulário compartilhado. O resultado perfeito nessa validação é apenas uma verificação interna. Em 16 situações sintéticas escritas separadamente, aceitou e acertou 12, abstendo-se em 4. **Não há estimativa de precisão em letras reais.** Metáforas, ironia, negação complexa e idiomas fora do conjunto podem falhar. A análise descreve o repertório, sem diagnóstico emocional ou de saúde.

Relatórios: `muse-training-v1.json`, `muse-evaluation-v1.json` e `personal-070-tests.json`. Em execução local, o núcleo C++ fez a inferência curta em aproximadamente 2,8–3,3 µs; no AVD, o caminho Java completo (normalização, JNI e JSON) mediu aproximadamente 4,7–5,4 ms por chamada em uma bateria de 1.000. São caminhos e ambientes distintos, não um comparativo de aparelhos.

## Limites e consumo

Entrada do modelo limitada a 12.000 bytes, 1.200 palavras e até 768 características. Animações limitadas a 20 fps, com quantidades fixas de elementos; ficam estáticas em pausa e suspendem o desenho fora da janela ativa. O classificador não roda por quadro. A apresentação usa fontes do sistema e desenhos vetoriais, sem vídeos de fundo ou biblioteca de LLM.

A tradução continua dependendo do Google Tradutor e a letra do LRCLIB. Somente a análise temática é inteiramente local. A disponibilidade e a versão exata das letras dependem desses serviços.

Os testes do emulador não substituem a audição no POCO ou no Bluetooth do carro. Esta versão não refaz o benchmark lado a lado com Spotify; o relatório diferencia a medição exploratória de CPU/RAM da validação funcional.

### Medição exploratória no AVD x86_64

| Cenário (8 segundos) | CPU, percentual de um núcleo | RAM PSS |
| --- | ---: | ---: |
| Áudio local | 4,1% | 48,4 MiB |
| Áudio local + apresentação animada | 15,0% | 55,6 MiB |

1.685.376 frames processados, zero underruns, zero blocos DSP acima do orçamento nessa execução. O custo da animação é real; Sem animação evita o desenho contínuo. Amostras curtas num computador compartilhado, sem medir bateria ou Bluetooth físico, sem extrapolar esses números para o POCO. O cenário de áudio local não mede letras reais nem a precisão temática.
