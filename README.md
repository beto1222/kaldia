# KALDIA

Gestão de pedras e boxes para centrais de abastecimento (CEASA). Produtores reservam
o espaço pelo celular, pagam por PIX ou cartão e fazem check-in com QR Code. A
fiscalização acompanha o pátio em tempo real, registra ocorrências e convida
produtores sem reserva.

Esta é a versão 2, um redesenho completo. A versão original está em [`v1/`](v1/index.html).

## Como rodar

É um site estático, sem etapa de build. Basta servir a pasta:

```bash
python3 -m http.server 8000
# abra http://localhost:8000
```

Também funciona publicado no GitHub Pages (Settings › Pages › branch principal, pasta raiz).

## O que tem

**Produtor**
- Entrada por CPF com máscara e validação dos dígitos verificadores
- Cadastro com WhatsApp, veículo, placa (antiga ou Mercosul) e pessoas autorizadas
- Verificação animada do cadastro
- Mapa do pátio por data (hoje + 6 dias ou qualquer data futura), até 3 pedras por dia
- Pagamento por PIX (QR Code e "copia e cola" no padrão BR Code, com CRC16) ou cartão
- Bilhete da reserva com código `KLD-XXXXX` e QR Code para a portaria
- Minhas reservas: check-in no dia e cancelamento

**Fiscal**
- Painel do dia com lotação, chegadas pendentes, ocorrências abertas e arrecadação
- Planta do pátio clicável (pavilhões, ruas de circulação e portaria)
- Detalhes de cada pedra: confirmar chegada, registrar e tratar ocorrência, liberar
- Locação no balcão para quem chega sem reserva
- Relatórios de 30 dias: lotação diária, pico por dia da semana, ocupação por setor,
  eficiência no tratamento de ocorrências e produtos mais reservados
- Base de produtores com busca, filtros, convite individual e campanha de safra

**Público**
- Consulta de reserva por CPF, placa ou código

## Dados de demonstração

Não há backend: tudo fica salvo no `localStorage` do navegador. As reservas de exemplo
são renovadas a cada dia, preservando o que você criou. O link "Restaurar dados de
exemplo", no rodapé, volta tudo ao estado inicial.

Para testar como produtor já aprovado, use o CPF **529.982.247-25**. Qualquer outro
CPF válido abre o cadastro.

## Estrutura

```
index.html                 casca da página
assets/css/app.css         estilos (tokens de cor, tema claro/escuro, responsivo)
assets/js/util.js          formatação, validação, datas, ícones e PIX
assets/js/store.js         pátio, produtores, reservas, histórico e persistência
assets/js/ui.js            modal, painel lateral, avisos, máscaras, QR Code, bilhete
assets/js/patio.js         desenho da planta do pátio
assets/js/views-*.js       telas (público, produtor, fiscal)
assets/js/app.js           navegação por #rota, cabeçalho e tema
assets/vendor/qrcode.js    qrcode-generator 1.4.4 (MIT, Kazuhiko Arase)
v1/                        versão anterior
```

## Próximos passos sugeridos

- Backend com banco de dados e login real (Supabase ou Firebase)
- Integração com a API do WhatsApp
- Pagamento real (provedor de PIX/cartão) quando sair da fase de demonstração; por enquanto o pagamento é simulado
- PWA para instalar no celular
