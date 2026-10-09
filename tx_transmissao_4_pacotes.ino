// Código utilizado para fazer o teste de transmissão via LoRa conforme o experimento solicitado 
// transmitir 4 pacote por segundos com 255 bytes cada um, durante 10 minutos, oq da 2400 pacotes, é necessario fazer teste 3 vezes
// Mostrar no display para ficar bem visualmente o RSII, número de pacotes transmitidos, numero de pacotes recebidos, colocar uma taxa de recebimento e de perda de pacotes
#include <Arduino.h>
#include "LoRaWan_APP.h"
#include "LoRaConfig.h"

#define TOTAL_PACOTES   2400
#define TAMANHO_PACOTE 255

// Intervalo entre pacotes
#define DELAY_ENTRE_PACOTES 150

// Valor usado para identificar o pacote de finalização
#define BYTE_FINALIZACAO 0xFF

static RadioEvents_t RadioEvents;

// ============================================================
// BUFFERS
// ============================================================

// Buffer dos pacotes normais
uint8_t bufferTx[TAMANHO_PACOTE];

// Buffer do pacote de finalização
uint8_t bufferFinalizacao[TAMANHO_PACOTE];


// ============================================================
// VARIÁVEIS
// ============================================================

uint32_t pacotesEnviados = 0;

bool lora_idle = true;
bool enviandoFinalizacao = false;
bool transmissaoFinalizada = false;

unsigned long tempoUltimoEnvio = 0;


// ============================================================
// CALLBACK: transmissão concluída
// ============================================================

void OnTxDone(void)
{
    lora_idle = true;

    // --------------------------------------------------------
    // Se era o pacote de finalização
    // --------------------------------------------------------

    if (enviandoFinalizacao)
    {
        Serial.println();
        Serial.println("Pacote de finalizacao enviado.");

        transmissaoFinalizada = true;

        Serial.println();
        Serial.println("=================================");
        Serial.println("     TRANSMISSAO FINALIZADA");
        Serial.println("=================================");

        Serial.print("Total enviado: ");
        Serial.println(pacotesEnviados);

        return;
    }


    // --------------------------------------------------------
    // Pacote normal
    // --------------------------------------------------------

    pacotesEnviados++;

    Serial.print("Pacotes enviados: ");
    Serial.print(pacotesEnviados);
    Serial.print("/");
    Serial.println(TOTAL_PACOTES);

    tempoUltimoEnvio = millis();
}


// ============================================================
// CALLBACK: timeout de transmissão
// ============================================================

void OnTxTimeout(void)
{
    Serial.println("Timeout na transmissao.");

    lora_idle = true;

    tempoUltimoEnvio = millis();
}


// ============================================================
// ENVIA PACOTE DE FINALIZAÇÃO
// ============================================================

void enviarFinalizacao()
{
    Serial.println();
    Serial.println("=================================");
    Serial.println("   ENVIANDO FINALIZACAO");
    Serial.println("=================================");
    Serial.println();

    enviandoFinalizacao = true;
    lora_idle = false;

    Radio.Send(
        bufferFinalizacao,
        TAMANHO_PACOTE
    );
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
    Serial.println();


    // --------------------------------------------------------
    // Inicializa placa Heltec
    // --------------------------------------------------------

    Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);


    // --------------------------------------------------------
    // Cria o pacote normal UMA ÚNICA VEZ
    // --------------------------------------------------------
    //
    // 255 bytes:
    //
    // 00 01 02 03 ... FD FE
    //
    // O valor FF não faz parte do pacote normal.
    //

    for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
    {
        bufferTx[i] = (uint8_t)i;
    }


    // --------------------------------------------------------
    // Cria o pacote de finalização UMA ÚNICA VEZ
    // --------------------------------------------------------

    for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
    {
        bufferFinalizacao[i] = BYTE_FINALIZACAO;
    }


    // --------------------------------------------------------
    // Configura callbacks
    // --------------------------------------------------------

    RadioEvents.TxDone = OnTxDone;
    RadioEvents.TxTimeout = OnTxTimeout;


    // --------------------------------------------------------
    // Inicializa rádio
    // --------------------------------------------------------

    Radio.Init(&RadioEvents);

    Radio.SetChannel(RF_FREQUENCY);


    // --------------------------------------------------------
    // Configuração de transmissão
    // --------------------------------------------------------

    Radio.SetTxConfig(
        MODEM_LORA,
        TX_OUTPUT_POWER,
        0,
        LORA_BANDWIDTH,
        LORA_SPREADING_FACTOR,
        LORA_CODINGRATE,
        LORA_PREAMBLE_LENGTH,
        LORA_FIX_LENGTH_PAYLOAD_ON,
        true,
        0,
        0,
        LORA_IQ_INVERSION_ON,
        3000
    );


    // --------------------------------------------------------
    // Informações
    // --------------------------------------------------------

    Serial.println("Radio inicializado.");
    Serial.println();

    Serial.print("Tamanho do pacote: ");
    Serial.println(TAMANHO_PACOTE);

    Serial.print("Total de pacotes: ");
    Serial.println(TOTAL_PACOTES);

    Serial.println();

    Serial.println("Buffer normal criado: 00 ate FE.");
    Serial.println("Buffer de finalizacao criado: FF.");
    Serial.println();

    Serial.println("Iniciando transmissao...");
    Serial.println();


    lora_idle = true;
    tempoUltimoEnvio = millis();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // Processa eventos do rádio
    // --------------------------------------------------------

    Radio.IrqProcess();


    // --------------------------------------------------------
    // Se a transmissão já terminou, não faz mais nada
    // --------------------------------------------------------

    if (transmissaoFinalizada)
    {
        delay(1000);
        return;
    }


    // --------------------------------------------------------
    // Envia os 2400 pacotes normais
    // --------------------------------------------------------

    if (pacotesEnviados < TOTAL_PACOTES)
    {
        if (lora_idle &&
            (millis() - tempoUltimoEnvio >= DELAY_ENTRE_PACOTES))
        {
            Serial.print("Enviando pacote ");
            Serial.print(pacotesEnviados + 1);
            Serial.print("/");
            Serial.println(TOTAL_PACOTES);

            lora_idle = false;

            // ------------------------------------------------
            // O buffer NÃO é mais recriado aqui.
            //
            // bufferTx já foi preenchido no setup().
            // ------------------------------------------------

            Radio.Send(
                bufferTx,
                TAMANHO_PACOTE
            );
        }
    }


    // --------------------------------------------------------
    // Depois dos 2400 pacotes, envia finalização
    // --------------------------------------------------------

    else if (!enviandoFinalizacao)
    {
        if (lora_idle &&
            (millis() - tempoUltimoEnvio >= DELAY_ENTRE_PACOTES))
        {
            enviarFinalizacao();
        }
    }
}