## Instalar

| Arquivo | Para quem |
|---|---|
| **Tonal-{VERSION}-setup.exe** | Instalador. Não precisa de administrador, cria atalho no Menu Iniciar e aparece em "Adicionar ou remover programas". |
| **Tonal-{VERSION}-win64.zip** | Versão portátil: descompacte numa pasta e abra o `Tonal.exe`. |

**Requisitos:** Windows 10 (1809) ou 11, 64 bits, e uma conta **Spotify Premium**. Na primeira vez, o Tonal guia a
criação de um app gratuito no painel de desenvolvedor do Spotify (uns 2 minutos).

O executável não é assinado digitalmente: o SmartScreen pode mostrar "O Windows protegeu o computador". Clique em
**Mais informações → Executar assim mesmo**. Os hashes SHA-256 estão em `SHA256SUMS.txt`.

## Inclui

- Qt {QT} (LGPLv3) e o runtime do Visual C++
- librespot {LIBRESPOT} (MIT), o motor de áudio
- Licenças na pasta `licenses`

Não precisa instalar Qt, Visual Studio nem Rust.
