# NEXIR 0.7

NEXIR executa aplicativos binários `.nx` e jogos `.nxg` com um frontend X11.
O motor NXG7 usa entidades, primitivas, cenas, teclado, colisão AABB e um
passo fixo de 1/60 s. O parser valida o bytecode inteiro antes da execução.
As regras dos jogos ficam nos arquivos gerados por `examples/build.py`.
A [especificação NXG7](docs/nxg7.md) descreve o formato e as operações.

## Compilar e testar

Em Debian, Ubuntu ou Ubuntu no WSL, instale as dependências. Use GNU Make
4.3 ou posterior.

```sh
sudo apt-get install build-essential libx11-dev pkg-config python3
make
make test
make sanitize
```

`make test` funciona sem X11 e cobre o motor NXG7, a VM de aplicativos NXIR
v3 e a VM NXG6. Para o teste de janelas em um servidor virtual, instale
`xvfb`, `xauth` e `xdotool` e execute `make smoke`.

## Executar com X11 ou WSLg

Com um servidor X11 disponível e `DISPLAY` configurado, use o mesmo
`nexir-gui` para iniciar os três jogos NXG7.

```sh
make run
make run GAME=build/bounce.nxg
make run GAME=build/crossing.nxg
```

- **River Flight** usa esquerda/direita ou A/D para navegar, Espaço para
  disparar e R para reiniciar. Desvie das margens e obstáculos e recolha
  os itens verdes para reabastecer.
- **Bounce** usa esquerda/direita para mover a raquete. Espaço reinicia
  após perder.
- **Crossing** usa esquerda/direita e cima para atravessar as faixas móveis
  e alcançar a linha verde. Espaço reinicia após a travessia.
- **Esc** fecha a janela.

No macOS, o caminho headless requer as ferramentas de linha de comando do
Xcode, Python 3 e GNU Make 4.3 ou posterior (`gmake test`). Para experimentar
o frontend X11, inicie o XQuartz e execute `gmake run` com `pkg-config`
disponível e `PKG_CONFIG_PATH=/opt/X11/lib/pkgconfig`. Ajuste esse caminho
se necessário para a instalação local do XQuartz.

## Arquivos da versão 0.6

Os formatos NXIR v3 e NXG6 continuam disponíveis pelos comandos legados.
F3 abre o River Flight original no shell X11. A migração NXG7 usa pistas de
obstáculos fixas, um projétil e encerramento na primeira colisão ou quando o
combustível acaba. Pontuação, três vidas, margens sinuosas, pausa e variantes
de velocidade e treinamento continuam disponíveis no modo NXG6.

```sh
./nexir-runtime --validate build/legacy/wallet.nx
./nexir-runtime --run-seq build/legacy/toggle.nx 2 2
./nexir-runtime build/legacy/calculator.nx
./nexir-gui build/legacy/wallet.nx build/legacy/river.nxg
```

Os exemplos da 0.6 são gerados sem alteração de formato. Consulte
[o estado da importação](IMPORT_STATUS.md) para a lista de arquivos.

## Resultados locais

- Debian 13 e GCC: `make all`, `make test`, `make sanitize` e `make smoke`
  passaram. O build X11 usou headers extraídos dos pacotes Debian e
  `X11_LIBS=-l:libX11.so.6`.
- Os testes headless executaram Wallet, Calculator e Toggle, as três
  variantes NXG6 e regras dos jogos NXG7. ASan e UBSan rodaram os testes
  do motor NXG7, da VM de aplicativos e da VM NXG6.
- No Xvfb, os três jogos NXG7, Wallet e o shell com as três variantes NXG6
  abriram e fecharam com Esc. O smoke test cobre a abertura, a navegação
  até a aba River Flight e o encerramento.
- WSL, WSLg e macOS não foram testados. O backend Cocoa fica pendente.
  A colisão NXG7 é discreta e pode perder contatos entre passos.
