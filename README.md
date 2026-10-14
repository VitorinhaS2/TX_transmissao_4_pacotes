# Transmissor LoRa — Experimento de Comunicação sem Fio

## 1. Sobre o projeto

Este projeto implementa um transmissor LoRa utilizando a placa **Heltec WiFi LoRa 32 V3.2**, equipada com o microcontrolador ESP32-S3 e o transceptor SX1262.

O firmware foi desenvolvido para realizar experimentos de comunicação sem fio, permitindo avaliar a transmissão de pacotes e analisar o desempenho do enlace em conjunto com um dispositivo receptor (RX).

A transmissão é composta por uma sequência de **2.400 pacotes de dados, cada um com 255 bytes**, seguida pelo envio de um pacote especial de finalização.

Antes de iniciar o envio dos dados, o transmissor verifica se o receptor está respondendo e solicita a confirmação de início de uma nova sessão. Esse procedimento utiliza mensagens de controle `PING`, `PONG`, `START` e `ACK`.

O firmware também utiliza um identificador de sessão para associar o início da transmissão à resposta correspondente do receptor, além de disponibilizar informações de progresso pela porta serial e pelo display OLED.

## 2. Objetivos

- Implementar a comunicação LoRa entre transmissor e receptor.
- Verificar se o receptor está ativo antes de iniciar o experimento.
- Estabelecer uma sessão de transmissão por meio de mensagens de controle.
- Transmitir 2.400 pacotes normais de 255 bytes.
- Utilizar uma sequência de bytes conhecida para facilitar a verificação da integridade dos dados.
- Controlar o envio dos pacotes por meio de estados e eventos do rádio.
- Exibir o progresso da transmissão pela porta serial e pelo display OLED.
- Enviar um pacote especial para indicar o término da sequência de dados.
- Permitir a análise de perdas e da qualidade do enlace em conjunto com o firmware RX.

## 3. Hardware utilizado

- Placa transmissora Heltec WiFi LoRa 32 V3.2.
- Microcontrolador ESP32-S3.
- Transceptor LoRa SX1262 integrado à placa.
- Antena compatível com a frequência de operação.
- Computador para programação e monitoramento pela porta serial.

**Atenção:** conecte a antena apropriada antes de realizar transmissões por radiofrequência. Verifique também se a antena é compatível com a frequência configurada no experimento.

## 4. Configuração da transmissão

Os principais parâmetros do experimento estão definidos no código do transmissor e no arquivo `LoRaConfig.h`.

| Parâmetro | Valor ou configuração |
|---|---|
| Total de pacotes normais | 2.400 |
| Tamanho dos pacotes normais | 255 bytes |
| Intervalo mínimo configurado entre transmissões | 0 ms |
| Intervalo entre tentativas de PING | 5.000 ms |
| Timeout de espera pelo PONG | 2.000 ms |
| Timeout de espera pelo ACK de início | 2.000 ms |
| Potência de transmissão | Definida em `LoRaConfig.h` |
| Frequência de operação | Definida em `LoRaConfig.h` |
| Largura de banda | Definida em `LoRaConfig.h` |
| Fator de espalhamento (SF) | Definido em `LoRaConfig.h` |
| Taxa de codificação | Definida em `LoRaConfig.h` |
| Tamanho do pacote de finalização | 255 bytes |

O arquivo `LoRaConfig.h` centraliza os parâmetros de radiofrequência utilizados pelo projeto, facilitando a configuração do transmissor e do receptor.

O intervalo `DELAY_ENTRE_PACOTES` está configurado em `0 ms`. Isso significa que o firmware não adiciona uma espera intencional entre o término de uma transmissão normal e o início da próxima. Ainda assim, o tempo no ar de cada pacote e o processamento dos eventos do rádio determinam a duração real do experimento.

Os timeouts de PONG e ACK são mecanismos de controle do protocolo. Eles não representam o tempo de transmissão dos pacotes normais.

## 5. Estrutura dos dados transmitidos

### 5.1. Pacotes normais

O transmissor utiliza um buffer de 255 bytes, preparado uma única vez durante a execução de `setup()`.

Cada posição do buffer recebe o valor correspondente ao seu índice:

```cpp
for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
{
    bufferTx[i] = (uint8_t)i;
}
```

Dessa forma, o conteúdo de cada pacote segue a sequência:

```text
00 01 02 03 04 05 06 07 ... FC FD FE
```

Os valores estão representados em hexadecimal. Como cada byte pode representar valores entre `0x00` e `0xFF`, a sequência contém 255 bytes, de `0x00` a `0xFE`.

Todos os pacotes normais utilizam o mesmo conteúdo. Isso permite que o receptor compare os dados recebidos com a sequência esperada e identifique pacotes incompletos ou alterações nos bytes.

### 5.2. Reutilização do buffer

O buffer normal é preenchido apenas uma vez, durante `setup()`, e reutilizado nas transmissões seguintes.

Essa abordagem evita reconstruir o conteúdo dos pacotes a cada envio e mantém a rotina de transmissão simples.

### 5.3. Pacote de finalização

Após a conclusão dos 2.400 pacotes normais, o transmissor envia um pacote adicional de 255 bytes, preenchido integralmente com `0xFF`.

```text
FF FF FF FF FF FF FF FF ... FF
```

Esse buffer é preparado separadamente do buffer normal. Assim, o conteúdo dos pacotes de dados permanece inalterado.

O receptor pode reconhecer esse padrão como indicador de encerramento do experimento.

O pacote de finalização é adicional: ele não faz parte dos 2.400 pacotes normais e deve ser contabilizado separadamente na análise dos resultados.

### 5.4. Mensagens de controle

Antes da transmissão dos dados, o firmware utiliza mensagens específicas para verificar a disponibilidade do receptor e estabelecer uma sessão.

| Mensagem | Função |
|---|---|
| `PING` | Verifica se o receptor está respondendo. |
| `PONG` | Resposta do receptor ao PING. |
| `START` | Solicita o início de uma nova sessão de transmissão. |
| `ACKS` | Confirma o início da sessão e fornece o número do experimento. |

O `PING` e o `PONG` possuem 5 bytes. A mensagem `START` possui 8 bytes e contém o identificador da sessão. A confirmação `ACKS` possui 12 bytes e inclui o identificador da sessão e o número do experimento.

O transmissor gera um valor de identificação chamado `nonceTX`, utilizado para associar a confirmação de início à solicitação enviada. A confirmação só é aceita se o identificador recebido corresponder ao identificador da sessão atual.

## 6. Funcionamento do firmware

O firmware está organizado em etapas que controlam a inicialização, a comunicação com o receptor, a transmissão dos dados e o encerramento do experimento.

### 6.1. Inicialização — `setup()`

Durante a inicialização, o firmware:

- Inicia a comunicação serial a 115200 baud.
- Inicializa a placa Heltec.
- Inicializa o display OLED.
- Prepara o buffer dos pacotes normais.
- Prepara o buffer do pacote de finalização.
- Gera o identificador da sessão.
- Configura os callbacks do rádio.
- Inicializa o transceptor LoRa.
- Configura os parâmetros de transmissão e recepção.
- Exibe informações iniciais do experimento.

Após essa etapa, os buffers estão preparados e o transmissor começa a procurar o receptor.

### 6.2. Processamento dos eventos do rádio

A função `Radio.IrqProcess()` é executada no `loop()` para processar os eventos do rádio gerenciados pela biblioteca Heltec.

Os callbacks permitem que o programa reaja à conclusão de uma transmissão, à recepção de uma mensagem e à ocorrência de timeouts ou erros de recepção.

O controle do fluxo é realizado por uma máquina de estados, representada pela enumeração `EstadoTX`.

### 6.3. Verificação do receptor — PING/PONG

Inicialmente, o transmissor envia uma mensagem `PING` e aguarda uma resposta `PONG`.

Se a resposta válida for recebida, o firmware prossegue para a solicitação de início da sessão.

Caso o PONG não seja recebido dentro do timeout configurado, o transmissor retorna ao estado de busca e realiza uma nova tentativa após o intervalo definido.

Esse procedimento permite verificar a disponibilidade do receptor antes do início do envio dos dados.

### 6.4. Solicitação de início — START/ACK

Após receber o PONG, o transmissor envia uma mensagem `START` contendo o identificador da sessão.

Em seguida, aguarda a confirmação `ACKS` do receptor. Para aceitar a resposta, o firmware verifica o formato da mensagem e se o identificador recebido corresponde ao `nonceTX` da sessão atual.

A confirmação também fornece o número do experimento, utilizado para identificar a sequência de transmissão.

Se o ACK não for recebido dentro do tempo configurado, o transmissor repete a solicitação de início.

A transmissão dos pacotes normais só começa após o recebimento de uma confirmação válida.

### 6.5. Transmissão dos dados — `loop()`

Depois da confirmação de início, o transmissor envia o buffer normal por meio de `Radio.Send()`.

O programa verifica se:

- O rádio está disponível para um novo envio.
- O estado atual permite transmitir dados.
- Ainda existem pacotes normais a enviar.
- As condições temporais para o próximo envio foram atendidas.

O contador `pacotesEnviados` é incrementado no callback `OnTxDone()`, após o rádio informar a conclusão de uma transmissão normal.

O processo continua até que o contador alcance 2.400 pacotes concluídos pelo rádio.

### 6.6. Callback `OnTxDone()`

Esse callback é executado quando o rádio informa que uma transmissão foi concluída.

O comportamento depende do estado atual do transmissor:

- Após o envio do PING, inicia a espera pelo PONG.
- Após o envio do START, inicia a espera pelo ACK.
- Após um pacote normal, incrementa o contador e atualiza o tempo de referência.
- Após o pacote de finalização, registra o encerramento do experimento.

O progresso da transmissão também é apresentado pela porta serial em intervalos de dez pacotes, além do registro do último pacote normal.

### 6.7. Callback `OnTxTimeout()`

Esse callback trata os timeouts de transmissão informados pelo rádio.

O firmware libera o estado de ocupação do rádio e ajusta o estado da máquina de transmissão de acordo com a etapa em andamento.

No caso do PING, uma nova tentativa é programada. No caso do START, a solicitação pode ser enviada novamente.

**Limitação:** o contador de pacotes normais é incrementado no callback de conclusão, mas o código não implementa uma estratégia completa de recuperação de todos os possíveis timeouts de transmissão. Portanto, o número registrado pelo TX não comprova que os pacotes foram recebidos pelo RX.

### 6.8. Recepção das respostas — `OnRxDone()`

O callback `OnRxDone()` processa as mensagens recebidas durante as etapas de controle.

Quando o transmissor aguarda o PONG, verifica se a mensagem recebida corresponde à resposta esperada.

Quando aguarda o ACK, verifica o formato da confirmação e o identificador da sessão. Após uma confirmação válida, registra o número do experimento, zera o contador de pacotes e inicia a transmissão dos dados.

Mensagens que não correspondem à resposta esperada não autorizam o início do envio normal.

### 6.9. Envio da finalização

Quando os 2.400 pacotes normais são concluídos, o transmissor envia o buffer de finalização preenchido com `0xFF`.

Após a conclusão dessa transmissão, o firmware altera o estado para `TRANSMISSAO_FINALIZADA`, registra as informações finais na porta serial e deixa de iniciar novos envios.

O pacote de finalização é um indicador de encerramento. Seu envio não garante, por si só, que o receptor tenha recebido ou reconhecido a mensagem.

## 7. Monitoramento pela porta serial e pelo display OLED

A comunicação serial deve ser monitorada a **115200 baud**.

Durante a execução, o firmware apresenta informações como:

- Inicialização do rádio.
- Tamanho e quantidade total de pacotes normais.
- Identificador da sessão.
- Tentativas de PING.
- Confirmação de início da sessão.
- Número de pacotes normais concluídos pelo rádio.
- Mensagens relacionadas a timeouts.
- Envio do pacote de finalização.
- Encerramento do experimento.

O display OLED apresenta o estado atual do transmissor, o número do experimento, a quantidade de pacotes concluídos, o progresso percentual e o número de tentativas de PING.

O percentual de progresso é calculado a partir do contador de pacotes normais concluídos em relação ao total de 2.400.

## 8. Tecnologias e bibliotecas

- **Arduino IDE:** ambiente de desenvolvimento e gravação do firmware.
- **ESP32-S3:** microcontrolador integrado à placa.
- **SX1262:** transceptor de radiofrequência utilizado na comunicação LoRa.
- **Heltec ESP32 Dev-Boards:** biblioteca utilizada para inicialização e controle do rádio.
- **`LoRaConfig.h`:** arquivo de configuração dos parâmetros de radiofrequência.
- **`HT_SSD1306Wire.h`:** biblioteca utilizada para controlar o display OLED.
- **`esp_system.h`:** utilizada para gerar o identificador da sessão por meio de `esp_random()`.

## 9. Como executar

1. Instale a Arduino IDE e o suporte à placa Heltec ESP32.
2. Instale a biblioteca Heltec ESP32 Dev-Boards.
3. Disponibilize os arquivos necessários do projeto, incluindo `LoRaConfig.h`.
4. Confira os parâmetros de radiofrequência utilizados pelo transmissor e pelo receptor.
5. Abra o projeto TX na Arduino IDE.
6. Selecione a placa Heltec WiFi LoRa 32 V3.2 e a porta serial correspondente.
7. Compile e grave o firmware na placa transmissora.
8. Configure e ligue o receptor com parâmetros de rádio compatíveis.
9. Abra o monitor serial em 115200 baud.
10. Observe o display OLED e as mensagens da porta serial durante a execução.

O transmissor procura o receptor, aguarda o PONG, solicita o início da sessão e espera pelo ACK válido antes de começar a transmitir os dados.

**Importante:** para que a comunicação funcione, transmissor e receptor precisam utilizar parâmetros de rádio compatíveis, incluindo frequência, largura de banda, fator de espalhamento, taxa de codificação, preâmbulo e configurações relacionadas ao formato do pacote.

## 10. Relação com o receptor (RX)

O transmissor foi desenvolvido para operar em conjunto com um receptor que implemente o protocolo de controle correspondente.

O RX deve ser capaz de responder ao PING, processar a solicitação START, confirmar o início da sessão com ACKS e receber os pacotes normais e o pacote de finalização.

Durante o experimento, o receptor pode registrar informações como:

- Quantidade de pacotes recebidos.
- Tamanho dos dados recebidos.
- Integridade dos bytes.
- RSSI (*Received Signal Strength Indicator*).
- SNR (*Signal-to-Noise Ratio*).
- Tempo de recebimento.
- Número de pacotes perdidos.
- Percentual de perda de pacotes.

Essas informações permitem analisar o comportamento do enlace LoRa em diferentes condições de distância, obstáculos e propagação.

O transmissor registra o progresso dos envios, mas não determina sozinho quantos pacotes chegaram corretamente ao receptor.

## 11. Limitações e considerações experimentais

- A conclusão de uma transmissão pelo rádio não garante a recepção correta do pacote pelo RX.
- A quantidade de pacotes recebidos e as perdas devem ser determinadas a partir dos registros do receptor.
- O intervalo configurado de 0 ms não significa que os pacotes sejam transmitidos sem qualquer intervalo físico entre eles.
- O tempo total depende do tempo no ar de cada pacote, dos parâmetros de rádio e do processamento dos eventos.
- O pacote de finalização deve ser contabilizado separadamente dos 2.400 pacotes normais.
- O início da transmissão depende da resposta válida do receptor ao protocolo de controle.
- A identificação da sessão ajuda a associar o ACK ao início solicitado pelo transmissor, mas não substitui a verificação da integridade dos pacotes de dados.
- O código atual não implementa confirmação individual de recebimento nem retransmissão automática de todos os pacotes normais.
- O alcance e a qualidade do enlace variam conforme antenas, obstáculos, altura de instalação, interferências e condições ambientais.

## 12. Resultados esperados

Ao final de uma execução bem-sucedida, espera-se que o transmissor:

- Identifique um receptor ativo por meio do procedimento PING/PONG.
- Solicite e receba a confirmação de início da sessão.
- Conclua as transmissões de 2.400 pacotes normais, conforme os eventos reportados pelo rádio.
- Envie um pacote adicional de finalização.
- Registre o progresso e o encerramento pela porta serial.
- Atualize o display OLED durante o experimento.
- Forneça os dados necessários para que o receptor avalie a integridade dos pacotes e o desempenho da comunicação.

Os resultados efetivos de recepção, integridade, RSSI, SNR e perda de pacotes devem ser obtidos a partir dos registros do receptor e analisados em conjunto com as informações do transmissor.
