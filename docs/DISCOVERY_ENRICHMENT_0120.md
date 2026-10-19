# Descoberta 2+1 — 0.12.0

## Modos independentes

As músicas externas aceitas passam a fazer parte da fila temporária. Shake
e Aleatório reorganizam essa fila preservando suas descobertas e a faixa atual.
Não fazem outra consulta ao catálogo nem substituem as sugestões aceitas.

Na página da música, toque novamente em Descoberta para desligá-la. A prévia
remove as próximas descobertas; Aplicar confirma a fila. Se uma descoberta
estiver tocando, ela termina normalmente. Reativar após desligar permite outro
sorteio. Abrir outra seleção também pode preparar novas sugestões.

A cadência continua sendo duas originais e uma descoberta. Se restarem poucas
originais, as descobertas já aceitas que sobraram ficam ao final, sem descarte.
O Aleatório usa prioridades de reprodução; Shake usa Fisher–Yates. A fila
salva na playlist continua intacta. Fechar a prévia sem aplicar não troca a
reprodução. O estado dos modos acompanha a fila ao reabrir os controles.

## Letras das candidatas, sem esperar para tocar

Ao preparar uma nova Descoberta com candidatos de catálogo, a fila é calculada
imediatamente com as informações disponíveis. Em paralelo, até 12 referências
da seleção e 120 candidatas únicas entram na análise de letras. Preparar outra
seleção substitui o trabalho pendente. Não se percorre todo o catálogo.

O LRCLIB recebe título, artista, álbum e duração pelo endpoint `/api/get`.
O app confere título/artista normalizados e duração com tolerância de cinco
segundos antes de usar uma resposta. Uma letra indisponível não é inventada.
O classificador TonalMuse existente analisa localmente até 12 mil caracteres;
somente os oito escores de temas alimentam a afinidade já documentada.

Os resultados valem para **preparações futuras**. A análise não altera a fila
confirmada, não busca uma tradução e não mexe na letra exibida ou no transporte.
Há controle **Analisar letras das candidatas em segundo plano** na janela Fila.
Desligá-lo cancela o trabalho pendente; escores previamente armazenados continuam
disponíveis até expirar. Os pedidos usam rede externa; a inferência é local.

| Proteção | Limite |
|---|---|
| Requisições | Uma por vez; intervalo mínimo de 1,25 s |
| Prazo por pedido | Até 6 s; resposta de até 256 KiB |
| Serviço ocupado | Espera de 10–60 s em HTTP 429/503; nova seleção respeita a espera |
| Cache | Até 2.000 entradas; gravação atômica no trabalhador de disco existente |
| Validade | 30 dias para resposta compatível; 1 dia para 404; 15 min para falha transitória/incompatível |
| Dados persistidos | Assinatura SHA-256 dos metadados, escores, validade e preferência; sem letra completa |

O cache é derivado e reconstruível, separado de favoritos e estatísticas.
Não cria escutas artificiais nem substitui o backup da biblioteca. A cobertura
depende do acervo LRCLIB e da rede; um lote completo pode levar minutos.

Esta é a primeira etapa do enriquecimento. **Não há extração de BPM, timbre ou
tonalidade nesta versão.** Esses sinais exigem metadados acessíveis ou análise
de áudio em uma etapa posterior. TonalMuse continua sendo um classificador
experimental de corpus sintético, sem precisão demonstrada em letras reais.

## Validação

Regressões no núcleo C++, interface QML, integração do catálogo Windows e
JNI/interface Android cobrem retenção das descobertas, independência dos modos,
desativação, cache, falhas e cancelamento. O relatório final fica em
[discovery-enrichment-0120-tests.json](discovery-enrichment-0120-tests.json).

Não houve novo benchmark de CPU/RAM nem teste físico de Bluetooth nesta entrega.
Os números históricos continuam em [BENCHMARKS.md](BENCHMARKS.md).

Regras completas: [DESCOBERTA_2_MAIS_1.md](DESCOBERTA_2_MAIS_1.md).
API: [documentação LRCLIB](https://lrclib.net/docs).
Base original: Matheus Nunes (natheus), [Tonal](https://github.com/natheus/tonal-player), MIT.
