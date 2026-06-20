#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <pwm_z42.h>

//=============================================================
// CONFIGURAÇÕES DE VELOCIDADE
//=============================================================
#define TPM_MODULE          1000

uint16_t VEL_FRENTE         = TPM_MODULE;           // 100% - Linha detectada
uint16_t VEL_FRENTE_A       = TPM_MODULE * 0.9123;  // 91,23% para o Motor A para compensar falha mecânica no B
uint16_t VEL_CURVA_RAPIDA   = TPM_MODULE * 0.75;    // 75% - Roda externa da curva
uint16_t VEL_CURVA_LENTA    = TPM_MODULE * 0.55;    // 50% - Roda interna da curva
uint16_t VEL_PARADO         = 0;

//============================================================
// PINOS
//============================================================
#define PORTA_A             DT_NODELABEL(gpioa)
#define PORTA_E             DT_NODELABEL(gpioe)

#define SENSOR_A_PIN        1                      // PTA1 - sensor esquerdo
#define SENSOR_B_PIN        30                     // PTE30 - sensor direito

#define TRIG_PIN	    2			   // PTA2 - disparo do Ultrassom
// O pino de Echo é configurado direto no TPM1: PTE20

// LEDs via DeviceTree
static const struct gpio_dt_spec led_blue   = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green  = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

//===========================================================
// VARIÁVEIS DA INTERRUPÇÃO (ULTRASSOM)
//===========================================================
#define TPM_IRQ_LINE	    TPM1_IRQn
#define TPM_IRQ_PRIORITY    1

volatile uint16_t t0	          = 0;
volatile uint16_t pulse_ticks     = 0;
volatile bool	  edge_state      = false;
volatile bool     nova_leitura_us = false;
volatile uint32_t distancia_atual = 999;	// Inicia com valor seguro

//===========================================================
// MÁQUINA DE ESTADOS
//===========================================================
typedef enum{ 
    Parado, 
    Frente, 
    Curva_Esquerda,     // Sensor A perdeu linha -> vira à esquerda
    Curva_Direita,      // Sensor B perdeu linha -> vira à direita
    Obstaculo
} carrinho_t;

//===========================================================
// FUNÇÕES AUXILIARES E INTERRUPÇÃO
//===========================================================
static inline void set_velocidade(uint16_t motor_a, uint16_t motor_b){
    pwm_tpm_CnV(TPM2, 0, motor_a);
    pwm_tpm_CnV(TPM2, 1, motor_b);
}
static inline void set_led(int blue, int green){
    gpio_pin_set_dt(&led_blue, blue);
    gpio_pin_set_dt(&led_green, green);
}

void tpm1_isr(void *arg){
    TPM1->STATUS |= TPM_STATUS_CH0F_MASK;
    uint16_t current_capture = TPM1->CONTROLS[0].CnV;

    if(!edge_state){
	t0 = current_capture;
	edge_state = true;
    } 
    else{
	if(current_capture >= t0) {
		pulse_ticks = current_capture - t0;
	} else {
		pulse_ticks = (65535 - t0) + current_capture + 1;
	}
	edge_state = false;
	nova_leitura_us = true;		
    }
}

//===========================================================
// MAIN
//===========================================================
int main(void){
    // 1. PWM - Motores (TPM2)
    pwm_tpm_Init(TPM2, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_128, EDGE_PWM);     
    pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOB, 2);      // ENA - Motor A    (Esquerda)
    pwm_tpm_Ch_Init(TPM2, 1, TPM_PWM_H, GPIOB, 3);      // ENB - Motor B    (Direito)

    // 2. Ultrassom (TPM1 - Input Capture)
    IRQ_CONNECT(TPM_IRQ_LINE, TPM_IRQ_PRIORITY, tpm1_isr, NULL, 0);
    irq_enable(TPM_IRQ_LINE);
    pwm_tpm_Init(TPM1, TPM_PLLFLL, 65535, TPM_CLK, PS_128, EDGE_PWM);
    pwm_tpm_Ch_Init(TPM1, 0, TPM_INPUT_CAPTURE_BOTH | TPM_CHANNEL_INTERRUPT, GPIOE, 20);

    // 3. Portas e Pinos - Direção dos motores e Sensores
    const struct device *gpioa_dev = DEVICE_DT_GET(PORTA_A);                    
    const struct device *gpioe_dev = DEVICE_DT_GET(PORTA_E);                    
    if(!device_is_ready(gpioa_dev) || !device_is_ready(gpioe_dev)) return 0;

    // Direção: ambos os motores para frente
    gpio_pin_configure(gpioe_dev, 0, GPIO_OUTPUT_INACTIVE);     // IN1 = 0      Motor A --> Para Frente
    gpio_pin_configure(gpioe_dev, 1, GPIO_OUTPUT_ACTIVE);       // IN2 = 1
    gpio_pin_configure(gpioa_dev, 16, GPIO_OUTPUT_INACTIVE);    // IN3 = 0      Motor B --> Para Frente
    gpio_pin_configure(gpioa_dev, 17, GPIO_OUTPUT_ACTIVE);      // IN4 = 1

    // Sensores com pull-up (retorna 0 quando detecta linha) e Pino Trigger (Ultrassom)
    gpio_pin_configure(gpioa_dev, SENSOR_A_PIN, GPIO_INPUT | GPIO_PULL_UP); 
    gpio_pin_configure(gpioe_dev, SENSOR_B_PIN, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_configure(gpioa_dev, TRIG_PIN, GPIO_OUTPUT_INACTIVE);

    // 4. LEDs
    if(!gpio_is_ready_dt(&led_blue) || !gpio_is_ready_dt(&led_green)) return 0;
    gpio_pin_configure_dt(&led_blue,	GPIO_OUTPUT_INACTIVE);                 
    gpio_pin_configure_dt(&led_green,	GPIO_OUTPUT_INACTIVE);

    // Garante que os motores iniciam parados
    set_velocidade(VEL_PARADO, VEL_PARADO);

    carrinho_t estado_atual = Parado;   

    int contador_trigger = 0;	// Otimização para disparo não-bloqueante       

    while(1){      
	// =============================== Ultrassom (Assíncrono) =============================
	// Dispara o Trigger a cada 5 ciclos (50ms) para não sobrecarregar de som e não travar o 'while'
	if(contador_trigger++ >= 5){
		gpio_pin_set(gpioa_dev, TRIG_PIN, 1);
		k_busy_wait(10);			// 10us não afeta o loop do carrinho
		gpio_pin_set(gpioa_dev, TRIG_PIN, 0);
		contador_trigger = 0;
	}
	// Se a interrupção capturou um novo pulso, calculamos a distância
	if(nova_leitura_us){
		distancia_atual = (pulse_ticks * 46) / 1000;
		nova_leitura_us	= false;
	} 

        // ========= Leitura dos Sensores IR (Lógica invertida: 0 = detecta linha) =============
        bool linha_A = (gpio_pin_get(gpioa_dev, SENSOR_A_PIN) == 0);    
        bool linha_B = (gpio_pin_get(gpioe_dev, SENSOR_B_PIN) == 0);    

        // =============================== Transição de estados  ===============================
        //      A   B       |       Ação
        //      1   1       |       Frente          - Ambos na linha
        //      1   0       |       Curva Dir.      - B perdeu  -   corrige para direita
        //      0   1       |       Curva Esq.      - A perdeu  -   corrige para esquerda
        //      1   1       |       Parado          - linha perdida completamente

	// Lógica de prioridade: Ultrassom é a prioridade máxima.
	if(distancia_atual > 0 && distancia_atual <= 20)  estado_atual = Obstaculo;
        else if (linha_A  &&  linha_B)  estado_atual = Frente;   
        else if (linha_A  && !linha_B)  estado_atual = Curva_Direita;
        else if (!linha_A &&  linha_B)  estado_atual = Curva_Esquerda;
        else                            estado_atual = Parado; 

        // =============================== Ações por Estado ====================================
        switch(estado_atual){     
	    case Obstaculo:
		set_velocidade(VEL_PARADO, VEL_PARADO);			// Carrinho para, indicando barreira
		set_led(1, 1);						// Ciano
		break;
		                                  
            case Curva_Esquerda:
                set_velocidade(VEL_CURVA_LENTA, VEL_CURVA_RAPIDA);      // Sensor A perdeu linha -> reduz motor A (esq.), acelera B (dir.)
                set_led(1, 0);                                          // Azul
                break;
                
            case Curva_Direita:
                set_velocidade(VEL_CURVA_RAPIDA, VEL_CURVA_LENTA);      // Sensor B perdeu linha -> reduz motor B (dir.), acelera A (esq.)
                set_led(0, 1);                                          // Verde
                break;

            case Frente:
                set_velocidade(VEL_FRENTE_A, VEL_FRENTE);     // Ambos os sensores na linha -> velocidade máxima
                set_led(1, 1);                                // Ciano
                break;

            case Parado:
                set_velocidade(VEL_PARADO, VEL_PARADO);     // Nenhum sensor detecta linha -> para tudo
                set_led(0, 0);                              // Apagado
                break;
        }
            k_msleep(10);   // 100 Hz de atualização
    }
    return 0;
}
//======================================================================
//PINAGEM FÍSICA
//======================================================================
//                              Microcontrolador       Ponte H		Função
//MOTOR A - PWM (Esq)           PTB2                   ENA		PWM - Velocidade
//MOTOR B - PWM (Dir)           PTB3                   ENB		PWM - Velocidade
//MOTOR A - DIREÇÃO 1           PTE0                   IN1		Direção 1
//MOTOR A - DIREÇÃO 2           PTE1                   IN2		Direção 2
//MOTOR B - DIREÇÃO 1           PTA16                  IN3		Direção 1
//MOTOR B - DIREÇÃO 2           PTA17                  IN4		Direção 2
//SENSOR A (CONTROLA MOTOR A)   PTA1            			Digital Input - Linha Esq.
//SENSOR B (CONTROLA MOTOR B)   PTE30            			Digital Input - Linha Dir.
//HC-SR04 TRIGGER		PTA2					Digital Output - Disparo (adaptado)
//HC-SR04 ECHO			PTE20					TPM1_CH0 - Interrupção Captura

// Considerações a levar:
// Estabelecer os comandos definitivos para as curvas
// Estabelecer potências para as curvas
// Implementar lógica para não o carrinho não parar quando estiver no cruzamento da pista