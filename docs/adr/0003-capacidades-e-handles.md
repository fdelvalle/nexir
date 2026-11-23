# ADR 0003 — Capacidades síncronas e handles por invocação

**Estado:** aceita para Core 1.0.

O host concede permissões por operação e escopo. O núcleo valida a
identidade e assinatura de cada grant, vincula o tipo nominal de handle ao
seu produtor e entrega ao adaptador somente os recursos recebidos naquela
chamada. Handles pertencem à invocação corrente, são descartados mesmo em
caso de erro e nunca são serializados. O host implementa escopos concretos,
cotas de I/O e limites de tempo.

As capacidades são síncronas. Suspensão, retomada e cancelamento precisam de
uma extensão com semântica para continuations e descarte de recursos.
A v1 reserva a evolução por versão da IR e das interfaces.
