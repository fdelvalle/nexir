# ADR 0002 — SSA por bloco, codificação canônica e aritmética

**Estado:** aceita para Core 1.0.

Índices SSA são locais ao bloco. Arestas passam argumentos explicitamente e
de forma simultânea. Assim, a validade de uma referência de valor depende
da disposição do próprio bloco e das assinaturas dos destinos.

A codificação usa largura fixa, seções em ordem, tabelas ordenadas quando
seus elementos têm uma chave, referências verificáveis e UTF‑8 estrito
para texto. Declarações e operações cuja posição define identidade mantêm
a ordem. O fixture de identidade testa a emissão byte a byte.

Inteiros são assinados de 64 bits. As operações detectam overflow antes
da operação C correspondente. A divisão trunca em direção a zero e rejeita
divisor zero e `INT64_MIN / -1`. Falhas abortam a invocação com código explícito.
