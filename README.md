# Estação Meteorológica BitDogLab

Este projeto implementa uma estação meteorológica completa utilizando a placa Raspberry Pi Pico W (BitDogLab), capaz de coletar dados ambientais, exibi-los localmente em um display OLED, e fornecer uma interface web responsiva para monitoramento em tempo real e calibração de offsets.

## 🚀 Funcionalidades

*   **Coleta de Dados de Sensores:** Leitura de temperatura, umidade (AHT20), pressão e altitude (BMP280).
*   **Display OLED (SSD1306):** Exibição local dos dados dos sensores e informações de conexão Wi-Fi, com navegação via botões.
*   **Conectividade Wi-Fi:** Conexão a uma rede Wi-Fi para acesso remoto à estação.
*   **Servidor Web Embarcado:** Interface web minimalista e responsiva para visualização dos dados em tempo real e calibração de offsets.
*   **Calibração de Offsets:** Ajuste fino dos valores dos sensores (temperatura, umidade, pressão, altitude) através da interface web.
*   **Indicadores Visuais (LEDs):**
    *   **LED RGB:** Indica o status da conexão Wi-Fi (verde para conectado, vermelho para desconectado).
    *   **Matriz de LEDs WS2812:** Exibe padrões visuais (bom, alerta, crítico) com base no status de um sensor selecionado.
*   **Alertas Sonoros (Buzzer):** Feedback audível para interações com botões e alertas de condições ambientais críticas.
*   **Atualização em Tempo Real:** Dados atualizados a cada segundo na interface web.

## 🛠️ Hardware Necessário

*   Placa [BitDogLab](https://bitdoglab.com.br/) (baseada em Raspberry Pi Pico W)
*   Sensor de Temperatura e Umidade AHT20
*   Sensor de Pressão Barométrica BMP280
*   Display OLED SSD1306 (128x64 pixels)
*   Matriz de LEDs WS2812 (NeoPixel)
*   Buzzer
*   Botões (integrados na BitDogLab ou externos)

## 💻 Software Necessário

*   [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
*   Compilador C/C++ (GCC ARM Embedded)
*   CMake
*   Make
*   Bibliotecas de drivers para AHT20, BMP280, SSD1306 (já incluídas no projeto)
*   LwIP (Lightweight IP) stack (parte do Pico SDK)

## ⚙️ Configuração e Instalação

### 1. Configuração do Ambiente de Desenvolvimento

Certifique-se de ter o Raspberry Pi Pico SDK configurado em seu sistema. Siga as instruções oficiais do Raspberry Pi para configurar o ambiente de desenvolvimento para C/C++.

### 2. Configuração do Wi-Fi

Abra o arquivo `estacao_meteorologica_responsiva.c` e edite as seguintes linhas com as credenciais da sua rede Wi-Fi:

```c
#define WIFI_SSID "SEU_SSID_AQUI"
#define WIFI_PASS "SUA_SENHA_AQUI"