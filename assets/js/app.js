/* Estrutura da página, navegação por #rota e tema claro/escuro */
'use strict';

(function (K) {
  const LOGO = window.KALDIA_LOGO || 'assets/img/kaldia-k.png';
  const ROTA_PADRAO = 'inicio';

  // ---------- Tema ----------
  // Fundo branco por padrão; o tema escuro só entra se a pessoa escolher no botão
  const TEMAS = ['claro', 'escuro'];
  let tema = 'claro';
  try { tema = localStorage.getItem('kaldia:tema') === 'escuro' ? 'escuro' : 'claro'; } catch (e) { /* sem armazenamento */ }
  const aplicarTema = () => {
    const raiz = document.documentElement;
    raiz.setAttribute('data-theme', tema === 'escuro' ? 'dark' : 'light');
  };
  const botaoTema = () => {
    const ic = tema === 'escuro' ? 'moon' : 'sun';
    const rot = tema === 'escuro' ? 'Tema: escuro' : 'Tema: claro';
    return `<button type="button" class="icon-btn" id="btn-tema" aria-label="${rot}. Trocar tema" title="${rot}">${K.icon(ic, 18)}</button>`;
  };
  aplicarTema();

  // ---------- Navegação ----------
  K.ir = hash => {
    if (location.hash === hash) K.render();
    else location.hash = hash;
  };

  const rotaAtual = () => {
    const nome = location.hash.replace(/^#/, '') || ROTA_PADRAO;
    return K.views[nome] ? nome : ROTA_PADRAO;
  };

  const NAV = {
    produtor: [['reservar', 'Nova reserva', 'grid'], ['minhas-reservas', 'Minhas reservas', 'ticket']],
    fiscal: [['painel', 'Painel do dia', 'grid'], ['relatorios', 'Relatórios', 'chart'], ['produtores', 'Produtores', 'users']]
  };

  const cabecalho = (area, rota) => {
    const p = K.store.logado();
    let nav = '';
    let direita = '';

    if (area === 'fiscal') {
      nav = NAV.fiscal.map(([r, t, i]) => `<a href="#${r}" class="nav-link" ${r === rota ? 'aria-current="page"' : ''}>${K.icon(i, 18)}<span>${t}</span></a>`).join('');
      direita = `<span class="selo-perfil">${K.icon('shield', 16)}<span>Fiscal de pátio</span></span>
        <a class="icon-btn" href="#inicio" aria-label="Sair da área do fiscal" title="Sair">${K.icon('logout', 18)}</a>`;
    } else if (area === 'produtor' && p) {
      nav = NAV.produtor.map(([r, t, i]) => `<a href="#${r}" class="nav-link" ${r === rota ? 'aria-current="page"' : ''}>${K.icon(i, 18)}<span>${t}</span></a>`).join('');
      direita = `<span class="selo-perfil"><span class="avatar avatar-sm">${K.esc(K.iniciais(p.nome))}</span><span>${K.esc(K.primeiroNome(p.nome))}</span></span>
        <button type="button" class="icon-btn" id="btn-sair" aria-label="Sair da conta" title="Sair">${K.icon('logout', 18)}</button>`;
    } else {
      nav = `<a href="#consulta" class="nav-link" ${rota === 'consulta' ? 'aria-current="page"' : ''}>${K.icon('search', 18)}<span>Consultar reserva</span></a>
        <a href="#painel" class="nav-link">${K.icon('shield', 18)}<span>Área do fiscal</span></a>`;
      direita = p
        ? `<a class="btn btn-primario btn-sm" href="#reservar">${K.icon('user', 16)}${K.esc(K.primeiroNome(p.nome))}</a>`
        : `<a class="btn btn-primario btn-sm" href="#entrar">${K.icon('login', 16)}Entrar</a>`;
    }

    return `
      <div class="topo-interno">
        <a class="marca" href="#inicio" aria-label="KALDIA, página inicial">
          <span class="marca-k"><img src="${LOGO}" alt="" width="28" height="28"></span>
          <span class="marca-nome">KALDIA</span>
        </a>
        <nav class="nav ${area === 'fiscal' ? 'nav-fiscal' : ''}" aria-label="Principal">${nav}</nav>
        <div class="topo-direita">${botaoTema()}${direita}</div>
      </div>`;
  };

  const rodape = () => `
    <div class="rodape-interno">
      <p><strong>KALDIA</strong> · gestão de espaços para centrais de abastecimento</p>
      <p class="sutil">Demonstração: os dados ficam salvos só neste navegador.
        <button type="button" class="link" id="btn-restaurar">Restaurar dados de exemplo</button>
        ${window.KALDIA_PREVIEW ? '' : '· <a class="link" href="v1/index.html">Ver versão anterior</a>'}</p>
    </div>`;

  K.render = () => {
    K.limparTimers();
    K.ui.fecharTudo();
    let rota = rotaAtual();
    let view = K.views[rota];

    if (view.guarda) {
      const destino = view.guarda();
      if (destino && destino !== '#' + rota) {
        history.replaceState(null, '', destino);
        rota = destino.slice(1);
        view = K.views[rota];
        if (view.guarda && view.guarda()) { // evita laços
          rota = ROTA_PADRAO;
          view = K.views[rota];
        }
      }
    }

    const area = view.area || 'publico';
    document.body.dataset.area = area;
    document.title = `${view.titulo} · KALDIA`;
    document.getElementById('topo').innerHTML = cabecalho(area, rota);
    document.getElementById('rodape').innerHTML = rodape();

    const main = document.getElementById('conteudo');
    main.innerHTML = `<div class="tela tela-${rota}">${view.render()}</div>`;
    if (view.mount) view.mount(main.firstElementChild);

    ligarCabecalho();
    if (K.rotaAnterior !== rota) {
      window.scrollTo(0, 0);
      const h1 = main.querySelector('h1');
      if (h1 && K.rotaAnterior) { h1.setAttribute('tabindex', '-1'); h1.focus({ preventScroll: true }); }
    }
    K.rotaAnterior = rota;
  };

  const ligarCabecalho = () => {
    const bt = document.getElementById('btn-tema');
    if (bt) bt.addEventListener('click', () => {
      tema = TEMAS[(TEMAS.indexOf(tema) + 1) % TEMAS.length];
      try { localStorage.setItem('kaldia:tema', tema); } catch (e) { /* ok */ }
      aplicarTema();
      bt.outerHTML = botaoTema();
      ligarCabecalho();
      document.getElementById('btn-tema').focus();
    });
    const sair = document.getElementById('btn-sair');
    if (sair) sair.addEventListener('click', () => {
      K.store.sair();
      K.rascunho = null;
      K.ui.toast('Você saiu da sua conta.');
      K.ir('#inicio');
    });
    const rest = document.getElementById('btn-restaurar');
    if (rest && !rest.dataset.ligado) {
      rest.dataset.ligado = '1';
      rest.addEventListener('click', async () => {
        const ok = await K.ui.confirmar({
          titulo: 'Restaurar dados de exemplo?',
          texto: 'Reservas, cadastros e campanhas criados neste navegador serão apagados e o pátio volta ao estado de demonstração.',
          ok: 'Restaurar',
          perigo: true
        });
        if (!ok) return;
        K.store.resetar();
        K.rascunho = null;
        K.ultimoPedido = null;
        K.dataFiscal = K.hoje();
        K.ui.toast('Dados de exemplo restaurados.');
        K.ir('#inicio');
      });
    }
  };

  window.addEventListener('hashchange', K.render);
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', K.render);
  else K.render();
})(window.K);
