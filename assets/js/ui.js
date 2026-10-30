/* Componentes de interface: modal, painel lateral, avisos, QR Code, máscaras e bilhete */
'use strict';

(function (K) {
  const UI = K.ui = {};

  // ---------- Temporizadores ligados à tela atual ----------
  const timers = new Set();
  K.later = (fn, ms) => {
    const t = setTimeout(() => { timers.delete(t); fn(); }, ms);
    timers.add(t);
    return t;
  };
  K.every = (fn, ms) => {
    const t = setInterval(fn, ms);
    timers.add(t);
    return t;
  };
  K.limparTimers = () => {
    timers.forEach(t => { clearTimeout(t); clearInterval(t); });
    timers.clear();
  };

  // ---------- Avisos rápidos ----------
  UI.toast = (msg, tipo = 'ok') => {
    const area = document.getElementById('toasts');
    const el = document.createElement('div');
    el.className = `toast toast-${tipo}`;
    el.setAttribute('role', tipo === 'erro' ? 'alert' : 'status');
    const ic = tipo === 'erro' ? 'alert' : tipo === 'info' ? 'info' : 'check';
    el.innerHTML = `${K.icon(ic, 18)}<span>${K.esc(msg)}</span>`;
    area.appendChild(el);
    setTimeout(() => el.classList.add('saindo'), 3600);
    setTimeout(() => el.remove(), 4000);
  };

  // ---------- Camada (modal ou painel lateral) ----------
  const pilha = [];

  const abrirCamada = ({ tipo, titulo, corpo, acoes = [], onMount, largura }) => new Promise(resolve => {
    const origemFoco = document.activeElement;
    const fundo = document.createElement('div');
    fundo.className = `camada camada-${tipo}`;
    const tituloId = 'cam-' + K.uid();
    fundo.innerHTML = `
      <div class="${tipo}" role="dialog" aria-modal="true" aria-labelledby="${tituloId}" ${largura ? `style="--largura:${largura}"` : ''}>
        <header class="camada-topo">
          <h2 id="${tituloId}">${titulo}</h2>
          <button type="button" class="icon-btn" data-fechar aria-label="Fechar">${K.icon('x', 20)}</button>
        </header>
        <div class="camada-corpo">${corpo}</div>
        ${acoes.length ? `<footer class="camada-rodape">${acoes.map((a, i) => `
          <button type="button" class="btn ${a.estilo || 'btn-ghost'}" data-acao-idx="${i}">${a.icone ? K.icon(a.icone, 18) : ''}${K.esc(a.rotulo)}</button>`).join('')}
        </footer>` : ''}
      </div>`;

    const caixa = fundo.firstElementChild;
    const fechar = valor => {
      document.removeEventListener('keydown', teclado);
      const idx = pilha.indexOf(fechar);
      if (idx >= 0) pilha.splice(idx, 1);
      fundo.classList.add('saindo');
      setTimeout(() => fundo.remove(), 180);
      if (origemFoco && origemFoco.focus && document.contains(origemFoco)) origemFoco.focus();
      resolve(valor);
    };
    const teclado = e => {
      if (e.key === 'Escape' && pilha[pilha.length - 1] === fechar) fechar(null);
      if (e.key === 'Tab') {
        const foc = caixa.querySelectorAll('button:not([disabled]), input:not([disabled]), select, textarea, [tabindex="0"]');
        if (!foc.length) return;
        const prim = foc[0], ult = foc[foc.length - 1];
        if (e.shiftKey && document.activeElement === prim) { e.preventDefault(); ult.focus(); }
        else if (!e.shiftKey && document.activeElement === ult) { e.preventDefault(); prim.focus(); }
      }
    };

    fundo.addEventListener('mousedown', e => { if (e.target === fundo) fechar(null); });
    caixa.querySelector('[data-fechar]').addEventListener('click', () => fechar(null));
    caixa.querySelectorAll('[data-acao-idx]').forEach(btn => {
      btn.addEventListener('click', () => {
        const a = acoes[Number(btn.dataset.acaoIdx)];
        if (a.enviar) {
          const v = a.enviar(caixa);
          if (v !== undefined && v !== false) fechar(v);
        } else {
          fechar(a.valor ?? null);
        }
      });
    });
    caixa.addEventListener('submit', e => {
      e.preventDefault();
      const principal = acoes.findIndex(a => a.enviar);
      if (principal >= 0) caixa.querySelector(`[data-acao-idx="${principal}"]`).click();
    });

    document.getElementById('camadas').appendChild(fundo);
    document.addEventListener('keydown', teclado);
    pilha.push(fechar);
    if (onMount) onMount(caixa, fechar);
    const primeiro = caixa.querySelector('input, textarea, select') || caixa.querySelector('[data-acao-idx]') || caixa.querySelector('[data-fechar]');
    requestAnimationFrame(() => primeiro && primeiro.focus());
  });

  UI.modal = opts => abrirCamada({ ...opts, tipo: 'modal' });
  UI.painel = opts => abrirCamada({ ...opts, tipo: 'painel' });
  UI.fecharTudo = () => [...pilha].reverse().forEach(f => f(null));

  UI.confirmar = ({ titulo, texto, ok = 'Confirmar', cancelar = 'Voltar', perigo = false }) =>
    UI.modal({
      titulo: K.esc(titulo),
      corpo: `<p class="texto-modal">${K.esc(texto)}</p>`,
      acoes: [
        { rotulo: cancelar, valor: false },
        { rotulo: ok, valor: true, estilo: perigo ? 'btn-perigo' : 'btn-primario' }
      ]
    }).then(v => v === true);

  // ---------- Campos com máscara ----------
  UI.mascaras = raiz => {
    raiz.querySelectorAll('[data-mascara]').forEach(inp => {
      const tipo = inp.dataset.mascara;
      const fmt = {
        cpf: K.fmtCPF,
        tel: K.fmtTelefone,
        placa: K.fmtPlaca,
        cartao: v => K.soDigitos(v).slice(0, 16).replace(/(\d{4})(?=\d)/g, '$1 '),
        validade: v => K.soDigitos(v).slice(0, 4).replace(/^(\d{2})(\d)/, '$1/$2'),
        cvv: v => K.soDigitos(v).slice(0, 4)
      }[tipo];
      if (!fmt) return;
      inp.addEventListener('input', () => {
        const fim = inp.selectionEnd === inp.value.length;
        inp.value = fmt(inp.value);
        if (fim) inp.setSelectionRange(inp.value.length, inp.value.length);
      });
    });
  };

  UI.erroCampo = (inp, msg) => {
    const grupo = inp.closest('.campo');
    if (!grupo) return;
    grupo.classList.toggle('com-erro', !!msg);
    let el = grupo.querySelector('.campo-erro');
    if (msg) {
      if (!el) {
        el = document.createElement('p');
        el.className = 'campo-erro';
        el.id = inp.id + '-erro';
        grupo.appendChild(el);
      }
      el.textContent = msg;
      inp.setAttribute('aria-invalid', 'true');
      inp.setAttribute('aria-describedby', el.id);
    } else {
      if (el) el.remove();
      inp.removeAttribute('aria-invalid');
      inp.removeAttribute('aria-describedby');
    }
  };

  // ---------- QR Code ----------
  UI.qr = (el, texto, tam = 148) => {
    if (!el) return;
    el.innerHTML = '';
    if (typeof window.qrcode === 'function') {
      try {
        const q = window.qrcode(0, 'M');
        q.addData(texto);
        q.make();
        el.style.width = `${tam}px`;
        el.innerHTML = q.createSvgTag({ cellSize: 4, margin: 0, scalable: true })
          .replace('fill="black"', 'fill="#122643"')
          .replace('<svg ', '<svg role="img" aria-label="QR Code" ');
        return;
      } catch (e) { /* cai no aviso abaixo */ }
    }
    el.innerHTML = `<div class="qr-indisponivel">${K.icon('qr', 40)}<span>QR Code indisponível sem internet</span></div>`;
  };

  // ---------- Peças visuais ----------
  UI.pill = (status, visao = 'rotulo') => {
    const s = K.STATUS[status] || K.STATUS.reservado;
    return `<span class="pill pill-${s.tom}"><span class="pill-dot"></span>${K.esc(s[visao])}</span>`;
  };

  UI.copiar = (texto, botao) => {
    const ok = () => {
      UI.toast('Copiado para a área de transferência.');
      if (botao) {
        const orig = botao.innerHTML;
        botao.innerHTML = `${K.icon('check', 16)}Copiado`;
        setTimeout(() => { botao.innerHTML = orig; }, 1800);
      }
    };
    const falha = () => {
      const alvo = botao && botao.closest('.copiavel')?.querySelector('input, textarea');
      if (alvo) { alvo.focus(); alvo.select(); }
      UI.toast('Não deu para copiar automaticamente. O texto ficou selecionado.', 'info');
    };
    try {
      navigator.clipboard.writeText(texto).then(ok, falha);
    } catch (e) {
      falha();
    }
  };

  // Bilhete de reserva (um pedido pode ter até 3 pedras)
  UI.bilhete = (grupo, { qr = true, acoes = '' } = {}) => {
    const r0 = grupo[0];
    const total = grupo.reduce((s, r) => s + r.valor, 0);
    const status = grupo.some(r => r.status === 'alerta') ? 'alerta'
      : grupo.some(r => r.status === 'revisao') ? 'revisao'
        : grupo.every(r => r.status === 'cancelada') ? 'cancelada'
          : grupo.some(r => r.status === 'ocupado') ? 'ocupado' : 'reservado';
    const setores = [...new Set(grupo.map(r => K.setorInfo(K.boxInfo(r.box).setor).nome))].join(' · ');
    return `
      <article class="bilhete ${status === 'cancelada' ? 'bilhete-cancelado' : ''}">
        <div class="bilhete-principal">
          <div class="bilhete-topo">
            <div>
              <p class="rotulo">Reserva</p>
              <p class="bilhete-codigo">${K.esc(r0.codigo)}</p>
            </div>
            ${UI.pill(status)}
          </div>
          <div class="bilhete-pedras">
            ${grupo.map(r => `<span class="chip-pedra">${K.esc(r.box)}</span>`).join('')}
            <span class="bilhete-setor">${K.esc(setores)}</span>
          </div>
          <dl class="bilhete-dados">
            <div><dt>Data</dt><dd>${K.esc(K.rotuloDia(r0.data))}${K.diffDias(r0.data, K.hoje()) > 1 || K.diffDias(r0.data, K.hoje()) < -1 ? '' : ` <span class="sutil">${K.fmtData(r0.data)}</span>`}</dd></div>
            <div><dt>Produtor</dt><dd>${K.esc(r0.produtor)}</dd></div>
            <div><dt>Carga</dt><dd>${K.esc(r0.produto || '—')}${r0.qtd ? ` <span class="sutil">· ${K.esc(r0.qtd)}</span>` : ''}</dd></div>
            <div><dt>Pagamento</dt><dd>${K.fmtMoeda(total)} <span class="sutil">· ${K.esc(K.PAGAMENTOS[r0.pagamento] || '')}</span></dd></div>
            ${r0.checkinEm ? `<div><dt>Check-in</dt><dd>${K.fmtHora(r0.checkinEm)}</dd></div>` : ''}
          </dl>
          ${acoes ? `<div class="bilhete-acoes">${acoes}</div>` : ''}
        </div>
        ${qr ? `
        <div class="bilhete-canhoto">
          <div class="qr" data-qr="${K.esc(r0.codigo)}"></div>
          <p>Mostre na portaria</p>
        </div>` : ''}
      </article>`;
  };

  UI.montarQRs = raiz => {
    raiz.querySelectorAll('[data-qr]').forEach(el => UI.qr(el, `KALDIA|${el.dataset.qr}`, Number(el.dataset.tam) || 120));
  };

  UI.vazio = (icone, titulo, texto, acao = '') => `
    <div class="vazio">
      <div class="vazio-icone">${K.icon(icone, 28)}</div>
      <h3>${K.esc(titulo)}</h3>
      <p>${K.esc(texto)}</p>
      ${acao}
    </div>`;

  // ---------- Som curto de confirmação (só após um clique do usuário) ----------
  UI.chime = () => {
    try {
      const ctx = new (window.AudioContext || window.webkitAudioContext)();
      [523.25, 659.25, 783.99].forEach((f, i) => {
        const o = ctx.createOscillator();
        const g = ctx.createGain();
        o.type = 'sine';
        o.frequency.value = f;
        o.connect(g); g.connect(ctx.destination);
        const t = ctx.currentTime + i * 0.14;
        g.gain.setValueAtTime(0.0001, t);
        g.gain.exponentialRampToValueAtTime(0.18, t + 0.02);
        g.gain.exponentialRampToValueAtTime(0.0001, t + 0.35);
        o.start(t); o.stop(t + 0.4);
      });
    } catch (e) { /* sem áudio */ }
  };
})(window.K);
