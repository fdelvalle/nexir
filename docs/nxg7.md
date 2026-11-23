# Formato binário NXG7

Todos os inteiros de 16 bits usam little-endian. Os valores assinados usam
complemento de dois. Uma cor tem três bytes na ordem vermelho, verde, azul.
Índices começam em zero. A ordem dos blocos é fixa e o arquivo termina após a
última regra.

| Bloco | Tamanho por registro | Campos em ordem |
| --- | ---: | --- |
| Cabeçalho | 12 | Assinatura ASCII `NXG7`, revisão de 8 bits (0), reservado (0), quantidades de cenas, entidades e regras (`u16` cada) |
| Cena | 4 | Cor de fundo (RGB), reservado (0) |
| Entidade | 18 | Cena (`u8`), forma (`u8`), x, y, vx, vy (`i16` cada), largura, altura (`u16` cada), cor (RGB), reservado (0) |
| Regra | 10 | Cena, evento, A, B, ação, alvo (`u8` cada), X e Y (`i16` cada) |

São aceitas de 1 a 16 cenas, até 128 entidades e até 256 regras. As dimensões
de cada entidade ficam entre 1×1 e 640×480 pixels. A origem está no canto
superior esquerdo. Formas 0, 1 e 2 representam retângulo preenchido, contorno
retangular e elipse preenchida. Todas usam seu retângulo delimitador na colisão.

| Evento | Código | Operandos |
| --- | ---: | --- |
| Tick | 0 | A = B = 0 |
| Tecla mantida | 1 | A = tecla, B = 0 |
| Tecla pressionada | 2 | A = tecla, B = 0 |
| Tecla solta | 3 | A = tecla, B = 0 |
| Colisão AABB | 4 | A e B = entidades distintas da cena da regra |

As teclas 0 a 5 são esquerda, direita, cima, baixo, espaço e R. As transições
capturadas entre ticks são consumidas no tick seguinte. Uma tecla clicada
inteiramente entre dois ticks pode acionar os eventos de pressionar e soltar
no mesmo tick.

| Ação | Código | Operandos |
| --- | ---: | --- |
| Definir velocidade | 0 | Alvo = entidade da cena, X/Y em pixels por segundo |
| Definir posição | 1 | Alvo = entidade da cena, X/Y em pixels |
| Entrar em cena | 2 | Alvo = cena, X = Y = 0 |
| Posicionar em relação a uma entidade | 3 | Alvo = entidade movida, X = índice de outra entidade na mesma cena, Y = deslocamento vertical em pixels |

A ação 3 alinha o centro horizontal do alvo ao centro da entidade de referência
e ajusta sua posição vertical pelo deslocamento Y.

O runtime inicia na cena 0. A cada passo de 1/60 s, executa as regras de tick e
teclado na ordem do arquivo, integra as velocidades das entidades da cena e
avalia as regras de colisão na ordem do arquivo. Regras de colisão podem
alterar a posição observada pelas regras seguintes. Caixas que apenas se
encostam não colidem. A detecção é discreta, portanto objetos rápidos podem
atravessar caixas estreitas entre passos.

A integração usa coordenadas inteiras em unidades de 1/60 pixel e satura cada
eixo em ±32768 pixels. Uma mudança de cena restaura o estado inicial das
entidades e encerra o passo. A renderização desenha entidades na ordem do
arquivo. O loop X11 usa relógio monotônico e limita a recuperação após uma
pausa a 250 ms.

O parser valida o arquivo inteiro antes de inicializar a VM. Assinaturas,
revisões, registros incompletos, dados excedentes, campos reservados,
instruções desconhecidas e índices ou operandos inválidos são rejeitados.
As operações do formato acessam somente cenas, entidades e componentes
validados. O formato não oferece ponteiros ou endereços de memória.
