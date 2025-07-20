#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"
#include "hardware/pio.h"
#include "lwip/tcp.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "font.h"
#include "ws2812.pio.h"

// Configurações de pinos
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define ENDERECO_DISPLAY 0x3C

#define BOTAO_A 5
#define BOTAO_B 6
#define BUZZER_PIN 10
#define WS2812_PIN 7
#define LED_RGB_R 18
#define LED_RGB_G 19
#define LED_RGB_B 20

#define NUM_PIXELS 25
#define SEA_LEVEL_PRESSURE 101325.0

// Configurações Wi-Fi
#define WIFI_SSID "KASATECH CARVALHO"
#define WIFI_PASS "Ph01felix!"

// Estruturas globais
typedef struct {
    float temperatura_aht;
    float umidade;
    float temperatura_bmp;
    float pressao;
    float altitude;
    bool wifi_conectado;
} DadosSensores;

typedef enum {
    TELA_SENSORES,
    TELA_WIFI
} TipoTela;

typedef enum {
    STATUS_TEMPERATURA,
    STATUS_UMIDADE,
    STATUS_PRESSAO,
    STATUS_ALTITUDE
} TipoStatus;

typedef enum {
    NIVEL_BOM,
    NIVEL_ALERTA,
    NIVEL_CRITICO
} NivelStatus;

// Variáveis globais
DadosSensores dados_sensores = {0};
TipoTela tela_atual = TELA_SENSORES;
TipoStatus status_atual = STATUS_TEMPERATURA;
ssd1306_t ssd;
PIO pio = pio0;
int sm = 0;
char ip_str[24] = "0.0.0.0";
volatile bool botao_a_pressionado = false;
volatile bool botao_b_pressionado = false;
volatile uint32_t ultimo_debounce_a = 0;
volatile uint32_t ultimo_debounce_b = 0;

// Nomes dos status para exibição no display
const char* nomes_status[] = {"Temp", "Umid", "Pres", "Alt"};

// Padrões para matriz de LEDs
static const bool padrao_bom[NUM_PIXELS] = {
    0,0,1,0,0,
    0,1,1,1,0,
    1,1,1,1,1,
    0,1,1,1,0,
    0,0,1,0,0
};

static const bool padrao_alerta[NUM_PIXELS] = {
    0,0,1,0,0,
    0,1,1,1,0,
    0,1,1,1,0,
    0,0,1,0,0,
    0,1,1,1,0
};

static const bool padrao_critico[NUM_PIXELS] = {
    1,0,0,0,1,
    0,1,0,1,0,
    0,0,1,0,0,
    0,1,0,1,0,
    1,0,0,0,1
};

// HTML responsivo com gráficos melhorados
const char HTML_BODY[] =
    "<html><head><meta charset='UTF-8' name='viewport' content='width=device-width,initial-scale=1'><title>Estacao Meteorologica</title>"
    "<style>"
    "body{font-family:Arial,sans-serif;margin:10px;background:#f8f9fa;color:#333;}"
    "h2{text-align:center;color:#2c3e50;margin:15px 0;}"
    ".container{max-width:600px;margin:0 auto;}"
    ".card{background:#fff;margin:8px 0;padding:12px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    ".row{display:flex;align-items:center;margin:8px 0;}"
    ".label{font-weight:bold;min-width:60px;color:#34495e;}"
    ".value{font-size:18px;font-weight:bold;min-width:80px;text-align:right;}"
    ".chart{flex:1;margin-left:15px;height:20px;background:#ecf0f1;border-radius:10px;overflow:hidden;position:relative;}"
    ".bar{height:100%;border-radius:10px;transition:width 0.5s ease;position:relative;}"
    ".bar::after{content:attr(data-value);position:absolute;right:5px;top:50%;transform:translateY(-50%);font-size:10px;color:#fff;text-shadow:1px 1px 1px rgba(0,0,0,0.5);}"
    ".temp{background:linear-gradient(90deg,#27ae60,#f39c12,#e74c3c);}"
    ".humid{background:linear-gradient(90deg,#3498db,#2ecc71);}"
    ".press{background:linear-gradient(90deg,#9b59b6,#3498db);}"
    ".alt{background:linear-gradient(90deg,#8b4513,#27ae60);}"
    ".status{padding:4px 12px;border-radius:12px;color:#fff;font-size:12px;font-weight:bold;}"
    ".online{background:#27ae60;}"
    ".offline{background:#e74c3c;}"
    ".footer{text-align:center;margin-top:20px;font-size:12px;color:#7f8c8d;}"
    "@media (max-width:480px){"
    ".row{flex-direction:column;align-items:flex-start;}"
    ".chart{width:100%;margin:8px 0 0 0;}"
    ".value{text-align:left;margin:4px 0;}"
    "}"
    "</style>"
    "<script>"
    "function updateData(){"
    "fetch('/d').then(r=>r.json()).then(d=>{"
    "document.getElementById('temp-val').textContent=d.t.toFixed(1)+'°C';"
    "document.getElementById('humid-val').textContent=d.h.toFixed(1)+'%';"
    "document.getElementById('press-val').textContent=d.p.toFixed(1)+' kPa';"
    "document.getElementById('alt-val').textContent=d.a.toFixed(0)+' m';"
    "var tempPct=Math.min(Math.max(d.t*2.5,0),100);"
    "var humidPct=Math.min(Math.max(d.h,0),100);"
    "var pressPct=Math.min(Math.max((d.p-95)*10,0),100);"
    "var altPct=Math.min(Math.max(d.a/20,0),100);"
    "document.getElementById('temp-bar').style.width=tempPct+'%';"
    "document.getElementById('humid-bar').style.width=humidPct+'%';"
    "document.getElementById('press-bar').style.width=pressPct+'%';"
    "document.getElementById('alt-bar').style.width=altPct+'%';"
    "document.getElementById('temp-bar').setAttribute('data-value',tempPct.toFixed(0)+'%');"
    "document.getElementById('humid-bar').setAttribute('data-value',humidPct.toFixed(0)+'%');"
    "document.getElementById('press-bar').setAttribute('data-value',pressPct.toFixed(0)+'%');"
    "document.getElementById('alt-bar').setAttribute('data-value',altPct.toFixed(0)+'%');"
    "}).catch(e=>{"
    "console.log('Erro:',e);"
    "document.getElementById('status').textContent='Offline';"
    "document.getElementById('status').className='status offline';"
    "});"
    "}"
    "setInterval(updateData,2000);"
    "updateData();"
    "</script>"
    "</head><body>"
    "<div class='container'>"
    "<h2>🌤️ Estação Meteorológica</h2>"
    "<div class='card'>"
    "<div style='text-align:center;margin-bottom:10px;'>"
    "<span id='status' class='status online'>Online</span>"
    "</div>"
    "</div>"
    "<div class='card'>"
    "<div class='row'>"
    "<span class='label'>🌡️ Temp:</span>"
    "<span class='value' id='temp-val'>--°C</span>"
    "<div class='chart'><div id='temp-bar' class='bar temp' data-value='0%'></div></div>"
    "</div>"
    "<div class='row'>"
    "<span class='label'>💧 Umid:</span>"
    "<span class='value' id='humid-val'>--%</span>"
    "<div class='chart'><div id='humid-bar' class='bar humid' data-value='0%'></div></div>"
    "</div>"
    "<div class='row'>"
    "<span class='label'>🌪️ Pres:</span>"
    "<span class='value' id='press-val'>-- kPa</span>"
    "<div class='chart'><div id='press-bar' class='bar press' data-value='0%'></div></div>"
    "</div>"
    "<div class='row'>"
    "<span class='label'>⛰️ Alt:</span>"
    "<span class='value' id='alt-val'>-- m</span>"
    "<div class='chart'><div id='alt-bar' class='bar alt' data-value='0%'></div></div>"
    "</div>"
    "</div>"
    "<div class='footer'>"
    "<p>BitDogLab - CEPEDI TIC37 EMBARCATECH</p>"
    "<p>Atualização automática a cada 2 segundos</p>"
    "</div>"
    "</div>"
    "</body></html>";

// Protótipos de funções
void init_hardware(void);
void init_wifi(void);
void init_sensores(void);
void ler_sensores(void);
void atualizar_display(void);
void atualizar_matriz_leds(void);
void atualizar_led_rgb(void);
void verificar_alertas(void);
NivelStatus avaliar_temperatura(float temp);
NivelStatus avaliar_umidade(float umidade);
NivelStatus avaliar_pressao(float pressao);
void gpio_irq_handler(uint gpio, uint32_t events);
void start_http_server(void);

// Funções para matriz de LEDs
uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

void display_matriz(const bool *buffer, uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = urgb_u32(r, g, b);
    for (int i = 0; i < NUM_PIXELS; i++) {
        put_pixel(buffer[i] ? color : 0);
    }
}

// Função para tocar som no buzzer
void play_sound(int frequency, int duration_ms) {
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    if (frequency <= 0) {
        pwm_set_gpio_level(BUZZER_PIN, 0);
        sleep_ms(duration_ms);
        return;
    }
    float divider = 20.0f;
    pwm_set_clkdiv(slice_num, divider);
    uint16_t wrap = (125000000 / (frequency * divider)) - 1;
    pwm_set_wrap(slice_num, wrap);
    pwm_set_gpio_level(BUZZER_PIN, wrap / 2);
    sleep_ms(duration_ms);
    pwm_set_gpio_level(BUZZER_PIN, 0);
}

// Tratamento de interrupções dos botões
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (gpio == BOTAO_A && (now - ultimo_debounce_a) > 200) {
        botao_a_pressionado = true;
        ultimo_debounce_a = now;
    } else if (gpio == BOTAO_B && (now - ultimo_debounce_b) > 200) {
        botao_b_pressionado = true;
        ultimo_debounce_b = now;
    }
}

// Avaliação de níveis dos sensores
NivelStatus avaliar_temperatura(float temp) {
    if (temp < 10 || temp > 35) return NIVEL_CRITICO;
    if (temp < 15 || temp > 30) return NIVEL_ALERTA;
    return NIVEL_BOM;
}

NivelStatus avaliar_umidade(float umidade) {
    if (umidade < 30 || umidade > 80) return NIVEL_CRITICO;
    if (umidade < 40 || umidade > 70) return NIVEL_ALERTA;
    return NIVEL_BOM;
}

NivelStatus avaliar_pressao(float pressao) {
    float pressao_kpa = pressao / 1000.0;
    if (pressao_kpa < 95 || pressao_kpa > 105) return NIVEL_CRITICO;
    if (pressao_kpa < 98 || pressao_kpa > 102) return NIVEL_ALERTA;
    return NIVEL_BOM;
}

// Estrutura para HTTP
struct http_state {
    char response[4096];
    size_t len;
    size_t sent;
};

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) {
        tcp_close(tpcb);
        return ERR_OK;
    }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) {
        pbuf_free(p);
        tcp_close(tpcb);
        return ERR_MEM;
    }
    hs->sent = 0;

    // JSON ultra compacto
    if (strstr(req, "GET /d")) {
        char json[128];
        int json_len = snprintf(json, sizeof(json),
                               "{\"t\":%.1f,\"h\":%.1f,\"p\":%.1f,\"a\":%.0f}",
                               dados_sensores.temperatura_aht,
                               dados_sensores.umidade,
                               dados_sensores.pressao / 1000.0,
                               dados_sensores.altitude);

        hs->len = snprintf(hs->response, sizeof(hs->response),
                          "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                          json_len, json);
    } else {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                          "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                          (int)strlen(HTML_BODY), HTML_BODY);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb) {
        printf("Erro ao criar PCB TCP\n");
        return;
    }
    if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Erro ao ligar o servidor na porta 80\n");
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
    printf("Servidor HTTP rodando na porta 80...\n");
}

void init_hardware(void) {
    stdio_init_all();
    
    // Inicializar botões
    gpio_init(BOTAO_A);
    gpio_set_dir(BOTAO_A, GPIO_IN);
    gpio_pull_up(BOTAO_A);
    gpio_set_irq_enabled_with_callback(BOTAO_A, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    gpio_init(BOTAO_B);
    gpio_set_dir(BOTAO_B, GPIO_IN);
    gpio_pull_up(BOTAO_B);
    gpio_set_irq_enabled_with_callback(BOTAO_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
    
    // Inicializar LEDs RGB
    gpio_init(LED_RGB_R);
    gpio_init(LED_RGB_G);
    gpio_init(LED_RGB_B);
    gpio_set_dir(LED_RGB_R, GPIO_OUT);
    gpio_set_dir(LED_RGB_G, GPIO_OUT);
    gpio_set_dir(LED_RGB_B, GPIO_OUT);
    
    // Inicializar buzzer
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_init(slice_num, &config, true);
    pwm_set_gpio_level(BUZZER_PIN, 0);
    
    // Inicializar matriz de LEDs
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);
    
    // Inicializar display
    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);
    
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, ENDERECO_DISPLAY, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);
}

void init_sensores(void) {
    // Inicializar I2C para sensores
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
    
    // Inicializar BMP280
    bmp280_init(I2C_PORT);
    
    // Inicializar AHT20
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);
}

void init_wifi(void) {
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "Iniciando Wi-Fi", 0, 0);
    ssd1306_draw_string(&ssd, "Aguarde...", 0, 20);
    ssd1306_send_data(&ssd);
    
    if (cyw43_arch_init()) {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => FALHA", 0, 0);
        ssd1306_send_data(&ssd);
        dados_sensores.wifi_conectado = false;
        return;
    }
    
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 10000)) {
        ssd1306_fill(&ssd, false);
        ssd1306_draw_string(&ssd, "WiFi => ERRO", 0, 0);
        ssd1306_send_data(&ssd);
        dados_sensores.wifi_conectado = false;
        return;
    }
    
    uint8_t *ip = (uint8_t *)&(cyw43_state.netif[0].ip_addr.addr);
    snprintf(ip_str, sizeof(ip_str), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    dados_sensores.wifi_conectado = true;
    
    ssd1306_fill(&ssd, false);
    ssd1306_draw_string(&ssd, "WiFi => OK", 0, 0);
    ssd1306_draw_string(&ssd, ip_str, 0, 20);
    ssd1306_send_data(&ssd);
    
    start_http_server();
    sleep_ms(2000);
}

void ler_sensores(void) {
    // Ler BMP280
    struct bmp280_calib_param params;
    bmp280_get_calib_params(I2C_PORT, &params);
    
    int32_t raw_temp_bmp, raw_pressure;
    bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
    
    int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
    int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);
    
    float temp_bmp = temperature / 100.0;
    dados_sensores.pressao = pressure;
    dados_sensores.altitude = 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
    
    // Ler AHT20
    AHT20_Data data;
    if (aht20_read(I2C_PORT, &data)) {
        // Calcular média das temperaturas
        dados_sensores.temperatura_aht = (data.temperature + temp_bmp) / 2.0;
        dados_sensores.temperatura_bmp = temp_bmp; // Manter para referência
        dados_sensores.umidade = data.humidity;
    }
}

void atualizar_display(void) {
    ssd1306_fill(&ssd, false);
    
    if (tela_atual == TELA_SENSORES) {
        char str_temp[10], str_umid[10], str_press[10], str_alt[10];
        char header[20];
        
        snprintf(str_temp, sizeof(str_temp), "%.1fC", dados_sensores.temperatura_aht);
        snprintf(str_umid, sizeof(str_umid), "%.1f%%", dados_sensores.umidade);
        snprintf(str_press, sizeof(str_press), "%.1fkPa", dados_sensores.pressao / 1000.0);
        snprintf(str_alt, sizeof(str_alt), "%.0fm", dados_sensores.altitude);
        snprintf(header, sizeof(header), "-> %s", nomes_status[status_atual]);
        
        ssd1306_rect(&ssd, 2, 2, 124, 60, true, false);
        ssd1306_draw_string(&ssd, header, 4, 5);
        ssd1306_line(&ssd, 2, 15, 126, 15, true);
        
        ssd1306_draw_string(&ssd, "Temp:", 4, 20);
        ssd1306_draw_string(&ssd, str_temp, 45, 20);
        
        ssd1306_draw_string(&ssd, "Umid:", 4, 30);
        ssd1306_draw_string(&ssd, str_umid, 45, 30);
        
        ssd1306_draw_string(&ssd, "Pres:", 4, 40);
        ssd1306_draw_string(&ssd, str_press, 45, 40);
        
        ssd1306_draw_string(&ssd, "Alt:", 4, 50);
        ssd1306_draw_string(&ssd, str_alt, 35, 50);
        
    } else { // TELA_WIFI
        ssd1306_rect(&ssd, 2, 2, 124, 60, true, false);
        ssd1306_draw_string(&ssd, "STATUS CONEXAO", 10, 5);
        ssd1306_line(&ssd, 2, 15, 126, 15, true);
        
        if (dados_sensores.wifi_conectado) {
            ssd1306_draw_string(&ssd, "WiFi: CONECTADO", 4, 20);
            ssd1306_draw_string(&ssd, "IP:", 4, 30);
            ssd1306_draw_string(&ssd, ip_str, 4, 40);
            ssd1306_draw_string(&ssd, "Porta: 80", 4, 50);
        } else {
            ssd1306_draw_string(&ssd, "WiFi: DESCONECT.", 4, 25);
            ssd1306_draw_string(&ssd, "Verifique config", 4, 40);
        }
    }
    
    ssd1306_send_data(&ssd);
}

void atualizar_matriz_leds(void) {
    NivelStatus nivel;
    const bool *padrao;
    uint8_t r, g, b;
    
    switch (status_atual) {
        case STATUS_TEMPERATURA:
            nivel = avaliar_temperatura(dados_sensores.temperatura_aht); // Já é a média
            break;
        case STATUS_UMIDADE:
            nivel = avaliar_umidade(dados_sensores.umidade);
            break;
        case STATUS_PRESSAO:
            nivel = avaliar_pressao(dados_sensores.pressao);
            break;
        case STATUS_ALTITUDE:
            nivel = (dados_sensores.altitude > 1000) ? NIVEL_CRITICO : 
                   (dados_sensores.altitude > 500) ? NIVEL_ALERTA : NIVEL_BOM;
            break;
    }
    
    switch (nivel) {
        case NIVEL_BOM:
            padrao = padrao_bom;
            r = 0; g = 10; b = 0;
            break;
        case NIVEL_ALERTA:
            padrao = padrao_alerta;
            r = 10; g = 5; b = 0;
            break;
        case NIVEL_CRITICO:
            padrao = padrao_critico;
            r = 10; g = 0; b = 0;
            break;
    }
    
    display_matriz(padrao, r, g, b);
}

void atualizar_led_rgb(void) {
    if (dados_sensores.wifi_conectado) {
        gpio_put(LED_RGB_R, 0);
        gpio_put(LED_RGB_G, 1);
        gpio_put(LED_RGB_B, 0);
    } else {
        gpio_put(LED_RGB_R, 1);
        gpio_put(LED_RGB_G, 0);
        gpio_put(LED_RGB_B, 0);
    }
}

void verificar_alertas(void) {
    static uint32_t ultimo_alerta = 0;
    uint32_t agora = to_ms_since_boot(get_absolute_time());
    
    // Verificar apenas a cada 5 segundos
    if ((agora - ultimo_alerta) < 5000) return;
    
    bool alerta_temperatura = (dados_sensores.temperatura_aht < 10 || dados_sensores.temperatura_aht > 35);
    bool alerta_umidade = (dados_sensores.umidade < 30 || dados_sensores.umidade > 80);
    
    if (alerta_temperatura || alerta_umidade) {
        play_sound(800, 200);
        sleep_ms(100);
        play_sound(1000, 200);
        ultimo_alerta = agora;
    }
}

int main(void) {
    init_hardware();
    sleep_ms(1000);
    
    init_sensores();
    init_wifi();
    
    uint32_t ultimo_update = 0;
    
    while (true) {
        uint32_t agora = to_ms_since_boot(get_absolute_time());
        
        // Processar botões
        if (botao_a_pressionado) {
            botao_a_pressionado = false;
            status_atual = (status_atual + 1) % 4;
            play_sound(1200, 100);
        }
        
        if (botao_b_pressionado) {
            botao_b_pressionado = false;
            tela_atual = (tela_atual == TELA_SENSORES) ? TELA_WIFI : TELA_SENSORES;
            play_sound(800, 100);
        }
        
        // Atualizar sensores e displays a cada 1 segundo
        if ((agora - ultimo_update) >= 1000) {
            ler_sensores();
            atualizar_display();
            atualizar_matriz_leds();
            atualizar_led_rgb();
            verificar_alertas();
            ultimo_update = agora;
        }
        
        // Processar requisições web
        if (dados_sensores.wifi_conectado) {
            cyw43_arch_poll();
        }
        
        sleep_ms(50);
    }
    
    return 0;
}
