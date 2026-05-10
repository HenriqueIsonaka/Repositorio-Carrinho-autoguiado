Construção do carrinho: atividade 4
===================================
Este trabalho foi feito em dupla por Gean Santos e Henrique Isonaka

*OBS: ignorar commits que não começam com "Commit atividade 4"


Commit 1

Objetivo: fazer a placa freedom responder aos sensores por meio de LEDs

Resultado: Quando os sensores detectavam algo, o LED da placa piscava em azul ou verde(dependendo do sensor) ou ciano, caso os dois detectassem ao mesmo tempo.

<img width="426" height="240" alt="Gif das luzes respondendo aos sensores" src="https://github.com/user-attachments/assets/6da97207-16d8-4ac6-a913-198e4ca01980" />

Commit 2

Objetivo: fazer com que as rodas do carrinho girassem quando os sensores detectassem algo

Resultado: Quando os sensores detectavam algo, uma das rodas ou ambas (caso os dois sensores detectassem ao mesmo tempo) giravam. Os LEDs também piscavam como na implementação anterior.

<img width="426" height="240" alt="Video Project 14" src="https://github.com/user-attachments/assets/2c7902bf-5b88-4654-b524-fa8e1a1ef47e" />

Início dos testes para competição

O carrinho foi montado a partir de uma estrutura de tração traseira e motor central, baseando-se nos carros de corrida da vida real. No entanto, diferente desses, o carrinho controlado por arduino faria as curvas a partir da diferença de velocidade entre suas rodas e seria suportado por uma roda dianteira, que acompanharia seu movimento. Além disso, ele ficou com os dois sensores posicionados na frente, fixados por meio de um suporte em L e fita de velcro.

O circuito era constituído de fita isolante preta em uma lousa branca, para que os sensores fossem capazes de detectar a linha preta formada, a qual o carrinho deveria seguir. O formato do circuito era o de um 8, com uma linha de partida bem definida como um traço perpendicular ao seu traçado.

Objetivo dos Commits 3,4 e 5: fazer o carrinho andar na pista
---
Commit 3

Foi implementado um código em que a roda interna à curva ficasse com uma velocidade mais lenta e a roda externa ficasse com uma velocidade mais rápida.

Resultado: o carrinho detectava a linha do circuito e começava a fazer a curva, mas acabava saindo por fora, sem completá-la. O que acontecia era que a curva que o carrinho executava não era fechada o suficiente para que o sensor continuasse identificando a linha, fator que iniciava a função de curva. Dessa forma, ele acabava saindo com os dois sensores do circuito, não podendo voltar a identificar as linhas da pista, ou seja, não podendo completar a trajetória esperada. 

Dessa forma, para resolver o problema, seria necessário fazer o carrinho fazer curvas mais fechadas com sua função de curva.


<img width="426" height="240" alt="Gif carrinho saindo da pista" src="https://github.com/user-attachments/assets/3b3537de-71ba-4d9a-8a46-44ec11a823a1" />

Commit 4

Surgiu um novo problema para a confecção do carrinho: o motor B perdeu potência e ficou relativamente mais lento do que o motor A. Isso fazia com que o carrinho, quando em função de andar em linha reta, se movesse fazendo uma curva na direção do motor B por esse gerar menos velocidade.

Foi necessário implementar uma velocidade máxima limitada para o motor A, para que esse não ficasse com uma velocidade maior do que o motor B em retas.

Resultado: o motor A teve sua potência limitada em 91,23%, o que resolveu o problema, sendo que essa configuração continuou a ser utilizada até a finalização do projeto(já que o motor B continuou defeituoso).

Commit 5
*OBS: nesse código já havia sido iniciado o desenvolvimento da atividade com o ultrassom, o que significa que nele e nos posteriores haverão partes do programa relacionado ao sensor ultrassônico.

Foi implementado um código em que o carrinho percorreria o circuito em uma velocidade máxima mais lenta, para não ter que lutar tanto contra a inércia gerada pela velocidade nas retas, o que permitiria curvas mais fechadas.

Resultado: Sucesso, o carrinho conseguia completar voltas consistentemente, mesmo não fazendo isso de forma muito rápida.

<img width="426" height="240" alt="Gif carrinho completando volta" src="https://github.com/user-attachments/assets/a0f54bb5-34dd-4df2-9f88-053b8b090f4e" />

Objetivo dos Commits 6, 7 e 8: fazer o carrinho percorrer o circuito de forma mais rápida
---
Commit 6

O código foi implementado visando fazer o carrinho completar o percurso do circuito em menos tempo, aumentando a velocidade máxima e fazendo com que a roda interna do carrinho em uma curva girasse no sentido reverso, na função de curva, para freá-la e fazer com que o eixo de rotação do carrinho ficasse mais centralizado.

Resultado: apesar de ter diminuido o tempo de volta do carrinho, a implementação fez com que em muitos casos o carrinho não conseguisse completar uma volta por sair por fora da curva.

Desse modo, o primeiro problema da execução do carrinho surgiu novamente na tentativa de tornar o carrinho mais rápido.

<img width="426" height="240" alt="Gif carrinho inconsistente" src="https://github.com/user-attachments/assets/ac66ef77-7378-43de-9638-291b8a2054be" />

Commit 7

Foi implementada uma atualização no código anterior que fez com que, em uma curva, a velocidade da roda externa fosse aumentada para o máximo.

Resultado: isso permitiu que o carrinho fizesse a curva da forma mais fechada possível, já que ele agora estaria girando ao redor do seu próprio eixo e poderia quebrar a inércia da roda interior à curva na função de curva, o que também permitiu que ele tivesse a maior velocidade de reta possível.

Assim, o carrinho conseguia executar múltiplas voltas consecutivas em tempos muito menores.

Commit 8

Por fim, foi adicionado um delay nas funções de curva para que elas continuassem a ser executadas por 100 microssegundos após os sensores pararem de detectar a linha da pista (por meio da função k_busy_wait()). Isso foi feito com o objetivo de fazer o carrinho gastar o mínimo de tempo possível em funções de curva, para que ele executasse voltas mais rápidas.

Resultado: O carrinho diminuiu o tempo de volta em meio segundo com o delay. Também foram testados outros valores de delay, mas o de 100 microssegundos se mostrou como o melhor para executar voltas mais rápidas na pista da competição.

<img width="426" height="240" alt="Carrinho final circuito" src="https://github.com/user-attachments/assets/d8da4c43-41f4-4531-a7df-2035c6eb408e" />


