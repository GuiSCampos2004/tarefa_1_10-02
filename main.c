//-------------------------------------------- Bibliotecas --------------------------------------------
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "hardware/pwm.h"
#include "hardware/adc.h"


//--------------------------------------------- Variáveis ---------------------------------------------
const uint botao_a = 5;
const uint led_g = 11;
const uint led_b = 12;
const uint led_r = 13;

const uint I2C_SDA = 14;
const uint I2C_SCL = 15;
const uint8_t endereco = 0x3C;
#define I2C_PORT i2c1
ssd1306_t ssd;

const uint16_t wrap = 50000;
const float divisor = 50.0;
uint16_t level_azul = 0, level_vermelho = 0; //Controle de Duty Cycle

const uint botao_joy = 22;
const uint eixo_x = 26;
const uint eixo_y = 27;

static volatile uint32_t ultimo_tempo = 0; //Usada para Debounce
uint8_t top=27, left=59; //Coordenadas do quadrado no display
bool troca1 = 1, troca2 = 0, aceso_verde = 0; //Troca de estados para uso dos botões


//-------------------------------------- Interrupção para Botões --------------------------------------
static void gpio_irq_handler(uint gpio, uint32_t events){

    uint32_t tempo_atual = to_us_since_boot(get_absolute_time());

    if (tempo_atual - ultimo_tempo > 200000){ //Debounce evitando que algo seja realizado caso 
        ultimo_tempo = tempo_atual;           //a interrupção ocorra numa frequencia alta

        if(gpio == 5){ //Caso o Botão A seja apertado, entra nesse if

            troca1=!troca1; //Ativa/desativa os leds vermelho e azul
            pwm_set_gpio_level(led_b, 0);
            pwm_set_gpio_level(led_r, 0);

        }else{

            troca2 = !troca2; // Desenha ou apaga a segunda borda

            aceso_verde = !aceso_verde; //Acende ou apaga o led verde
            gpio_put(led_g, aceso_verde);
        }
        
    }
}


//------------------------------------------ Função Principal -----------------------------------------
int main(){
//--------------------------------------- Inicialização de GPIO ---------------------------------------
    stdio_init_all();
    
    gpio_init(botao_a);
    gpio_set_dir(botao_a, GPIO_IN);
    gpio_pull_up(botao_a);

    gpio_init(botao_joy);
    gpio_set_dir(botao_joy, GPIO_IN);
    gpio_pull_up(botao_joy);

    gpio_init(led_g);
    gpio_set_dir(led_g, GPIO_OUT);
    gpio_put(botao_a, aceso_verde);
    

//---------------------------------------- Inicialização de PWM ---------------------------------------
    gpio_set_function(led_b, GPIO_FUNC_PWM); //habilitar o pino GPIO como PWM
    uint slice_azul = pwm_gpio_to_slice_num(led_b); //obter o canal PWM da GPIO
    pwm_set_clkdiv(slice_azul, divisor); //define o divisor de clock do PWM
    pwm_set_wrap(slice_azul, wrap); //definir o valor de wrap
    pwm_set_enabled(slice_azul, true); //habilita o pwm no slice correspondente

    gpio_set_function(led_r, GPIO_FUNC_PWM); //habilitar o pino GPIO como PWM
    uint slice_vermelho = pwm_gpio_to_slice_num(led_r); //obter o canal PWM da GPIO
    pwm_set_clkdiv(slice_vermelho, divisor); //define o divisor de clock do PWM
    pwm_set_wrap(slice_vermelho, wrap); //definir o valor de wrap
    pwm_set_enabled(slice_azul, true); //habilita o pwm no slice correspondente


//-------------------------------------- Inicialização do Display -------------------------------------
    // Inicialização do I2C. Utilizando a 400Khz.
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); // Configurando GPIO para I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); // Configurando GPIO para I2C
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd); // Configura o display
    ssd1306_send_data(&ssd); // Envia os dados para o display

    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

//---------------------------------------- Inicialização do ADC ---------------------------------------
    adc_init(); //Inicializa o módulo ADC

    adc_gpio_init(eixo_x); //Configura a porta 26 para leitura analógica do ADC.

    adc_gpio_init(eixo_y); //Configura a porta 27 para leitura analógica do ADC.


//------------------------------------- Configuração de interrupção -----------------------------------
    gpio_set_irq_enabled_with_callback(botao_a, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    gpio_set_irq_enabled_with_callback(botao_joy, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);


//------------------------------------------- Loop Principal ------------------------------------------
    while (true){
        adc_select_input(0); //Seleciona o canal 0 (pino 26)
        uint16_t vry_value = adc_read(); //Realiza leitura do canal atual

        adc_select_input(1); //Seleciona o canal 1 (pino 27)
        uint16_t vrx_value = adc_read(); //Realiza leitura do canal atual

        if (vry_value>=1900&&vry_value<=2200){ //Se o joystick estiver na margem de erro entre 
                                               //1900 e 2200 ele será considerado como no centro
            level_azul = 0; //led azul apagado
            top = 27; //Quadrado centralizado no eixo y

        }else{
            if (vry_value<1900){ //Caso seja lido um valor menor que o relacionado ao centro entra nesse if

                level_azul = (1900-vry_value)*(wrap/1900); //Converte o valor lido para respectivo duty cycle
                top = (1900-vry_value)*0.01315789 + 27; //Move o quadrado de forma proporcional (eixo y)

                //Os valores limite de (1900-vry_value)*0.01315789 são 0 e 27, fazendo com
                //que o quadrado se desloque do centro até a segunda borda para baixo

            }else{ //Caso seja lido um valor maior que o relacionado ao centro entra nesse if

                level_azul = (vry_value-2200)*(wrap/1900); //Converte o valor lido para respectivo duty cycle
                top = (2200-vry_value)*0.01263158 + 27; //Move o quadrado de forma proporcional (eixo y)

                //Os valores limite de (2200-vry_value)*0.01263158 são 0 e -24, fazendo com
                //que o quadrado se desloque do centro até a segunda borda para cima

            }

        }
        if (vrx_value>=1900&&vrx_value<=2200){ //Se o joystick estiver na margem de erro entre 
                                               //1900 e 2200 ele será considerado como no centro

            level_vermelho = 0; //led vermelho apagado
            left = 59; //Quadrado centralizado no eixo x

        }else{
            if (vrx_value<1900){//Caso seja lido um valor menor que o relacionado ao centro entra nesse if

                level_vermelho = (1900-vrx_value)*(wrap/1900); //Converte o valor lido para respectivo duty cycle
                left = (vrx_value-1900)*0.02947368 + 59; //Move o quadrado de forma proporcional (eixo x)

                //Os valores limite de (vrx_value-1900)*0.02947368 são 0 e -56, fazendo com
                //que o quadrado se desloque do centro até a segunda borda para esquerda

            }else{
                level_vermelho = (vrx_value-2200)*(wrap/1900); //Converte o valor lido para respectivo duty cycle
                left = (vrx_value-2200)*0.03 + 59; //Move o quadrado de forma proporcional (eixo x)

                //Os valores limite de (vrx_value-1900)*0.03 são 0 e 57, fazendo com
                //que o quadrado se desloque do centro até a segunda borda para direita

            }
        }

        if (troca1){ //Dependendo da ativação do Botão A, os leds vermelho e azul podem ser ativados ou não
            pwm_set_gpio_level(led_b, level_azul);
            pwm_set_gpio_level(led_r, level_vermelho);
        }

        ssd1306_fill(&ssd, false); //Apaga display
        ssd1306_rect(&ssd, 0, 0, 128, 64, true, false); //Desenha primeira borda
        if(troca2){
            ssd1306_rect(&ssd, 2, 2, 124, 60, true, false); //Desenha segunda borda
        }
        ssd1306_rect(&ssd, top, left, 8, 8, true, true); //Desenha quadrado no display
        ssd1306_send_data(&ssd);

    }
}