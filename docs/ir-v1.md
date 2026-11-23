# NEXIR IR 1.0

Esta especificação descreve a linguagem binária. A API C11 em
[`core/include/nexir.h`](../core/include/nexir.h) é a interface da implementação
de referência. Nenhum layout de struct C faz parte do formato.

## Contêiner e codificação canônica

Todos os inteiros de largura fixa usam little-endian. O cabeçalho tem 16 bytes.
Ele contém `NXIRIRBC` (8 bytes ASCII), versão maior `u16=1`, menor `u16=0` e
contagem de seções `u32`. O reader 1.0 admite apenas a versão 1.0. Um arquivo
tem no máximo 262.144 bytes e 32 seções. O cabeçalho de cada seção contém
`id:u16 flags:u16 length:u32`, seguido de exatamente `length` bytes.

As seções 1 a 5 aparecem uma vez cada, nessa ordem, com flags iguais a 1.
Seções adicionais têm IDs estritamente crescentes maiores que 5 e flags
iguais a zero. O reader 1.0 ignora seus payloads. Flags reservadas,
seções obrigatórias desconhecidas, duplicatas, padding e bytes após a última
seção são inválidos. O emissor de referência escreve apenas as cinco seções
obrigatórias.

Cada tabela ou lista começa com `count:u16`, seguido de seus elementos.
Referências e contagens dentro das tabelas são `u16`, salvo indicação em
contrário. Os índices são zero-based. Uma assinatura é o índice do tipo de
retorno seguido da lista dos tipos de parâmetros (até 8).

| ID | Tabela | Codificação de cada elemento | Máximo |
| --- | --- | --- | ---: |
| 1 | Tipos | `kind:u16`, lista de referências | 64 |
| 2 | Constantes | `type:u16 length:u16`, seguido dos bytes do valor | 128 |
| 3 | Importações | Nome da interface, `version:u16`, nome da operação, assinatura | 32 |
| 4 | Funções | Assinatura e lista de blocos | 32 |
| 5 | Exportações | Nome e índice da função | 32 |

Nomes usam `length:u8` e 1–63 bytes ASCII que correspondem a
`[a-z][a-z0-9._]*`. Strings de valor usam UTF‑8 válido em sua forma mais curta,
sem substituição de bytes nem normalização Unicode. NUL é permitido em
valores de texto. Bytes e texto têm comprimento explícito de até 4.096 bytes.
Um `bool` literal tem um byte (`00` ou `01`). Um `i64` literal tem oito bytes
em complemento de dois. Literais compostos e handles literais são inválidos.

Constantes são ordenadas lexicograficamente pela tripla tipo, comprimento e
bytes sem sinal. Importações são ordenadas por interface, versão numérica e
operação. A versão de uma importação começa em 1. Exportações são ordenadas
por nome. Duplicatas nessas três tabelas são inválidas. Tipos com a mesma
espécie e a mesma lista de referências também são duplicatas. Funções,
blocos, instruções e tipos têm ordem declarada, que define sua identidade.
A codificação canônica é única para um mesmo módulo ordenado. Ela não
normaliza módulos com reordenação de declarações ou programas semanticamente
equivalentes. Um emissor que reordene uma tabela deve remapear suas referências.

## Tipos, funções e escopo

A tabela contém os tipos primitivos em ordem fixa. Os índices 0 a 3 são
`bool`, `i64`, `text`, `bytes`, com `kind` igual ao índice e lista vazia.
Os tipos seguintes usam estas espécies:

| Kind | Tipo | Conteúdo da lista |
| ---: | --- | --- |
| 4 | Registro | Tipos dos campos em ordem, de 0 a 8 campos |
| 5 | Variante | Tipos dos payloads em ordem, de 1 a 8 alternativas |
| 6 | Handle | Um índice de importação que identifica a operação proprietária |

Registros e variantes só podem referenciar tipos anteriores. Seus valores
são imutáveis. Um registro sem campos pode representar unidade internamente.
As fronteiras da referência 1.0 são escalares: exportações admitem os quatro
primitivos e importações admitem primitivos e handles. Funções internas podem
usar todos os tipos. Uma operação sem resultado útil na fronteira pode
retornar um booleano de confirmação.

Cada função tem de 1 a 16 blocos. Um bloco contém a lista dos tipos de seus
parâmetros (até 8), a contagem de instruções `u16` e as instruções em ordem.
O bloco 0 é a entrada e seus parâmetros têm exatamente os tipos da assinatura.
Há no máximo 4.096 instruções no módulo.

A identidade SSA é a tripla **(função, bloco, índice local)**. O wire codifica
somente o índice local. Parâmetros ocupam os primeiros índices. Cada instrução
que produz valor acrescenta implicitamente um índice. Só parâmetros ou
definições anteriores do mesmo bloco podem ser referenciados. O limite é de
128 locais por bloco, incluindo parâmetros. Arestas passam os valores
explicitamente, com tipo e aridade idênticos aos parâmetros do destino.
A atribuição dos parâmetros é simultânea. A VM inicia apenas por uma
exportação nomeada e executa cada bloco até seu único terminador final.
O verifier aplica as mesmas regras a blocos inalcançáveis.

## Instruções

Cada instrução contém `opcode:u8`, quantidade de operandos `u8` e operandos
`u16` na ordem da tabela. Uma aresta contém o índice de bloco seguido dos
índices locais dos argumentos. Sua aridade é inferida do bloco de destino.
Não há encoding alternativo, salto por offset em bytes ou acesso a memória
por endereço.

| Opcode | Instrução | Operandos | Resultado |
| ---: | --- | --- | --- |
| 1 | `const` | constante | tipo da constante |
| 2 | `record.new` | tipo, valores dos campos | registro |
| 3 | `record.get` | registro, índice do campo | tipo do campo |
| 4 | `variant.new` | tipo, índice da alternativa, payload | variante |
| 5 | `variant.tag` | variante | índice da alternativa como `i64` |
| 6 | `variant.get` | variante, índice da alternativa | payload |
| 7–10 | `i64.add/sub/mul/div` | dois locais `i64` | `i64` |
| 11–12 | `i64.eq/lt` | dois locais `i64` | `bool` |
| 13 | `bool.not` | um local `bool` | `bool` |
| 14–15 | `bool.and/or` | dois locais `bool` | `bool` |
| 16 | `call` | função, argumentos | tipo de retorno da função |
| 17 | `call.cap` | importação, argumentos | tipo de retorno da importação |
| 18 | `br` | aresta | terminador |
| 19 | `br.if` | local `bool`, aresta verdadeira, aresta falsa | terminador |
| 20 | `return` | local do tipo de retorno | terminador |
| 21 | `trap` | nenhum | terminador |

`i64.div` trunca em direção a zero. Divisão por zero gera `NX_DIV_ZERO`.
`INT64_MIN / -1` e qualquer resultado aritmético fora de
[−2⁶³, 2⁶³−1] geram `NX_OVERFLOW`. `variant.get` com alternativa inativa
gera `NX_WRONG_TAG`. `trap` gera `NX_TRAPPED`. Esses erros interrompem a
invocação inteira, sem modificar o resultado do chamador. Efeitos de
capacidades já executadas permanecem sob responsabilidade do host.

## Capacidades e recursos opacos

Importação é uma exigência do programa. Um grant é uma autorização explícita
do host vinculada a interface, versão, operação e assinatura. Antes da
primeira instrução, a implementação exige exatamente um grant compatível por
importação, inclusive para importações que seriam inalcançáveis. Nenhuma
capacidade é instalada por padrão. Para parâmetros handle, o host também
declara a identidade externa completa da operação proprietária em cada slot.
O contexto do grant carrega escopos como raiz de filesystem, superfície de
UI, identidade de serviço ou lista de origens de rede permitidas. O adaptador
confiável aplica esse escopo e os respectivos limites. O núcleo não abre
sockets, arquivos, dispositivos ou superfícies de UI em nome do bytecode.

`call.cap` é síncrono e executado na ordem da IR. O host fornece seu próprio
prazo e limites para I/O, rede, filesystem, UI e serviços. Um host pode expor
falhas recuperáveis por códigos escalares definidos na assinatura da
interface. Na API C, uma falha de callback vira `NX_HOST`, salvo negação
(`NX_PERMISSION`) ou limite (`NX_LIMIT`). Valores de entrada e os retornos
de capacidade são validados. Slices de retorno devem permanecer válidos
após o callback até sua cópia imediata pelo núcleo. O adaptador não retém
`NxResources`, nem ponteiros emprestados ou tokens de handle. Adaptadores C
executam como código confiável do processo.

A operação proprietária é a única que pode produzir seu tipo de handle. Cada
retorno handle aponta para uma entrada criada na chamada atual. Um registro
bem-sucedido transfere propriedade ao núcleo e exige uma função de descarte.
Valores da IR e parâmetros de callback apenas emprestam o recurso. Um
callback só resolve handles presentes nos argumentos da própria chamada.
Handles não são constantes, argumentos de exportação, resultados externos
nem serializáveis. O núcleo libera os recursos uma vez em ordem inversa de
criação, inclusive após falhas. Uma nova invocação tem sua própria tabela.
Atribuir ou passar um handle internamente preserva a identidade do recurso.

Uma futura revisão pode definir suspensão com continuação explícita, ordem
de retomada, cancelamento e regras de retenção. A v1 não tem instruções de
suspensão nem expõe uma pilha C no formato.

## Determinismo e limites de execução

Uma invocação usa no máximo 1.000.000 instruções, conforme o orçamento
concedido, 32 frames simultâneos, 1.024 objetos compostos imutáveis,
64 handles e 65.536 bytes acumulados de slices retornados pelo host.
Cada instrução, inclusive terminadores e chamadas, consome uma unidade
antes de executar. A chamada interna consome também as instruções de sua
função. Objetos e slices do host duram até o fim da invocação. Os limites de
wall clock e I/O do host são externos à contagem de instruções.

A execução pura produz o mesmo resultado, ou o mesmo erro semântico, para
o mesmo módulo, argumentos e orçamento, supondo memória do processo
disponível. Uma execução com efeitos exige a mesma sequência de respostas
e erros do host para ser reproduzível. O núcleo não fornece relógio nem
aleatoriedade implícitos. JIT e AOT devem respeitar erros, ordem de efeitos,
orçamento e descarte observáveis do interpreter de referência.
