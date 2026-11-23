# Instruções para agentes de programação — NEXIR

## Objetivo
Transformar NEXIR em um runtime binário portátil que suporte aplicações gráficas e jogos escritos como arquivos `.nx`/`.nxg`, sem geração obrigatória de código-fonte humano no runtime.

## Estado e compatibilidade
- A base conhecida mais recente é NEXIR 0.6, escrita em C11, com backend X11.
- `runtime.c` implementa a VM de aplicação; `gui.c` implementa shell visual e jogo; `game_vm.h` valida/despacha opcodes NXG6.
- `build.py`/`make_game.py` geram programas binários de exemplo e não são dependências de execução.
- Preserve `wallet.nx`, `calculator.nx`, `toggle.nx`, `river.nxg`, `river_fast.nxg`, `river_training.nxg` ou documente claramente qualquer mudança de formato.

## Próximo milestone
Consulte a [issue #1](https://github.com/fdelvalle/nexir/issues/1) antes de implementar a versão 0.7.

## Regras
1. Não introduzir execução de bytes sem validação de limites, tipos e instruções.
2. Priorizar parsing seguro, arquitetura independente do sistema e testes headless determinísticos.
3. Manter a interface X11 atrás de uma abstração de renderização; não presumir que X11 exista no macOS.
4. Não versionar executáveis compilados, arquivos temporários, credenciais ou dados pessoais.
5. Registrar comandos executados e distinguir testes realmente realizados de testes planejados.
6. Trabalhar em branch, enviar pull request com riscos e resultados de testes antes de integrar na `main`.
