#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include "pwm_z42.h"

#define TPM_IRQ_LINE TPM1_IRQn
#define TPM_IRQ_PRIORITY 1

// Pinos
#define PORTA_A DT_NODELABEL(gpioa)
#define TRIG_PIN 1 // PTA1

// LED para feedback visual do obstáculo
static const struct gpio_dt_spec led_red = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

// Variáveis voláteis (modificadas dentro da interrupção)
volatile uint16_t t0 = 0;
volatile uint16_t pulse_ticks = 0;
volatile bool edge_state = false; // false = esperando subida, true = esperando descida
volatile bool nova_leitura = false;

/* --- INTERRUPÇÃO DO TPM1 --- */
void tpm1_isr(void *arg){
    TPM1->STATUS |= TPM_STATUS_CH0F_MASK; // Zera a flag da interrupção
    uint16_t current_capture = TPM1->CONTROLS[0].CnV;

    if (!edge_state) {
        // Capturou a Borda de Subida (Início do pulso Echo)
        t0 = current_capture;
        edge_state = true;
    } else {
        // Capturou a Borda de Descida (Fim do pulso Echo)
        if (current_capture >= t0) {
            pulse_ticks = current_capture - t0;
        } else {
            // Trata o caso de "overflow" do timer (passou de 65535 para 0)
            pulse_ticks = (65535 - t0) + current_capture + 1;
        }
        edge_state = false;
        nova_leitura = true; // Avisa o main() que o cálculo terminou
    }
}

void main(void)
{
    // 1. Configuração do Pino Trigger e LED
    const struct device *gpioa_dev = DEVICE_DT_GET(PORTA_A);
    gpio_pin_configure(gpioa_dev, TRIG_PIN, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&led_red, GPIO_OUTPUT_INACTIVE);

    // 2. Conecta a interrupção via Zephyr
    IRQ_CONNECT(TPM_IRQ_LINE, TPM_IRQ_PRIORITY, tpm1_isr, NULL, 0);
    irq_enable(TPM_IRQ_LINE);
 
    // 3. Inicializa TPM1 e Input Capture (Borda de Subida e Descida)
    pwm_tpm_Init(TPM1, TPM_PLLFLL, 65535, TPM_CLK, PS_128, EDGE_PWM);
    pwm_tpm_Ch_Init(TPM1, 0, TPM_INPUT_CAPTURE_BOTH | TPM_CHANNEL_INTERRUPT, GPIOE, 20);

    /* --- LOOP DE LEITURA E CALIBRAÇÃO --- */
    while (1){
        // A. Garante que a máquina de estados da interrupção está zerada
        edge_state = false;
        nova_leitura = false;

        // B. Gera o pulso de 10us no pino Trigger para iniciar a leitura
        gpio_pin_set(gpioa_dev, TRIG_PIN, 1);
        k_busy_wait(10); // Espera 10 microssegundos
        gpio_pin_set(gpioa_dev, TRIG_PIN, 0);

        // C. Aguarda o som ir e voltar (máx ~40ms para o HC-SR04)
        k_msleep(50); 

        // D. Se a interrupção capturou o pulso com sucesso
        if (nova_leitura) {
            // Aplica a fórmula matemática calibrada para cm
            uint32_t distancia_cm = (pulse_ticks * 46) / 1000;
            
            printk("Ticks: %u | Distancia: %u cm ", pulse_ticks, distancia_cm);

            // E. Lógica do Limite de 20cm
            if (distancia_cm > 0 && distancia_cm <= 10) {
                printk("-> [OBSTACULO DETECTADO!]\n");
                gpio_pin_set_dt(&led_red, 1); // Acende LED
            } else {
                printk("\n");
                gpio_pin_set_dt(&led_red, 0); // Apaga LED
            }
        } else {
             printk("Falha na leitura (Sensor nao conectado ou fora de alcance).\n");
             gpio_pin_set_dt(&led_red, 0);
        }
        // Aguarda meio segundo até o próximo disparo (facilita ler no terminal)
        k_msleep(500); 
    }
}