# Instruções para agentes de programação — NEXIR

## Objetivo

NEXIR é uma linguagem binária tipada para geração direta por agentes. A
[especificação da IR](docs/ir-v1.md) define a linguagem. O código C11 implementa
a VM de referência. Mudanças de semântica, formato ou limites devem
atualizar a especificação e a suíte de conformidade.

## Linhas de desenvolvimento

- **Core 1.0** contém o formato, parser, verifier, interpreter, emitter e
  inspector em `core/`. Use `make core test` e `make sanitize`.
- **Applications 0.7** contém as experiências de GUI e jogos do
  [PR #2](https://github.com/fdelvalle/nexir/pull/2). Mantenha X11 e os
  formatos legados fora do Core.
- Preserve os exemplos 0.6 listados em [IMPORT_STATUS.md](IMPORT_STATUS.md).
  Consulte a [issue #1](https://github.com/fdelvalle/nexir/issues/1) para
  mudanças na linha Applications.

## Regras
1. Não introduzir execução de bytes sem validação de limites, tipos e instruções.
2. Priorizar parsing seguro, arquitetura independente do sistema e testes headless determinísticos.
3. Manter a interface X11 atrás de uma abstração de renderização; não presumir que X11 exista no macOS.
4. Não versionar executáveis compilados, arquivos temporários, credenciais ou dados pessoais.
5. Registrar comandos executados e distinguir testes realmente realizados de testes planejados.
6. Trabalhar em branch, enviar pull request com riscos e resultados de testes antes de integrar na `main`.
