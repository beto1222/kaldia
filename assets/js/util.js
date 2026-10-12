/* Utilitários gerais: formatação, validação, datas, ícones e PIX */
'use strict';

window.K = window.K || {};

(function (K) {
  const pad = n => String(n).padStart(2, '0');

  // ---------- Texto seguro ----------
  K.esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({
    '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;'
  }[c]));

  K.primeiroNome = nome => String(nome || '').trim().split(/\s+/)[0] || '';

  K.iniciais = nome => String(nome || '')
    .trim().split(/\s+/).filter(Boolean).slice(0, 2)
    .map(p => p[0].toUpperCase()).join('');

  // ---------- Datas (sempre em horário local, formato AAAA-MM-DD) ----------
  K.isoDate = d => `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
  K.hoje = () => K.isoDate(new Date());
  K.parseISO = iso => {
    const [y, m, d] = String(iso).split('-').map(Number);
    return new Date(y, m - 1, d);
  };
  K.addDias = (iso, n) => {
    const d = K.parseISO(iso);
    d.setDate(d.getDate() + n);
    return K.isoDate(d);
  };
  K.diffDias = (a, b) => Math.round((K.parseISO(a) - K.parseISO(b)) / 86400000);
  K.fmtData = iso => iso ? K.parseISO(iso).toLocaleDateString('pt-BR') : '—';
  K.fmtDataCurta = iso => K.parseISO(iso).toLocaleDateString('pt-BR', { day: '2-digit', month: '2-digit' });
  K.fmtDataLonga = iso => {
    const s = K.parseISO(iso).toLocaleDateString('pt-BR', { weekday: 'long', day: 'numeric', month: 'long' });
    return s.charAt(0).toUpperCase() + s.slice(1);
  };
  K.diaSemana = iso => K.parseISO(iso).toLocaleDateString('pt-BR', { weekday: 'short' }).replace('.', '');
  K.fmtHora = ts => ts ? new Date(ts).toLocaleTimeString('pt-BR', { hour: '2-digit', minute: '2-digit' }) : '—';
  K.rotuloDia = iso => {
    const d = K.diffDias(iso, K.hoje());
    if (d === 0) return 'Hoje';
    if (d === 1) return 'Amanhã';
    if (d === -1) return 'Ontem';
    return K.fmtDataLonga(iso);
  };

  // ---------- Números ----------
  K.fmtMoeda = v => Number(v || 0).toLocaleString('pt-BR', { style: 'currency', currency: 'BRL' });
  K.fmtPct = v => `${Math.round(v)}%`;

  // ---------- Documentos ----------
  K.soDigitos = s => String(s || '').replace(/\D/g, '');

  K.cpfComDV = base9 => {
    const n = K.soDigitos(base9).slice(0, 9).split('').map(Number);
    const dv = len => {
      let soma = 0;
      for (let i = 0; i < len; i++) soma += n[i] * (len + 1 - i);
      const r = (soma * 10) % 11;
      return r === 10 ? 0 : r;
    };
    n.push(dv(9));
    n.push(dv(10));
    return n.join('');
  };

  K.cpfValido = cpf => {
    const d = K.soDigitos(cpf);
    if (d.length !== 11 || /^(\d)\1{10}$/.test(d)) return false;
    return K.cpfComDV(d.slice(0, 9)) === d;
  };

  K.fmtCPF = cpf => {
    const d = K.soDigitos(cpf).slice(0, 11);
    return d
      .replace(/^(\d{3})(\d)/, '$1.$2')
      .replace(/^(\d{3})\.(\d{3})(\d)/, '$1.$2.$3')
      .replace(/\.(\d{3})(\d{1,2})$/, '.$1-$2');
  };

  K.cpfOculto = cpf => {
    const d = K.soDigitos(cpf);
    return d.length === 11 ? `${d.slice(0, 3)}.***.***-${d.slice(9)}` : '***';
  };

  K.fmtTelefone = tel => {
    const d = K.soDigitos(tel).slice(0, 11);
    if (d.length <= 2) return d ? `(${d}` : '';
    if (d.length <= 6) return `(${d.slice(0, 2)}) ${d.slice(2)}`;
    if (d.length <= 10) return `(${d.slice(0, 2)}) ${d.slice(2, 6)}-${d.slice(6)}`;
    return `(${d.slice(0, 2)}) ${d.slice(2, 7)}-${d.slice(7)}`;
  };

  // Placa antiga (ABC-1234) ou Mercosul (ABC1D23)
  K.normPlaca = p => String(p || '').toUpperCase().replace(/[^A-Z0-9]/g, '').slice(0, 7);
  K.placaValida = p => /^[A-Z]{3}\d[A-Z0-9]\d{2}$/.test(K.normPlaca(p));
  K.fmtPlaca = p => {
    const n = K.normPlaca(p);
    return /^[A-Z]{3}\d{4}$/.test(n) ? `${n.slice(0, 3)}-${n.slice(3)}` : n;
  };

  // ---------- Identificadores ----------
  const ALFABETO = 'ABCDEFGHJKMNPQRSTUVWXYZ23456789';
  K.gerarCodigo = (rand = Math.random) => {
    let s = '';
    for (let i = 0; i < 5; i++) s += ALFABETO[Math.floor(rand() * ALFABETO.length)];
    return `KLD-${s}`;
  };
  K.uid = () => Date.now().toString(36) + Math.random().toString(36).slice(2, 8);

  // PRNG determinístico (para dados de exemplo estáveis)
  K.prng = seedStr => {
    let h = 1779033703 ^ String(seedStr).length;
    for (let i = 0; i < seedStr.length; i++) {
      h = Math.imul(h ^ seedStr.charCodeAt(i), 3432918353);
      h = (h << 13) | (h >>> 19);
    }
    let a = h >>> 0;
    return () => {
      a |= 0; a = (a + 0x6D2B79F5) | 0;
      let t = Math.imul(a ^ (a >>> 15), 1 | a);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
  };

  // ---------- PIX (BR Code "copia e cola" com CRC16 real) ----------
  const crc16 = str => {
    let crc = 0xFFFF;
    for (let i = 0; i < str.length; i++) {
      crc ^= str.charCodeAt(i) << 8;
      for (let j = 0; j < 8; j++) {
        crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
        crc &= 0xFFFF;
      }
    }
    return crc.toString(16).toUpperCase().padStart(4, '0');
  };
  const campo = (id, valor) => `${id}${String(valor.length).padStart(2, '0')}${valor}`;

  K.pixPayload = ({ valor, txid }) => {
    const conta = campo('00', 'br.gov.bcb.pix') + campo('01', 'pagamentos@kaldia.app.br');
    const corpo =
      campo('00', '01') +
      campo('26', conta) +
      campo('52', '0000') +
      campo('53', '986') +
      campo('54', Number(valor).toFixed(2)) +
      campo('58', 'BR') +
      campo('59', 'KALDIA GESTAO DE ESPACOS') +
      campo('60', 'CEASA') +
      campo('62', campo('05', String(txid).replace(/[^A-Z0-9]/gi, '').slice(0, 25))) +
      '6304';
    return corpo + crc16(corpo);
  };

  // ---------- Ícones (traço 1.75, estilo linha) ----------
  const ICONES = {
    home: '<path d="m3 10 9-7 9 7v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><path d="M9 22V12h6v10"/>',
    user: '<path d="M19 21v-2a4 4 0 0 0-4-4H9a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>',
    shield: '<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/><path d="m9 12 2 2 4-4"/>',
    search: '<circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/>',
    calendar: '<rect x="3" y="4" width="18" height="18" rx="2"/><path d="M16 2v4M8 2v4M3 10h18"/>',
    grid: '<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="14" y="14" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/>',
    card: '<rect x="2" y="5" width="20" height="14" rx="2"/><path d="M2 10h20M6 15h4"/>',
    pix: '<path d="m12 2.5 4.2 4.2-4.2 4.2-4.2-4.2zM12 13.1l4.2 4.2-4.2 4.2-4.2-4.2zM2.5 12l4.2-4.2 4.2 4.2-4.2 4.2zM13.1 12l4.2-4.2 4.2 4.2-4.2 4.2z"/>',
    qr: '<rect x="3" y="3" width="7" height="7" rx="1"/><rect x="14" y="3" width="7" height="7" rx="1"/><rect x="3" y="14" width="7" height="7" rx="1"/><path d="M14 14h3v3h-3zM21 14v.01M14 21h.01M17 21h4v-4"/>',
    check: '<path d="M20 6 9 17l-5-5"/>',
    x: '<path d="M18 6 6 18M6 6l12 12"/>',
    alert: '<path d="m21.7 18-8-14a2 2 0 0 0-3.4 0l-8 14A2 2 0 0 0 4 21h16a2 2 0 0 0 1.7-3"/><path d="M12 9v4M12 17h.01"/>',
    wrench: '<path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.8-3.8a6 6 0 0 1-7.9 7.9l-6.9 6.9a2.1 2.1 0 0 1-3-3l6.9-6.9a6 6 0 0 1 7.9-7.9z"/>',
    trash: '<path d="M3 6h18M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>',
    plus: '<path d="M12 5v14M5 12h14"/>',
    chart: '<path d="M3 3v18h18"/><path d="m7 15 4-4 3 3 6-7"/>',
    users: '<path d="M16 21v-2a4 4 0 0 0-4-4H6a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M22 21v-2a4 4 0 0 0-3-3.9M16 3.1a4 4 0 0 1 0 7.8"/>',
    megaphone: '<path d="m3 11 18-5v12L3 14z"/><path d="M11.6 16.8a3 3 0 1 1-5.8-1.6"/>',
    send: '<path d="m22 2-7 20-4-9-9-4z"/><path d="M22 2 11 13"/>',
    logout: '<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4M16 17l5-5-5-5M21 12H9"/>',
    login: '<path d="M15 3h4a2 2 0 0 1 2 2v14a2 2 0 0 1-2 2h-4M10 17l5-5-5-5M15 12H3"/>',
    sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M2 12h2M20 12h2M6.3 17.7l-1.4 1.4M19.1 4.9l-1.4 1.4"/>',
    moon: '<path d="M12 3a6 6 0 0 0 9 9 9 9 0 1 1-9-9z"/>',
    monitor: '<rect x="2" y="3" width="20" height="14" rx="2"/><path d="M8 21h8M12 17v4"/>',
    left: '<path d="m12 19-7-7 7-7M19 12H5"/>',
    right: '<path d="M5 12h14M12 5l7 7-7 7"/>',
    truck: '<path d="M14 18V6a2 2 0 0 0-2-2H4a2 2 0 0 0-2 2v11a1 1 0 0 0 1 1h2M15 18H9M19 18h2a1 1 0 0 0 1-1v-3.6a1 1 0 0 0-.2-.6l-3.5-4.4a1 1 0 0 0-.8-.4H14"/><circle cx="17" cy="18" r="2"/><circle cx="7" cy="18" r="2"/>',
    box: '<path d="M21 8a2 2 0 0 0-1-1.7l-7-4a2 2 0 0 0-2 0l-7 4A2 2 0 0 0 3 8v8a2 2 0 0 0 1 1.7l7 4a2 2 0 0 0 2 0l7-4a2 2 0 0 0 1-1.7z"/><path d="m3.3 7 8.7 5 8.7-5M12 22V12"/>',
    clock: '<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>',
    pin: '<path d="M20 10c0 6-8 12-8 12s-8-6-8-12a8 8 0 0 1 16 0z"/><circle cx="12" cy="10" r="3"/>',
    copy: '<rect x="8" y="8" width="14" height="14" rx="2"/><path d="M4 16a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h10a2 2 0 0 1 2 2"/>',
    ticket: '<path d="M2 9a3 3 0 0 1 0 6v2a2 2 0 0 0 2 2h16a2 2 0 0 0 2-2v-2a3 3 0 0 1 0-6V7a2 2 0 0 0-2-2H4a2 2 0 0 0-2 2z"/><path d="M13 5v2M13 17v2M13 11v2"/>',
    refresh: '<path d="M3 12a9 9 0 0 1 15.5-6.3L21 8M21 3v5h-5M21 12a9 9 0 0 1-15.5 6.3L3 16M8 16H3v5"/>',
    info: '<circle cx="12" cy="12" r="9"/><path d="M12 16v-4M12 8h.01"/>',
    sprout: '<path d="M7 20h10M12 20v-8M12 12C12 8 9 6 5 6c0 4 3 6 7 6zM12 12c0-3 2.5-5 6-5 0 3-2.5 5-6 5z"/>',
    chat: '<path d="M7.9 20A9 9 0 1 0 4 16.1L2 22z"/>',
    door: '<path d="M13 4h3a2 2 0 0 1 2 2v14M2 20h3M13 20h9M10 12v.01M13 4.6v16.2a1 1 0 0 1-1.2 1L5 20V5.6a2 2 0 0 1 1.5-2l5-1.4A1.2 1.2 0 0 1 13 3.4z"/>',
    menu: '<path d="M4 6h16M4 12h16M4 18h16"/>',
    wallet: '<path d="M19 7V4a1 1 0 0 0-1-1H5a2 2 0 0 0 0 4h15a1 1 0 0 1 1 1v4h-3a2 2 0 0 0 0 4h3a1 1 0 0 0 1-1v-2a1 1 0 0 0-1-1"/><path d="M3 5v14a2 2 0 0 0 2 2h15a1 1 0 0 0 1-1v-4"/>'
  };

  K.icon = (nome, tam = 18) =>
    `<svg class="ico" width="${tam}" height="${tam}" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">${ICONES[nome] || ''}</svg>`;
})(window.K);
