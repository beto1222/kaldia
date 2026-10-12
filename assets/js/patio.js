/* Planta do pátio: pavilhões, ruas de circulação e pedras (boxes) */
'use strict';

(function (K) {
  const P = K.patio = {};

  P.legenda = (modo = 'fiscal') => {
    const itens = modo === 'produtor'
      ? [['livre', 'Livre'], ['selecionado', 'Sua escolha'], ['indisponivel', 'Indisponível']]
      : [['livre', 'Livre'], ['reservado', 'Aguardando chegada'], ['ocupado', 'No pátio'], ['alerta', 'Irregularidade'], ['revisao', 'Em revisão']];
    return `<ul class="legenda" aria-label="Legenda do mapa">${itens.map(([c, t]) =>
      `<li><span class="legenda-amostra pedra-${c}"></span>${t}</li>`).join('')}</ul>`;
  };

  const pedra = (box, data, modo, selecionados) => {
    const r = K.store.reservaDoBox(box.id, data);
    const cod = `<span class="pedra-cod">${box.id}</span>`;

    if (modo === 'mini') {
      const cls = r ? `pedra-${r.status}` : 'pedra-livre';
      return `<div class="pedra ${cls}" title="${box.id}${r ? ' · ' + K.esc(r.produtor) : ' · livre'}">${cod}</div>`;
    }

    if (modo === 'produtor') {
      if (r) {
        return `<button type="button" class="pedra pedra-indisponivel" disabled aria-label="${box.id} indisponível, ocupada com ${K.esc(r.produto)}">
          ${cod}<span class="pedra-info">${K.esc(r.produto.split(/ e |,/)[0])}</span></button>`;
      }
      const sel = selecionados.includes(box.id);
      return `<button type="button" class="pedra ${sel ? 'pedra-selecionado' : 'pedra-livre'}" data-box="${box.id}" aria-pressed="${sel}"
          aria-label="${box.id}, ${sel ? 'selecionada' : 'livre'}, ${K.fmtMoeda(box.preco)}">
          ${cod}<span class="pedra-info">${sel ? `${K.icon('check', 14)} Escolhida` : K.fmtMoeda(box.preco).replace(',00', '')}</span></button>`;
    }

    // fiscal
    if (!r) {
      return `<button type="button" class="pedra pedra-livre" data-box="${box.id}" aria-label="${box.id} livre">
        ${cod}<span class="pedra-info">Livre</span></button>`;
    }
    const alerta = r.status === 'alerta' || r.status === 'revisao';
    return `<button type="button" class="pedra pedra-${r.status}" data-box="${box.id}" data-reserva="${r.id}"
        aria-label="${box.id}, ${K.esc(K.STATUS[r.status].fiscal)}, ${K.esc(r.produtor)}">
        ${cod}${alerta ? `<span class="pedra-flag">${K.icon('alert', 13)}</span>` : ''}
        <span class="pedra-info">${K.esc(K.primeiroNome(r.produtor))}</span></button>`;
  };

  // Ruas entre pavilhões, como na planta real do mercado
  const RUAS = { A: 'Rua 1 · circulação de carga', B: 'Rua 2 · descarga e pedestres' };

  P.html = ({ data, modo = 'fiscal', selecionados = [] }) => {
    const blocos = K.SETORES.map(s => {
      const boxes = K.BOXES.filter(b => b.setor === s.id);
      const livres = boxes.filter(b => !K.store.reservaDoBox(b.id, data)).length;
      const cab = modo === 'mini'
        ? `<p class="pav-nome">${s.nome}</p>`
        : `<header class="pav-cab">
             <div><p class="pav-nome">${s.nome}</p><p class="pav-desc">${s.desc}</p></div>
             <p class="pav-meta"><strong>${livres}</strong> de ${boxes.length} livres${modo === 'produtor' ? ` · ${K.fmtMoeda(s.preco).replace(',00', '')}/dia` : ''}</p>
           </header>`;
      const rua = RUAS[s.id] ? `<div class="rua" aria-hidden="true"><span>${modo === 'mini' ? '' : RUAS[s.id]}</span></div>` : '';
      return `<section class="pav pav-${s.id}" aria-label="${s.nome}">
          ${cab}
          <div class="pav-grade">${boxes.map(b => pedra(b, data, modo, selecionados)).join('')}</div>
        </section>${rua}`;
    }).join('');

    return `<div class="planta planta-${modo}">
        ${blocos}
        <div class="portaria" aria-hidden="true">${K.icon('door', 15)}<span>Portaria · entrada de veículos</span></div>
      </div>`;
  };
})(window.K);
