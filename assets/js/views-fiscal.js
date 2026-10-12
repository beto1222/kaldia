/* Área do fiscal: painel do dia, relatórios e base de produtores */
'use strict';

(function (K) {
  const V = K.views = K.views || {};
  const S = () => K.store;

  // Data observada pelo fiscal (persiste entre telas da sessão)
  K.dataFiscal = K.dataFiscal || K.hoje();
  let filtroStatus = 'todos';
  let buscaFiscal = '';

  const OCORRENCIAS_RAPIDAS = [
    'Volume diferente do declarado',
    'Produto diferente do informado',
    'Mercadoria sobre o corredor',
    'Veículo não autorizado na descarga'
  ];

  // ---------- Ações sobre uma reserva ----------
  const registrarOcorrencia = async id => {
    const r = S().reserva(id);
    const texto = await K.ui.modal({
      titulo: `Registrar ocorrência · ${K.esc(r.box)}`,
      corpo: `
        <form novalidate>
          <p class="texto-modal">${K.esc(r.produtor)} · ${K.esc(r.produto)}</p>
          <div class="chips-rapidos" role="group" aria-label="Motivos comuns">
            ${OCORRENCIAS_RAPIDAS.map(t => `<button type="button" class="chip-sugestao" data-motivo="${K.esc(t)}">${K.esc(t)}</button>`).join('')}
          </div>
          <div class="campo">
            <label for="oc-texto">O que foi encontrado</label>
            <textarea id="oc-texto" rows="3" placeholder="Ex.: declarou 80 cx, contagem deu 110 cx"></textarea>
          </div>
        </form>`,
      onMount: m => m.querySelectorAll('[data-motivo]').forEach(b => b.addEventListener('click', () => {
        const t = m.querySelector('#oc-texto');
        t.value = t.value ? `${t.value}. ${b.dataset.motivo}` : b.dataset.motivo;
        t.focus();
      })),
      acoes: [
        { rotulo: 'Cancelar' },
        {
          rotulo: 'Registrar e alertar', estilo: 'btn-perigo', icone: 'alert',
          enviar: m => {
            const t = m.querySelector('#oc-texto');
            if (!t.value.trim()) { K.ui.erroCampo(t, 'Descreva a ocorrência para o produtor e a equipe.'); t.focus(); return; }
            return t.value.trim();
          }
        }
      ]
    });
    if (!texto) return false;
    S().registrarOcorrencia(id, texto);
    K.ui.toast(`Ocorrência registrada na pedra ${r.box}.`);
    return true;
  };

  const tratarOcorrencia = async id => {
    const r = S().reserva(id);
    const aberta = [...r.ocorrencias].reverse().find(o => !o.resolvidaEm);
    const v = await K.ui.modal({
      titulo: `Tratar ocorrência · ${K.esc(r.box)}`,
      corpo: `
        <blockquote class="citacao">${K.esc(aberta ? aberta.texto : 'Sem descrição')}<footer>Registrada às ${K.fmtHora(aberta && aberta.em)}</footer></blockquote>
        <p class="texto-modal">A situação foi corrigida no local?</p>`,
      acoes: [
        { rotulo: 'Ainda não, deixar em revisão', valor: 'revisao', estilo: 'btn-ghost', icone: 'clock' },
        { rotulo: 'Sim, resolvida', valor: 'resolvida', estilo: 'btn-primario', icone: 'check' }
      ]
    });
    if (!v) return false;
    S().tratarOcorrencia(id, v === 'resolvida');
    K.ui.toast(v === 'resolvida' ? `Ocorrência da ${r.box} resolvida.` : `${r.box} ficou em revisão.`);
    return true;
  };

  const cancelarReserva = async id => {
    const r = S().reserva(id);
    const ok = await K.ui.confirmar({
      titulo: `Liberar a pedra ${r.box}?`,
      texto: `A reserva de ${r.produtor} será cancelada e a pedra fica livre para nova locação.`,
      ok: 'Liberar pedra',
      perigo: true
    });
    if (!ok) return false;
    S().cancelarReserva(id);
    K.ui.toast(`Pedra ${r.box} liberada.`);
    return true;
  };

  const locacaoBalcao = async boxInicial => {
    const data = K.dataFiscal;
    const livres = S().livres(data);
    if (!livres.length) { K.ui.toast('Não há pedras livres nesta data.', 'info'); return false; }
    const nomes = S().state.produtores.filter(p => p.aprovado).map(p => p.nome);
    const res = await K.ui.modal({
      titulo: 'Locação no balcão',
      corpo: `
        <form class="form-grade" novalidate>
          <p class="texto-modal campo-largo">Para produtores que chegaram sem reserva. A pedra já entra como ocupada em ${K.esc(K.rotuloDia(data).toLowerCase())}.</p>
          <div class="campo campo-largo">
            <label for="bal-nome">Produtor</label>
            <input id="bal-nome" type="text" list="bal-lista" placeholder="Nome do produtor ou sítio" autocomplete="off">
            <datalist id="bal-lista">${nomes.map(n => `<option value="${K.esc(n)}"></option>`).join('')}</datalist>
          </div>
          <div class="campo">
            <label for="bal-box">Pedra</label>
            <select id="bal-box">${livres.map(b => `<option value="${b.id}" ${b.id === boxInicial ? 'selected' : ''}>${b.id} · ${K.setorInfo(b.setor).nome}</option>`).join('')}</select>
          </div>
          <div class="campo">
            <label for="bal-qtd">Volume</label>
            <input id="bal-qtd" type="text" placeholder="Ex.: 40 cx">
          </div>
          <div class="campo campo-largo">
            <label for="bal-produto">Produto</label>
            <input id="bal-produto" type="text" placeholder="O que está vendendo">
          </div>
        </form>`,
      onMount: m => {
        const nome = m.querySelector('#bal-nome');
        nome.addEventListener('change', () => {
          const p = S().state.produtores.find(x => x.nome === nome.value);
          const prod = m.querySelector('#bal-produto');
          if (p && !prod.value) prod.value = p.produto;
        });
      },
      acoes: [
        { rotulo: 'Cancelar' },
        {
          rotulo: 'Registrar locação', estilo: 'btn-primario', icone: 'plus',
          enviar: m => {
            const $ = id => m.querySelector('#' + id);
            const erros = [
              [$('bal-nome'), $('bal-nome').value.trim().length >= 3 ? '' : 'Informe o nome do produtor.'],
              [$('bal-produto'), $('bal-produto').value.trim() ? '' : 'Informe o produto.']
            ];
            erros.forEach(([i, msg]) => K.ui.erroCampo(i, msg));
            const prim = erros.find(([, msg]) => msg);
            if (prim) { prim[0].focus(); return; }
            return {
              nome: $('bal-nome').value.trim(),
              box: $('bal-box').value,
              produto: $('bal-produto').value.trim(),
              qtd: $('bal-qtd').value.trim()
            };
          }
        }
      ]
    });
    if (!res) return false;
    const out = S().locacaoBalcao({ ...res, data });
    if (out.erro) { K.ui.toast(out.erro, 'erro'); return false; }
    K.ui.toast(`Locação da ${res.box} registrada para ${res.nome}.`);
    return true;
  };

  // Painel lateral com detalhes de uma pedra
  const abrirPedra = async box => {
    const data = K.dataFiscal;
    const r = S().reservaDoBox(box, data);
    const setor = K.setorInfo(K.boxInfo(box).setor);

    if (!r) {
      const v = await K.ui.painel({
        titulo: `Pedra ${box}`,
        corpo: `
          <p class="painel-sub">${setor.nome} · ${setor.desc}</p>
          <div class="painel-estado"><span class="pill pill-muted"><span class="pill-dot"></span>Livre</span></div>
          <p class="texto-modal">Sem reserva para ${K.esc(K.rotuloDia(data).toLowerCase())}. Valor de ${K.fmtMoeda(setor.preco)} por pregão.</p>`,
        acoes: [{ rotulo: 'Registrar locação no balcão', valor: 'balcao', estilo: 'btn-primario', icone: 'plus' }]
      });
      if (v === 'balcao' && await locacaoBalcao(box)) K.render();
      return;
    }

    const p = r.produtorId ? S().produtor(r.produtorId) : null;
    const acoes = [];
    if (r.status === 'reservado') acoes.push({ rotulo: 'Confirmar chegada', valor: 'chegada', estilo: 'btn-primario', icone: 'check' });
    if (r.status === 'alerta' || r.status === 'revisao') acoes.push({ rotulo: 'Tratar ocorrência', valor: 'tratar', estilo: 'btn-azul', icone: 'wrench' });
    else acoes.push({ rotulo: 'Registrar ocorrência', valor: 'ocorrencia', estilo: 'btn-ghost', icone: 'alert' });
    acoes.push({ rotulo: 'Liberar pedra', valor: 'liberar', estilo: 'btn-ghost btn-texto-perigo', icone: 'trash' });

    const v = await K.ui.painel({
      titulo: `Pedra ${box}`,
      corpo: `
        <p class="painel-sub">${setor.nome} · ${K.esc(K.rotuloDia(data))}</p>
        <div class="painel-estado">${K.ui.pill(r.status, 'fiscal')}<span class="mono sutil">${K.esc(r.codigo)}</span></div>
        <dl class="ficha">
          <div><dt>Produtor</dt><dd>${K.esc(r.produtor)}</dd></div>
          ${p ? `<div><dt>WhatsApp</dt><dd class="mono">${K.esc(K.fmtTelefone(p.telefone))}</dd></div>
          <div><dt>Veículo</dt><dd>${K.esc(p.veiculo)} · <span class="mono">${K.esc(K.fmtPlaca(p.placa))}</span></dd></div>` : ''}
          <div><dt>Produto</dt><dd>${K.esc(r.produto || '—')}</dd></div>
          <div><dt>Volume declarado</dt><dd>${K.esc(r.qtd || '—')}</dd></div>
          <div><dt>Pagamento</dt><dd>${K.fmtMoeda(r.valor)} · ${K.esc(K.PAGAMENTOS[r.pagamento])}</dd></div>
          <div><dt>Check-in</dt><dd>${r.checkinEm ? K.fmtHora(r.checkinEm) : 'Ainda não chegou'}</dd></div>
          ${p && p.pessoas ? `<div><dt>Autorizados</dt><dd>${K.esc(p.pessoas)}</dd></div>` : ''}
        </dl>
        ${r.ocorrencias.length ? `
          <h3 class="painel-secao">Ocorrências</h3>
          <ul class="linha-tempo">${r.ocorrencias.map(o => `
            <li class="${o.resolvidaEm ? 'resolvida' : ''}">
              <span class="lt-hora">${K.fmtHora(o.em)}</span>
              <p>${K.esc(o.texto)}</p>
              <span class="lt-estado">${o.resolvidaEm ? `Resolvida às ${K.fmtHora(o.resolvidaEm)}` : (r.status === 'revisao' ? 'Em revisão' : 'Aberta')}</span>
            </li>`).join('')}</ul>` : ''}`,
      acoes
    });
    let mudou = false;
    if (v === 'chegada') { S().confirmarChegada(r.id); K.ui.toast(`Chegada confirmada na ${box}.`); mudou = true; }
    if (v === 'ocorrencia') mudou = await registrarOcorrencia(r.id);
    if (v === 'tratar') mudou = await tratarOcorrencia(r.id);
    if (v === 'liberar') mudou = await cancelarReserva(r.id);
    if (mudou) K.render();
  };

  // ---------- Painel do dia ----------
  V.painel = {
    area: 'fiscal',
    titulo: 'Painel do dia',
    render() {
      const data = K.dataFiscal;
      const ativas = S().ativas(data);
      const cont = st => ativas.filter(r => r.status === st).length;
      const lot = S().lotacao(data);
      const receita = ativas.reduce((s, r) => s + r.valor, 0);
      const abertas = cont('alerta') + cont('revisao');
      const tomLot = lot >= 85 ? 'red' : lot >= 60 ? 'amber' : 'green';

      return `
        <header class="cab-pagina cab-linha">
          <div>
            <p class="eyebrow">Fiscalização de pátio</p>
            <h1>${K.esc(K.rotuloDia(data))}${data === K.hoje() ? ` <span class="h1-data">${K.fmtData(data)}</span>` : ''}</h1>
          </div>
          <div class="cab-acoes">
            <div class="seletor-data">
              <button type="button" class="icon-btn" data-dia="-1" aria-label="Dia anterior">${K.icon('left', 18)}</button>
              <label class="sr-only" for="fiscal-data">Data</label>
              <input type="date" id="fiscal-data" value="${data}">
              <button type="button" class="icon-btn" data-dia="1" aria-label="Próximo dia">${K.icon('right', 18)}</button>
            </div>
            <button type="button" class="btn btn-primario" id="btn-balcao">${K.icon('plus', 18)}Locação no balcão</button>
          </div>
        </header>

        <section class="kpis" aria-label="Resumo do dia">
          <div class="kpi kpi-destaque">
            <p class="kpi-rot">Lotação</p>
            <p class="kpi-val">${K.fmtPct(lot)}</p>
            <div class="medidor medidor-${tomLot}" aria-hidden="true"><span style="width:${lot}%"></span></div>
            <p class="kpi-nota">${ativas.length} de ${K.BOXES.length} pedras</p>
          </div>
          <div class="kpi"><p class="kpi-rot">Aguardando chegada</p><p class="kpi-val">${cont('reservado')}</p><p class="kpi-nota">reservas pagas</p></div>
          <div class="kpi"><p class="kpi-rot">No pátio</p><p class="kpi-val">${cont('ocupado')}</p><p class="kpi-nota">com check-in</p></div>
          <div class="kpi ${abertas ? 'kpi-alerta' : ''}"><p class="kpi-rot">Ocorrências abertas</p><p class="kpi-val">${abertas}</p><p class="kpi-nota">${cont('revisao')} em revisão</p></div>
          <div class="kpi"><p class="kpi-rot">Arrecadação</p><p class="kpi-val kpi-val-sm">${K.fmtMoeda(receita)}</p><p class="kpi-nota">reservas + balcão</p></div>
        </section>

        <section class="cartao" aria-labelledby="planta-tit">
          <div class="cartao-cab">
            <h2 id="planta-tit">Planta do pátio</h2>
            ${K.patio.legenda('fiscal')}
          </div>
          <div id="planta-fiscal">${K.patio.html({ data, modo: 'fiscal' })}</div>
          <p class="nota-rodape">${K.icon('info', 14)}Toque em uma pedra para ver detalhes, confirmar chegada ou registrar ocorrência.</p>
        </section>

        <section class="cartao" aria-labelledby="agenda-tit">
          <div class="cartao-cab cartao-cab-wrap">
            <h2 id="agenda-tit">Agendamentos</h2>
            <div class="filtros">
              <label class="busca-mini">${K.icon('search', 16)}<span class="sr-only">Buscar</span>
                <input type="search" id="busca-agenda" placeholder="Produtor, pedra ou produto" value="${K.esc(buscaFiscal)}">
              </label>
              <div class="segmentos" role="group" aria-label="Filtrar por situação" id="filtro-status"></div>
            </div>
          </div>
          <div id="tabela-agenda"></div>
        </section>`;
    },
    mount(el) {
      const data = K.dataFiscal;

      el.querySelector('#fiscal-data').addEventListener('change', e => {
        if (e.target.value) { K.dataFiscal = e.target.value; K.render(); }
      });
      el.querySelectorAll('[data-dia]').forEach(b => b.addEventListener('click', () => {
        K.dataFiscal = K.addDias(K.dataFiscal, Number(b.dataset.dia));
        K.render();
      }));
      el.querySelector('#btn-balcao').addEventListener('click', async () => { if (await locacaoBalcao()) K.render(); });
      el.querySelectorAll('#planta-fiscal [data-box]').forEach(b => b.addEventListener('click', () => abrirPedra(b.dataset.box)));

      const filtros = [
        ['todos', 'Todos'], ['reservado', 'Aguardando'], ['ocupado', 'No pátio'], ['alerta', 'Irregular'], ['revisao', 'Revisão']
      ];

      const desenharTabela = () => {
        const ativas = S().ativas(data);
        el.querySelector('#filtro-status').innerHTML = filtros.map(([v, t]) => {
          const n = v === 'todos' ? ativas.length : ativas.filter(r => r.status === v).length;
          return `<button type="button" data-filtro="${v}" aria-pressed="${filtroStatus === v}">${t} <span class="contagem-mini">${n}</span></button>`;
        }).join('');
        el.querySelectorAll('[data-filtro]').forEach(b => b.addEventListener('click', () => {
          filtroStatus = b.dataset.filtro;
          desenharTabela();
        }));

        const termo = buscaFiscal.trim().toLowerCase();
        const linhas = ativas
          .filter(r => filtroStatus === 'todos' || r.status === filtroStatus)
          .filter(r => !termo || [r.produtor, r.box, r.produto, r.codigo].join(' ').toLowerCase().includes(termo))
          .sort((a, b) => a.box.localeCompare(b.box));

        const alvo = el.querySelector('#tabela-agenda');
        if (!linhas.length) {
          alvo.innerHTML = K.ui.vazio('calendar', ativas.length ? 'Nada com esse filtro' : 'Nenhum agendamento nesta data',
            ativas.length ? 'Tente outra situação ou limpe a busca.' : 'As reservas feitas pelos produtores aparecem aqui.');
          return;
        }
        alvo.innerHTML = `
          <div class="tabela-rolagem">
            <table class="tabela">
              <thead><tr><th>Pedra</th><th>Produtor</th><th>Produto</th><th>Volume</th><th>Situação</th><th><span class="sr-only">Ações</span></th></tr></thead>
              <tbody>${linhas.map(r => {
                const oc = [...r.ocorrencias].reverse().find(o => !o.resolvidaEm);
                return `<tr>
                  <td data-rot="Pedra"><span class="chip-pedra">${K.esc(r.box)}</span></td>
                  <td data-rot="Produtor"><strong>${K.esc(r.produtor)}</strong>${r.pagamento === 'balcao' ? '<span class="tag">balcão</span>' : ''}</td>
                  <td data-rot="Produto">${K.esc(r.produto)}</td>
                  <td data-rot="Volume" class="num">${K.esc(r.qtd || '—')}</td>
                  <td data-rot="Situação">${K.ui.pill(r.status, 'fiscal')}${oc && (r.status === 'alerta' || r.status === 'revisao') ? `<p class="obs">${K.esc(oc.texto)}</p>` : ''}</td>
                  <td class="acoes-linha">
                    ${r.status === 'reservado' ? `<button type="button" class="btn btn-sm btn-primario" data-chegada="${r.id}">${K.icon('check', 16)}Chegou</button>` : ''}
                    ${(r.status === 'alerta' || r.status === 'revisao')
                      ? `<button type="button" class="btn btn-sm btn-azul" data-tratar="${r.id}">${K.icon('wrench', 16)}Tratar</button>`
                      : `<button type="button" class="btn btn-sm btn-ghost" data-ocorrencia="${r.id}">${K.icon('alert', 16)}Ocorrência</button>`}
                    <button type="button" class="icon-btn" data-detalhe="${K.esc(r.box)}" aria-label="Detalhes da ${K.esc(r.box)}">${K.icon('right', 18)}</button>
                  </td>
                </tr>`;
              }).join('')}</tbody>
            </table>
          </div>`;

        alvo.querySelectorAll('[data-chegada]').forEach(b => b.addEventListener('click', () => {
          const r = S().reserva(b.dataset.chegada);
          S().confirmarChegada(r.id);
          K.ui.toast(`Chegada confirmada na ${r.box}.`);
          K.render();
        }));
        alvo.querySelectorAll('[data-ocorrencia]').forEach(b => b.addEventListener('click', async () => {
          if (await registrarOcorrencia(b.dataset.ocorrencia)) K.render();
        }));
        alvo.querySelectorAll('[data-tratar]').forEach(b => b.addEventListener('click', async () => {
          if (await tratarOcorrencia(b.dataset.tratar)) K.render();
        }));
        alvo.querySelectorAll('[data-detalhe]').forEach(b => b.addEventListener('click', () => abrirPedra(b.dataset.detalhe)));
      };

      el.querySelector('#busca-agenda').addEventListener('input', e => {
        buscaFiscal = e.target.value;
        desenharTabela();
      });
      desenharTabela();
    }
  };

  // ---------- Relatórios ----------
  const graficoLotacao = serie => {
    const W = 720, H = 240, m = { t: 16, r: 16, b: 30, l: 40 };
    const iw = W - m.l - m.r, ih = H - m.t - m.b;
    const x = i => m.l + (serie.length === 1 ? iw / 2 : (i / (serie.length - 1)) * iw);
    const y = v => m.t + ih - (v / 100) * ih;
    const pts = serie.map((d, i) => [x(i), y(d.pct)]);
    const linha = pts.map((p, i) => `${i ? 'L' : 'M'}${p[0].toFixed(1)},${p[1].toFixed(1)}`).join('');
    const area = `${linha}L${pts[pts.length - 1][0].toFixed(1)},${y(0)}L${pts[0][0].toFixed(1)},${y(0)}Z`;
    const media = serie.reduce((s, d) => s + d.pct, 0) / serie.length;
    const ult = pts[pts.length - 1];
    const marcas = serie.map((d, i) => i).filter(i => (i % 7 === 0 && i < serie.length - 4) || i === serie.length - 1);

    return `
      <svg viewBox="0 0 ${W} ${H}" class="grafico" role="img" aria-label="Lotação diária dos últimos ${serie.length} dias, média de ${Math.round(media)}%">
        <defs>
          <linearGradient id="grad-area" x1="0" y1="0" x2="0" y2="1">
            <stop offset="0%" stop-color="var(--leaf)" stop-opacity="0.28"/>
            <stop offset="100%" stop-color="var(--leaf)" stop-opacity="0"/>
          </linearGradient>
        </defs>
        ${[0, 25, 50, 75, 100].map(v => `
          <line x1="${m.l}" x2="${W - m.r}" y1="${y(v)}" y2="${y(v)}" class="g-grade"/>
          <text x="${m.l - 8}" y="${y(v) + 4}" text-anchor="end" class="g-rot">${v}%</text>`).join('')}
        <line x1="${m.l}" x2="${W - m.r}" y1="${y(media)}" y2="${y(media)}" class="g-media"/>
        <text x="${W - m.r}" y="${y(media) - 6}" text-anchor="end" class="g-rot g-rot-media">média ${Math.round(media)}%</text>
        <path d="${area}" fill="url(#grad-area)"/>
        <path d="${linha}" class="g-linha" fill="none"/>
        ${pts.map((p, i) => `<circle cx="${p[0].toFixed(1)}" cy="${p[1].toFixed(1)}" r="9" class="g-alvo"><title>${K.fmtDataCurta(serie[i].data)} · ${Math.round(serie[i].pct)}%</title></circle>`).join('')}
        <circle cx="${ult[0]}" cy="${ult[1]}" r="5" class="g-ponto"/>
        ${marcas.map(i => `<text x="${x(i)}" y="${H - 8}" text-anchor="${i === 0 ? 'start' : i === serie.length - 1 ? 'end' : 'middle'}" class="g-rot">${i === serie.length - 1 ? 'Hoje' : K.fmtDataCurta(serie[i].data)}</text>`).join('')}
      </svg>`;
  };

  V.relatorios = {
    area: 'fiscal',
    titulo: 'Relatórios',
    render() {
      const hoje = K.hoje();
      const st = S().state;
      const ativasHoje = S().ativas(hoje);
      const ocHoje = st.reservas.filter(r => r.data === hoje).flatMap(r => r.ocorrencias);
      const cancHoje = st.reservas.filter(r => r.data === hoje && r.status === 'cancelada').length;

      const serie = [
        ...st.historico.map(h => ({ data: h.data, pct: (h.ocupados / h.total) * 100 })),
        { data: hoje, pct: S().lotacao(hoje) }
      ];
      const media = serie.reduce((s, d) => s + d.pct, 0) / serie.length;
      const receita = st.historico.reduce((s, h) => s + h.receita, 0) + ativasHoje.reduce((s, r) => s + r.valor, 0);
      const ocorrencias = st.historico.reduce((s, h) => s + h.ocorrencias, 0) + ocHoje.length;
      const resolvidas = st.historico.reduce((s, h) => s + h.resolvidas, 0) + ocHoje.filter(o => o.resolvidaEm).length;
      const cancelamentos = st.historico.reduce((s, h) => s + h.cancelamentos, 0) + cancHoje;
      const eficiencia = ocorrencias ? (resolvidas / ocorrencias) * 100 : 100;

      // Dia da semana mais cheio
      const porDow = Array.from({ length: 7 }, () => []);
      serie.forEach(d => porDow[K.parseISO(d.data).getDay()].push(d.pct));
      const nomesDow = ['Domingo', 'Segunda', 'Terça', 'Quarta', 'Quinta', 'Sexta', 'Sábado'];
      const medDow = porDow.map(a => a.length ? a.reduce((s, v) => s + v, 0) / a.length : 0);
      const pico = medDow.indexOf(Math.max(...medDow));

      const setores = K.SETORES.map(s => {
        const total = K.BOXES.filter(b => b.setor === s.id).length;
        const oc = ativasHoje.filter(r => K.boxInfo(r.box).setor === s.id).length;
        return { ...s, total, oc, pct: (oc / total) * 100 };
      });

      const produtos = {};
      st.reservas.filter(r => r.status !== 'cancelada' && r.data >= K.addDias(hoje, -30)).forEach(r => {
        const chave = (r.produto || 'Não informado').split(/ e |,|\(/)[0].trim();
        produtos[chave] = (produtos[chave] || 0) + 1;
      });
      const topProdutos = Object.entries(produtos).sort((a, b) => b[1] - a[1]).slice(0, 6);
      const maxProd = topProdutos.length ? topProdutos[0][1] : 1;

      return `
        <header class="cab-pagina">
          <p class="eyebrow">Últimos 30 dias</p>
          <h1>Relatórios do pátio</h1>
          <p class="sub">Números consolidados para planejar escala de fiscais, preços e campanhas.</p>
        </header>

        <section class="kpis kpis-4" aria-label="Indicadores">
          <div class="kpi"><p class="kpi-rot">Lotação média</p><p class="kpi-val">${K.fmtPct(media)}</p><p class="kpi-nota">pico às ${nomesDow[pico].toLowerCase()}s · ${K.fmtPct(medDow[pico])}</p></div>
          <div class="kpi"><p class="kpi-rot">Arrecadação</p><p class="kpi-val kpi-val-sm">${K.fmtMoeda(receita)}</p><p class="kpi-nota">${K.fmtMoeda(receita / serie.length)} por dia</p></div>
          <div class="kpi"><p class="kpi-rot">Ocorrências</p><p class="kpi-val">${ocorrencias}</p><p class="kpi-nota">${resolvidas} resolvidas no turno</p></div>
          <div class="kpi"><p class="kpi-rot">Cancelamentos</p><p class="kpi-val">${cancelamentos}</p><p class="kpi-nota">reservas desistidas</p></div>
        </section>

        <section class="cartao" aria-labelledby="graf-tit">
          <div class="cartao-cab"><h2 id="graf-tit">Lotação diária</h2><p class="sutil">% das ${K.BOXES.length} pedras ocupadas</p></div>
          <div class="grafico-caixa">${graficoLotacao(serie)}</div>
        </section>

        <div class="duas-colunas">
          <section class="cartao" aria-labelledby="setor-tit">
            <h2 id="setor-tit">Ocupação por setor hoje</h2>
            <ul class="barras">${setores.map(s => `
              <li>
                <div class="barra-cab"><span>${s.nome}</span><span class="num">${s.oc}/${s.total}</span></div>
                <div class="barra-trilho"><span style="width:${s.pct}%"></span></div>
              </li>`).join('')}</ul>
          </section>
          <section class="cartao" aria-labelledby="efic-tit">
            <h2 id="efic-tit">Eficiência no tratamento</h2>
            <div class="anel-linha">
              <div class="anel" style="--p:${eficiencia}" role="img" aria-label="${Math.round(eficiencia)}% das ocorrências resolvidas"><span>${K.fmtPct(eficiencia)}</span></div>
              <p class="sutil">Ocorrências resolvidas pela equipe no mesmo turno em que foram registradas. Meta da administração: 80%.</p>
            </div>
          </section>
        </div>

        <section class="cartao" aria-labelledby="prod-tit">
          <h2 id="prod-tit">Produtos mais reservados</h2>
          ${topProdutos.length ? `<ul class="barras barras-produtos">${topProdutos.map(([n, v]) => `
            <li>
              <div class="barra-cab"><span>${K.esc(n)}</span><span class="num">${v} ${v === 1 ? 'pedra' : 'pedras'}</span></div>
              <div class="barra-trilho barra-azul"><span style="width:${(v / maxProd) * 100}%"></span></div>
            </li>`).join('')}</ul>` : '<p class="sutil">Sem reservas no período.</p>'}
        </section>`;
    }
  };

  // ---------- Produtores (CRM) ----------
  let filtroCRM = 'todos';
  let buscaCRM = '';
  const SITUACOES = {
    ativo_locado: ['Com reserva', 'green'],
    ativo_sem_locacao: ['Sem reserva', 'amber'],
    inativo: ['Inativo', 'muted'],
    pendente: ['Em análise', 'orange']
  };

  V.produtores = {
    area: 'fiscal',
    titulo: 'Produtores',
    render() {
      const camp = S().state.campanhas.slice(0, 3);
      return `
        <header class="cab-pagina">
          <p class="eyebrow">Base de produtores</p>
          <h1>Produtores e convites</h1>
          <p class="sub">Encontre quem está sem reserva e chame de volta pelo WhatsApp, principalmente na safra do produto.</p>
        </header>

        <section class="cartao campanha" aria-labelledby="camp-tit">
          <div class="campanha-txt">
            <span class="perfil-icone perfil-verde">${K.icon('megaphone', 22)}</span>
            <div>
              <h2 id="camp-tit">Campanha de safra</h2>
              <p class="sutil">Convida, de uma vez, todos os produtores daquele produto que não têm reserva.</p>
            </div>
          </div>
          <form class="campanha-form" id="form-campanha" novalidate>
            <div class="campo">
              <label for="camp-produto">Produto em safra</label>
              <input id="camp-produto" type="text" list="camp-sugestoes" placeholder="Ex.: Morango" value="Morango">
              <datalist id="camp-sugestoes">${['Morango', 'Tomate', 'Alface', 'Milho', 'Batata', 'Laranja'].map(p => `<option value="${p}"></option>`).join('')}</datalist>
            </div>
            <button class="btn btn-primario" type="submit">${K.icon('send', 18)}Preparar convites</button>
          </form>
          ${camp.length ? `<ul class="campanhas-recentes">${camp.map(c => `
            <li>${K.icon('check', 14)}<span><strong>${K.esc(c.produto)}</strong> · ${c.total} convite${c.total === 1 ? '' : 's'}</span><span class="sutil">${new Date(c.em).toLocaleDateString('pt-BR')} ${K.fmtHora(c.em)}</span></li>`).join('')}
          </ul>` : ''}
        </section>

        <section class="cartao" aria-labelledby="base-tit">
          <div class="cartao-cab cartao-cab-wrap">
            <h2 id="base-tit">Base cadastrada</h2>
            <div class="filtros">
              <label class="busca-mini">${K.icon('search', 16)}<span class="sr-only">Buscar produtor</span>
                <input type="search" id="busca-crm" placeholder="Nome, produto ou placa" value="${K.esc(buscaCRM)}">
              </label>
              <div class="segmentos" role="group" aria-label="Filtrar por situação" id="filtro-crm"></div>
            </div>
          </div>
          <div id="tabela-crm"></div>
        </section>`;
    },
    mount(el) {
      const desenhar = () => {
        const todos = S().state.produtores.map(p => ({ p, sit: S().situacao(p) }));
        const filtros = [['todos', 'Todos'], ['ativo_locado', 'Com reserva'], ['ativo_sem_locacao', 'Sem reserva'], ['inativo', 'Inativos']];
        el.querySelector('#filtro-crm').innerHTML = filtros.map(([v, t]) => {
          const n = v === 'todos' ? todos.length : todos.filter(x => x.sit === v).length;
          return `<button type="button" data-filtro="${v}" aria-pressed="${filtroCRM === v}">${t} <span class="contagem-mini">${n}</span></button>`;
        }).join('');
        el.querySelectorAll('#filtro-crm [data-filtro]').forEach(b => b.addEventListener('click', () => { filtroCRM = b.dataset.filtro; desenhar(); }));

        const termo = buscaCRM.trim().toLowerCase();
        const lista = todos
          .filter(x => filtroCRM === 'todos' || x.sit === filtroCRM)
          .filter(x => !termo || [x.p.nome, x.p.produto, x.p.placa].join(' ').toLowerCase().includes(termo))
          .sort((a, b) => a.p.nome.localeCompare(b.p.nome, 'pt-BR'));

        const alvo = el.querySelector('#tabela-crm');
        if (!lista.length) {
          alvo.innerHTML = K.ui.vazio('users', 'Nenhum produtor encontrado', 'Ajuste a busca ou o filtro.');
          return;
        }
        alvo.innerHTML = `
          <div class="tabela-rolagem">
            <table class="tabela">
              <thead><tr><th>Produtor</th><th>Produto principal</th><th>Última reserva</th><th>Situação</th><th><span class="sr-only">Ações</span></th></tr></thead>
              <tbody>${lista.map(({ p, sit }) => {
                const [rot, tom] = SITUACOES[sit];
                const dias = p.ultimaLocacao ? K.diffDias(K.hoje(), p.ultimaLocacao) : null;
                const ult = sit === 'ativo_locado' ? 'Reserva ativa' : dias === null ? 'Nunca' : `há ${dias} dia${dias === 1 ? '' : 's'}`;
                const podeConvidar = sit === 'inativo' || sit === 'ativo_sem_locacao';
                return `<tr>
                  <td data-rot="Produtor"><div class="pessoa"><span class="avatar">${K.esc(K.iniciais(p.nome))}</span><span><strong>${K.esc(p.nome)}</strong><span class="mono sutil">${K.esc(K.fmtTelefone(p.telefone))}</span></span></div></td>
                  <td data-rot="Produto">${K.esc(p.produto)}</td>
                  <td data-rot="Última reserva">${ult}</td>
                  <td data-rot="Situação"><span class="pill pill-${tom}"><span class="pill-dot"></span>${rot}</span></td>
                  <td class="acoes-linha">${podeConvidar
                    ? (p.convidadoEm === K.hoje()
                      ? `<span class="sutil enviado">${K.icon('check', 14)}Convidado hoje</span>`
                      : `<button type="button" class="btn btn-sm btn-ghost" data-convidar="${p.id}">${K.icon('chat', 16)}Convidar</button>`)
                    : ''}</td>
                </tr>`;
              }).join('')}</tbody>
            </table>
          </div>`;
        alvo.querySelectorAll('[data-convidar]').forEach(b => b.addEventListener('click', () => {
          const p = S().produtor(b.dataset.convidar);
          S().convidar(p.id);
          K.ui.toast(`Convite enviado por WhatsApp para ${p.nome}.`);
          desenhar();
        }));
      };

      el.querySelector('#busca-crm').addEventListener('input', e => { buscaCRM = e.target.value; desenhar(); });

      el.querySelector('#form-campanha').addEventListener('submit', async e => {
        e.preventDefault();
        const inp = el.querySelector('#camp-produto');
        const termo = inp.value.trim();
        if (!termo) { K.ui.erroCampo(inp, 'Escreva o produto da campanha.'); inp.focus(); return; }
        K.ui.erroCampo(inp, '');
        const alvo = S().state.produtores.filter(p =>
          p.produto.toLowerCase().includes(termo.toLowerCase()) &&
          ['inativo', 'ativo_sem_locacao'].includes(S().situacao(p)));
        if (!alvo.length) {
          K.ui.toast(`Todos os produtores de "${termo}" já têm reserva, ou não há nenhum na base.`, 'info');
          return;
        }
        const ok = await K.ui.modal({
          titulo: `Convite de safra · ${K.esc(termo)}`,
          corpo: `
            <p class="texto-modal">${alvo.length} produtor${alvo.length === 1 ? '' : 'es'} sem reserva vão receber pelo WhatsApp:</p>
            <blockquote class="citacao citacao-msg">Olá! A safra de ${K.esc(termo.toLowerCase())} chegou e o pátio da CEASA tem pedras livres nesta semana. Reserve pelo KALDIA em poucos toques.</blockquote>
            <ul class="lista-destinatarios">${alvo.map(p => `<li><span class="avatar avatar-sm">${K.esc(K.iniciais(p.nome))}</span>${K.esc(p.nome)}<span class="sutil">${K.esc(p.produto)}</span></li>`).join('')}</ul>`,
          acoes: [
            { rotulo: 'Voltar' },
            { rotulo: `Enviar ${alvo.length} convite${alvo.length === 1 ? '' : 's'}`, valor: true, estilo: 'btn-primario', icone: 'send' }
          ]
        });
        if (!ok) return;
        S().registrarCampanha(termo, alvo.map(p => p.id));
        K.ui.toast(`Campanha enviada para ${alvo.length} produtor${alvo.length === 1 ? '' : 'es'} de ${termo.toLowerCase()}.`);
        K.render();
      });

      desenhar();
    }
  };
})(window.K);
