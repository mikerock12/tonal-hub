param([switch]$TransportOnly)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$env:PATH="$env:USERPROFILE/.cargo/bin;$env:PATH"
Push-Location $root
try {
    cargo build --manifest-path native/jam-transport/Cargo.toml --release --locked --offline --jobs 2
    if($LASTEXITCODE){throw 'Build do transporte Jam falhou'}
    Copy-Item -LiteralPath native/jam-transport/target/release/TonalJam.exe -Destination out/build/release/TonalJam.exe -Force
    if(-not $TransportOnly){
        & out/tools/python/Scripts/python.exe -X utf8 packaging/prepare-jam-cli.py
        if($LASTEXITCODE){throw 'Verificação das fontes do CLI falhou'}
        Push-Location out/jam-cli-source/librespot-0.8.0
        try {
            cargo build --release --offline --no-default-features --features rustls-tls-webpki-roots --jobs 2
            if($LASTEXITCODE){throw 'Build do CLI com protocolo Jam falhou'}
            Copy-Item -LiteralPath target/release/librespot.exe -Destination (Join-Path $root 'out/build/release/librespot.exe') -Force
        }finally{Pop-Location}
    }
}finally{Pop-Location}
