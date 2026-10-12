/* Telas públicas: início e consulta de reserva */
'use strict';

(function (K) {
  const V = K.views = K.views || {};

  // ---------- Início ----------
  V.inicio = {
    area: 'publico',
    titulo: 'Início',
    render() {
      const hoje = K.hoje();
      const ativas = K.store.ativas(hoje);
      const lot = K.store.lotacao(hoje);
      const livres = K.BOXES.length - ativas.length;
      const noPatio = ativas.filter(r => r.status !== 'reservado').length;
      return `
        <section class="hero">
          <div class="hero-texto">
            <p class="eyebrow">CEASA · gestão de pedras e boxes</p>
            <h1>O pátio organizado antes do caminhão chegar.</h1>
            <p class="hero-sub">Produtores reservam a pedra pelo celular, pagam com PIX e entram pela portaria com um QR Code. A fiscalização acompanha cada box em tempo real.</p>
            <div class="hero-cta">
              <a class="btn btn-primario btn-lg" href="#entrar">${K.icon('sprout', 20)}Reservar uma pedra</a>
              <a class="btn btn-ghost btn-lg" href="#consulta">${K.icon('search', 20)}Consultar reserva</a>
            </div>
          </div>
          <figure class="hero-mapa">
            <figcaption>
              <span class="ao-vivo"><span class="ao-vivo-dot"></span>Pátio hoje</span>
              <span class="hero-mapa-num"><strong>${K.fmtPct(lot)}</strong> ocupado</span>
            </figcaption>
            ${K.patio.html({ data: hoje, modo: 'mini' })}
            <div class="hero-mapa-rodape">
              <span><strong>${livres}</strong> pedras livres</span>
              <span><strong>${noPatio}</strong> produtores no pátio</span>
              <span>Abertura <strong>04:00</strong></span>
            </div>
          </figure>
        </section>

        <section class="perfis" aria-label="Escolha seu acesso">
          <a class="perfil" href="#entrar">
            <span class="perfil-icone perfil-verde">${K.icon('truck', 24)}</span>
            <span class="perfil-txt">
              <strong>Sou produtor</strong>
              <span>Reserve até ${K.MAX_BOXES} pedras por dia, pague e faça check-in.</span>
            </span>
            ${K.icon('right', 18)}
          </a>
          <a class="perfil" href="#painel">
            <span class="perfil-icone perfil-azul">${K.icon('shield', 24)}</span>
            <span class="perfil-txt">
              <strong>Sou fiscal</strong>
              <span>Mapa do pátio, ocorrências, relatórios e base de produtores.</span>
            </span>
            ${K.icon('right', 18)}
          </a>
          <a class="perfil" href="#consulta">
            <span class="perfil-icone perfil-neutro">${K.icon('ticket', 24)}</span>
            <span class="perfil-txt">
              <strong>Consultar reserva</strong>
              <span>Busque por CPF, placa do veículo ou código KLD.</span>
            </span>
            ${K.icon('right', 18)}
          </a>
        </section>

        <section class="como" aria-labelledby="como-tit">
          <h2 id="como-tit">Como funciona a reserva</h2>
          <ol class="passos">
            <li><span class="passo-n">1</span><div><strong>Cadastro com CPF</strong><p>Informe seus dados, o veículo e quem pode descarregar. A verificação leva segundos.</p></div></li>
            <li><span class="passo-n">2</span><div><strong>Escolha da pedra</strong><p>Veja o mapa do dia, escolha até ${K.MAX_BOXES} pedras livres e pague por PIX ou cartão.</p></div></li>
            <li><span class="passo-n">3</span><div><strong>Check-in na chegada</strong><p>Mostre o QR Code na portaria e confirme o check-in. A pedra é sua até o fim do pregão.</p></div></li>
          </ol>
        </section>`;
    }
  };

  // ---------- Consulta ----------
  V.consulta = {
    area: 'publico',
    titulo: 'Consultar reserva',
    render() {
      const exemplo = K.store.ativas(K.hoje()).find(r => r.status === 'reservado');
      const sugestoes = [
        ['529.982.247-25', 'CPF'],
        exemplo ? [exemplo.codigo, 'Código'] : null,
        ['RTA4E12', 'Placa']
      ].filter(Boolean);
      return `
        <div class="estreito">
          <header class="cab-pagina">
            <p class="eyebrow">Portaria e produtores</p>
            <h1>Consultar reserva</h1>
            <p class="sub">Use o CPF do produtor, a placa do veículo ou o código que começa com KLD.</p>
          </header>
          <form class="busca-grande" id="form-consulta" novalidate>
            <label for="consulta-termo" class="sr-only">CPF, placa ou código</label>
            ${K.icon('search', 20)}
            <input id="consulta-termo" type="text" autocomplete="off" placeholder="000.000.000-00, ABC1D23 ou KLD-XXXXX">
            <button class="btn btn-primario" type="submit">Buscar</button>
          </form>
          <p class="sugestoes">Para testar:
            ${sugestoes.map(([v, t]) => `<button type="button" class="chip-sugestao" data-termo="${K.esc(v)}">${t} <code>${K.esc(v)}</code></button>`).join('')}
          </p>
          <div id="consulta-resultado" aria-live="polite"></div>
        </div>`;
    },
    mount(el) {
      const inp = el.querySelector('#consulta-termo');
      const res = el.querySelector('#consulta-resultado');

      const buscar = termo => {
        const t = termo.trim();
        if (!t) {
          res.innerHTML = '';
          inp.focus();
          return;
        }
        const dig = K.soDigitos(t);
        let grupos = [];
        let titulo = '';

        if (/^KLD/i.test(t) || (/^[A-Z0-9-]{5,9}$/i.test(t) && K.store.pedido(t).length)) {
          const g = K.store.pedido(t);
          if (g.length) grupos = [g];
          titulo = 'Reserva encontrada';
        } else if (dig.length === 11 && !/[A-Z]/i.test(t)) {
          if (!K.cpfValido(dig)) {
            res.innerHTML = UI_aviso('Esse CPF não é válido. Confira os 11 números.');
            return;
          }
          const p = K.store.produtorPorCPF(dig);
          if (p) grupos = K.store.pedidosDoProdutor(p.id);
          titulo = p ? `Reservas de ${p.nome}` : '';
        } else if (K.placaValida(t)) {
          const ps = K.store.produtoresPorPlaca(t);
          grupos = ps.flatMap(p => K.store.pedidosDoProdutor(p.id));
          titulo = ps.length ? `Veículo ${K.fmtPlaca(t)} · ${ps.map(p => p.nome).join(', ')}` : '';
        }

        const hoje = K.hoje();
        grupos = grupos
          .filter(g => g[0].data >= K.addDias(hoje, -7))
          .sort((a, b) => a[0].data.localeCompare(b[0].data));

        if (!grupos.length) {
          res.innerHTML = K.ui.vazio('search', 'Nenhuma reserva encontrada',
            'Confira se digitou certo. Reservas aparecem aqui até 7 dias depois da data de uso.');
          return;
        }
        res.innerHTML = `
          <h2 class="resultado-tit">${K.esc(titulo)}</h2>
          <div class="lista-bilhetes">${grupos.map(g => K.ui.bilhete(g)).join('')}</div>`;
        K.ui.montarQRs(res);
      };

      const UI_aviso = msg => `<div class="aviso aviso-erro">${K.icon('alert', 18)}<span>${K.esc(msg)}</span></div>`;

      el.querySelector('#form-consulta').addEventListener('submit', e => {
        e.preventDefault();
        buscar(inp.value);
      });
      el.querySelectorAll('[data-termo]').forEach(b => b.addEventListener('click', () => {
        inp.value = b.dataset.termo;
        buscar(inp.value);
      }));
    }
  };
})(window.K);
