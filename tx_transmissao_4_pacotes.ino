// Código utilizado para fazer o teste de transmissão via LoRa conforme o experimento solicitado 
// transmitir 4 pacote por segundos com 255 bytes cada um, durante 10 minutos, oq da 2400 pacotes, é necessario fazer teste 3 vezes
// Mostrar no display para ficar bem visualmente o RSII, número de pacotes transmitidos, numero de pacotes recebidos, colocar uma taxa de recebimento e de perda de pacotes

#include <Arduino.h>
#include <esp_system.h>
#include "LoRaWan_APP.h"
#include "LoRaConfig.h"
#include "HT_SSD1306Wire.h"


#define TOTAL_PACOTES 2400
#define TAMANHO_PACOTE 255

//Intervalos e tempos de espera
#define DELAY_ENTRE_PACOTES 0
#define DELAY_ENTRE_TENTATIVAS 5000
#define TIMEOUT_PONG 2000
#define TIMEOUT_ACK_START 2000

// Valores usados para identificar os pacotes de controle
#define BYTE_FINALIZACAO 0xFF
#define TAMANHO_PING 5
#define TAMANHO_PONG 5
#define TAMANHO_START 8
#define TAMANHO_ACK_START 12

static RadioEvents_t RadioEvents;

// ============================================================
// OLED
// ============================================================

static SSD1306Wire display(0x3C, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// ============================================================
// BUFFERS
// ============================================================

// Buffer dos pacotes normais
uint8_t bufferTx[TAMANHO_PACOTE];

// Buffer utilizado para indicar o fim da transmissao
uint8_t bufferFinalizacao[TAMANHO_PACOTE];

// Buffer para verificar se o receptor esta ativo
uint8_t bufferPing[TAMANHO_PING] = {
    'P', 'I', 'N', 'G', 0x01
};

// Buffer para solicitar o inicio de uma nova sessao
uint8_t bufferStart[TAMANHO_START] = {
    'S', 'T', 'R', 'T', 0, 0, 0, 0
};

// Buffer para receber a confirmacao do inicio
uint8_t bufferAck[TAMANHO_ACK_START];

// ============================================================
// ESTADOS DA TRANSMISSAO
// ============================================================

enum EstadoTX
{
    AGUARDANDO_INICIO_PING,
    PING_EM_TRANSMISSAO,
    AGUARDANDO_PONG,
    ENVIANDO_START,
    START_EM_TRANSMISSAO,
    AGUARDANDO_ACK_START,
    TRANSMITINDO_DADOS,
    FINALIZACAO_EM_TRANSMISSAO,
    TRANSMISSAO_FINALIZADA
};

EstadoTX estadoTX = AGUARDANDO_INICIO_PING;

// ============================================================
// VARIAVEIS
// ============================================================

uint32_t nonceTX = 0;
uint32_t numeroExperimento = 0;
uint32_t pacotesEnviados = 0;
uint32_t tentativasPing = 0;

bool lora_idle = true;
bool transmissaoFinalizada = false;

unsigned long tempoUltimoEnvio = 0;
unsigned long tempoUltimaTentativa = 0;
unsigned long inicioEsperaPong = 0;
unsigned long inicioEsperaAck = 0;

// ============================================================
// FUNCOES AUXILIARES
// ============================================================

void VextON()
{
    pinMode(Vext, OUTPUT);
    digitalWrite(Vext, LOW);
}

// Separa um numero de 32 bits em quatro bytes
void gravarU32(uint8_t *p, uint32_t valor)
{
    p[0] = valor & 0xFF;
    p[1] = (valor >> 8) & 0xFF;
    p[2] = (valor >> 16) & 0xFF;
    p[3] = (valor >> 24) & 0xFF;
}

// Reconstroi um numero de 32 bits a partir de quatro bytes
uint32_t lerU32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

// Verifica se a resposta recebida corresponde ao PONG esperado
bool ehPong(uint8_t *payload, uint16_t tamanho)
{
    return tamanho == TAMANHO_PONG &&
           payload[0] == 'P' &&
           payload[1] == 'O' &&
           payload[2] == 'N' &&
           payload[3] == 'G' &&
           payload[4] == 0x01;
}

// Verifica se o ACK pertence a esta sessao de transmissao
bool ehAckStart(uint8_t *payload, uint16_t tamanho)
{
    return tamanho == TAMANHO_ACK_START &&
           payload[0] == 'A' &&
           payload[1] == 'C' &&
           payload[2] == 'K' &&
           payload[3] == 'S' &&
           lerU32(&payload[4]) == nonceTX;
}

// ============================================================
// PREPARA OS PACOTES
// ============================================================

// Todos os pacotes normais possuem exatamente os mesmos 255 bytes:
// byte_000 = 0, byte_001 = 1, ..., byte_254 = 254.
void prepararPacote()
{
    for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
    {
        bufferTx[i] = (uint8_t)i;
    }
}

// ============================================================
// DISPLAY
// ============================================================

void atualizarDisplay()
{
    float progresso =
        100.0f * pacotesEnviados / TOTAL_PACOTES;

    String estado = "Aguardando RX...";

    if (estadoTX == AGUARDANDO_PONG ||
        estadoTX == PING_EM_TRANSMISSAO ||
        estadoTX == AGUARDANDO_INICIO_PING)
    {
        estado = "Buscando RX...";
    }
    else if (estadoTX == ENVIANDO_START ||
             estadoTX == START_EM_TRANSMISSAO ||
             estadoTX == AGUARDANDO_ACK_START)
    {
        estado = "Confirmando inicio";
    }
    else if (estadoTX == TRANSMITINDO_DADOS ||
             estadoTX == FINALIZACAO_EM_TRANSMISSAO)
    {
        estado = "Transmitindo...";
    }
    else if (estadoTX == TRANSMISSAO_FINALIZADA)
    {
        estado = "Concluido";
    }

    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);

    display.drawString(0, 0,
        "TX LoRa - EXP " +
        (numeroExperimento ? String(numeroExperimento) : String("--")));

    display.drawString(0, 12, estado);
    display.drawString(0, 24,
        "TX: " + String(pacotesEnviados) +
        "/" + String(TOTAL_PACOTES));
    display.drawString(0, 36,
        "Progresso: " + String(progresso, 1) + "%");
    display.drawString(0, 48,
        "PING: " + String(tentativasPing));

    display.display();
}

// ============================================================
// CALLBACK: TRANSMISSAO CONCLUIDA
// ============================================================

void OnTxDone(void)
{
    lora_idle = true;

    // Se o PING foi enviado, aguarda a resposta do receptor
    if (estadoTX == PING_EM_TRANSMISSAO)
    {
        estadoTX = AGUARDANDO_PONG;
        inicioEsperaPong = millis();
        Radio.Rx(0);
        return;
    }

     // Depois do START, aguarda a confirmacao da sessao
    if (estadoTX == START_EM_TRANSMISSAO)
    {
        estadoTX = AGUARDANDO_ACK_START;
        inicioEsperaAck = millis();
        Radio.Rx(0);
        return;
    }
    
    // A transmissao do pacote final foi concluida
    if (estadoTX == FINALIZACAO_EM_TRANSMISSAO)
    {
        transmissaoFinalizada = true;
        estadoTX = TRANSMISSAO_FINALIZADA;

        Serial.println();
        Serial.println("=================================");
        Serial.println("     TRANSMISSAO FINALIZADA");
        Serial.println("=================================");
        Serial.print("Experimento: ");
        Serial.println(numeroExperimento);
        Serial.print("Total enviado: ");
        Serial.println(pacotesEnviados);

        atualizarDisplay();
        return;
    }
    
    // Atualiza a contagem somente apos a conclusao do envio
    if (estadoTX == TRANSMITINDO_DADOS)
    {
        pacotesEnviados++;
        tempoUltimoEnvio = millis();

        if (pacotesEnviados % 10 == 0 ||
            pacotesEnviados == TOTAL_PACOTES)
        {
            Serial.print("EXP ");
            Serial.print(numeroExperimento);
            Serial.print(" | Enviados: ");
            Serial.print(pacotesEnviados);
            Serial.print("/");
            Serial.println(TOTAL_PACOTES);
        }
    }
}

// ============================================================
// CALLBACK: TIMEOUT DE TRANSMISSAO
// ============================================================

void OnTxTimeout(void)
{
    Serial.println("Timeout na transmissao.");
    lora_idle = true;

    if (estadoTX == PING_EM_TRANSMISSAO)
    {
        estadoTX = AGUARDANDO_INICIO_PING;
        tempoUltimaTentativa = millis();
        return;
    }

    if (estadoTX == START_EM_TRANSMISSAO)
    {
        estadoTX = ENVIANDO_START;
        return;
    }

    if (estadoTX == FINALIZACAO_EM_TRANSMISSAO)
    {
        estadoTX = TRANSMITINDO_DADOS;
        return;
    }

    if (estadoTX == TRANSMITINDO_DADOS)
    {
        tempoUltimoEnvio = millis();
    }
}

// ============================================================
// CALLBACK: RECEPCAO
// ============================================================

void OnRxDone(uint8_t *payload, uint16_t tamanho,
              int16_t rssi, int8_t snr)
{ 
    // Verifica a resposta ao PING
    if (estadoTX == AGUARDANDO_PONG)
    {
        if (ehPong(payload, tamanho))
        {
            Radio.Sleep();
            Serial.println("PONG recebido.");
            Serial.println("Solicitando inicio de sessao.");
            estadoTX = ENVIANDO_START;
            lora_idle = true;
        }
        else
        {
            Radio.Rx(0);
        }
        return;
    }
    
    // Verifica a confirmacao do inicio da sessao
    if (estadoTX == AGUARDANDO_ACK_START)
    {
        if (ehAckStart(payload, tamanho))
        {
            numeroExperimento = lerU32(&payload[8]);

            Radio.Sleep();

            pacotesEnviados = 0;
            tempoUltimoEnvio =
                millis() - DELAY_ENTRE_PACOTES;

            estadoTX = TRANSMITINDO_DADOS;
            lora_idle = true;

            Serial.println();
            Serial.println("=================================");
            Serial.println("       INICIO CONFIRMADO");
            Serial.println("=================================");
            Serial.print("Experimento: ");
            Serial.println(numeroExperimento);
            Serial.println("Iniciando transmissao dos dados.");

            atualizarDisplay();
        }
        else
        {
            // Mantem a recepcao ativa enquanto aguarda uma resposta valida
            Radio.Rx(0);
        }
        return;
    }

    Radio.Rx(0);
}

// ============================================================
// CALLBACKS DE TIMEOUT E ERRO DE RECEPCAO
// ============================================================

void OnRxTimeout(void)
{
    if (estadoTX == AGUARDANDO_PONG ||
        estadoTX == AGUARDANDO_ACK_START)
    {
        Radio.Rx(0);
    }
}

void OnRxError(void)
{
    if (estadoTX == AGUARDANDO_PONG ||
        estadoTX == AGUARDANDO_ACK_START)
    {
        Radio.Rx(0);
    }
}

// ============================================================
// ENVIA PING
// ============================================================

void enviarPing()
{
    tentativasPing++;

    Serial.print("Enviando PING - tentativa ");
    Serial.println(tentativasPing);

    estadoTX = PING_EM_TRANSMISSAO;
    lora_idle = false;

    Radio.Send(bufferPing, TAMANHO_PING);
}

// ============================================================
// ENVIA SOLICITACAO DE INICIO
// ============================================================

void enviarStart()
{
    bufferStart[0] = 'S';
    bufferStart[1] = 'T';
    bufferStart[2] = 'R';
    bufferStart[3] = 'T';

    gravarU32(&bufferStart[4], nonceTX);

    estadoTX = START_EM_TRANSMISSAO;
    lora_idle = false;

    Radio.Send(bufferStart, TAMANHO_START);
}

// ============================================================
// ENVIA PACOTE DE FINALIZACAO
// ============================================================

void enviarFinalizacao()
{
    Serial.println();
    Serial.println("Enviando pacote de finalizacao.");

    estadoTX = FINALIZACAO_EM_TRANSMISSAO;
    lora_idle = false;

    Radio.Send(bufferFinalizacao, TAMANHO_PACOTE);
}

// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       TX LoRa - EXPERIMENTO");
    Serial.println("=================================");

    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);

    VextON();
    delay(200);

    display.init();
    display.clear();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.drawString(0, 0, "TX LoRa");
    display.drawString(0, 15, "Inicializando...");
    display.display();

    // Prepara uma unica vez o pacote normal, identico em todas as 2400 transmissoes.
    prepararPacote();

    // O pacote de finalizacao possui 255 bytes iguais a FF.
    for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
    {
        bufferFinalizacao[i] = BYTE_FINALIZACAO;
    }
    // Gera um identificador para diferenciar cada sessao
    nonceTX = esp_random();

    if (nonceTX == 0)
    {
        nonceTX = (uint32_t)millis() | 1;
    }
 // Configura os callbacks do radio
    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;
    RadioEvents.RxDone = OnRxDone;
    RadioEvents.RxTimeout = OnRxTimeout;
    RadioEvents.RxError = OnRxError;

// Inicializa o radio e configura a frequencia
    Radio.Init(&RadioEvents);
    Radio.SetChannel(RF_FREQUENCY);
   
   // Configuracao de transmissao
    Radio.SetTxConfig(
        MODEM_LORA, TX_OUTPUT_POWER, 0,
        LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
        LORA_CODINGRATE, LORA_PREAMBLE_LENGTH,
        LORA_FIX_LENGTH_PAYLOAD_ON, true, 0, 0,
        LORA_IQ_INVERSION_ON, 3000
    );
   
    // Configuracao de recepcao para os pacotes de resposta
    Radio.SetRxConfig(
        MODEM_LORA, LORA_BANDWIDTH,
        LORA_SPREADING_FACTOR, LORA_CODINGRATE,
        0, LORA_PREAMBLE_LENGTH, LORA_SYMBOL_TIMEOUT,
        LORA_FIX_LENGTH_PAYLOAD_ON, 0, true, 0, 0,
        LORA_IQ_INVERSION_ON, true
    );

    Serial.println("Radio inicializado.");
    Serial.print("Tamanho do pacote: ");
    Serial.println(TAMANHO_PACOTE);
    Serial.print("Total de pacotes: ");
    Serial.println(TOTAL_PACOTES);
    Serial.print("Identificador da sessao: ");
    Serial.println(nonceTX);
    Serial.println("Todos os pacotes de dados sao identicos.");
    Serial.println("Aguardando RX.");

    tempoUltimaTentativa =
        millis() - DELAY_ENTRE_TENTATIVAS;
    tempoUltimoEnvio = millis();

    atualizarDisplay();
}

// ============================================================
// LOOP
// ============================================================

void loop()
{
    Radio.IrqProcess();

    if (transmissaoFinalizada)
    {
        delay(250);
        return;
    }

    if (estadoTX == AGUARDANDO_PONG &&
        millis() - inicioEsperaPong >= TIMEOUT_PONG)
    {
        Radio.Sleep();
        Serial.println("PONG nao recebido. Nova tentativa.");
        estadoTX = AGUARDANDO_INICIO_PING;
        lora_idle = true;
        tempoUltimaTentativa = millis();
    }

    if (estadoTX == AGUARDANDO_ACK_START &&
        millis() - inicioEsperaAck >= TIMEOUT_ACK_START)
    {
        Radio.Sleep();
        Serial.println("ACK de inicio nao recebido. Repetindo START.");
        estadoTX = ENVIANDO_START;
        lora_idle = true;
    }

    if (estadoTX == AGUARDANDO_INICIO_PING &&
        lora_idle &&
        millis() - tempoUltimaTentativa >=
            DELAY_ENTRE_TENTATIVAS)
    {
        enviarPing();
    }

    if (estadoTX == ENVIANDO_START && lora_idle)
    {
        enviarStart();
    }

    if (estadoTX == TRANSMITINDO_DADOS &&
        lora_idle &&
        pacotesEnviados < TOTAL_PACOTES &&
        millis() - tempoUltimoEnvio >= DELAY_ENTRE_PACOTES)
    {
        lora_idle = false;
        Radio.Send(bufferTx, TAMANHO_PACOTE);
    }
    else if (estadoTX == TRANSMITINDO_DADOS &&
             lora_idle &&
             pacotesEnviados >= TOTAL_PACOTES &&
             millis() - tempoUltimoEnvio >= DELAY_ENTRE_PACOTES)
    {
        enviarFinalizacao();
    }

    static unsigned long ultimoDisplay = 0;

    if (millis() - ultimoDisplay >= 250)
    {
        ultimoDisplay = millis();
        atualizarDisplay();
    }
}
