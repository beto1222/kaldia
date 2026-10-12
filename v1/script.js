// ================= BANCOS DE DADOS SIMULADOS =================

let usuarioLogado = null;
let dadosAtuaisProdutor = {};
let verificacaoTimer = null;

// Tabela de Produtores Cadastrados
let bancoUsuarios = [
    { cpf: '123', nome: 'Antônio da Costa (Teste)', email: '', produto: 'Milho Verde', veiculo: 'Kombi', pessoas: '', aprovado: true }
];

// Tabela de Reservas Ativas
let reservas = [
    { id: 1, box: 'A-01', produtor: 'João da Silva', produto: 'Tomate', qtd: '50 cx', status: 'ocupado', obs: '' },
    { id: 2, box: 'A-02', produtor: 'Maria Fernandes', produto: 'Batata', qtd: '120 sc', status: 'reservado', obs: '' },
    { id: 3, box: 'B-01', produtor: 'Carlos Souza', produto: 'Folhagens', qtd: '30 cx', status: 'alerta', obs: 'Ocupou espaço a mais' }
];

const todosOsBoxes = ['A-01', 'A-02', 'A-03', 'A-04', 'A-05', 'B-01', 'B-02', 'B-03', 'B-04', 'B-05'];

// ================= NAVEGAÇÃO =================

let viewHistory = [];

// Exibe a tela especificada e oculta as demais, gerenciando o histórico de navegação
function showView(viewId, isBack = false) {
    if (verificacaoTimer) {
        clearTimeout(verificacaoTimer);
        verificacaoTimer = null;
    }

    if (viewId === 'view-home') {
        viewHistory = [];
    } else {
        let currentActive = document.querySelector('.view.active');
        if (currentActive && currentActive.id !== viewId && !isBack) {
            viewHistory.push(currentActive.id);
        }
    }

    document.querySelectorAll('.view').forEach(v => v.classList.remove('active'));
    document.getElementById(viewId).classList.add('active');
}

// Retorna para a tela anterior com base no histórico de navegação
function voltarPagina() {
    if (viewHistory.length > 0) {
        let prevId = viewHistory.pop();
        showView(prevId, true);
    } else {
        showView('view-home', true);
    }
}

// ================= AUTENTICAÇÃO E CADASTRO =================

// Abre a tela de autenticação do produtor, pré-preenchendo um CPF de teste
function abrirAutenticacao() {
    document.getElementById('auth-cpf').value = '09876543211';
    showView('view-prod-auth');
}

// Verifica o CPF informado no login e direciona para cadastro, aprovação ou painel do produtor
function verificarLogin() {
    let cpfInformado = document.getElementById('auth-cpf').value.trim();
    if (!cpfInformado) {
        alert("Por favor, digite o CPF.");
        return;
    }

    let user = bancoUsuarios.find(u => u.cpf === cpfInformado);
    if (user) {
        if (!user.aprovado) {
            usuarioLogado = user;
            preencherDadosAprovacao(user);
            showView('view-prod-aprovacao');
            iniciarSimulacaoVerificacao();
        } else {
            usuarioLogado = user;
            iniciarProdutor();
        }
    } else {
        // CPF não encontrado — vai direto para cadastro sem mensagem
        document.getElementById('cad-cpf').value = cpfInformado;
        showView('view-prod-cadastro');
    }
}

// Simula o processo de salvar um novo cadastro de produtor e vai para a fila de aprovação
function salvarNovoCadastro() {
    let nome = document.getElementById('cad-nome').value;
    let cpf = document.getElementById('cad-cpf').value;
    let email = document.getElementById('cad-email').value;
    let produto = document.getElementById('cad-produto').value;
    let veiculo = document.getElementById('cad-veiculo').value;
    let pessoas = document.getElementById('cad-pessoas').value;

    if (!nome || !cpf || !produto || !veiculo) {
        alert("Preencha todos os campos obrigatórios (*).");
        return;
    }

    // Cria o usuário com status pendente de aprovação
    let novoUser = { cpf, nome, email, produto, veiculo, pessoas, aprovado: false };
    bancoUsuarios.push(novoUser);

    // Guarda referência do usuário para aprovar depois
    usuarioLogado = novoUser;

    // Preenche os dados na tela de verificação
    preencherDadosAprovacao(novoUser);

    // Vai direto para tela de verificação do cadastro
    showView('view-prod-aprovacao');

    // Inicia simulação de verificação
    iniciarSimulacaoVerificacao();
}

// Preenche os dados na tela de aprovação com base nas informações do usuário
function preencherDadosAprovacao(user) {
    document.getElementById('aprovacao-nome').innerText = user.nome;
    document.getElementById('aprovacao-cpf').innerText = mascaraCPF(user.cpf);
    document.getElementById('aprovacao-produto').innerText = user.produto;
    document.getElementById('aprovacao-veiculo').innerText = user.veiculo;
}

// Aplica a máscara de formatação (XXX.XXX.XXX-XX) na exibição do CPF
function mascaraCPF(cpf) {
    if (cpf.length <= 3) return cpf + '.***.***-**';
    return cpf.substring(0, 3) + '.***.***-**';
}

// ================= SIMULAÇÃO DE VERIFICAÇÃO =================

// Inicia a barra de progresso simulando a verificação de documentos pela equipe
function iniciarSimulacaoVerificacao() {
    const progressBar = document.getElementById('aprovacao-progress');
    const progressText = document.getElementById('aprovacao-progress-text');

    // Reset de todas as etapas
    for (let i = 1; i <= 5; i++) {
        let etapa = document.getElementById('etapa-' + i);
        etapa.className = 'aprovacao-etapa';
        etapa.querySelector('.aprovacao-etapa-icon').innerText = i;
    }
    progressBar.style.width = '0%';

    // Etapa 1: Recebendo dados (0.3s → 1s)
    setTimeout(() => {
        document.getElementById('etapa-1').classList.add('ativa');
        progressBar.style.width = '10%';
        progressText.innerText = 'Recebendo dados cadastrais...';
    }, 200);

    setTimeout(() => {
        document.getElementById('etapa-1').classList.remove('ativa');
        document.getElementById('etapa-1').classList.add('concluida');
        document.getElementById('etapa-1').querySelector('.aprovacao-etapa-icon').innerText = '✓';
        progressBar.style.width = '25%';
    }, 1000);

    // Etapa 2: Verificando CPF (1s → 2s)
    setTimeout(() => {
        document.getElementById('etapa-2').classList.add('ativa');
        progressBar.style.width = '35%';
        progressText.innerText = 'Consultando base de dados...';
    }, 1100);

    setTimeout(() => {
        document.getElementById('etapa-2').classList.remove('ativa');
        document.getElementById('etapa-2').classList.add('concluida');
        document.getElementById('etapa-2').querySelector('.aprovacao-etapa-icon').innerText = '✓';
        progressBar.style.width = '50%';
    }, 2000);

    // Etapa 3: Validando veículo (2s → 3s)
    setTimeout(() => {
        document.getElementById('etapa-3').classList.add('ativa');
        progressBar.style.width = '60%';
        progressText.innerText = 'Validando informações do veículo...';
    }, 2100);

    setTimeout(() => {
        document.getElementById('etapa-3').classList.remove('ativa');
        document.getElementById('etapa-3').classList.add('concluida');
        document.getElementById('etapa-3').querySelector('.aprovacao-etapa-icon').innerText = '✓';
        progressBar.style.width = '75%';
    }, 3000);

    // Etapa 4: Confirmação Produtor Rural (3s → 4s)
    setTimeout(() => {
        document.getElementById('etapa-4').classList.add('ativa');
        progressBar.style.width = '75%';
        progressText.innerText = 'Confirmando registro de Produtor Rural...';
    }, 3100);

    setTimeout(() => {
        document.getElementById('etapa-4').classList.remove('ativa');
        document.getElementById('etapa-4').classList.add('concluida');
        document.getElementById('etapa-4').querySelector('.aprovacao-etapa-icon').innerText = '✓';
        progressBar.style.width = '85%';
    }, 4000);

    // Etapa 5: Aprovação final (4s → 5s)
    setTimeout(() => {
        document.getElementById('etapa-5').classList.add('ativa');
        progressBar.style.width = '92%';
        progressText.innerText = 'Finalizando aprovação...';
    }, 4100);

    verificacaoTimer = setTimeout(() => {
        document.getElementById('etapa-5').classList.remove('ativa');
        document.getElementById('etapa-5').classList.add('concluida');
        document.getElementById('etapa-5').querySelector('.aprovacao-etapa-icon').innerText = '✓';
        progressBar.style.width = '100%';
        progressText.innerText = 'Verificação concluída!';

        // Marca usuário como aprovado
        if (usuarioLogado) {
            usuarioLogado.aprovado = true;
        }

        // Toca som de notificação de sucesso
        tocarNotificacaoSucesso();

        // Redireciona para tela de aprovado após breve pausa
        setTimeout(() => {
            showView('view-prod-aprovado');
            iniciarCountdownRedirect();
        }, 500);
    }, 5000);
}

// ================= SOM DE NOTIFICAÇÃO =================

// Reproduz um som de notificação quando um processo é concluído com sucesso
function tocarNotificacaoSucesso() {
    try {
        const audioCtx = new (window.AudioContext || window.webkitAudioContext)();

        // Primeiro beep (Dó - C5)
        const osc1 = audioCtx.createOscillator();
        const gain1 = audioCtx.createGain();
        osc1.connect(gain1);
        gain1.connect(audioCtx.destination);
        osc1.frequency.value = 523.25;
        osc1.type = 'sine';
        gain1.gain.setValueAtTime(0.3, audioCtx.currentTime);
        gain1.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.3);
        osc1.start(audioCtx.currentTime);
        osc1.stop(audioCtx.currentTime + 0.3);

        // Segundo beep (Mi - E5)
        const osc2 = audioCtx.createOscillator();
        const gain2 = audioCtx.createGain();
        osc2.connect(gain2);
        gain2.connect(audioCtx.destination);
        osc2.frequency.value = 659.25;
        osc2.type = 'sine';
        gain2.gain.setValueAtTime(0.3, audioCtx.currentTime + 0.2);
        gain2.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.5);
        osc2.start(audioCtx.currentTime + 0.2);
        osc2.stop(audioCtx.currentTime + 0.5);

        // Terceiro beep (Sol - G5)
        const osc3 = audioCtx.createOscillator();
        const gain3 = audioCtx.createGain();
        osc3.connect(gain3);
        gain3.connect(audioCtx.destination);
        osc3.frequency.value = 783.99;
        osc3.type = 'sine';
        gain3.gain.setValueAtTime(0.3, audioCtx.currentTime + 0.4);
        gain3.gain.exponentialRampToValueAtTime(0.01, audioCtx.currentTime + 0.8);
        osc3.start(audioCtx.currentTime + 0.4);
        osc3.stop(audioCtx.currentTime + 0.8);
    } catch (e) {
        console.log('Audio não suportado:', e);
    }
}

// ================= COUNTDOWN E REDIRECT =================

// Inicia a contagem regressiva para redirecionar o usuário após a aprovação
function iniciarCountdownRedirect() {
    let segundos = 3;
    const timerEl = document.getElementById('countdown-timer');
    if (timerEl) timerEl.innerText = segundos;

    const interval = setInterval(() => {
        segundos--;
        if (timerEl) timerEl.innerText = segundos;

        if (segundos <= 0) {
            clearInterval(interval);
            iniciarProdutor();
        }
    }, 1000);
}

// ================= FUNÇÕES DO PRODUTOR =================

const MAX_BOXES = 3;
const PRECO_POR_BOX = 35;
let boxesSelecionados = [];

// Inicializa a interface principal do produtor (reserva de espaço)
function iniciarProdutor() {
    // Limpa seleções anteriores
    boxesSelecionados = [];
    atualizarMsgBoxSelecionado();

    // Preenche a tela de reserva com os dados do usuário logado
    document.getElementById('nome-produtor-logado').innerText = `👤 Olá, ${usuarioLogado.nome.split(' ')[0]}`;
    document.getElementById('prod-produto').value = usuarioLogado.produto;

    renderizarMapaProdutor();
    showView('view-prod-reserva');
}

// Atualiza o painel inferior mostrando os boxes selecionados e o valor total
function atualizarMsgBoxSelecionado() {
    const msg = document.getElementById('msg-box-selecionado');
    if (boxesSelecionados.length === 0) {
        msg.innerHTML = 'Nenhum box selecionado <span style="font-size:12px; opacity:0.7;">(máx. 3)</span>';
        msg.style.background = '#e2e8f0';
        msg.style.color = 'var(--primary-blue)';
    } else {
        const listaBoxes = boxesSelecionados.join(', ');
        const total = boxesSelecionados.length * PRECO_POR_BOX;
        const contador = `<span style="font-size:12px; opacity:0.85;">(${boxesSelecionados.length}/${MAX_BOXES})</span>`;
        const valorDisplay = `<span style="display:block; font-size:13px; margin-top:4px; opacity:0.9;">💰 Total: R$ ${total.toFixed(2).replace('.', ',')}</span>`;
        msg.innerHTML = `✅ Boxes Escolhidos: ${listaBoxes} ${contador}${valorDisplay}`;
        msg.style.background = 'var(--primary-green)';
        msg.style.color = 'white';
    }
}

// Desenha o mapa de ocupação do pátio para o produtor escolher os boxes
function renderizarMapaProdutor() {
    const mapa = document.getElementById('mapa-produtor');
    mapa.innerHTML = '';

    todosOsBoxes.forEach(nomeBox => {
        let reserva = reservas.find(r => r.box === nomeBox);
        let div = document.createElement('div');

        if (!reserva) {
            let estaSelecionado = boxesSelecionados.includes(nomeBox);
            div.className = estaSelecionado
                ? 'pedra selecionavel status-selecionado'
                : 'pedra selecionavel status-livre';
            let labelInfo = estaSelecionado ? '✓ Selecionado' : 'Livre';
            div.innerHTML = `<span>${nomeBox}</span><div class="produto-info">${labelInfo}</div>`;
            div.onclick = () => selecionarBox(nomeBox);
        } else {
            div.className = 'pedra nao-selecionavel status-reservado';
            div.title = `Ocupado com: ${reserva.produto}`;
            div.innerHTML = `<span>${nomeBox}</span><div class="produto-info" title="${reserva.produto}">${reserva.produto}</div>`;
        }
        mapa.appendChild(div);
    });
}

// Alterna a seleção de um box (até o limite de 3) pelo produtor
function selecionarBox(nomeBox) {
    let idx = boxesSelecionados.indexOf(nomeBox);
    if (idx !== -1) {
        // Já selecionado → desselecionar
        boxesSelecionados.splice(idx, 1);
    } else {
        // Verificar limite
        if (boxesSelecionados.length >= MAX_BOXES) {
            alert(`Você pode selecionar no máximo ${MAX_BOXES} boxes!\nDesmarque um box antes de escolher outro.`);
            return;
        }
        boxesSelecionados.push(nomeBox);
    }
    atualizarMsgBoxSelecionado();
    renderizarMapaProdutor();
}

// Salva a reserva dos boxes selecionados no banco de dados simulado
function confirmarReserva() {
    if (boxesSelecionados.length === 0) {
        alert("Por favor, selecione ao menos um Box disponível no mapa.");
        return;
    }

    dadosAtuaisProdutor = {
        data: document.getElementById('prod-data').value,
        produto: document.getElementById('prod-produto').value || 'Não informado',
        qtd: document.getElementById('prod-qtd').value || 'Não informado',
        boxes: [...boxesSelecionados]
    };

    const listaBoxes = dadosAtuaisProdutor.boxes.join(', ');
    const totalPagar = dadosAtuaisProdutor.boxes.length * PRECO_POR_BOX;
    document.getElementById('pagamento-box').innerText = "Box(es) " + listaBoxes;
    document.getElementById('pagamento-valor').innerHTML = `Valor: R$ ${totalPagar.toFixed(2).replace('.', ',')} <span style="font-size:14px; font-weight:normal; display:block; margin-top:5px;">(${dadosAtuaisProdutor.boxes.length} box(es) × R$ ${PRECO_POR_BOX.toFixed(2).replace('.', ',')})</span>`;
    showView('view-prod-pagamento');
}

// Simula a tela e o processamento do pagamento (Pix)
function realizarPagamento() {
    // Para o MVP, aceitar apenas PIX
    let metodoSelecionado = document.querySelector('input[name="metodo-pagamento"]:checked');
    if(metodoSelecionado && metodoSelecionado.value !== 'pix') {
        alert("Para fins de Demonstração (MVP), por favor selecione a opção PIX.");
        return;
    }

    // Cria uma reserva para cada box selecionado
    dadosAtuaisProdutor.boxes.forEach(box => {
        reservas.push({
            id: Date.now() + Math.random(),
            box: box,
            produtor: usuarioLogado.nome,
            produto: dadosAtuaisProdutor.produto,
            qtd: dadosAtuaisProdutor.qtd,
            status: 'reservado',
            obs: ''
        });
    });

    const listaBoxes = dadosAtuaisProdutor.boxes.join(', ');
    document.getElementById('status-box').innerText = listaBoxes;
    document.getElementById('status-data').innerText = dadosAtuaisProdutor.data;
    showView('view-prod-status');
}

// Realiza o check-in do produtor alterando o status da reserva
function fazerCheckin() {
    const listaBoxes = dadosAtuaisProdutor.boxes.join(', ');
    document.getElementById('resumo-nome').innerText = usuarioLogado.nome;
    document.getElementById('resumo-box').innerText = listaBoxes;
    document.getElementById('resumo-produto').innerText = dadosAtuaisProdutor.produto;
    document.getElementById('resumo-qtd').innerText = dadosAtuaisProdutor.qtd;

    // Marca todos os boxes como ocupados
    dadosAtuaisProdutor.boxes.forEach(box => {
        let res = reservas.find(r => r.box === box && r.produtor === usuarioLogado.nome);
        if (res) res.status = 'ocupado';
    });

    showView('view-prod-resumo');
}

// ================= FUNÇÕES DO FISCAL =================

// Inicializa a interface principal do Fiscal (Dashboard gerencial)
function iniciarFiscal() {
    renderizarDashboardFiscal();
    showView('view-fiscal-dash');
}

// Desenha a tabela principal de agendamentos e status no painel do fiscal
function renderizarDashboardFiscal() {
    const mapa = document.getElementById('mapa-patio-fiscal');
    mapa.innerHTML = '';

    let ocupadosCount = 0;

    todosOsBoxes.forEach(nomeBox => {
        let r = reservas.find(res => res.box === nomeBox);
        let div = document.createElement('div');

        if (!r) {
            div.className = 'pedra status-livre';
            div.innerHTML = `<span>${nomeBox}</span>`;
        } else {
            ocupadosCount++;
            div.className = `pedra status-${r.status}`;
            div.title = `${r.produtor} - ${r.produto}`;
            let nomeCurto = r.produtor.split(' ')[0];
            div.innerHTML = `<span>${nomeBox}</span><div class="produto-info">${nomeCurto}</div>`;
        }
        mapa.appendChild(div);
    });

    let percentual = Math.round((ocupadosCount / todosOsBoxes.length) * 100);
    const spanPercentual = document.getElementById('percentual-lotacao');
    if (spanPercentual) {
        spanPercentual.innerText = `${percentual}% Lotado`;
        if (percentual >= 80) {
            spanPercentual.style.background = '#FC8181'; // Vermelho
            spanPercentual.style.color = 'white';
            spanPercentual.style.borderColor = '#E53E3E';
        } else if (percentual >= 50) {
            spanPercentual.style.background = '#F6E05E'; // Amarelo
            spanPercentual.style.color = '#744210';
            spanPercentual.style.borderColor = '#D69E2E';
        } else {
            spanPercentual.style.background = '#C6F6D5'; // Verde
            spanPercentual.style.color = '#22543D';
            spanPercentual.style.borderColor = '#38A169';
        }
    }

    const tbody = document.getElementById('tabela-agendamentos');
    tbody.innerHTML = '';

    const filtroSelect = document.getElementById('filtro-status');
    const filtroStatus = filtroSelect ? filtroSelect.value : 'todos';

    reservas.forEach(r => {
        if (filtroStatus !== 'todos' && r.status !== filtroStatus) return;

        let tr = document.createElement('tr');
        let corStatus = r.status === 'livre' ? '#666' :
            r.status === 'reservado' ? '#D69E2E' :
                r.status === 'ocupado' ? 'var(--primary-green)' :
                    r.status === 'revisao' ? 'var(--box-revisao)' : 'var(--box-alert)';

        let textoStatus = r.status.toUpperCase();
        if (r.status === 'alerta' || r.status === 'revisao') textoStatus += ` <br><small>(${r.obs})</small>`;
        if (r.status === 'revisao') textoStatus = `EM REVISÃO <br><small>(${r.obs})</small>`;

        tr.innerHTML = `
            <td><strong>${r.box}</strong></td>
            <td>${r.produtor}</td>
            <td>${r.produto}</td>
            <td>${r.qtd}</td>
            <td style="color: ${corStatus}; font-weight: bold;">${textoStatus}</td>
            <td>
                ${r.status === 'reservado' ? `<button class="btn-sm btn-blue" onclick="marcarOcupado(${r.id})">✔ Confirmar</button>` : ''}
                ${(r.status === 'alerta' || r.status === 'revisao') ? `<button class="btn-sm btn-blue" onclick="abrirModalTratar(${r.id})">🛠 Tratar</button>` : `<button class="btn-sm btn-warning" onclick="abrirModalIrregularidade(${r.id})">⚠️ Ocorrência</button>`}
                <button class="btn-sm btn-danger" onclick="excluirReserva(${r.id})">🗑 Excluir</button>
            </td>
        `;
        tbody.appendChild(tr);
    });
}

// Altera o status de uma reserva para 'ocupado' (quando o produtor chega no local)
function marcarOcupado(id) {
    let r = reservas.find(res => res.id === id);
    if (r) { r.status = 'ocupado'; renderizarDashboardFiscal(); }
}

// Remove uma reserva da lista de agendamentos
function excluirReserva(id) {
    if (confirm("Deseja realmente cancelar e liberar este espaço?")) {
        reservas = reservas.filter(res => res.id !== id);
        renderizarDashboardFiscal();
    }
}

// Fecha todos os modais/janelas sobrepostas que estejam abertos
function fecharModais() {
    document.querySelectorAll('.modal-overlay').forEach(m => m.classList.remove('active'));
}

// Abre a janela para relatar um problema/irregularidade em um box específico
function abrirModalIrregularidade(id) {
    document.getElementById('irreg-box-id').value = id;
    document.getElementById('irreg-obs').value = '';
    document.getElementById('modal-irregular').classList.add('active');
}

// Salva a irregularidade reportada, mudando o status da reserva para 'alerta'
function salvarIrregularidade() {
    let id = parseInt(document.getElementById('irreg-box-id').value);
    let obs = document.getElementById('irreg-obs').value;
    let r = reservas.find(res => res.id === id);
    if (r) {
        r.status = 'alerta';
        r.obs = obs;
    }
    fecharModais();
    renderizarDashboardFiscal();
}

// Abre a janela para o fiscal adicionar uma locação manual
function abrirModalNovo() {
    document.getElementById('novo-nome').value = '';
    document.getElementById('novo-box').value = '';
    document.getElementById('novo-produto').value = '';
    document.getElementById('modal-novo').classList.add('active');
}

// Salva uma nova reserva criada manualmente pelo fiscal no sistema
function salvarNovoManual() {
    let box = document.getElementById('novo-box').value.toUpperCase();
    if (reservas.find(r => r.box === box)) {
        alert("Este box já está locado!");
        return;
    }

    if (!todosOsBoxes.includes(box) && box !== '') {
        todosOsBoxes.push(box);
    }

    reservas.push({
        id: Date.now(),
        box: box,
        produtor: document.getElementById('novo-nome').value || 'Anônimo',
        produto: document.getElementById('novo-produto').value || 'N/I',
        qtd: 'N/I',
        status: 'ocupado',
        obs: 'Locação Feita no Balcão'
    });

    fecharModais();
    renderizarDashboardFiscal();
}

// Abre a janela para o fiscal visualizar e tratar uma ocorrência/alerta
function abrirModalTratar(id) {
    document.getElementById('tratar-box-id').value = id;
    document.getElementById('modal-tratar-alerta').classList.add('active');
}

// Resolve um alerta, retornando o status do box para 'ocupado'
function resolverOcorrencia() {
    let id = parseInt(document.getElementById('tratar-box-id').value);
    let r = reservas.find(res => res.id === id);
    if (r) {
        r.status = 'ocupado';
        r.obs = '';
    }
    fecharModais();
    renderizarDashboardFiscal();
}

// Coloca a ocorrência em status de 'revisão' (aguardando análise)
function revisarOcorrencia() {
    let id = parseInt(document.getElementById('tratar-box-id').value);
    let r = reservas.find(res => res.id === id);
    if (r) {
        r.status = 'revisao';
    }
    fecharModais();
    renderizarDashboardFiscal();
}

// Calcula métricas simuladas e exibe a tela de relatórios gerenciais
function abrirRelatorios() {
    // Definindo a lotação média desejada de ~68%
    let percentualLotacao = 68;

    document.getElementById('rep-lotacao').innerText = percentualLotacao + '%';

    // Gerar gráfico de 30 dias variando em torno de 68%
    const chartContainer = document.getElementById('grafico-lotacao');
    if (chartContainer) {
        chartContainer.innerHTML = '';
        for (let i = 0; i < 30; i++) {
            // Variação de +- 15% (53% a 83%)
            let val = Math.floor(Math.random() * 30) - 15 + 68;
            let bar = document.createElement('div');
            bar.className = 'chart-bar';
            bar.style.height = val + '%';
            bar.setAttribute('data-val', val);
            chartContainer.appendChild(bar);
        }
    }

    let cancelamentos = Math.floor(Math.random() * 5) + 2;
    let alertas = Math.floor(Math.random() * 10) + 5;
    let resolvidas = Math.floor(alertas * (Math.random() * 0.4 + 0.5));
    let eficiencia = Math.round((resolvidas / alertas) * 100);

    document.getElementById('rep-cancelamentos').innerText = cancelamentos;
    document.getElementById('rep-alertas').innerText = alertas;
    document.getElementById('rep-resolvidas').innerText = resolvidas;

    document.getElementById('rep-barra-eficiencia').style.width = eficiencia + '%';
    document.getElementById('rep-texto-eficiencia').innerText = eficiencia + '% de Eficiência';

    if (eficiencia >= 80) {
        document.getElementById('rep-barra-eficiencia').style.background = 'var(--primary-green)';
        document.getElementById('rep-texto-eficiencia').style.color = 'var(--primary-green)';
    } else if (eficiencia >= 50) {
        document.getElementById('rep-barra-eficiencia').style.background = '#DD6B20';
        document.getElementById('rep-texto-eficiencia').style.color = '#DD6B20';
    } else {
        document.getElementById('rep-barra-eficiencia').style.background = '#E53E3E';
        document.getElementById('rep-texto-eficiencia').style.color = '#E53E3E';
    }

    showView('view-fiscal-relatorios');
}

// =================== GESTÃO DE PRODUTORES (CRM) ===================

let baseProdutores = [
    { id: 1, nome: "Carlos Mendonça", produto: "Alface e Rúcula", status: "ativo_locado" },
    { id: 2, nome: "João Pedro Silva", produto: "Tomate e Pimentão", status: "ativo_locado" },
    { id: 3, nome: "Maria Fernanda", produto: "Morango", status: "inativo" },
    { id: 4, nome: "Roberto Alves", produto: "Batata", status: "ativo_sem_locacao" },
    { id: 5, nome: "Ana Costa", produto: "Morango", status: "inativo" },
    { id: 6, nome: "Fazenda Sol Nascente", produto: "Milho e Soja", status: "inativo" },
    { id: 7, nome: "Cooperativa Vale", produto: "Maçã e Pera", status: "ativo_locado" },
    { id: 8, nome: "Sítio das Frutas", produto: "Morango e Amora", status: "inativo" },
    { id: 9, nome: "Horta Familiar Nunes", produto: "Alface e Couve", status: "inativo" },
    { id: 10, nome: "Eduardo Gomes", produto: "Tomate", status: "ativo_sem_locacao" },
    { id: 11, nome: "Sítio Verde Vida", produto: "Cenoura e Beterraba", status: "ativo_locado" },
    { id: 12, nome: "Irmãos Souza", produto: "Laranja e Limão", status: "inativo" },
    { id: 13, nome: "Fazenda Boa Vista", produto: "Morango", status: "inativo" },
    { id: 14, nome: "Pedro Henrique", produto: "Cebola e Alho", status: "ativo_sem_locacao" },
    { id: 15, nome: "Luiza Campos", produto: "Abobrinha e Berinjela", status: "inativo" }
];

// Abre a área de CRM/Gestão da base de produtores cadastrados
function abrirGestaoProdutores() {
    renderizarTabelaProdutores();
    showView('view-fiscal-produtores');
}

// Filtra e desenha a lista de produtores e seus status no CRM
function renderizarTabelaProdutores() {
    const tbody = document.getElementById('tabela-produtores-crm');
    if (!tbody) return;
    tbody.innerHTML = '';

    let filtro = document.getElementById('filtro-crm') ? document.getElementById('filtro-crm').value : 'todos';

    baseProdutores.forEach(p => {
        if (filtro !== 'todos' && p.status !== filtro) return;

        let tr = document.createElement('tr');

        let statusBadge = '';
        if (p.status === 'ativo_locado') {
            statusBadge = `<span class="badge-ativo-locado">Ativo (Com Locação)</span>`;
        } else if (p.status === 'ativo_sem_locacao') {
            statusBadge = `<span class="badge-ativo-sem-locacao">Ativo (Sem Locação)</span>`;
        } else {
            statusBadge = `<span class="badge-inativo">Inativo</span>`;
        }

        let acoes = '';
        if (p.status === 'inativo' || p.status === 'ativo_sem_locacao') {
            acoes = `<button class="btn-sm" style="background-color: var(--primary-blue);" onclick="dispararConviteIndividual('${p.nome}')">📩 Enviar Convite</button>`;
        } else {
            acoes = `<span style="font-size: 12px; color: #666;">-</span>`;
        }

        tr.innerHTML = `
            <td><strong>${p.nome}</strong></td>
            <td>${p.produto}</td>
            <td>${statusBadge}</td>
            <td>${acoes}</td>
        `;
        tbody.appendChild(tr);
    });
}

// Simula o envio de um convite individual (WhatsApp) para um produtor inativo
function dispararConviteIndividual(nome) {
    alert(`Mensagem via WhatsApp enviada com sucesso para ${nome} convidando para reservar um espaço na CEASA.`);
}

// Dispara uma campanha simulada para um segmento específico de produtores
function dispararSazonal() {
    let produto = document.getElementById('crm-sazonal-produto').value.toLowerCase();
    if (!produto) {
        alert("Digite um produto para a campanha sazonal.");
        return;
    }

    let produtoresAlvo = baseProdutores.filter(p => p.produto.toLowerCase().includes(produto) && p.status !== 'ativo_locado');

    if (produtoresAlvo.length > 0) {
        let confirmar = confirm(`Deseja enviar essa notificação para todos os produtores de ${produto} sem locação ativa?`);
        if (confirmar) {
            alert(`🚀 Campanha Sazonal Disparada!\n\nForam enviados convites automáticos via WhatsApp e SMS para ${produtoresAlvo.length} produtor(es) de "${produto}".\n\nIsso ajuda a preencher os espaços ociosos baseados na safra atual!`);
        }
    } else {
        alert(`Nenhum produtor sem locação de "${produto}" encontrado na base de dados para receber o convite.`);
    }
}

// =================== CONSULTA DE RESERVA ===================

// Abre a tela pública para consultar o status de uma reserva (QR Code)
function abrirConsultaReserva() {
    document.getElementById('consulta-input').value = '';
    document.getElementById('resultado-consulta').style.display = 'none';
    showView('view-consulta-reserva');
}

// Procura e exibe os detalhes de uma reserva com base no CPF/Placa/Código informados
function buscarReserva() {
    let input = document.getElementById('consulta-input').value.trim();
    if (!input) {
        alert("Por favor, informe o CPF, Placa do veículo ou Código da reserva.");
        return;
    }

    let btn = document.getElementById('btn-buscar-reserva');
    let originalText = btn.innerText;
    btn.innerText = 'Buscando...';
    btn.disabled = true;
    btn.style.opacity = '0.7';

    setTimeout(() => {
        btn.innerText = originalText;
        btn.disabled = false;
        btn.style.opacity = '1';

        let user = bancoUsuarios.find(u => u.cpf === input || u.nome.toLowerCase() === input.toLowerCase());

        if (!user) {
            alert("Nenhuma reserva encontrada para os dados informados.");
            document.getElementById('resultado-consulta').style.display = 'none';
            return;
        }

        let nome = user.nome;

        // Gerar dados determinísticos para a reserva para não mudarem a cada clique
        let mockBoxes = ['A-01', 'A-02', 'B-04', 'C-10', 'D-15'];
        let box = mockBoxes[input.length % mockBoxes.length];

        let mockValores = ['35,00', '70,00', '105,00'];
        let valor = mockValores[input.length % mockValores.length];

        // Gerar uma data futura determinística (2 a 8 dias para frente) para evitar conflitos visuais
        let dataFutura = new Date();
        dataFutura.setDate(dataFutura.getDate() + (input.length % 7) + 2);
        let dataFormatada = dataFutura.toLocaleDateString('pt-BR');

        let codigoAleatorio = Math.floor(Math.random() * 900) + 100;

        document.getElementById('consulta-nome').innerText = nome;
        document.getElementById('consulta-data').innerText = dataFormatada;
        document.getElementById('consulta-box').innerText = box;
        document.getElementById('consulta-valor').innerText = valor;
        document.getElementById('consulta-codigo').innerText = box.replace('-', '') + '-' + codigoAleatorio;

        document.getElementById('resultado-consulta').style.display = 'block';
    }, 1000);
}
