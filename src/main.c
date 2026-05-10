
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>

// ==============================================================================================================================
// DEFINIÇÃO E SELEÇÃO/CONFIGURAÇÕES DE PARÂMETROS
// ==============================================================================================================================
#define TRIG_PIN 1                              // PTD1
#define ECHO_PIN 3                              // PTD3

/* EXPLICAÇÃO DOS STRUCTS:
 * Aqui criamos ponteiros para acessar o hardware da placa (Porta D) e para a estrutura de callback 
 * que o sistema operacional Zephyr exige para gerenciar as interrupções de pinos.
 */
static const struct device *gpio_dev;
static struct gpio_callback echo_cb_data;

/* EXPLICAÇÃO DO 'VOLATILE':
 * A palavra-chave 'volatile' foi crucial aqui. Ela avisa ao compilador: "Não otimize ou faça cache destas
 * variáveis! Elas podem mudar a qualquer instante por forças externas (o sensor)". Como essas variáveis são 
 * modificadas dentro de uma Interrupção (ISR) paralela ao código principal, sem o 'volatile', o 'main.c' 
 * leria valores congelados, e o carrinho nunca enxergaria o obstáculo.
 */
volatile uint32_t tempo_inicio = 0;
volatile uint32_t distancia_cm = 999;
volatile uint32_t isr_count = 0;                // Contador para saber se o sinal físico chegou

// ==============================================================================================================================
// ISR: Disparada em qualquer mudança (subida ou descida) no pino Echo
// ==============================================================================================================================
/* EXPLICAÇÃO DA ISR (Interrupt Service Routine):
 * Uma ISR paralisa a CPU instantaneamente no exato nanossegundo em que o pino Echo muda fisicamente de estado.
 * Isso resolve os atrasos de leitura. Se o 'main' estivesse ocupado controlando os motores, a ISR interrompe 
 * tudo, anota o tempo e devolve o controle.
 */
void echo_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    uint32_t agora = k_cycle_get_32();              // k_cycle_get_32() acessa o relógio interno do microcontrolador (ciclos de CPU).
    isr_count++;                                    // É um cronômetro de extrema precisão nativo do Zephyr.

    // Borda de Subida: Início do eco
    if(gpio_pin_get(dev, ECHO_PIN) > 0) tempo_inicio = agora;      // O som acabou de sair do sensor. Marcamos a "foto" desse instante salvando o ciclo da CPU atual.
    
    // Borda de Descida: Fim do eco
    else{    
        if(tempo_inicio != 0){                              // O som bateu no obstáculo e voltou.
            uint32_t diff = agora - tempo_inicio;           // Calculamos a diferença de ciclos desde que o som saiu até ele voltar
            
            // Converte ciclos de CPU para microssegundos e divide por 58 (constante do som)
            
            /* EXPLICAÇÃO DA MATEMÁTICA:
             * 'k_cyc_to_us_floor32' converte o tempo de forma independente do clock da sua placa.
             * A velocidade do som é ~340 m/s (ou 0.034 cm/us). 
             * Como o som vai e volta, a fórmula real é: Distância = (Tempo(us) * 0.034) / 2.
             * Dividir por 58 é a simplificação exata dessa fórmula, evitando uso de números quebrados (float) 
             * que deixariam o microcontrolador mais lento.
             */
            distancia_cm = k_cyc_to_us_floor32(diff) / 58;
            tempo_inicio = 0;                              // Zera o cronômetro para não processar dados fantasmas na próxima vez
        }
    }
}

// ============================= INICIALIZA OS PINOS E INTERRUPÇÕES DO ULTRASSOM HC-SR04 ========================================
void ultrassom_init(void) {
    gpio_dev = DEVICE_DT_GET(DT_NODELABEL(gpiod));
    
    // O Zephyr é muito rigoroso. Se o hardware (.overlay) não estiver declarado, tentar configurá-lo
    // gera um HardFault (tela azul da morte dos microcontroladores).
    if (!device_is_ready(gpio_dev)) {
                                                                printk("[ERRO] Porta D não habilitada no .overlay!\n");
        return;
    }
    gpio_pin_configure(gpio_dev, TRIG_PIN, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure(gpio_dev, ECHO_PIN, GPIO_INPUT);
    
    /* EXPLICAÇÃO DA BORDA:
     * Usar 'BOTH' permite que a mesma função ISR faça o papel duplo: ligar o cronômetro na subida do sinal
     * e desligar na descida. O código fica enxuto e não precisamos de dois timers separados.
     */
    gpio_pin_interrupt_configure(gpio_dev, ECHO_PIN, GPIO_INT_EDGE_BOTH);       // Habilita interrupção em ambas as bordas (Both Edges)  

    // Aqui, o sistema operacional amarra a nossa função 'echo_isr' ao evento de mudança física no pino ECHO_PIN.
    gpio_init_callback(&echo_cb_data, echo_isr, BIT(ECHO_PIN));
    gpio_add_callback(gpio_dev, &echo_cb_data);
                                                                printk("[SISTEMA] Ultrassom iniciado: Trig=PTD1, Echo=PTD3\n");
}

// ============================= ENVIA PULSO DE 10us PARA INICIAR A LEITURA (ASSÍNCRONO) ========================================
void ultrassom_trigger(void){
    /* EXPLICAÇÃO DO TRIGGER:
     * O HC-SR04 precisa de um "choque" de exatos 10 microssegundos para acordar e emitir o som.
     * O 'k_busy_wait' trava a CPU propositalmente por 10us (uma trava tão curta que não afeta os motores)
     * garantindo o pulso perfeito que o datasheet do sensor exige.
     */
    gpio_pin_set(gpio_dev, TRIG_PIN, 1);
    k_busy_wait(10); // Pulso de trigger de 10us
    gpio_pin_set(gpio_dev, TRIG_PIN, 0);
}

/* EXPLICAÇÃO DO ENCAPSULAMENTO:
 * Ao invés do 'main' acessar as variáveis globais direto, ele usa estas funções.
 * Isso blinda a lógica do sensor: o 'main' só pode "ler" os dados, nunca corromper ou escrever por acidente
 * por cima dos cálculos feitos durante a interrupção.
 */
uint32_t ultrassom_get_distancia(void){      return distancia_cm; }
uint32_t ultrassom_get_debug_count(void){    return isr_count;    }