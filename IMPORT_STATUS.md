# Importação da NEXIR 0.6

A base inclui `runtime.c`, `gui.c`, `game_vm.h`, `compile.sh`, os geradores
e o teste headless original. Esses arquivos fazem parte da base de
compatibilidade da linha Applications.

- `python3 build.py` gera `wallet.nx`, `calculator.nx` e `toggle.nx`.
- `python3 make_game.py` gera `river.nxg`, `river_fast.nxg` e `river_training.nxg`.
- `bash compile.sh` compila os executáveis legados e requer X11.
- `cc -std=c11 -Wall -Wextra test_game_vm.c -o test_game_vm -lm` compila o
  teste headless. Rode `./test_game_vm` no diretório dos exemplos NXG6.

O build da biblioteca Core 1.0 usa `make core`.
