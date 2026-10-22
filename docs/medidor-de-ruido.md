# Monitor de Ruído para Condomínios

**Disciplina:** ESMA002 – Inovações para Engenharia (UFABC)
**Status:** ideia inicial, antes das entrevistas com usuários (hipóteses, não evidências)

---

## 1. Resumo

Sistema que mede o nível de ruído em ambientes com regras de silêncio e avisa automaticamente quando o limite é ultrapassado. O foco é **apartamentos**, onde o barulho é restrito a certos horários. Cada unidade recebe um receptor na porta; a portaria acompanha tudo em um painel e sabe **onde** está o barulho sem depender de ligações de vizinhos.

Outros ambientes possíveis: hospitais, bibliotecas, salas de estudo.

## 2. Problema

- Condomínios têm regras de silêncio, mas a fiscalização depende de reclamações.
- Vizinhos precisam ligar várias vezes para a portaria, e o porteiro nem sempre sabe de qual unidade vem o barulho.
- Reclamar gera desgaste entre vizinhos; a origem real do ruído é frequentemente confundida (barulho atravessa paredes, pisos e tetos).

**Pergunta-chave:** o problema é falta de informação sobre a origem do ruído, ou falta de ação dos moradores/condomínio depois que ele é identificado?

## 3. Público-alvo

| Usuário | Necessidade |
|---|---|
| Morador incomodado | Resolver o problema sem conflito direto nem ligações repetidas |
| Morador barulhento | Ser avisado antes de virar reclamação |
| Porteiro / portaria | Saber rapidamente qual unidade está infringindo a regra |
| Síndico / administração | Registro objetivo para aplicar o regimento interno |

## 4. Como funciona

1. **Configuração da regra:** o condomínio define limite de ruído (dB) e horários de silêncio.
2. **Medição:** o receptor na porta mede continuamente o nível sonoro (somente o nível, ver seção 7).
3. **Verificação:** no horário de silêncio, o sistema compara a medição com o limite.
4. **Notificação:**
   - painel da portaria destaca a unidade;
   - aviso opcional ao morador (LED/sinal sonoro leve ou mensagem).
5. **Registro:** cada ocorrência guarda unidade, horário, duração e nível máximo.

## 5. Arquitetura proposta

```
[Receptor na porta]  --Wi-Fi-->  [Computador/servidor]  -->  [Painel da portaria]
  ESP32 + INMP441                 regras, horários,            lista por unidade,
                                  histórico, alertas           alertas, histórico
```

**Receptor (por unidade)**
- Microcontrolador **ESP32** (Wi-Fi integrado, interface I2S, programável pela Arduino IDE)
- Microfone MEMS digital **INMP441** (saída I2S)
- Alimentação via tomada/fonte USB
- LED indicador de status

### 5.1 Escolha do hardware: ESP32 + INMP441

Boa combinação para o projeto: o INMP441 entrega áudio digital direto ao ESP32 (sem amplificador analógico), e o ESP32 já tem Wi-Fi.

| Ponto | Avaliação |
|---|---|
| Sensibilidade/ruído | Piso de ruído em torno de 33 dBA e faixa até ~120 dB SPL: adequado para limites típicos de silêncio (40–55 dB) e para ruídos altos |
| Processamento local | O ESP32 calcula o nível (RMS em janelas de ~125 ms) e envia só o valor em dB; o áudio nunca sai do chip (ver seção 7) |
| Custo | Baixo; permite vários receptores dentro do limite de R$ 250,00 |
| Comunicação | Wi-Fi + MQTT ou HTTP para o computador |

**Cuidados**
- **Calibração:** o INMP441 mede em dB relativo à escala digital (dBFS). É preciso calibrar um offset para dB SPL, comparando com um decibelímetro ou app de celular (já previsto no Kanban). Cada unidade terá o seu offset.
- **Ponderação A:** para aproximar o dBA, aplicar um filtro IIR por software.
- **Resultado não certificado:** serve para alerta e registro interno, não como prova pericial.
- **Posição do microfone:** na porta, o sensor também capta ruído do corredor; testar o posicionamento e a orientação do furo do microfone.
- **Disponibilidade:** o INMP441 está em fim de linha em alguns fornecedores; alternativas compatíveis com I2S são o SPH0645, o ICS-43434 e o MSM261S4030H0.
- **Sincronização:** para comparar o instante dos picos entre receptores (seção 6), sincronizar relógios via NTP; comparar apenas níveis dispensa isso.

**Computador/servidor**
- Recebe as leituras, aplica regras por horário, dispara alertas e guarda histórico

**Painel da portaria**
- Visão simples: unidades em verde/amarelo/vermelho, com horário e nível

## 6. Identificação da origem do ruído (múltiplos receptores)

Problema: o receptor de um apartamento pode captar o barulho do vizinho, gerando falso alarme contra o morador errado.

Ideia: **comparar as leituras de receptores próximos ao mesmo tempo.**

- Se o barulho vem da unidade A, o microfone de A registra o nível **mais alto**; os vizinhos registram o mesmo evento **atenuado**.
- O sistema só atribui a infração à unidade com o maior nível, e marca as demais como "ruído recebido".
- Possível refinamento: comparar o instante de início do pico em cada receptor.

**Limitações a testar:** barulho vindo de fora do prédio, ruído de áreas comuns, paredes finas que igualam os níveis, e microfones com sensibilidades diferentes (exigem calibração).

## 7. Privacidade e aspectos legais

Ponto crítico, pois há microfone dentro de residências.

- O dispositivo deve **medir apenas o nível sonoro (dB)**; **não gravar nem transmitir áudio**. Esse processamento deve ocorrer no próprio receptor.
- Dados enviados: unidade, horário e nível. Nada de conteúdo de conversas.
- Comunicar de forma clara aos moradores o que é medido e por quê.
- Prever adesão voluntária ou aprovação em assembleia do condomínio.
- Avaliar adequação à LGPD (dados ligados a uma unidade/pessoa e seu horário de rotina).

## 8. Hipóteses a validar

| # | Hipótese | Como testar |
|---|---|---|
| H1 | Barulho é um problema recorrente e a reclamação por telefone é ineficiente | Entrevistar moradores, porteiros e síndicos |
| H2 | Moradores aceitariam um microfone na porta | Entrevistas sobre privacidade |
| H3 | Saber a unidade de origem reduz o conflito | Perguntar sobre experiências anteriores |
| H4 | Comparar níveis entre receptores identifica a origem | Teste de bancada com 2–3 receptores |
| H5 | Porteiros conseguem usar um painel durante o trabalho | Teste de usabilidade com protótipo |

## 9. Protótipo mínimo (MVP)

- 2 a 3 receptores (ESP32 + INMP441) ligados a um computador via Wi-Fi
- Calibração de cada receptor contra um decibelímetro ou app de referência
- Painel simples (página web ou tela de terminal) com nível por unidade
- Regra configurável de limite e horário
- Teste de bancada simulando "apartamentos" em cômodos ou mesas diferentes, para validar a atribuição de origem (H4)

Restrição do curso: gasto máximo de **R$ 250,00 por grupo** (documentado).

## 10. Riscos

- Falsos positivos e falsos negativos na detecção
- Diferença de sensibilidade entre microfones e erro de calibração (dB SPL)
- Rejeição por privacidade
- Custo por unidade em escala real
- Dependência de rede Wi-Fi dentro do condomínio
- Conflitos de uso: o sistema pode ser percebido como vigilância

## 11. Próximos passos (alinhados ao Design Thinking)

1. **Empatizar:** entrevistar ≥ 3 pessoas (morador, porteiro, síndico), sem apresentar a solução; perguntar sobre a última vez que o barulho foi um problema.
2. **Definir:** escrever o PDV: *[usuário] precisa de [necessidade] porque [insight]*.
3. **Idealizar:** considerar ≥ 3 alternativas, por exemplo: (a) o sistema atual com portaria; (b) aviso apenas ao morador, sem monitoramento central; (c) medição só em áreas comuns ou corredores.
4. **Decidir:** manter, modificar ou substituir a proposta com base nas evidências.
5. **Prototipar:** montar o MVP e testar a atribuição de origem do ruído.

**Evidência que faria o grupo mudar de ideia:** moradores recusarem o dispositivo por privacidade, ou a portaria já conseguir identificar a origem sem esforço.