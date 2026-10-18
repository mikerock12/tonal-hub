# Tonal Hub 0.8 — capas, repertório e letras

Atualização privada para Windows x64 e Android (ARM64 no POCO X7 Pro).
Derivada de tonal-player, de Matheus Nunes (natheus), sob MIT. Veja CREDITS.md.

## Música, álbum e artista

Os controles mantêm proporções corretas em qualquer largura; a página da música usa botões centrais circulares no Android. As faixas simplificadas de álbuns recebem nome, identificação e imagem do álbum antes de entrar na fila. Atualizações incompletas e favoritos preservam metadados já conhecidos. A capa é atualizada quando os dados chegam depois da abertura da página.

No Android, as imagens usam até 8 MiB de bitmaps compartilhados, 32 MiB de cache de arquivos, dois trabalhadores e três tentativas limitadas. Nenhuma credencial Spotify é enviada aos servidores de imagens. A página de álbum organiza capa, título, artista, data por extenso e quantidade de faixas; a de artista destaca foto, nome e estilos quando fornecidos.

Use o botão **Estatísticas** para mostrar/ocultar escutas e minutos nas listas. Os totais são do histórico registrado neste aparelho/player (até 180 dias), não do histórico integral da conta Spotify. Uma escuta qualificada exige 30 segundos ou metade da faixa curta. Pausas e saltos não contam como tempo ouvido.

A biblioteca mantém apenas o contexto da aplicação. Ao fechar a atividade, consultas de metadados são canceladas e o próximo player usa o cliente Spotify atual, evitando reter a janela anterior ou uma conexão já encerrada.

No Windows, o estado confirmado do Spotify Connect também reconcilia a pausa da saída nativa. Isso recupera uma retomada remota que não veio acompanhada do evento de transporte esperado. Comandos locais recentes têm uma janela de proteção, e respostas iniciadas antes deles são descartadas, para uma consulta antiga não desfazer a pausa do usuário.

## Repertório e metadados

Os cards separam tempo de escuta, temas das letras, artistas, álbuns e estilos. Informações ausentes não ocupam posições no ranking como “Não informado”. Isso não transforma ausência em informação conhecida: a tela informa quando há dados pendentes.

O Tonalify consulta a API Spotify e usa MusicBrainz como apoio para estilos. O IndieTonal também consulta MusicBrainz. A busca recusa homônimos sem identificação segura; pode cruzar o crédito de um álbum para resolver a ambiguidade. Consultas MusicBrainz são espaçadas, a fila é limitada e resultados são reaproveitados. Respostas 503 ou falta de dados podem deixar lacunas até uma próxima consulta. A fonte do estilo é preservada.

TonalMuse Tiny continua sendo um classificador local de 32 KiB treinado em exemplos sintéticos. Os temas são pistas experimentais, não uma avaliação psicológica. A atualização não introduz uma LLM nem executa inferência no callback de áudio.

## Sincronia das letras

- Desmarque **Sincronia** para ler e rolar sem destaque automático.
- Toque/clique na linha que está ouvindo para alinhar a letra ao momento atual do áudio. Isso não pula a música.
- O ajuste é salvo por música e impressão digital da versão da letra. Trocar a gravação/letra não reaproveita indevidamente o ajuste.
- Letras sem tempos podem receber marcas manuais por linha; não é inventada uma sincronização automática.
- **Exportar ajuste** salva um JSON com identificação da faixa e ajuste local, sem copiar a letra. Serve como registro para revisão/contribuição. Não envia feedback automaticamente ao LRCLIB.

## Tradução e aparência

Uma falha isolada não interrompe as linhas seguintes. Há duas novas tentativas para falhas temporárias, progresso por linha e retomada das pendências pelo botão de tradução. As linhas traduzidas em cache também notificam a interface, inclusive o final da letra. O cache de sessão é reaproveitado ao reabrir a tela. O idioma de origem é automático.

Há proteção local de 512 KiB/dia enviados para tradução; não é uma cota garantida pelo provedor. O serviço de tradução pode limitar ou falhar. Letras estrangeiras e tradução permanecem emparelhadas por linha, sem mudar os tempos originais.

As paletas substituem imediatamente o código hexadecimal e aplicam a escolha. Letra e tradução têm fontes, tamanhos, cores, cor de contorno/brilho, negrito, itálico e sublinhado independentes. Efeitos: normal, contorno, brilho, neon e piscante. Os efeitos visuais são opcionais; o piscante só anima durante reprodução e a apresentação pausa fora de foco. Prefira sem animação para o menor custo visual.

No Android, os controles da tela cheia recolhem após seis segundos. Toque para mostrá-los novamente. Preferências e ajustes acompanham o backup pessoal; contas e senhas ficam de fora.
