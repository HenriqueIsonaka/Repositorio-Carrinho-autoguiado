#include <zephyr/kernel.h>        // Funções básicas do Zephyr (ex: k_msleep, k_thread, etc.)
#include <zephyr/device.h>        // API para obter e utilizar dispositivos do sistema
#include <zephyr/drivers/gpio.h>  // API para controle de pinos de entrada/saída (GPIO)
#include <pwm_z42.h>        // Biblioteca personalizada com funções de controle do TPM (Timer/PWM Module)


#define PTA_NODE
// Define o valor do registrador MOD do TPM para configurar o período do PWM
#define TPM_MODULE 7500  // Define a frequência do PWM fpwm = (TPM_CLK / (TPM_MODULE * PS))
// Valores de duty cycle correspondentes a diferentes larguras de pulso
uint16_t zero  = TPM_MODULE*0.125; // 2,5ms
uint16_t minimo = TPM_MODULE*0.12; // 2,4ms-> necessário usar para o servomotor defeito
uint16_t quarentacinco = TPM_MODULE*0.1; //2ms  
uint16_t noventa  = TPM_MODULE* 0.075; //1,5ms
uint16_t centoetrintacinco = TPM_MODULE*0.05; //1ms
uint16_t centooitenta  = TPM_MODULE*0.025;//0,5ms


int main(void)
{
    // Inicializa o módulo TPM2 com:
    // - base do TPMx
    // - fonte de clock PLL/FLL (TPM_CLK)
    // - valor do registrador MOD
    // - tipo de clock (TPM_CLK)
    // - prescaler de 1 a 128 (PS)
    // - modo de operação EDGE_PWM
    pwm_tpm_Init(TPM2, TPM_PLLFLL, TPM_MODULE, TPM_CLK, PS_128, EDGE_PWM);




    pwm_tpm_Ch_Init(TPM2, 1, TPM_PWM_H, GPIOB, 3);// até aqui está ok(pino PTB0 E PTB3)
   
   


    for(;;) {
        pwm_tpm_CnV(TPM2, 1, minimo);
        k_msleep(2000);
        pwm_tpm_CnV(TPM2, 1, quarentacinco);
        k_msleep(2000);
        pwm_tpm_CnV(TPM2, 1, noventa);
        k_msleep(2000);
        pwm_tpm_CnV(TPM2, 1, centoetrintacinco);
        k_msleep(2000);
        pwm_tpm_CnV(TPM2, 1, centooitenta);
        k_msleep(2000);
    }
    return 0;
}
