# Transmissor LoRa — Experimento de Comunicação sem Fio

## 1. Sobre o projeto

Este projeto implementa um transmissor LoRa utilizando uma placa **Heltec WiFi LoRa 32 V3.2**, com o objetivo de realizar testes de comunicação sem fio e avaliar a recepção de pacotes por um dispositivo receptor.

O firmware transmite uma sequência de **2.400 pacotes de dados**, cada um com 255 bytes, e envia um pacote especial de finalização após concluir as transmissões normais.

Os pacotes contêm uma sequência de bytes conhecida, permitindo que o receptor verifique a integridade dos dados recebidos e identifique possíveis perdas ou alterações durante a comunicação.

O código utiliza a biblioteca oficial **Heltec ESP32 Dev-Boards** para configurar e controlar o rádio LoRa.

## 2. Objetivos

* Transmitir pacotes LoRa de tamanho fixo.
* Realizar uma sequência de 2.400 transmissões de dados.
* Utilizar uma carga útil conhecida para facilitar a verificação da integridade dos pacotes.
* Controlar o intervalo entre as transmissões.
* Informar pela porta serial o progresso do experimento.
* Enviar um pacote de finalização para indicar ao receptor que a sequência de transmissões foi concluída.
* Permitir a análise da comunicação em conjunto com o firmware do receptor (RX).

## 3. Hardware utilizado

* Placa transmissora Heltec WiFi LoRa 32 V3.2.
* Microcontrolador ESP32-S3.
* Antena compatível com a frequência de operação.
* Computador para programação e monitoramento pela porta serial.

**Atenção:** a antena deve ser conectada adequadamente antes de realizar transmissões por radiofrequência.

## 4. Configuração da transmissão

Os parâmetros principais definidos no firmware são:

| Parâmetro                                       | Valor                      |
| ----------------------------------------------- | -------------------------- |
| Total de pacotes normais                        | 2.400                      |
| Tamanho de cada pacote                          | 255 bytes                  |
| Intervalo mínimo configurado entre transmissões | 150 ms                     |
| Potência de transmissão                         | Definida em `LoRaConfig.h` |
| Frequência de operação                          | Definida em `LoRaConfig.h` |
| Largura de banda                                | Definida em `LoRaConfig.h` |
| Fator de espalhamento (SF)                      | Definido em `LoRaConfig.h` |
| Taxa de codificação                             | Definida em `LoRaConfig.h` |

Os parâmetros de radiofrequência são centralizados no arquivo `LoRaConfig.h`, facilitando a configuração do transmissor e do receptor para utilizar os mesmos parâmetros.

**Importante:** o intervalo de 150 ms é aplicado após a conclusão de cada transmissão normal. Portanto, o intervalo real entre o início de dois pacotes depende também do tempo de transmissão de cada pacote, que varia conforme a configuração LoRa.

## 5. Estrutura dos dados transmitidos

### 5.1. Pacotes normais

O transmissor utiliza um buffer de 255 bytes, preenchido uma única vez durante a execução da função `setup()`.

Cada posição do buffer recebe o valor correspondente ao seu índice:

```cpp
for (uint16_t i = 0; i < TAMANHO_PACOTE; i++)
{
    bufferTx[i] = (uint8_t)i;
}
```

Dessa forma, o conteúdo do pacote segue esta sequência:

```text
00 01 02 03 04 05 06 07 ... FC FD FE
```

Os valores são representados em hexadecimal. O pacote possui 255 bytes, numerados de `0x00` a `0xFE`.

Essa sequência conhecida permite ao receptor comparar os bytes recebidos com os valores esperados e verificar se o pacote chegou completo e sem alterações.

### 5.2. Reutilização do buffer

O buffer normal é criado e preenchido somente uma vez no `setup()`.

Durante a execução do `loop()`, o mesmo buffer é reutilizado nas transmissões seguintes, sem precisar preencher novamente os 255 bytes.

Essa abordagem reduz o processamento desnecessário e mantém o código de transmissão mais simples.

### 5.3. Pacote de finalização

Após concluir os 2.400 pacotes normais, o transmissor envia um pacote especial de 255 bytes, preenchido integralmente com `0xFF`.

```text
FF FF FF FF FF FF FF FF ... FF
```

O buffer de finalização é preparado separadamente durante o `setup()`, evitando modificar o conteúdo do buffer normal.

O receptor pode utilizar esse padrão para reconhecer o término da sequência de transmissões.

**Observação:** o pacote de finalização é adicional aos 2.400 pacotes normais. Ele não deve ser contabilizado como um pacote normal de dados no resultado do experimento.

## 6. Funcionamento do firmware

O programa está organizado nas seguintes etapas:

### 6.1. Inicialização — `setup()`

Na inicialização, o firmware:

1. Inicializa a comunicação serial a 115200 baud.
2. Inicializa a placa Heltec.
3. Preenche o buffer dos pacotes normais.
4. Preenche o buffer do pacote de finalização.
5. Configura os callbacks de transmissão.
6. Inicializa o rádio LoRa.
7. Define a frequência e os parâmetros de transmissão.
8. Exibe informações sobre a configuração do experimento.

Após essa etapa, os dois buffers já estão prontos para serem utilizados.

### 6.2. Processamento dos eventos do rádio

A função `Radio.IrqProcess()` é executada no `loop()` para processar os eventos e as interrupções do rádio gerenciados pela biblioteca.

Os callbacks permitem que o firmware reaja ao término ou ao timeout de uma transmissão.

### 6.3. Transmissão dos pacotes — `loop()`

O `loop()` verifica se:

* A transmissão anterior foi concluída.
* O intervalo mínimo configurado já foi respeitado.
* Ainda existem pacotes normais a transmitir.
* A transmissão não foi finalizada.

Quando as condições são atendidas, o firmware envia o buffer normal por meio de `Radio.Send()`.

O processo continua até que os 2.400 pacotes normais tenham sido enviados.

### 6.4. Callback `OnTxDone()`

Essa função é chamada quando o rádio informa que uma transmissão foi concluída.

Para os pacotes normais, o callback:

* Libera o estado de ocupação do rádio.
* Incrementa o contador de pacotes enviados.
* Exibe o progresso na porta serial.
* Atualiza a referência de tempo utilizada para controlar o intervalo entre transmissões.

Quando a transmissão concluída é o pacote de finalização, o firmware informa que o experimento terminou.

### 6.5. Callback `OnTxTimeout()`

Esse callback trata o timeout de transmissão informado pelo rádio.

Ele registra uma mensagem na porta serial, libera o estado de ocupação do rádio e atualiza a referência temporal.

**Nota:** o código atual não possui uma política específica de repetição automática para pacotes que apresentem timeout. Por isso, o contador de pacotes concluídos não deve ser interpretado, isoladamente, como uma garantia de que todos os pacotes foram recebidos pelo receptor.

### 6.6. Envio da finalização

Quando o contador de pacotes normais alcança 2.400, o programa aguarda o intervalo configurado e chama `enviarFinalizacao()`.

A função transmite o buffer especial de `0xFF`. Após a conclusão dessa transmissão, o firmware marca a execução como finalizada e deixa de iniciar novos envios.

## 7. Monitoramento pela porta serial

O firmware apresenta informações de acompanhamento durante a execução, incluindo:

* Inicialização do rádio.
* Tamanho dos pacotes.
* Quantidade total de pacotes normais.
* Número do pacote que está sendo enviado.
* Quantidade de pacotes normais com transmissão concluída.
* Mensagens de timeout.
* Confirmação do envio do pacote de finalização.
* Mensagem de encerramento da transmissão.

A comunicação serial deve ser monitorada com a velocidade de **115200 baud**.

## 8. Tecnologias e bibliotecas

* **Arduino IDE:** ambiente de desenvolvimento e compilação.
* **ESP32-S3:** microcontrolador da placa.
* **SX1262:** transceptor de radiofrequência utilizado na comunicação LoRa.
* **Heltec ESP32 Dev-Boards:** biblioteca utilizada para inicializar e controlar o rádio.
* **LoRaConfig.h:** arquivo com os parâmetros compartilhados de configuração LoRa.

## 9. Como executar

1. Instale a Arduino IDE e o suporte à placa Heltec ESP32.
2. Instale a biblioteca Heltec ESP32 Dev-Boards.
3. Configure o arquivo `LoRaConfig.h` com os parâmetros desejados.
4. Abra o projeto TX no ambiente de desenvolvimento.
5. Selecione a placa Heltec WiFi LoRa 32 V3.2 e a porta serial correspondente.
6. Compile e grave o firmware na placa transmissora.
7. Configure a placa receptora com parâmetros de rádio compatíveis.
8. Abra o monitor serial em 115200 baud.
9. Execute o experimento e acompanhe o progresso das transmissões.

Antes de iniciar o teste, confirme que transmissor e receptor utilizam configurações de frequência, largura de banda, fator de espalhamento, taxa de codificação e demais parâmetros de rádio compatíveis.

## 10. Relação com o receptor (RX)

Este firmware foi desenvolvido para funcionar em conjunto com um receptor que identifica os pacotes normais, verifica o conteúdo recebido e reconhece o pacote de finalização.

O receptor pode registrar informações como:

* Número de pacotes recebidos.
* Tamanho dos dados recebidos.
* Integridade dos bytes.
* RSSI (Received Signal Strength Indicator).
* SNR (Signal-to-Noise Ratio).
* Tempo de recebimento.
* Percentual de perda de pacotes.

Essas informações permitem avaliar o desempenho da comunicação em diferentes condições de distância e propagação.

O transmissor, por sua vez, registra o andamento das transmissões, mas não consegue determinar sozinho quantos pacotes chegaram corretamente ao receptor.

## 11. Limitações e considerações experimentais

* O envio de um pacote pelo transmissor não garante que ele tenha sido recebido.
* A quantidade de pacotes recebidos deve ser obtida no receptor.
* O intervalo configurado não corresponde necessariamente ao intervalo exato entre os inícios de transmissão.
* O tempo total do experimento depende dos parâmetros de rádio, do tempo no ar de cada pacote e do intervalo entre transmissões.
* O pacote de finalização deve ser reconhecido separadamente dos pacotes normais.
* A identificação de um pacote como íntegro depende da validação implementada no receptor.
* O alcance da comunicação varia conforme antenas, obstáculos, altura de instalação, interferências e condições ambientais.

## 12. Resultados esperados

Ao final da execução, espera-se que o transmissor:

1. Conclua a sequência de 2.400 transmissões normais, conforme os callbacks de conclusão do rádio.
2. Envie um pacote adicional de finalização.
3. Registre na porta serial o progresso e o encerramento.
4. Permita que o receptor compare os dados esperados com os dados efetivamente recebidos.
5. Forneça a base para analisar perdas e qualidade do enlace LoRa.

Os resultados efetivos de recepção, integridade e qualidade do enlace devem ser obtidos a partir dos registros do receptor.
