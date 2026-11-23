# Emissão binária por agentes

`nexir_emit` é uma ferramenta MCP que recebe um objeto estruturado e devolve
um módulo [NEXIR IR 1.0](ir-v1.md) validado. O objeto de autoria é transitório.
A representação executável é o binário produzido pelo emitter C11.
O servidor só emite módulos. Declarar uma capacidade na chamada não concede
permissões nem aciona o host da VM.

## Iniciar o servidor

Compile a ponte nativa com `make agent`. O servidor usa Python 3.9 ou
posterior e sua biblioteca padrão. O cliente MCP pode executar
`python3 /caminho/para/nexir/tools/agent/server.py` por stdio.
No Codex CLI, registre-o a partir do checkout:

```sh
make agent
codex mcp add nexir-emitter -- python3 "$PWD/tools/agent/server.py"
```

## Chamar a ferramenta

Use `tools/list` para descobrir o esquema da ferramenta. Em `tools/call`,
envie `name: "nexir_emit"` e passe o módulo dentro de `arguments.module`.
O [exemplo de cálculo](../tools/agent/calculation.json) é um objeto
`arguments` completo. Ele define uma função com dois parâmetros `i64`,
calcula `(a + b) × b` e exporta a função como `main`.

- Tipos primitivos podem ser referenciados pelo nome ou pelo índice. Os
  tipos compostos declarados em `types` começam no índice 4.
- `functions` e `exports` são obrigatórios. `types`, `constants` e
  `imports` são opcionais. A ordem das listas segue a especificação da IR.
- Os operandos de cada instrução são índices ou imediatos inteiros, conforme
  a tabela de opcodes. Índices SSA são locais ao bloco. Para `br.if`,
  informe condição, aresta verdadeira e aresta falsa na mesma lista.
- Literais `i64` usam strings decimais canônicas para preservar todos os
  64 bits em qualquer cliente JSON. Literais `bytes` usam base64 padrão
  com padding quando necessário. Literais `text` e `bool` usam string e
  booleano JSON.

O resultado MCP contém um bloco de texto com JSON. Quando `ok` é verdadeiro,
`bytecode_base64` contém o binário canônico, `size` contém o número de bytes
e `sha256` permite conferir a integridade após decodificar o base64.
O agente pode entregar o binário a um host NEXIR. O host aplica verificação,
permissões e limites ao executar o módulo, como faria com qualquer outra
origem de bytecode.

Erros de autoria retornam `isError: true` com `diagnostic.path` e
`diagnostic.message`. Quando o verifier rejeita uma instrução, o caminho
identifica sua posição na chamada. Outros diagnósticos nativos podem
retornar o caminho do módulo acompanhado de `native_index`. Envie o módulo
corrigido em outra chamada. O servidor é sem estado entre emissões.

## Verificar

`make agent-test` usa o protocolo MCP por stdio. Emite cálculo, autorização
de despesas e painel com capacidade de UI simulada e compara seus binários
com os produzidos pela API C de referência. A suíte também testa repetição
determinística, erro de SSA, precisão de `i64`, literais inválidos,
duplicatas JSON e limite de 2 MiB por mensagem.
