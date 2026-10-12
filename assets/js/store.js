/* Dados do sistema: pátio, produtores, reservas e histórico.
   Tudo fica salvo no navegador (localStorage). Os registros de exemplo
   são renovados a cada dia para a demonstração sempre mostrar um pátio vivo. */
'use strict';

(function (K) {
  const STORAGE_KEY = 'kaldia:v2';

  // ---------- Estrutura física do pátio ----------
  K.SETORES = [
    { id: 'A', nome: 'Pavilhão A', desc: 'Folhosas e hortaliças', preco: 35, qtd: 8 },
    { id: 'B', nome: 'Pavilhão B', desc: 'Legumes e tubérculos', preco: 35, qtd: 8 },
    { id: 'C', nome: 'Galpão C', desc: 'Coberto · frutas', preco: 45, qtd: 6 }
  ];
  K.BOXES = K.SETORES.flatMap(s =>
    Array.from({ length: s.qtd }, (_, i) => ({
      id: `${s.id}-${String(i + 1).padStart(2, '0')}`,
      setor: s.id,
      preco: s.preco
    }))
  );
  K.boxInfo = id => K.BOXES.find(b => b.id === id);
  K.setorInfo = id => K.SETORES.find(s => s.id === id);
  K.MAX_BOXES = 3;

  K.STATUS = {
    reservado: { rotulo: 'Reservado', fiscal: 'Aguardando chegada', tom: 'amber' },
    ocupado: { rotulo: 'No pátio', fiscal: 'No pátio', tom: 'green' },
    alerta: { rotulo: 'Irregularidade', fiscal: 'Irregularidade', tom: 'red' },
    revisao: { rotulo: 'Em revisão', fiscal: 'Em revisão', tom: 'orange' },
    cancelada: { rotulo: 'Cancelada', fiscal: 'Cancelada', tom: 'muted' }
  };

  K.PAGAMENTOS = {
    pix: 'PIX',
    credito: 'Cartão de crédito',
    debito: 'Cartão de débito',
    balcao: 'Pago no balcão'
  };

  // ---------- Persistência ----------
  const ler = () => {
    try {
      const raw = localStorage.getItem(STORAGE_KEY);
      return raw ? JSON.parse(raw) : null;
    } catch (e) {
      return null;
    }
  };
  const gravar = estado => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(estado));
    } catch (e) {
      /* armazenamento indisponível: segue só em memória */
    }
  };

  // ---------- Dados de exemplo ----------
  const PRODUTORES_EXEMPLO = [
    { base: '529982247', nome: 'Antônio da Costa', produto: 'Milho verde', veiculo: 'Kombi', placa: 'KLD1A24', tel: '11987654321', ult: 9 },
    { base: '318426705', nome: 'Carlos Mendonça', produto: 'Alface e rúcula (hidroponia)', veiculo: 'Caminhão baú', placa: 'RTA4E12', tel: '11991234567', ult: 0 },
    { base: '204816337', nome: 'João Pedro Silva', produto: 'Tomate e pimentão', veiculo: 'Caminhão 3/4', placa: 'FJP2B88', tel: '11993456789', ult: 0 },
    { base: '771203554', nome: 'Maria Fernanda Lopes', produto: 'Morango', veiculo: 'Fiorino', placa: 'MFL3C41', tel: '11995551234', ult: 112 },
    { base: '602938117', nome: 'Roberto Alves', produto: 'Batata', veiculo: 'Caminhão toco', placa: 'RBA7H02', tel: '11996662345', ult: 0 },
    { base: '415867290', nome: 'Ana Costa', produto: 'Morango', veiculo: 'Saveiro', placa: 'ANC5J19', tel: '11997773456', ult: 140 },
    { base: '839105426', nome: 'Fazenda Sol Nascente', produto: 'Milho verde e abóbora', veiculo: 'Caminhão truck', placa: 'FSN8K77', tel: '11998884567', ult: 0 },
    { base: '127594863', nome: 'Cooperativa Vale', produto: 'Maçã e pera', veiculo: 'Carreta refrigerada', placa: 'CVL6M30', tel: '11990005678', ult: 0 },
    { base: '956031748', nome: 'Sítio das Frutas', produto: 'Morango e amora', veiculo: 'Strada', placa: 'SDF1N65', tel: '11981116789', ult: 95 },
    { base: '348271956', nome: 'Horta Familiar Nunes', produto: 'Alface e couve', veiculo: 'Kombi', placa: 'HFN2P48', tel: '11982227890', ult: 18 },
    { base: '683450219', nome: 'Eduardo Gomes', produto: 'Tomate', veiculo: 'Caminhão 3/4', placa: 'EDG4Q13', tel: '11983338901', ult: 0 },
    { base: '210987634', nome: 'Sítio Verde Vida', produto: 'Cenoura e beterraba', veiculo: 'Caminhão baú', placa: 'SVV9R26', tel: '11984449012', ult: 0 },
    { base: '574326018', nome: 'Irmãos Souza', produto: 'Laranja e limão', veiculo: 'Caminhão toco', placa: 'IRS3S90', tel: '11985550123', ult: 0 },
    { base: '890143572', nome: 'Fazenda Boa Vista', produto: 'Morango', veiculo: 'Caminhão baú', placa: 'FBV7T54', tel: '11986661234', ult: 76 },
    { base: '461759083', nome: 'Pedro Henrique Ramos', produto: 'Cebola e alho', veiculo: 'Caminhão 3/4', placa: 'PHR5U37', tel: '11987772345', ult: 0 },
    { base: '735618249', nome: 'Luiza Campos', produto: 'Abobrinha e berinjela', veiculo: 'Fiorino', placa: 'LZC8V81', tel: '11988883456', ult: 24 }
  ];

  // [nome do produtor, box, dias a partir de hoje, status, volume, ocorrência]
  const RESERVAS_EXEMPLO = [
    ['João Pedro Silva', 'A-01', 0, 'ocupado', '50 cx'],
    ['Carlos Mendonça', 'A-02', 0, 'reservado', '60 cx'],
    ['Carlos Mendonça', 'A-03', 0, 'reservado', '60 cx'],
    ['Sítio Verde Vida', 'A-05', 0, 'ocupado', '80 cx'],
    ['Eduardo Gomes', 'B-03', 0, 'alerta', '40 cx', 'Mercadoria avançou meio metro sobre o corredor de carga.'],
    ['Roberto Alves', 'B-04', 0, 'revisao', '120 sc', 'Volume declarado 120 sc; contagem do fiscal: 150 sc.'],
    ['Pedro Henrique Ramos', 'B-06', 0, 'reservado', '90 sc'],
    ['Cooperativa Vale', 'C-01', 0, 'ocupado', '200 cx'],
    ['Cooperativa Vale', 'C-02', 0, 'ocupado', '200 cx'],
    ['Fazenda Sol Nascente', 'C-04', 0, 'reservado', '300 un'],
    ['Carlos Mendonça', 'A-02', 1, 'reservado', '60 cx'],
    ['Cooperativa Vale', 'C-01', 1, 'reservado', '180 cx'],
    ['Irmãos Souza', 'C-03', 1, 'reservado', '70 cx'],
    ['João Pedro Silva', 'A-01', 2, 'reservado', '50 cx'],
    ['Fazenda Sol Nascente', 'B-01', 3, 'reservado', '250 un']
  ];

  const gerarHistorico = hoje => {
    const hist = [];
    for (let i = 29; i >= 1; i--) {
      const data = K.addDias(hoje, -i);
      const r = K.prng(`hist:${data}`);
      const dow = K.parseISO(data).getDay();
      // Terça e sexta são dias de mercado mais cheios; domingo é fraco
      const base = dow === 0 ? 0.42 : (dow === 2 || dow === 5) ? 0.82 : 0.66;
      const taxa = Math.min(0.98, Math.max(0.25, base + (r() - 0.5) * 0.22));
      const ocupados = Math.round(taxa * K.BOXES.length);
      const ocorrencias = Math.floor(r() * 4);
      const resolvidas = Math.min(ocorrencias, Math.round(ocorrencias * (0.55 + r() * 0.45)));
      hist.push({
        data,
        ocupados,
        total: K.BOXES.length,
        receita: ocupados * 36.8,
        ocorrencias,
        resolvidas,
        cancelamentos: r() > 0.6 ? 1 : 0
      });
    }
    return hist;
  };

  const montarExemplos = (hoje, produtoresExistentes) => {
    const rand = K.prng(`seed:${hoje}`);
    const produtores = produtoresExistentes || PRODUTORES_EXEMPLO.map((p, i) => ({
      id: `p${i + 1}`,
      cpf: K.cpfComDV(p.base),
      nome: p.nome,
      telefone: p.tel,
      email: '',
      produto: p.produto,
      veiculo: p.veiculo,
      placa: p.placa,
      pessoas: '',
      aprovado: true,
      exemplo: true,
      criadoEm: K.addDias(hoje, -200 + i * 7),
      ultimaLocacao: p.ult ? K.addDias(hoje, -p.ult) : K.addDias(hoje, -7),
      convidadoEm: null
    }));

    const porNome = nome => produtores.find(p => p.nome === nome);
    const pedidos = {};
    const reservas = RESERVAS_EXEMPLO.map(([nome, box, dias, status, qtd, obs]) => {
      const p = porNome(nome);
      const data = K.addDias(hoje, dias);
      const chave = `${nome}|${data}`;
      pedidos[chave] = pedidos[chave] || K.gerarCodigo(rand);
      const agora = new Date();
      const checkin = new Date(agora.getFullYear(), agora.getMonth(), agora.getDate(), 4, 5 + Math.floor(rand() * 50));
      return {
        id: K.uid() + box,
        codigo: pedidos[chave],
        produtorId: p ? p.id : null,
        produtor: nome,
        box,
        data,
        produto: p ? p.produto : '',
        qtd,
        valor: K.boxInfo(box).preco,
        pagamento: rand() > 0.35 ? 'pix' : 'credito',
        status,
        exemplo: true,
        criadoEm: Date.now() - Math.floor(rand() * 5) * 86400000,
        checkinEm: status === 'reservado' ? null : checkin.getTime(),
        ocorrencias: obs ? [{ texto: obs, em: checkin.getTime() + 40 * 60000, resolvidaEm: null }] : []
      };
    });
    return { produtores, reservas };
  };

  const estadoInicial = () => {
    const hoje = K.hoje();
    const { produtores, reservas } = montarExemplos(hoje);
    return {
      versao: 2,
      exemplosEm: hoje,
      produtores,
      reservas,
      historico: gerarHistorico(hoje),
      campanhas: [],
      sessao: { produtorId: null }
    };
  };

  // Renova os registros de exemplo quando o dia muda, preservando o que o usuário criou
  const renovarExemplos = estado => {
    const hoje = K.hoje();
    if (estado.exemplosEm === hoje) return estado;
    const doUsuario = estado.reservas.filter(r => !r.exemplo);
    const { reservas } = montarExemplos(hoje, estado.produtores);
    const livres = reservas.filter(r =>
      !doUsuario.some(u => u.box === r.box && u.data === r.data && u.status !== 'cancelada'));
    return {
      ...estado,
      exemplosEm: hoje,
      reservas: [...doUsuario, ...livres],
      historico: gerarHistorico(hoje)
    };
  };

  let estado = ler();
  estado = estado && estado.versao === 2 ? renovarExemplos(estado) : estadoInicial();
  gravar(estado);

  // ---------- API ----------
  const S = K.store = {
    get state() { return estado; },
    salvar() { gravar(estado); },
    resetar() { estado = estadoInicial(); gravar(estado); },

    // Produtores
    produtor(id) { return estado.produtores.find(p => p.id === id) || null; },
    produtorPorCPF(cpf) {
      const d = K.soDigitos(cpf);
      return estado.produtores.find(p => p.cpf === d) || null;
    },
    produtoresPorPlaca(placa) {
      const n = K.normPlaca(placa);
      return estado.produtores.filter(p => K.normPlaca(p.placa) === n);
    },
    logado() { return S.produtor(estado.sessao.produtorId); },
    entrar(id) { estado.sessao.produtorId = id; S.salvar(); },
    sair() { estado.sessao.produtorId = null; S.salvar(); },
    cadastrar(dados) {
      const p = {
        id: 'p' + K.uid(),
        aprovado: false,
        exemplo: false,
        criadoEm: K.hoje(),
        ultimaLocacao: null,
        convidadoEm: null,
        ...dados
      };
      estado.produtores.push(p);
      S.salvar();
      return p;
    },

    // Situação do produtor para o CRM
    situacao(p) {
      if (!p.aprovado) return 'pendente';
      const hoje = K.hoje();
      const temFutura = estado.reservas.some(r =>
        r.produtorId === p.id && r.data >= hoje && r.status !== 'cancelada');
      if (temFutura) return 'ativo_locado';
      const ref = p.ultimaLocacao || p.criadoEm;
      return ref && K.diffDias(hoje, ref) <= 45 ? 'ativo_sem_locacao' : 'inativo';
    },

    // Reservas
    ativas(data) {
      return estado.reservas.filter(r => r.data === data && r.status !== 'cancelada');
    },
    reservaDoBox(box, data) {
      return estado.reservas.find(r => r.box === box && r.data === data && r.status !== 'cancelada') || null;
    },
    reserva(id) { return estado.reservas.find(r => r.id === id) || null; },
    pedido(codigo) {
      const c = String(codigo || '').toUpperCase().replace(/\s/g, '');
      const norm = c.startsWith('KLD-') ? c : c.startsWith('KLD') ? `KLD-${c.slice(3)}` : `KLD-${c}`;
      return estado.reservas.filter(r => r.codigo === norm);
    },
    pedidosDoProdutor(id) {
      const grupos = {};
      estado.reservas.filter(r => r.produtorId === id).forEach(r => {
        (grupos[r.codigo] = grupos[r.codigo] || []).push(r);
      });
      return Object.values(grupos).sort((a, b) => a[0].data.localeCompare(b[0].data));
    },
    livres(data) {
      return K.BOXES.filter(b => !S.reservaDoBox(b.id, data));
    },
    lotacao(data) {
      return (S.ativas(data).length / K.BOXES.length) * 100;
    },

    criarPedido({ produtor, data, boxes, produto, qtd, pagamento }) {
      const conflito = boxes.find(b => S.reservaDoBox(b, data));
      if (conflito) return { erro: `A pedra ${conflito} acabou de ser reservada por outra pessoa. Escolha outra.` };
      const codigo = K.gerarCodigo();
      const criadas = boxes.map(box => ({
        id: K.uid() + box,
        codigo,
        produtorId: produtor.id,
        produtor: produtor.nome,
        box,
        data,
        produto,
        qtd,
        valor: K.boxInfo(box).preco,
        pagamento,
        status: 'reservado',
        exemplo: false,
        criadoEm: Date.now(),
        checkinEm: null,
        ocorrencias: []
      }));
      estado.reservas.push(...criadas);
      produtor.ultimaLocacao = data > (produtor.ultimaLocacao || '') ? data : produtor.ultimaLocacao;
      S.salvar();
      return { codigo, reservas: criadas };
    },

    locacaoBalcao({ nome, box, data, produto, qtd }) {
      if (S.reservaDoBox(box, data)) return { erro: `A pedra ${box} já está ocupada nesta data.` };
      const p = estado.produtores.find(x => x.nome.toLowerCase() === nome.toLowerCase());
      const r = {
        id: K.uid() + box,
        codigo: K.gerarCodigo(),
        produtorId: p ? p.id : null,
        produtor: p ? p.nome : nome,
        box,
        data,
        produto,
        qtd,
        valor: K.boxInfo(box).preco,
        pagamento: 'balcao',
        status: 'ocupado',
        exemplo: false,
        criadoEm: Date.now(),
        checkinEm: Date.now(),
        ocorrencias: []
      };
      estado.reservas.push(r);
      if (p) p.ultimaLocacao = data;
      S.salvar();
      return { reserva: r };
    },

    checkin(codigo) {
      S.pedido(codigo).forEach(r => {
        if (r.status === 'reservado') { r.status = 'ocupado'; r.checkinEm = Date.now(); }
      });
      S.salvar();
    },
    cancelarPedido(codigo) {
      S.pedido(codigo).forEach(r => { if (r.status === 'reservado') r.status = 'cancelada'; });
      S.salvar();
    },
    confirmarChegada(id) {
      const r = S.reserva(id);
      if (r) { r.status = 'ocupado'; r.checkinEm = Date.now(); S.salvar(); }
    },
    cancelarReserva(id) {
      const r = S.reserva(id);
      if (r) { r.status = 'cancelada'; S.salvar(); }
    },
    registrarOcorrencia(id, texto) {
      const r = S.reserva(id);
      if (!r) return;
      r.status = 'alerta';
      r.ocorrencias.push({ texto, em: Date.now(), resolvidaEm: null });
      S.salvar();
    },
    tratarOcorrencia(id, resolvida) {
      const r = S.reserva(id);
      if (!r) return;
      const aberta = [...r.ocorrencias].reverse().find(o => !o.resolvidaEm);
      if (resolvida) {
        if (aberta) aberta.resolvidaEm = Date.now();
        r.status = r.checkinEm ? 'ocupado' : 'reservado';
      } else {
        r.status = 'revisao';
      }
      S.salvar();
    },

    // Campanhas
    registrarCampanha(produto, ids) {
      const hoje = K.hoje();
      ids.forEach(id => { const p = S.produtor(id); if (p) p.convidadoEm = hoje; });
      estado.campanhas.unshift({ id: K.uid(), produto, total: ids.length, em: Date.now() });
      S.salvar();
    },
    convidar(id) {
      const p = S.produtor(id);
      if (p) { p.convidadoEm = K.hoje(); S.salvar(); }
    }
  };
})(window.K);
