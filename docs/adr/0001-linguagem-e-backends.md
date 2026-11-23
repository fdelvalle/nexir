# ADR 0001 — Linguagem binária e backends independentes

**Estado:** aceita para Core 1.0.

A especificação NEXIR IR é o contrato da linguagem. C11 implementa parser,
verifier, interpreter, emissor e inspector de referência. O parser decodifica
tabelas limitadas, o verifier checa o módulo inteiro e a execução recebe um
`NxModule` opaco produzido pelo loader. O emissor recebe tabelas e operandos
estruturados por API e aplica o mesmo verifier antes de serializar.

O inspector só renderiza um módulo verificado. A ferramenta MCP de
emissão recebe o módulo em uma chamada estruturada, com diagnóstico de
autoria. Agentes não precisam escrever C ou Python. A fronteira do módulo verificado também serve como ponto de partida
para JIT/AOT, com a mesma suíte de conformidade.

NEXIR Applications 0.7 mantém a trilha experimental X11, jogos e formatos
legados no PR #2. O Core 1.0 não importa seus cabeçalhos, parsers, loops ou
renderizadores. A adaptação de River Flight para usar a IR genérica será
uma tarefa da linha Applications. A demo de UI deste núcleo usa um host
simulado.
