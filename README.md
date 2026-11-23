# NEXIR

**NEXIR Core 1.0** é uma linguagem binária tipada para emissão direta por
agentes. A [especificação da IR](docs/ir-v1.md) define a linguagem. O núcleo
C11 em `core/` é sua primeira implementação, com parser, verifier,
interpreter, emissor e inspector. A biblioteca compila sem X11, SDL ou jogos.

**NEXIR Applications 0.7** reúne o trabalho experimental de GUI e jogos no
[PR #2](https://github.com/fdelvalle/nexir/pull/2). Esta branch preserva a
[base legada 0.6](IMPORT_STATUS.md) e seu comando `bash compile.sh`, que
requer X11. O build do Core não executa esse script nem carrega os formatos
legados.

## Compilar e experimentar

```sh
make core
make test
make sanitize
make demo
./build/core/nexir --inspect build/core/panel.nxir
```

`make core` produz `build/core/libnexir.a`. A API pública está em
[`core/include/nexir.h`](core/include/nexir.h). Um emissor inicializa o
`NxBuilder`, preenche as tabelas tipadas e chama `nx_emit`. Os exemplos C
fazem chamadas à API para gerar bytes e carregar os módulos com `nx_load`.
O C usado no gerador é código do host. A linguagem NEXIR não tem etapa
de compilação a partir de fonte textual. A integração futura para agentes
usará a API por chamadas estruturadas.

| Módulo | Comportamento no demo | Bytes da IR |
| --- | --- | ---: |
| `calculation.nxir` | Calcula `(7 + 12) × 12` = 228 | 123 |
| `expense.nxir` | Aprova uma despesa positiva de 7 com limite de 12 | 229 |
| `panel.nxir` | Usa a mesma política e publica a decisão em uma UI simulada | 274 |

Os três módulos são gerados em `build/core/` e passam pelo mesmo loader e
interpreter. Os tamanhos são dos módulos da demonstração e não constituem
benchmark comparativo. Custo de geração, tempo de validação, execução e
composição por agentes devem ser medidos com cargas equivalentes em um
trabalho posterior.

As decisões de arquitetura estão em [docs/adr](docs/adr). A suíte de
conformidade cobre encoding canônico, rejeição de bytecode inválido, tipos,
fluxo de controle, erros de aritmética, orçamento e permissões.
