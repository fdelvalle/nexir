# NEXIR

**NEXIR** é um projeto experimental de runtime binário e aplicações gráficas orientadas à geração por agentes de IA.

## Estado

O repositório foi preparado para receber a base **NEXIR 0.6**. O objetivo seguinte é a versão **0.7**, com motor 2D genérico e bytecode validado.

- Planejamento e critérios de aceite: [issue #1](https://github.com/fdelvalle/nexir/issues/1)
- Plataforma inicialmente suportada: Linux/WSL com X11; macOS depende de backend gráfico específico ou alternativa portátil.

## Importar a versão 0.6

Copie os fontes e exemplos da versão 0.6 para este repositório, sem sobrescrever `AGENTS.md` nem `.gitignore`.

```bash
bash compile.sh
cc -std=c11 -O2 -Wall -Wextra test_game_vm.c -o test_game_vm -lm
./test_game_vm
./nexir-runtime --validate wallet.nx
./nexir-gui wallet.nx river.nxg
```

No Ubuntu/WSL, instale previamente `build-essential libx11-dev pkg-config`. Não confunda a validação headless com teste automatizado completo da interface gráfica.
