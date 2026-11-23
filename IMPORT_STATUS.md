# Importação da NEXIR 0.6

Importação parcial feita pelo ChatGPT diretamente via conector GitHub, sem acesso ao computador local.

## Já publicados
- `build.py` — gerador dos arquivos `.nx`
- `make_game.py` — gerador dos arquivos `.nxg`
- `game_vm.h` — máquina de jogo
- `compile.sh` — compilação X11
- `test_game_vm.c` — teste headless

## Ainda necessários para compilar
- `runtime.c`
- `gui.c`

## Arquivos binários gerados pelos scripts
- `wallet.nx`, `calculator.nx`, `toggle.nx` — `python3 build.py`
- `river.nxg`, `river_fast.nxg`, `river_training.nxg` — `python3 make_game.py`

Os executáveis compilados `nexir-gui`, `nexir-runtime` e `test_game_vm` não devem ser versionados.

**Atenção:** a base ainda não compila até que `runtime.c` e `gui.c` sejam importados. Não marcar a issue #1 como concluída antes disso.
