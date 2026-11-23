# NEXIR

**NEXIR** é um projeto experimental de runtime binário e aplicações gráficas orientadas à geração por agentes de IA.

## Estado

O repositório foi preparado para receber a base **NEXIR 0.6**. O objetivo seguinte é a versão **0.7**, com motor 2D genérico e bytecode validado.

- Planejamento e critérios de aceite: [issue #1](https://github.com/fdelvalle/nexir/issues/1)
- Plataforma inicialmente suportada: Linux/WSL com X11; macOS depende de backend gráfico específico ou alternativa portátil.

## Base 0.6

O [estado da importação](IMPORT_STATUS.md) lista os arquivos da 0.6. O alvo
`make legacy-fixtures` gera os seis exemplos originais em `build/legacy/`.
O alvo `make test` também compila e executa o teste headless original da
VM NXG6 com os três arquivos River Flight.

## Motor 2D NXG7

O motor executa regras de cena, teclado e colisão com passo fixo de 1/60 s.
`src/parser.c` valida o programa inteiro, `src/engine.c` executa as regras e
atualiza o mundo, e `src/x11.c` implementa o frontend gráfico. A lógica das
demonstrações fica em `examples/build.py`. Consulte a
[especificação NXG7](docs/nxg7.md) para criar outros jogos.

Em Debian, Ubuntu ou Ubuntu no WSL, instale as dependências e compile com
GNU Make 4.3 ou posterior.

```sh
sudo apt-get install build-essential libx11-dev pkg-config python3
make
make test
make sanitize
```

Com um servidor X11 disponível, inclusive via WSLg com `DISPLAY` configurado,
execute qualquer uma das demonstrações com o mesmo binário.

```sh
make run
make run GAME=build/bounce.nxg
```

- **Crossing** usa as setas esquerda, direita e cima para atravessar as faixas
  móveis e alcançar a linha verde. Espaço reinicia após a travessia.
- **Bounce** usa esquerda e direita para manter a bola em jogo. Espaço
  reinicia após perder.
- **Esc** fecha a janela em ambos.

No macOS, o caminho headless usa as ferramentas de linha de comando do Xcode,
Python 3 e GNU Make 4.3 ou posterior (`gmake test`). Para tentar o frontend
X11, instale e inicie o XQuartz, disponibilize `pkg-config` e execute
`gmake run` com `PKG_CONFIG_PATH=/opt/X11/lib/pkgconfig`. Se necessário,
ajuste o caminho para a instalação local do XQuartz. Backend Cocoa e teste
no macOS ficam pendentes.

### Resultados e limitações

- Debian 13, GCC, sessão sem `DISPLAY`: `make test` e `make sanitize` passaram
  com os dois jogos. Os testes cobrem validação, regras, entrada, passo fixo,
  colisão, transição de cena e chamadas ao renderizador headless.
- `make all` compilou o frontend com os headers X11 Debian extraídos
  localmente (`X11_CFLAGS` apontando para os headers e
  `X11_LIBS=-l:libX11.so.6`). `make smoke` passou em Xvfb, confirmando que os
  dois arquivos abrem uma janela visível e que Esc encerra o processo.
- WSL, WSLg e macOS não estavam disponíveis nesta verificação. O teste com
  Xvfb cobre abertura e fechamento, sem verificar a jogabilidade visual.
- O teste headless da VM 0.6 carregou e executou os três arquivos River Flight
  gerados pelos scripts originais. Os três aplicativos `.nx` foram gerados,
  mas a importação de `runtime.c` e `gui.c` ainda é necessária para testar sua
  execução, verificar a interface NXG6 e migrar o River Flight. Crossing e
  Bounce são exemplos novos. O frontend NXG7 aceita apenas a nova assinatura.
