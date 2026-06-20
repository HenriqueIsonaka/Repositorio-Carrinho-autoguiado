#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <pwm_z42.h>

#define TPM_MODULE          1000
#define TPM_MODULE_80       TPM_MODULE * 0.5
#define TPM_MODULE_40       TPM_MODULE * 0.2
#define PORTA_A             DT_NODELABEL(gpioa)
#define SENSOR_A_PIN        13      //PTA13
#define SENSOR_B_PIN        16      //PTA16

// Definição dos LEDs via DeviceTree
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec led_green = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

typedef enum{ Sistema_OFF, A_Curva_Esquerda, B_Curva_Direita, Frente } carrinho_t;     // Máquina de Estados

int main(void){
    pwm_tpm_Init(TPM2, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_128, EDGE_PWM);      //1. PWM (Motores)
    pwm_tpm_Ch_Init(TPM2, 0, TPM_PWM_H, GPIOB, 2);      // ENA  (Motor A) Esquerda
    pwm_tpm_Ch_Init(TPM2, 1, TPM_PWM_H, GPIOB, 3);      // ENB  (Motor B) Direita

    const struct device *gpioa_dev = DEVICE_DT_GET(PORTA_A);                    //2. Direção dos Motores (Porta A)
    if(!device_is_ready(gpioa_dev)) return 0;

    // Configuração de Direção (Ajustada para liberar o pino do botão)
    // Motor A --> Para Frente (Para trás)
    gpio_pin_configure(gpioa_dev, 1, GPIO_OUTPUT_INACTIVE);     // IN1 = 0
    gpio_pin_configure(gpioa_dev, 2, GPIO_OUTPUT_ACTIVE);       // IN2 = 1
    // Motor B --> Para Frente (Para trás)
    gpio_pin_configure(gpioa_dev, 5, GPIO_OUTPUT_INACTIVE);     // IN3 = 0
    gpio_pin_configure(gpioa_dev, 12, GPIO_OUTPUT_ACTIVE);      // IN4 = 1

    // Entradas: Sensores e Botão
    gpio_pin_configure(gpioa_dev, SENSOR_A_PIN, GPIO_INPUT | GPIO_PULL_UP);     //3. Sensores (Entradas com Pull-up)
    gpio_pin_configure(gpioa_dev, SENSOR_B_PIN, GPIO_INPUT | GPIO_PULL_UP);

    if(!gpio_is_ready_dt(&led_red) || !gpio_is_ready_dt(&led_green)) return 0;  //4. LEDs
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);                 
    gpio_pin_configure_dt(&led_green, GPIO_OUTPUT_INACTIVE);

    carrinho_t estado_atual = Sistema_OFF;          // VARIÁVEIS DE CONTROLE

    while(1){      
        bool Sensor_A = (gpio_pin_get(gpioa_dev, SENSOR_A_PIN) == 0);    //1. LEITURA DOS SENSORES (Inverter Lógica)
        bool Sensor_B = (gpio_pin_get(gpioa_dev, SENSOR_B_PIN) == 0);    // o Sensor retorna 0 quando detecta algo, isso inverte essa lógica, agora retornando 1.

            if(Sensor_A && Sensor_B)            estado_atual = Frente;   //2. DEFINIÇÃO DO PRÓXIMO ESTADO
            else if(Sensor_A && !Sensor_B)      estado_atual = A_Curva_Esquerda;
            else if(!Sensor_A && Sensor_B)      estado_atual = B_Curva_Direita;
            else                                estado_atual = Frente; 

        switch(estado_atual){                                       //3. AÇÕES DA MÁQUINA DE ESTADOS
            case A_Curva_Esquerda:
                pwm_tpm_CnV(TPM2, 0, TPM_MODULE_40);
                pwm_tpm_CnV(TPM2, 1, TPM_MODULE_80);
                gpio_pin_set_dt(&led_red, 1);           // LED Vermelho
                gpio_pin_set_dt(&led_green, 0);
                break;
                
            case B_Curva_Direita:
                pwm_tpm_CnV(TPM2, 0, TPM_MODULE_80);  
                pwm_tpm_CnV(TPM2, 1, TPM_MODULE_40);
                gpio_pin_set_dt(&led_red, 0);
                gpio_pin_set_dt(&led_green, 1);         // LED Verde 
                break;

            case Frente:
                pwm_tpm_CnV(TPM2, 0, TPM_MODULE_80);  
                pwm_tpm_CnV(TPM2, 1, TPM_MODULE_80);
                gpio_pin_set_dt(&led_red, 1);           // LED Amarelo
                gpio_pin_set_dt(&led_green, 1);         // LED Amarelo
                break;
            case Sistema_OFF:
                pwm_tpm_CnV(TPM2, 0, 0);  
                pwm_tpm_CnV(TPM2, 1, 0);
                gpio_pin_set_dt(&led_red, 0);           // LED Apagado;
                gpio_pin_set_dt(&led_green, 0);
                break;
        }
            k_msleep(10);   // Aguarda 5ms (100Hz)
    }
    return 0;
}
//PINAGEM FÍSICA
//                              Microcontrolar              Ponte H
//MOTOR A - PWM                 PTB2                        ENA
//MOTOR B - PWM                 PTB3                        ENB
//MOTOR A - DIREÇÃO 1           PTA1                        IN1
//MOTOR A - DIREÇÃO 2           PTA2                        IN2
//MOTOR B - DIREÇÃO 1           PTA5                        IN3
//MOTOR B - DIREÇÃO 2           PTA12                       IN4
//SENSOR A (CONTROLA MOTOR A)   PTA13
//SENSOR B (CONTROLA MOTOR B)   PTA16

// Considerações a levar:
// 1. Inverter o sentido de giro dos motores; (Feito)
// 2. Estabelecer os comandos para as curvas;
// 3. Utilizar o botão do Microcontrolador para ligar e desligar o circuito
// 4. Consertar o LED Vermelho, tá aparecendo o LED Azul