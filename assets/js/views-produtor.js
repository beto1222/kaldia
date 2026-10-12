/* Fluxo do produtor: entrada (cadastro e acesso na mesma tela), reserva, pagamento e minhas reservas */
'use strict';

(function (K) {
  const V = K.views = K.views || {};
  const S = () => K.store;

  const etapas = atual => {
    const nomes = ['Escolha das pedras', 'Pagamento', 'Confirmação'];
    return `<ol class="etapas" aria-label="Etapas da reserva">${nomes.map((n, i) => `
      <li class="${i < atual ? 'feita' : i === atual ? 'atual' : ''}" ${i === atual ? 'aria-current="step"' : ''}>
        <span class="etapa-n">${i < atual ? K.icon('check', 14) : i + 1}</span><span class="etapa-nome">${n}</span>
      </li>`).join('')}</ol>`;
  };

  const exigeLogin = () => (S().logado() ? null : '#entrar');

  const PRODUTOS_RAPIDOS = ['Frutas', 'Verduras', 'Legumes', 'Folhosas', 'Raízes e tubérculos', 'Ovos'];

  // ---------- Entrar: cadastro e acesso na mesma tela ----------
  V.entrar = {
    area: 'publico',
    titulo: 'Entrar',
    render() {
      return `
        <div class="cartao-centro">
          <div class="cartao-centro-cab">
            <span class="perfil-icone perfil-verde">${K.icon('truck', 24)}</span>
            <h1>Área do produtor</h1>
            <p class="sub">Preencha e pronto. Se você já usou o KALDIA, o CPF te leva direto para as reservas.</p>
          </div>
          <form id="form-entrar" novalidate>
            <div class="campo">
              <label for="entrar-cpf">CPF</label>
              <input id="entrar-cpf" type="text" inputmode="numeric" autocomplete="off" data-mascara="cpf" placeholder="000.000.000-00" maxlength="14">
              <p class="campo-nota" id="entrar-retorno" aria-live="polite" hidden></p>
            </div>
            <div class="campo">
              <label for="entrar-nome">Nome do produtor</label>
              <input id="entrar-nome" type="text" autocomplete="name" placeholder="Seu nome ou o nome do sítio">
            </div>
            <div class="campo">
              <label for="entrar-produto">O que você está trazendo</label>
              <input id="entrar-produto" type="text" placeholder="Ex.: frutas, tomate, alface">
              <div class="chips-rapidos" role="group" aria-label="Sugestões de produto">
                ${PRODUTOS_RAPIDOS.map(p => `<button type="button" class="chip-sugestao" data-produto="${p}">${p}</button>`).join('')}
              </div>
            </div>
            <button class="btn btn-primario btn-bloco" type="submit">Entrar${K.icon('right', 18)}</button>
          </form>
          <div class="dica">
            ${K.icon('info', 16)}
            <span>Demonstração: o CPF <button type="button" class="link" data-cpf="52998224725">529.982.247-25</button> já tem cadastro (Antônio). Qualquer outro CPF válido cria a conta na hora.</span>
          </div>
        </div>`;
    },
    mount(el) {
      K.ui.mascaras(el);
      const $ = id => el.querySelector('#' + id);
      const cpf = $('entrar-cpf');
      const nome = $('entrar-nome');
      const produto = $('entrar-produto');
      const retorno = $('entrar-retorno');

      // CPF já cadastrado: avisa e preenche o resto
      const conferirCPF = () => {
        const p = K.cpfValido(cpf.value) ? S().produtorPorCPF(cpf.value) : null;
        retorno.hidden = !p;
        if (!p) return;
        retorno.innerHTML = `${K.icon('check', 14)}Que bom te ver de novo, ${K.esc(K.primeiroNome(p.nome))}!`;
        if (!nome.value.trim()) { nome.value = p.nome; K.ui.erroCampo(nome, ''); }
        if (!produto.value.trim()) { produto.value = p.produto; K.ui.erroCampo(produto, ''); }
      };
      cpf.addEventListener('input', conferirCPF);
      [cpf, nome, produto].forEach(inp => inp.addEventListener('input', () => K.ui.erroCampo(inp, '')));

      el.querySelector('[data-cpf]').addEventListener('click', e => {
        cpf.value = K.fmtCPF(e.currentTarget.dataset.cpf);
        K.ui.erroCampo(cpf, '');
        conferirCPF();
        cpf.focus();
      });
      el.querySelectorAll('[data-produto]').forEach(b => b.addEventListener('click', () => {
        produto.value = b.dataset.produto;
        K.ui.erroCampo(produto, '');
        produto.focus();
      }));

      $('form-entrar').addEventListener('submit', e => {
        e.preventDefault();
        const regras = [
          [cpf, !K.soDigitos(cpf.value) ? 'Digite o seu CPF.'
            : !K.cpfValido(cpf.value) ? 'Esse CPF não é válido. Confira os 11 números.' : ''],
          [nome, nome.value.trim().length >= 3 ? '' : 'Escreva o seu nome.'],
          [produto, produto.value.trim() ? '' : 'Diga o que você está trazendo.']
        ];
        let primeiroErro = null;
        regras.forEach(([inp, msg]) => {
          K.ui.erroCampo(inp, msg);
          if (msg && !primeiroErro) primeiroErro = inp;
        });
        if (primeiroErro) { primeiroErro.focus(); return; }

        let p = S().produtorPorCPF(cpf.value);
        if (p) {
          p.nome = nome.value.trim();
          p.produto = produto.value.trim();
          S().salvar();
          K.ui.toast(`Bem-vindo de volta, ${K.primeiroNome(p.nome)}.`);
        } else {
          p = S().cadastrar({
            cpf: K.soDigitos(cpf.value),
            nome: nome.value.trim(),
            produto: produto.value.trim(),
            telefone: '',
            email: '',
            veiculo: '',
            placa: '',
            pessoas: ''
          });
          K.ui.toast(`Cadastro feito. Bem-vindo, ${K.primeiroNome(p.nome)}!`);
        }
        S().entrar(p.id);
        K.rascunho = null;
        K.ir('#reservar');
      });
    }
  };

  // ---------- Reservar ----------
  const rascunho = () => {
    const p = S().logado();
    if (!K.rascunho || K.rascunho.produtorId !== p.id) {
      K.rascunho = { produtorId: p.id, data: K.hoje(), boxes: [], produto: p.produto, qtd: '', unidade: 'cx' };
    }
    return K.rascunho;
  };
  const totalRascunho = r => r.boxes.reduce((s, b) => s + K.boxInfo(b).preco, 0);

  V.reservar = {
    area: 'produtor',
    titulo: 'Nova reserva',
    guarda: exigeLogin,
    render() {
      const p = S().logado();
      const r = rascunho();
      return `
        ${etapas(0)}
        <header class="cab-pagina cab-linha">
          <div>
            <p class="eyebrow">Olá, ${K.esc(K.primeiroNome(p.nome))}</p>
            <h1>Escolha suas pedras</h1>
          </div>
          <p class="sub">Até ${K.MAX_BOXES} pedras por dia. O valor é por pedra e cobre o pregão inteiro.</p>
        </header>
        <div class="faixa-datas" id="faixa-datas" role="group" aria-label="Data de uso"></div>
        <div class="reserva-grade">
          <section class="cartao cartao-mapa" aria-labelledby="mapa-tit">
            <div class="cartao-cab">
              <h2 id="mapa-tit">Mapa do pátio · <span id="mapa-data">${K.esc(K.rotuloDia(r.data))}</span></h2>
              ${K.patio.legenda('produtor')}
            </div>
            <div id="mapa-produtor"></div>
          </section>
          <aside class="cartao resumo" aria-labelledby="resumo-tit">
            <h2 id="resumo-tit">Sua reserva</h2>
            <div id="resumo-pedras"></div>
            <div class="campo">
              <label for="res-produto">Carga deste dia</label>
              <input id="res-produto" type="text" value="${K.esc(r.produto)}" placeholder="O que vai vender">
            </div>
            <div class="campo">
              <label for="res-qtd">Volume estimado</label>
              <div class="campo-composto">
                <input id="res-qtd" type="number" min="1" inputmode="numeric" value="${K.esc(r.qtd)}" placeholder="80">
                <select id="res-unidade" aria-label="Unidade">
                  ${[['cx', 'caixas'], ['sc', 'sacos'], ['un', 'unidades'], ['mç', 'maços'], ['kg', 'kg']].map(([v, t]) =>
                    `<option value="${v}" ${r.unidade === v ? 'selected' : ''}>${t}</option>`).join('')}
                </select>
              </div>
            </div>
            <div class="resumo-total">
              <span>Total</span>
              <strong id="resumo-total">${K.fmtMoeda(totalRascunho(r))}</strong>
            </div>
            <button class="btn btn-primario btn-bloco" id="btn-pagar">Continuar para pagamento${K.icon('right', 18)}</button>
          </aside>
        </div>`;
    },
    mount(el) {
      const r = rascunho();
      const hoje = K.hoje();

      const desenharDatas = () => {
        const dias = Array.from({ length: 7 }, (_, i) => K.addDias(hoje, i));
        const fora = !dias.includes(r.data);
        el.querySelector('#faixa-datas').innerHTML = dias.map(d => {
          const livres = S().livres(d).length;
          const ativo = d === r.data;
          return `<button type="button" class="dia ${ativo ? 'ativo' : ''}" data-dia="${d}" aria-pressed="${ativo}">
            <span class="dia-sem">${d === hoje ? 'Hoje' : K.diaSemana(d)}</span>
            <span class="dia-num">${K.parseISO(d).getDate()}</span>
            <span class="dia-livres ${livres <= 3 ? 'poucas' : ''}">${livres} livres</span>
          </button>`;
        }).join('') + `
          <label class="dia dia-outra ${fora ? 'ativo' : ''}">
            <span class="dia-sem">Outra data</span>
            ${K.icon('calendar', 18)}
            <input type="date" id="res-data" min="${hoje}" value="${fora ? r.data : ''}" aria-label="Escolher outra data">
          </label>`;
        el.querySelectorAll('[data-dia]').forEach(b => b.addEventListener('click', () => mudarData(b.dataset.dia)));
        el.querySelector('#res-data').addEventListener('change', e => {
          if (e.target.value && e.target.value >= hoje) mudarData(e.target.value);
        });
      };

      const mudarData = d => {
        if (d === r.data) return;
        const perdidas = r.boxes.filter(b => S().reservaDoBox(b, d));
        r.data = d;
        if (perdidas.length) {
          r.boxes = r.boxes.filter(b => !perdidas.includes(b));
          K.ui.toast(`${perdidas.join(', ')} não está livre nesse dia e saiu da seleção.`, 'info');
        }
        el.querySelector('#mapa-data').textContent = K.rotuloDia(d);
        desenharDatas();
        desenharMapa();
        desenharResumo();
      };

      const desenharMapa = () => {
        const alvo = el.querySelector('#mapa-produtor');
        alvo.innerHTML = K.patio.html({ data: r.data, modo: 'produtor', selecionados: r.boxes });
        alvo.querySelectorAll('[data-box]').forEach(b => b.addEventListener('click', () => alternar(b.dataset.box)));
      };

      const alternar = box => {
        const i = r.boxes.indexOf(box);
        if (i >= 0) r.boxes.splice(i, 1);
        else if (r.boxes.length >= K.MAX_BOXES) {
          K.ui.toast(`O limite é de ${K.MAX_BOXES} pedras por dia. Desmarque uma para trocar.`, 'info');
          return;
        } else r.boxes.push(box);
        desenharMapa();
        desenharResumo();
        const foco = el.querySelector(`[data-box="${box}"]`);
        if (foco) foco.focus();
      };

      const desenharResumo = () => {
        el.querySelector('#resumo-pedras').innerHTML = r.boxes.length
          ? `<ul class="lista-escolhidas">${r.boxes.map(b => {
              const s = K.setorInfo(K.boxInfo(b).setor);
              return `<li><span class="chip-pedra">${b}</span><span class="sutil">${s.nome}</span>
                <span class="valor">${K.fmtMoeda(K.boxInfo(b).preco)}</span>
                <button type="button" class="icon-btn icon-btn-sm" data-remover="${b}" aria-label="Remover ${b}">${K.icon('x', 16)}</button></li>`;
            }).join('')}</ul>
            <p class="sutil contador">${r.boxes.length} de ${K.MAX_BOXES} pedras · ${K.esc(K.rotuloDia(r.data))}</p>`
          : `<p class="resumo-vazio">${K.icon('grid', 18)}Toque nas pedras livres do mapa para escolher.</p>`;
        el.querySelectorAll('[data-remover]').forEach(b => b.addEventListener('click', () => alternar(b.dataset.remover)));
        el.querySelector('#resumo-total').textContent = K.fmtMoeda(totalRascunho(r));
        el.querySelector('#btn-pagar').disabled = !r.boxes.length;
      };

      el.querySelector('#res-produto').addEventListener('input', e => { r.produto = e.target.value; });
      el.querySelector('#res-qtd').addEventListener('input', e => { r.qtd = e.target.value; });
      el.querySelector('#res-unidade').addEventListener('change', e => { r.unidade = e.target.value; });
      el.querySelector('#btn-pagar').addEventListener('click', () => {
        const prod = el.querySelector('#res-produto');
        if (!r.produto.trim()) {
          K.ui.erroCampo(prod, 'Diga o que vai vender neste dia.');
          prod.focus();
          return;
        }
        K.ui.erroCampo(prod, '');
        K.ir('#pagamento');
      });

      desenharDatas();
      desenharMapa();
      desenharResumo();
    }
  };

  // ---------- Pagamento ----------
  V.pagamento = {
    area: 'produtor',
    titulo: 'Pagamento',
    guarda() {
      const g = exigeLogin();
      if (g) return g;
      if (!K.rascunho || !K.rascunho.boxes.length) return '#reservar';
      return null;
    },
    render() {
      const r = K.rascunho;
      const total = totalRascunho(r);
      return `
        ${etapas(1)}
        <a class="voltar" href="#reservar">${K.icon('left', 16)}Alterar pedras</a>
        <div class="reserva-grade">
          <section class="cartao" aria-labelledby="pag-tit">
            <h1 id="pag-tit" class="h2">Como você quer pagar?</h1>
            <div class="abas" role="tablist" aria-label="Forma de pagamento">
              <button role="tab" type="button" id="aba-pix" aria-selected="true" aria-controls="painel-pix" data-aba="pix">${K.icon('pix', 18)}PIX</button>
              <button role="tab" type="button" id="aba-credito" aria-selected="false" aria-controls="painel-cartao" data-aba="credito" tabindex="-1">${K.icon('card', 18)}Crédito</button>
              <button role="tab" type="button" id="aba-debito" aria-selected="false" aria-controls="painel-cartao" data-aba="debito" tabindex="-1">${K.icon('wallet', 18)}Débito</button>
            </div>

            <div role="tabpanel" id="painel-pix" aria-labelledby="aba-pix" class="painel-pix">
              <div class="pix-qr"><div class="qr" id="pix-qr"></div></div>
              <div class="pix-info">
                <ol class="pix-passos">
                  <li>Abra o app do seu banco e escolha <strong>PIX › Ler QR Code</strong>.</li>
                  <li>Ou copie o código abaixo e use <strong>PIX Copia e Cola</strong>.</li>
                  <li>Confirme o valor de <strong>${K.fmtMoeda(total)}</strong>.</li>
                </ol>
                <div class="copiavel">
                  <label for="pix-codigo" class="rotulo">PIX copia e cola</label>
                  <div class="copiavel-linha">
                    <input id="pix-codigo" type="text" readonly class="mono">
                    <button type="button" class="btn btn-ghost btn-sm" id="pix-copiar">${K.icon('copy', 16)}Copiar</button>
                  </div>
                </div>
                <p class="pix-expira">${K.icon('clock', 16)}O código expira em <strong id="pix-tempo">15:00</strong></p>
                <button type="button" class="btn btn-primario btn-bloco" id="pix-pago">${K.icon('check', 18)}Já paguei</button>
              </div>
            </div>

            <form role="tabpanel" id="painel-cartao" class="painel-cartao form-grade" hidden novalidate>
              <div class="campo campo-largo">
                <label for="cartao-num">Número do cartão</label>
                <input id="cartao-num" type="text" inputmode="numeric" autocomplete="cc-number" data-mascara="cartao" maxlength="19" placeholder="0000 0000 0000 0000" class="mono">
              </div>
              <div class="campo campo-largo">
                <label for="cartao-nome">Nome impresso</label>
                <input id="cartao-nome" type="text" autocomplete="cc-name" placeholder="Como está no cartão">
              </div>
              <div class="campo">
                <label for="cartao-val">Validade</label>
                <input id="cartao-val" type="text" inputmode="numeric" autocomplete="cc-exp" data-mascara="validade" maxlength="5" placeholder="MM/AA" class="mono">
              </div>
              <div class="campo">
                <label for="cartao-cvv">CVV</label>
                <input id="cartao-cvv" type="text" inputmode="numeric" autocomplete="cc-csc" data-mascara="cvv" maxlength="4" placeholder="123" class="mono">
              </div>
              <div class="form-acoes campo-largo">
                <button type="submit" class="btn btn-primario btn-bloco" id="cartao-pagar">Pagar ${K.fmtMoeda(total)}</button>
              </div>
            </form>
            <p class="aviso-demo">${K.icon('info', 16)}Ambiente de demonstração. Nenhuma cobrança real é feita.</p>
          </section>

          <aside class="cartao resumo" aria-labelledby="pedido-tit">
            <h2 id="pedido-tit">Resumo do pedido</h2>
            <dl class="pedido-linhas">
              <div><dt>Data</dt><dd>${K.esc(K.fmtDataLonga(r.data))}</dd></div>
              <div><dt>Carga</dt><dd>${K.esc(r.produto)}${r.qtd ? ` · ${K.esc(r.qtd)} ${K.esc(r.unidade)}` : ''}</dd></div>
            </dl>
            <ul class="lista-escolhidas">${r.boxes.map(b => `
              <li><span class="chip-pedra">${b}</span><span class="sutil">${K.setorInfo(K.boxInfo(b).setor).nome}</span><span class="valor">${K.fmtMoeda(K.boxInfo(b).preco)}</span></li>`).join('')}
            </ul>
            <div class="resumo-total"><span>Total</span><strong>${K.fmtMoeda(total)}</strong></div>
          </aside>
        </div>`;
    },
    mount(el) {
      const r = K.rascunho;
      const total = totalRascunho(r);
      const txid = 'KLD' + K.uid().toUpperCase();
      const payload = K.pixPayload({ valor: total, txid });
      el.querySelector('#pix-codigo').value = payload;
      K.ui.qr(el.querySelector('#pix-qr'), payload, 184);
      K.ui.mascaras(el);

      let restante = 15 * 60;
      K.every(() => {
        restante = Math.max(0, restante - 1);
        const t = el.querySelector('#pix-tempo');
        if (t) t.textContent = `${String(Math.floor(restante / 60)).padStart(2, '0')}:${String(restante % 60).padStart(2, '0')}`;
      }, 1000);

      let metodo = 'pix';
      const abas = [...el.querySelectorAll('[role=tab]')];
      const selecionar = aba => {
        metodo = aba.dataset.aba;
        abas.forEach(a => {
          const sel = a === aba;
          a.setAttribute('aria-selected', String(sel));
          a.tabIndex = sel ? 0 : -1;
        });
        el.querySelector('#painel-pix').hidden = metodo !== 'pix';
        el.querySelector('#painel-cartao').hidden = metodo === 'pix';
        el.querySelector('#painel-cartao').setAttribute('aria-labelledby', `aba-${metodo === 'pix' ? 'credito' : metodo}`);
      };
      abas.forEach((a, i) => {
        a.addEventListener('click', () => selecionar(a));
        a.addEventListener('keydown', e => {
          if (e.key !== 'ArrowRight' && e.key !== 'ArrowLeft') return;
          const prox = abas[(i + (e.key === 'ArrowRight' ? 1 : abas.length - 1)) % abas.length];
          prox.focus();
          selecionar(prox);
        });
      });

      el.querySelector('#pix-copiar').addEventListener('click', e => K.ui.copiar(payload, e.currentTarget));

      const finalizar = (botao, textoOcupado) => {
        botao.disabled = true;
        botao.innerHTML = `<span class="spinner" aria-hidden="true"></span>${textoOcupado}`;
        K.later(() => {
          const p = S().logado();
          const res = S().criarPedido({
            produtor: p,
            data: r.data,
            boxes: r.boxes,
            produto: r.produto.trim(),
            qtd: r.qtd ? `${r.qtd} ${r.unidade}` : '',
            pagamento: metodo
          });
          if (res.erro) {
            K.ui.toast(res.erro, 'erro');
            r.boxes = r.boxes.filter(b => !S().reservaDoBox(b, r.data));
            K.ir('#reservar');
            return;
          }
          K.ultimoPedido = res.codigo;
          K.rascunho = null;
          K.ui.chime();
          K.ir('#confirmacao');
        }, 1400);
      };

      el.querySelector('#pix-pago').addEventListener('click', e => finalizar(e.currentTarget, 'Confirmando pagamento…'));

      el.querySelector('#painel-cartao').addEventListener('submit', e => {
        e.preventDefault();
        const $ = id => el.querySelector('#' + id);
        const [mm, aa] = $('cartao-val').value.split('/').map(Number);
        const agora = new Date();
        const vencido = !mm || mm > 12 || !aa || (2000 + aa < agora.getFullYear()) ||
          (2000 + aa === agora.getFullYear() && mm < agora.getMonth() + 1);
        const regras = [
          ['cartao-num', K.soDigitos($('cartao-num').value).length === 16 ? '' : 'O número tem 16 dígitos.'],
          ['cartao-nome', $('cartao-nome').value.trim().length >= 3 ? '' : 'Escreva o nome como está no cartão.'],
          ['cartao-val', vencido ? 'Validade inválida ou vencida.' : ''],
          ['cartao-cvv', K.soDigitos($('cartao-cvv').value).length >= 3 ? '' : 'São 3 ou 4 números no verso.']
        ];
        let erro = null;
        regras.forEach(([id, msg]) => { K.ui.erroCampo($(id), msg); if (msg && !erro) erro = $(id); });
        if (erro) { erro.focus(); return; }
        finalizar($('cartao-pagar'), 'Processando cartão…');
      });
    }
  };

  // ---------- Confirmação ----------
  V.confirmacao = {
    area: 'produtor',
    titulo: 'Reserva confirmada',
    guarda() {
      const g = exigeLogin();
      if (g) return g;
      if (!K.ultimoPedido || !S().pedido(K.ultimoPedido).length) return '#minhas-reservas';
      return null;
    },
    render() {
      const g = S().pedido(K.ultimoPedido);
      const ehHoje = g[0].data === K.hoje();
      const pendente = g.some(r => r.status === 'reservado');
      return `
        ${etapas(3)}
        <div class="estreito">
          <header class="sucesso">
            <span class="sucesso-ic">${K.icon('check', 30)}</span>
            <h1>${pendente ? 'Pagamento confirmado' : 'Check-in feito'}</h1>
            <p class="sub">${pendente
              ? (ehHoje ? 'Suas pedras estão reservadas para hoje. Ao chegar, mostre o QR Code na portaria e faça o check-in.'
                : `Suas pedras estão reservadas para ${K.esc(K.fmtDataLonga(g[0].data).toLowerCase())}. Guarde o código abaixo.`)
              : 'Bom pregão! O fiscal já vê você no mapa do pátio.'}</p>
          </header>
          ${K.ui.bilhete(g, {
            acoes: pendente
              ? (ehHoje
                ? `<button type="button" class="btn btn-azul" data-checkin="${K.esc(g[0].codigo)}">${K.icon('pin', 18)}Cheguei, fazer check-in</button>`
                : `<p class="sutil">${K.icon('clock', 16)} O check-in abre no dia da reserva.</p>`)
              : ''
          })}
          <div class="linha-acoes">
            <a class="btn btn-ghost" href="#minhas-reservas">${K.icon('ticket', 18)}Minhas reservas</a>
            <a class="btn btn-ghost" href="#reservar">${K.icon('plus', 18)}Nova reserva</a>
          </div>
        </div>`;
    },
    mount(el) {
      K.ui.montarQRs(el);
      ligarCheckin(el);
    }
  };

  const ligarCheckin = el => {
    el.querySelectorAll('[data-checkin]').forEach(b => b.addEventListener('click', () => {
      S().checkin(b.dataset.checkin);
      K.ui.toast('Check-in feito. Bom pregão!');
      K.render();
    }));
    el.querySelectorAll('[data-cancelar]').forEach(b => b.addEventListener('click', async () => {
      const ok = await K.ui.confirmar({
        titulo: 'Cancelar esta reserva?',
        texto: 'As pedras voltam a ficar livres para outros produtores. O valor do PIX ou cartão é estornado em até 2 dias úteis.',
        ok: 'Cancelar reserva',
        cancelar: 'Manter',
        perigo: true
      });
      if (!ok) return;
      S().cancelarPedido(b.dataset.cancelar);
      K.ui.toast('Reserva cancelada. As pedras foram liberadas.');
      K.render();
    }));
  };

  // ---------- Minhas reservas ----------
  V['minhas-reservas'] = {
    area: 'produtor',
    titulo: 'Minhas reservas',
    guarda: exigeLogin,
    render() {
      const p = S().logado();
      const hoje = K.hoje();
      const grupos = S().pedidosDoProdutor(p.id);
      const proximas = grupos.filter(g => g[0].data >= hoje && !g.every(r => r.status === 'cancelada'));
      const anteriores = grupos.filter(g => !proximas.includes(g)).reverse();

      const acoes = g => {
        const r0 = g[0];
        const pend = g.some(r => r.status === 'reservado');
        if (!pend) return '';
        return `${r0.data === hoje ? `<button type="button" class="btn btn-azul btn-sm" data-checkin="${K.esc(r0.codigo)}">${K.icon('pin', 16)}Fazer check-in</button>` : ''}
          <button type="button" class="btn btn-ghost btn-sm" data-cancelar="${K.esc(r0.codigo)}">Cancelar</button>`;
      };

      return `
        <header class="cab-pagina cab-linha">
          <div>
            <p class="eyebrow">${K.esc(p.nome)}</p>
            <h1>Minhas reservas</h1>
          </div>
          <a class="btn btn-primario" href="#reservar">${K.icon('plus', 18)}Nova reserva</a>
        </header>
        <section aria-labelledby="prox-tit">
          <h2 id="prox-tit" class="secao-tit">Próximas</h2>
          ${proximas.length
            ? `<div class="lista-bilhetes">${proximas.map(g => K.ui.bilhete(g, { acoes: acoes(g) })).join('')}</div>`
            : K.ui.vazio('calendar', 'Nenhuma reserva marcada', 'Escolha as pedras do próximo pregão no mapa do pátio.',
              `<a class="btn btn-primario" href="#reservar">Reservar agora</a>`)}
        </section>
        ${anteriores.length ? `
        <section aria-labelledby="ant-tit">
          <h2 id="ant-tit" class="secao-tit">Anteriores e canceladas</h2>
          <div class="lista-bilhetes">${anteriores.map(g => K.ui.bilhete(g, { qr: false })).join('')}</div>
        </section>` : ''}`;
    },
    mount(el) {
      K.ui.montarQRs(el);
      ligarCheckin(el);
    }
  };
})(window.K);
