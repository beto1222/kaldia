# Gera todos os sons do jogo proceduralmente em AudioFonte/ (WAV 16 bits mono).
# Só usa a biblioteca padrão do Python 3: rode com  python3 Scripts/gerar_audio.py
# Depois, no editor da Unreal, rode Scripts/setup_projeto.py para importá-los.
#
# São sons sintetizados (ruído filtrado, osciladores, formantes de voz, reverberação simples):
# funcionam como base atmosférica completa, mas qualquer um pode ser trocado por uma gravação
# real com o mesmo nome de arquivo para um resultado ainda mais realista.
import math
import os
import random
import struct
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "AudioFonte")
SR = 32000          # efeitos curtos
SR_LOOP = 22050     # ambientes em loop (mais leves)
TAU = 2.0 * math.pi

rng = random.Random(1313)


# ---------------------------------------------------------------------------------------------
# Blocos básicos
# ---------------------------------------------------------------------------------------------

def N(sec, sr=SR):
    return max(1, int(sec * sr))


def zeros(n):
    return [0.0] * n


def white(n):
    return [rng.uniform(-1.0, 1.0) for _ in range(n)]


def brown(n, leak=0.995):
    y, out = 0.0, []
    for _ in range(n):
        y = y * leak + rng.uniform(-1.0, 1.0) * 0.1
        out.append(y)
    return normalize(out, 1.0)


def lowpass(x, fc, sr=SR):
    a = math.exp(-TAU * fc / sr)
    y, out = 0.0, []
    for v in x:
        y = (1.0 - a) * v + a * y
        out.append(y)
    return out


def highpass(x, fc, sr=SR):
    lp = lowpass(x, fc, sr)
    return [a - b for a, b in zip(x, lp)]


def bandpass(x, f0, q=4.0, sr=SR):
    """Biquad passa-faixa (RBJ, ganho 0 dB no pico). f0 pode ser número ou função do tempo."""
    out = []
    x1 = x2 = y1 = y2 = 0.0
    fixed = not callable(f0)
    if fixed:
        coeffs = _bp_coeffs(f0, q, sr)
    for i, v in enumerate(x):
        if not fixed and i % 32 == 0:
            coeffs = _bp_coeffs(f0(i / sr), q, sr)
        b0, b2, a1, a2 = coeffs
        y = b0 * v + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1 = x1, v
        y2, y1 = y1, y
        out.append(y)
    return out


def _bp_coeffs(f0, q, sr):
    f0 = min(max(f0, 20.0), sr * 0.45)
    w = TAU * f0 / sr
    alpha = math.sin(w) / (2.0 * q)
    a0 = 1.0 + alpha
    return (alpha / a0, -alpha / a0, -2.0 * math.cos(w) / a0, (1.0 - alpha) / a0)


def osc(freq, n, sr=SR, shape="sine", jitter=0.0):
    """Oscilador com frequência constante ou função do tempo. jitter = irregularidade de altura (vozes)."""
    out, ph = [], 0.0
    jit = 0.0
    for i in range(n):
        t = i / sr
        f = freq(t) if callable(freq) else freq
        if jitter:
            jit = jit * 0.995 + rng.uniform(-1, 1) * jitter * 0.05
            f *= 1.0 + jit
        ph = (ph + f / sr) % 1.0
        if shape == "sine":
            out.append(math.sin(TAU * ph))
        elif shape == "saw":
            out.append(2.0 * ph - 1.0)
        elif shape == "square":
            out.append(1.0 if ph < 0.5 else -1.0)
        else:  # triangle
            out.append(4.0 * abs(ph - 0.5) - 1.0)
    return out


def env_exp(n, decay, sr=SR, attack=0.002):
    a = max(1, int(attack * sr))
    return [(i / a if i < a else 1.0) * math.exp(-(i / sr) / decay) for i in range(n)]


def env_pts(n, pts, sr=SR):
    """Envelope por pontos [(t, v), ...] com interpolação linear."""
    out = []
    j = 0
    for i in range(n):
        t = i / sr
        while j < len(pts) - 2 and t > pts[j + 1][0]:
            j += 1
        (t0, v0), (t1, v1) = pts[j], pts[min(j + 1, len(pts) - 1)]
        if t <= t0:
            out.append(v0)
        elif t >= t1:
            out.append(v1)
        else:
            out.append(v0 + (v1 - v0) * (t - t0) / (t1 - t0))
    return out


def mul(a, b):
    return [x * y for x, y in zip(a, b)]


def gain(x, g):
    return [v * g for v in x]


def mix(*tracks):
    n = max(len(t) for t in tracks)
    out = zeros(n)
    for t in tracks:
        for i, v in enumerate(t):
            out[i] += v
    return out


def place(dst, src, at, g=1.0, sr=SR):
    off = int(at * sr)
    if off + len(src) > len(dst):
        dst.extend(zeros(off + len(src) - len(dst)))
    for i, v in enumerate(src):
        dst[off + i] += v * g
    return dst


def softclip(x, drive=2.0):
    k = math.tanh(drive)
    return [math.tanh(v * drive) / k for v in x]


def normalize(x, peak=0.9):
    m = max(1e-9, max(abs(v) for v in x))
    return [v * peak / m for v in x]


def fade(x, fin=0.005, fout=0.02, sr=SR):
    n = len(x)
    a, b = int(fin * sr), int(fout * sr)
    out = list(x)
    for i in range(min(a, n)):
        out[i] *= i / a
    for i in range(min(b, n)):
        out[n - 1 - i] *= i / b
    return out


def reverb(x, wet=0.3, size=1.0, tail=1.0, sr=SR, damp=3500.0):
    """Reverberação de Schroeder (4 filtros pente + 2 passa-tudo)."""
    x = list(x) + zeros(N(tail, sr))
    combs = [0.0297, 0.0371, 0.0411, 0.0437]
    acc = zeros(len(x))
    for d in combs:
        dl = int(d * size * sr)
        buf = zeros(dl)
        g = 0.82 if size >= 1 else 0.72
        lp_a = math.exp(-TAU * damp / sr)
        lp_y = 0.0
        for i, v in enumerate(x):
            y = buf[i % dl]
            lp_y = (1 - lp_a) * y + lp_a * lp_y
            buf[i % dl] = v + lp_y * g
            acc[i] += y
    for d in (0.005, 0.0017):
        dl = int(d * sr)
        buf = zeros(dl)
        out = []
        for i, v in enumerate(acc):
            b = buf[i % dl]
            y = -0.7 * v + b
            buf[i % dl] = v + 0.7 * y
            out.append(y)
        acc = out
    return [d * (1 - wet) + w * wet * 0.25 for d, w in zip(x, acc)]


def loopify(x, xfade, sr=SR_LOOP):
    """Torna o som contínuo em loop: o fim é misturado ao começo."""
    k = int(xfade * sr)
    body = list(x[:-k])
    tail = x[-k:]
    for i in range(k):
        a = i / k
        body[i] = body[i] * a + tail[i] * (1 - a)
    return body


def voice(f0, dur, formants, sr=SR, jitter=0.6, breath=0.25, shape="saw"):
    """Voz sintética: fonte (dente de serra com irregularidade) + ruído de respiração por formantes."""
    n = N(dur, sr)
    src = osc(f0, n, sr, shape, jitter)
    src = mix(src, gain(white(n), breath))
    out = zeros(n)
    for f, q, g in formants:
        out = mix(out, gain(bandpass(src, f, q, sr), g))
    return out


def thud(freq=60.0, dur=0.25, decay=0.06, sr=SR):
    n = N(dur, sr)
    body = mul(osc(lambda t: freq * (1 + 1.5 * math.exp(-t / 0.02)), n, sr), env_exp(n, decay, sr))
    click = mul(lowpass(white(n), 1800, sr), env_exp(n, 0.012, sr))
    return mix(body, gain(click, 0.5))


def metal_ring(freqs, dur, decay, sr=SR):
    n = N(dur, sr)
    out = zeros(n)
    for k, f in enumerate(freqs):
        out = mix(out, gain(mul(osc(f, n, sr), env_exp(n, decay / (1 + 0.4 * k), sr)), 1.0 / (1 + k * 0.5)))
    return out


def noise_burst(dur, lo, hi, decay, sr=SR):
    n = N(dur, sr)
    x = white(n)
    if hi:
        x = lowpass(x, hi, sr)
    if lo:
        x = highpass(x, lo, sr)
    return mul(x, env_exp(n, decay, sr))


def clicks(times, dur, sr=SR, tone=3000.0):
    out = zeros(N(dur, sr))
    for t in times:
        c = mul(bandpass(white(N(0.03, sr)), tone * rng.uniform(0.8, 1.2), 3.0, sr), env_exp(N(0.03, sr), 0.006, sr))
        place(out, c, t, 1.0, sr)
    return out


def write(name, x, sr=SR, peak=0.9):
    os.makedirs(OUT, exist_ok=True)
    x = normalize(fade(x, 0.002, 0.03, sr), peak)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767)) for v in x))
    print("  %-26s %5.2f s" % (name, len(x) / sr))


# ---------------------------------------------------------------------------------------------
# Vozes e criaturas
# ---------------------------------------------------------------------------------------------

def moan(dur=1.8, base=95.0, vowel=((420, 5, 1.0), (850, 6, 0.6), (2400, 8, 0.15))):
    f = lambda t: base * (1 + 0.12 * math.sin(TAU * 0.7 * t) - 0.15 * t / dur)
    v = voice(f, dur, vowel, jitter=1.2, breath=0.35)
    env = env_pts(len(v), [(0, 0), (0.25, 1), (dur * 0.7, 0.8), (dur, 0)])
    return reverb(mul(v, env), 0.15, 0.7, 0.4)


def growl(dur=0.9, base=70.0):
    n = N(dur)
    f = lambda t: base * (1 + 0.08 * math.sin(TAU * 3 * t))
    v = voice(f, dur, ((600, 4, 1.0), (1100, 5, 0.6), (2600, 6, 0.2)), jitter=3.0, breath=0.5)
    rough = [1 + 0.6 * math.sin(TAU * 31 * i / SR) for i in range(n)]
    env = env_pts(n, [(0, 0), (0.08, 1), (dur * 0.6, 0.9), (dur, 0)])
    return softclip(mul(mul(v, rough), env), 2.5)


def scream(dur=1.3, lo=360.0, hi=560.0, rasp=0.6):
    f = lambda t: (lo + (hi - lo) * math.sin(math.pi * min(1, t / dur))) * (1 + 0.025 * math.sin(TAU * 8 * t))
    v = voice(f, dur, ((950, 6, 1.0), (1600, 7, 0.8), (2900, 8, 0.45), (4000, 9, 0.2)), jitter=2.0, breath=rasp)
    env = env_pts(len(v), [(0, 0), (0.05, 1), (dur * 0.75, 0.85), (dur, 0)])
    return softclip(mul(v, env), 3.0)


def breath(dur, inhale=True, level=1.0, center=1400.0):
    n = N(dur)
    x = bandpass(white(n), center if inhale else center * 0.7, 1.5)
    env = env_pts(n, [(0, 0), (dur * (0.7 if inhale else 0.2), 1), (dur, 0)])
    return gain(mul(x, env), level)


def footstep(wet=True, heavy=1.0):
    x = mix(gain(thud(70 / heavy, 0.25, 0.04 * heavy), 0.9), noise_burst(0.25, 700, 2600, 0.05))
    if wet:
        x = mix(x, gain(noise_burst(0.25, 3000, None, 0.09), 0.25))
    return x


def gunshot(body_lp=2500.0, body_decay=0.12, sub=90.0, tail=0.9, crack=1.0):
    n = N(0.4)
    crk = noise_burst(0.4, 1500, None, 0.012)
    body = mul(lowpass(white(n), body_lp), env_exp(n, body_decay))
    s = thud(sub, 0.4, 0.09)
    shot = mix(gain(crk, crack), body, gain(s, 0.9))
    shot = softclip(shot, 2.0)
    return reverb(shot, 0.45, 1.3, tail, damp=2500)


# ---------------------------------------------------------------------------------------------
# Catálogo
# ---------------------------------------------------------------------------------------------

def build_all():
    print("Gerando sons em", OUT)

    # --- Kael ---
    write("passo", footstep())
    write("esquiva", mul(bandpass(white(N(0.4)), lambda t: 400 + 2500 * t, 1.2), env_pts(N(0.4), [(0, 0), (0.15, 1), (0.4, 0)])))
    hurt = mix(gain(thud(55, 0.4, 0.07), 0.8), gain(voice(lambda t: 140 - 60 * t, 0.35, ((700, 5, 1), (1200, 6, 0.6))), 0.7))
    write("dano", mul(hurt, env_exp(len(hurt), 0.18)))
    beat = zeros(N(0.65))
    place(beat, lowpass(thud(52, 0.2, 0.07), 300), 0.0, 1.0)
    place(beat, lowpass(thud(48, 0.2, 0.06), 300), 0.27, 0.7)
    write("batimento", beat)
    breathing = zeros(N(4.0))
    for start in (0.0, 2.0):
        place(breathing, breath(0.85, True, 0.7), start)
        place(breathing, breath(1.0, False, 1.0), start + 0.95)
    write("respiracao_loop", loopify(lowpass(breathing, 3000) + zeros(N(0.3)), 0.3, SR), SR, 0.7)
    gasp = zeros(N(1.6))
    for k in range(3):
        place(gasp, breath(0.3, True, 1.0, 1700), k * 0.5)
        place(gasp, breath(0.25, False, 0.8, 1500), k * 0.5 + 0.28)
    write("ofegar", gasp)
    death = mix(voice(lambda t: 160 * math.exp(-t * 0.6), 1.6, ((650, 5, 1), (1100, 6, 0.5)), jitter=2.5, breath=0.5), zeros(1))
    death = mul(death, env_pts(len(death), [(0, 0), (0.1, 1), (1.2, 0.5), (1.6, 0)]))
    place(death, thud(45, 0.5, 0.12), 1.2, 1.2)
    write("morte_kael", reverb(death, 0.2, 0.8, 0.6))
    write("lanterna_click", clicks([0.0, 0.035], 0.12, tone=4500))
    write("faca", mul(bandpass(white(N(0.22)), lambda t: 2000 + 14000 * t, 2.0), env_pts(N(0.22), [(0, 0), (0.08, 1), (0.22, 0)])))
    stab = zeros(N(0.45))
    place(stab, mul(bandpass(white(N(0.15)), lambda t: 2000 + 10000 * t, 2.0), env_pts(N(0.15), [(0, 0), (0.05, 1), (0.15, 0)])), 0.0, 0.6)
    squelch = mul(bandpass(white(N(0.3)), 450, 2.0), [abs(math.sin(TAU * 25 * i / SR)) * math.exp(-i / SR / 0.1) for i in range(N(0.3))])
    place(stab, thud(110, 0.3, 0.04), 0.06, 1.0)
    place(stab, squelch, 0.07, 1.4)
    write("faca_acerto", stab)
    write("lancar", mix(gain(breath(0.25, False, 1.0, 1200), 0.6), mul(bandpass(white(N(0.35)), lambda t: 300 + 1500 * t, 1.5), env_pts(N(0.35), [(0, 0), (0.12, 1), (0.35, 0)]))))
    write("quique", metal_ring([2100, 3420, 5150, 6900], 0.25, 0.05))
    write("inventario", mix(mul(bandpass(white(N(0.35)), lambda t: 2500 + 3000 * t, 3.0), [abs(math.sin(TAU * 40 * i / SR)) for i in range(N(0.35))]), zeros(1)))

    # --- Armas ---
    pistol = gunshot()
    write("tiro_pistola", pistol)
    shotgun = gunshot(body_lp=1600, body_decay=0.25, sub=60, tail=1.6, crack=1.3)
    place(shotgun, clicks([0.55, 0.68], 0.3, tone=2200), 0.0, 0.25)
    place(shotgun, gain(noise_burst(0.12, 800, 4000, 0.04), 0.3), 0.6)
    write("tiro_espingarda", shotgun)
    write("vazio", clicks([0.0], 0.1, tone=3500))
    reload_ = zeros(N(1.3))
    place(reload_, clicks([0.0, 0.02], 0.1, tone=2500), 0.05)
    place(reload_, mul(bandpass(white(N(0.15)), 1500, 2.0), env_exp(N(0.15), 0.05)), 0.4, 0.6)
    place(reload_, clicks([0.0, 0.015], 0.1, tone=1800), 0.75, 1.2)
    place(reload_, clicks([0.0, 0.05], 0.15, tone=3000), 1.1, 0.9)
    write("recarga", reload_)
    write("troca_arma", mix(gain(mul(bandpass(white(N(0.3)), 1800, 1.0), env_pts(N(0.3), [(0, 0), (0.1, 1), (0.3, 0)])), 0.6), clicks([0.22], 0.35, tone=2600)))
    imp = mix(noise_burst(0.35, 1200, None, 0.015), gain(noise_burst(0.35, 200, 1500, 0.05), 0.6))
    if True:
        ric = mul(osc(lambda t: 3200 - 1800 * t, N(0.35)), env_exp(N(0.35), 0.08))
        imp = mix(imp, gain(ric, 0.15))
    write("impacto", imp)
    flesh = mix(thud(120, 0.25, 0.04), gain(mul(bandpass(white(N(0.25)), 600, 1.5), env_exp(N(0.25), 0.05)), 0.8))
    write("impacto_corpo", flesh)
    boom = zeros(N(3.0))
    place(boom, noise_burst(0.2, 1000, None, 0.02), 0.0, 1.0)
    place(boom, mul(lowpass(brown(N(3.0)), 500), env_exp(N(3.0), 0.7)), 0.0, 1.2)
    place(boom, thud(38, 1.2, 0.35), 0.0, 1.0)
    write("explosao", reverb(softclip(boom, 1.8), 0.35, 1.5, 1.0, damp=1500))

    # --- Itens e mundo ---
    write("item", mix(gain(mul(bandpass(white(N(0.3)), 2200, 1.0), env_pts(N(0.3), [(0, 0), (0.08, 1), (0.3, 0)])), 0.5), clicks([0.12], 0.3, tone=2000)))
    rip = mul(bandpass(white(N(0.7)), 2500, 1.2), [(0.5 + 0.5 * math.sin(TAU * 60 * i / SR)) * (1 - i / N(0.7)) for i in range(N(0.7))])
    write("cura", rip)
    write("municao", clicks([0.0, 0.05, 0.09, 0.16, 0.21, 0.3], 0.45, tone=4200))
    write("bateria", mix(clicks([0.0, 0.18], 0.35, tone=2400), gain(mul(bandpass(white(N(0.35)), 3000, 2.0), env_exp(N(0.35), 0.05)), 0.3)))
    beep = zeros(N(0.4))
    place(beep, mul(osc(1800, N(0.08), shape="square"), env_exp(N(0.08), 0.05)), 0.0, 0.4)
    place(beep, mul(osc(1350, N(0.12), shape="square"), env_exp(N(0.12), 0.08)), 0.1, 0.4)
    write("cartao", lowpass(beep, 5000))
    door = zeros(N(2.6))
    hum = mul(lowpass(osc(lambda t: 55 + 5 * t, N(2.4), shape="saw"), 300), env_pts(N(2.4), [(0, 0), (0.3, 1), (2.1, 1), (2.4, 0)]))
    rumble = mul(lowpass(white(N(2.4)), 400), env_pts(N(2.4), [(0, 0), (0.3, 0.6), (2.4, 0)]))
    place(door, mix(gain(hum, 0.6), rumble), 0.0)
    place(door, thud(70, 0.4, 0.08), 2.2, 1.0)
    write("porta", reverb(door, 0.3, 1.2, 0.8))
    slam = mix(thud(45, 2.0, 0.15), gain(metal_ring([180, 437, 912, 1530, 2280], 2.0, 0.8), 0.5), gain(noise_burst(2.0, 300, 3000, 0.05), 0.6))
    write("porta_batida", reverb(softclip(slam, 2.0), 0.45, 1.6, 1.5, damp=2000))
    term = zeros(N(1.2))
    for k, f in enumerate([880, 1320, 990, 1760]):
        place(term, mul(osc(f, N(0.07), shape="square"), env_exp(N(0.07), 0.05)), k * 0.12, 0.3)
    place(term, thud(90, 0.3, 0.05), 0.7, 0.8)
    write("terminal", lowpass(term, 4500))
    sting = zeros(N(3.0))
    for f in (55.0, 58.27, 82.4, 116.5):
        place(sting, mul(lowpass(osc(f, N(3.0), shape="saw"), 400), env_pts(N(3.0), [(0, 0), (0.8, 1), (2.0, 0.6), (3.0, 0)])), 0.0, 0.3)
    write("objetivo", reverb(sting, 0.4, 1.5, 1.0), peak=0.6)
    thunder = zeros(N(6.0))
    place(thunder, noise_burst(0.15, 800, None, 0.03), 0.0, 0.7)
    rumble = mul(lowpass(brown(N(6.0)), 250), env_pts(N(6.0), [(0, 0), (0.15, 1), (1.0, 0.7), (2.5, 0.5), (6.0, 0)]))
    rumble = mul(rumble, [0.6 + 0.4 * math.sin(TAU * 0.9 * i / SR + 2 * math.sin(TAU * 0.3 * i / SR)) for i in range(N(6.0))])
    place(thunder, rumble, 0.05, 1.3)
    write("trovao", reverb(thunder, 0.3, 1.6, 1.0, damp=1200))
    pop = mix(noise_burst(0.5, 2000, None, 0.008), gain(metal_ring([5200, 6700, 8100, 9400], 0.5, 0.12), 0.2))
    write("luz_estouro", reverb(pop, 0.3, 0.9, 0.4))
    buzz_n = N(1.0)
    buzz = mul(lowpass(osc(60, buzz_n, shape="square"), 2500), [0.3 + 0.7 * (rng.random() < 0.3) for _ in range(buzz_n)])
    buzz = mix(gain(buzz, 0.5), gain(mul(highpass(white(buzz_n), 3000), [1.0 if rng.random() < 0.02 else 0.05 for _ in range(buzz_n)]), 0.8))
    write("eletrica", mul(buzz, env_pts(buzz_n, [(0, 0), (0.05, 1), (0.8, 0.8), (1.0, 0)])), peak=0.6)
    st_n = N(2.6)
    static = bandpass(white(st_n), 1800, 0.7)
    radio_voice = voice(lambda t: 130 + 30 * math.sin(TAU * 1.3 * t), 2.6, ((700, 4, 1), (1200, 5, 0.7), (2500, 6, 0.3)), jitter=1.0)
    syll = [max(0.0, math.sin(TAU * 4.5 * i / SR)) * (1 if (i // N(0.6)) % 2 == 0 else 0.2) for i in range(st_n)]
    write("estatica", mix(gain(static, 0.6), gain(highpass(mul(radio_voice, syll), 400), 0.5)), peak=0.7)
    collapse = zeros(N(2.8))
    place(collapse, mul(lowpass(brown(N(2.8)), 300), env_pts(N(2.8), [(0, 0), (0.2, 1), (2.8, 0)])), 0.0, 1.2)
    for k in range(14):
        place(collapse, gain(noise_burst(0.15, 600, 5000, 0.03), rng.uniform(0.2, 0.7)), rng.uniform(0.3, 2.4))
    write("desabamento", reverb(collapse, 0.35, 1.4, 1.0))

    # --- Infectados ---
    for name, base in (("infectado_gemido", 95.0),):
        write(name, moan(1.9, base))
    write("infectado_rosnado", reverb(growl(0.9, 72.0), 0.15, 0.7, 0.3))
    write("infectado_grito", reverb(scream(1.4), 0.3, 1.2, 0.8))
    atk = mix(gain(scream(0.5, 300, 420, 0.9), 0.8), gain(growl(0.5, 90), 0.6))
    write("infectado_ataque", atk)
    bite = zeros(N(0.5))
    for k in range(5):
        place(bite, noise_burst(0.06, 1500, 7000, 0.01), 0.03 * k + rng.uniform(0, 0.02), rng.uniform(0.5, 1.0))
    place(bite, mul(bandpass(white(N(0.35)), 500, 2.0), [abs(math.sin(TAU * 18 * i / SR)) * math.exp(-i / SR / 0.12) for i in range(N(0.35))]), 0.08, 1.2)
    place(bite, thud(90, 0.3, 0.05), 0.0, 0.8)
    write("infectado_mordida", bite)
    rattle_n = N(1.4)
    rattle = voice(lambda t: 85 * math.exp(-t * 0.5), 1.4, ((500, 4, 1), (900, 5, 0.6)), jitter=4.0, breath=0.7)
    rattle = mul(rattle, [(0.4 + 0.6 * abs(math.sin(TAU * 7 * i / SR))) * math.exp(-i / SR / 0.6) for i in range(rattle_n)])
    write("infectado_morte", reverb(rattle, 0.15, 0.7, 0.4))
    rise = zeros(N(1.6))
    for k in range(9):
        place(rise, noise_burst(0.04, 2500, 9000, 0.004), rng.uniform(0.0, 1.0), rng.uniform(0.4, 1.0))
    place(rise, moan(1.2, 80), 0.3, 0.8)
    write("infectado_levantar", rise)

    # --- Hospedeiro ---
    roar_d = 2.6
    roar = mix(
        voice(lambda t: 52 * (1 + 0.1 * math.sin(TAU * 2.5 * t)), roar_d, ((300, 3, 1), (620, 4, 0.8), (1300, 5, 0.4)), jitter=4.0, breath=0.8),
        gain(voice(lambda t: 104 * (1 + 0.08 * math.sin(TAU * 3.1 * t)), roar_d, ((800, 4, 1), (1600, 5, 0.5)), jitter=3.0, breath=0.6), 0.6))
    roar = mul(roar, env_pts(len(roar), [(0, 0), (0.25, 1), (1.9, 0.85), (roar_d, 0)]))
    write("hospedeiro_rugido", reverb(softclip(roar, 3.5), 0.4, 1.6, 1.4, damp=1800))
    write("hospedeiro_passo", reverb(mix(thud(36, 0.7, 0.18), gain(mul(bandpass(white(N(0.7)), 300, 1.5), env_exp(N(0.7), 0.08)), 0.7)), 0.3, 1.3, 0.5))
    hit = mix(thud(40, 1.3, 0.25), gain(noise_burst(1.3, 100, 3000, 0.12), 0.8))
    for k in range(10):
        place(hit, gain(noise_burst(0.1, 800, 6000, 0.02), rng.uniform(0.1, 0.4)), rng.uniform(0.2, 1.0))
    write("hospedeiro_golpe", reverb(softclip(hit, 2.0), 0.35, 1.5, 1.0))
    crash = mix(thud(32, 1.6, 0.3), gain(noise_burst(1.6, 80, 2500, 0.25), 1.0))
    for k in range(18):
        place(crash, gain(noise_burst(0.12, 700, 6000, 0.025), rng.uniform(0.15, 0.5)), rng.uniform(0.15, 1.4))
    write("hospedeiro_impacto", reverb(softclip(crash, 2.2), 0.4, 1.6, 1.2))
    hb = zeros(N(2.2))
    place(hb, gain(lowpass(breath(1.0, True, 1.0, 500), 900), 1.0), 0.0)
    place(hb, gain(lowpass(mix(breath(1.1, False, 1.0, 380), gain(growl(1.1, 45), 0.4)), 900), 1.0), 1.0)
    write("hospedeiro_respiracao", reverb(hb, 0.3, 1.3, 0.6))
    hd = voice(lambda t: 60 * math.exp(-t * 0.35), 3.2, ((280, 3, 1), (560, 4, 0.7)), jitter=5.0, breath=0.9)
    hd = mul(hd, env_pts(len(hd), [(0, 0), (0.2, 1), (1.5, 0.7), (3.2, 0)]))
    gurgle = mul(bandpass(white(N(3.2)), 400, 2.0), [abs(math.sin(TAU * 9 * i / SR)) * (i / N(3.2)) for i in range(N(3.2))])
    write("hospedeiro_morte", reverb(softclip(mix(hd, gain(gurgle, 0.6)), 3.0), 0.4, 1.6, 1.5))
    acid_n = N(1.3)
    acid = mul(highpass(white(acid_n), 2500), [(0.3 + 0.7 * (rng.random() < 0.15)) * math.exp(-i / SR / 0.5) for i in range(acid_n)])
    acid = mix(acid, gain(mul(bandpass(white(acid_n), 500, 1.5), env_exp(acid_n, 0.08)), 0.8))
    write("acido", acid)
    tear = zeros(N(1.6))
    place(tear, mul(bandpass(white(N(0.8)), 1200, 1.0), [(0.5 + 0.5 * math.sin(TAU * 45 * i / SR)) * (1 - i / N(0.8)) for i in range(N(0.8))]), 0.0)
    place(tear, mul(lowpass(white(N(1.2)), 700), env_pts(N(1.2), [(0, 0), (0.1, 1), (1.2, 0)])), 0.3, 0.8)
    write("membrana_rompe", reverb(tear, 0.3, 1.3, 0.7))

    # --- Medo: sustos e sons do escuro ---
    hit_n = N(2.8)
    stinger = mix(gain(noise_burst(2.8, 200, 6000, 0.08), 0.8), thud(40, 2.8, 0.4))
    for f in (65.4, 69.3, 98.0, 1046.5, 1108.7, 1567.9):
        stinger = mix(stinger, gain(mul(osc(f * (1 + rng.uniform(-0.004, 0.004)), hit_n, shape="saw"), env_exp(hit_n, 0.9 if f < 200 else 0.6, attack=0.003)), 0.35))
    write("susto", reverb(softclip(lowpass(stinger, 7000), 2.5), 0.45, 1.6, 1.5))
    wh_n = N(2.2)
    whisper = bandpass(white(wh_n), lambda t: 2600 + 900 * math.sin(TAU * 1.7 * t), 2.0)
    whisper = mix(whisper, gain(bandpass(white(wh_n), 5200, 3.0), 0.5))
    syll = [max(0.0, math.sin(TAU * rng.choice([4.5, 5.5]) * i / SR)) ** 2 for i in range(wh_n)]
    syll = lowpass(syll, 20)
    write("sussurro", reverb(mul(mul(whisper, syll), env_pts(wh_n, [(0, 0), (0.2, 1), (1.8, 1), (2.2, 0)])), 0.4, 1.0, 0.8), peak=0.75)
    steps = zeros(N(2.6))
    for k in range(4):
        place(steps, footstep(True, 1.1), 0.1 + k * 0.55 + rng.uniform(-0.04, 0.04), 1.0 - 0.15 * k)
    write("passos_atras", reverb(steps, 0.35, 1.2, 0.6))
    cr_n = N(2.2)
    creak = mul(osc(lambda t: 90 + 60 * math.sin(TAU * 0.4 * t) + 20 * math.sin(TAU * 3 * t), cr_n, shape="saw", jitter=2.0),
                [0.4 + 0.6 * abs(math.sin(TAU * 11 * i / SR)) for i in range(cr_n)])
    creak = bandpass(creak, 900, 1.2)
    write("metal_rangido", reverb(mul(creak, env_pts(cr_n, [(0, 0), (0.3, 1), (1.8, 0.8), (2.2, 0)])), 0.5, 1.6, 1.2))
    far = lowpass(scream(1.6, 420, 640, 0.4), 1800)
    write("grito_distante", reverb(far, 0.7, 1.8, 2.0, damp=1200), peak=0.8)
    bang = zeros(N(2.8))
    t = 0.0
    for k in range(5):
        place(bang, mix(thud(55, 0.6, 0.1), gain(metal_ring([160, 390, 870], 0.6, 0.3), 0.6)), t, rng.uniform(0.7, 1.0))
        t += rng.choice([0.28, 0.32, 0.6, 0.75])
    write("batidas", reverb(softclip(bang, 1.8), 0.45, 1.6, 1.2))
    cry = zeros(N(2.8))
    for k in range(4):
        sob = voice(lambda tt: 290 - 60 * tt, 0.45, ((800, 5, 1), (1300, 6, 0.6), (2700, 8, 0.2)), jitter=1.5, breath=0.5)
        place(cry, mul(sob, env_pts(len(sob), [(0, 0), (0.05, 1), (0.45, 0)])), 0.1 + k * 0.62, 0.8)
        place(cry, breath(0.18, True, 0.4, 1800), 0.5 + k * 0.62)
    write("choro", reverb(lowpass(cry, 2500), 0.55, 1.6, 1.4), peak=0.7)

    # --- Loops ---
    n = N(8.0, SR_LOOP)
    rain = mix(gain(lowpass(highpass(white(n), 400, SR_LOOP), 6000, SR_LOOP), 0.6), zeros(1))
    for _ in range(900):
        d = rng.uniform(0, 8.0)
        drop = mul(bandpass(white(N(0.02, SR_LOOP)), rng.uniform(2000, 6000), 2.0, SR_LOOP), env_exp(N(0.02, SR_LOOP), 0.004, SR_LOOP))
        place(rain, drop, d, rng.uniform(0.2, 0.8), SR_LOOP)
    write("chuva_loop", loopify(rain + zeros(N(0.5, SR_LOOP)), 0.5), SR_LOOP, 0.7)

    n = N(12.0, SR_LOOP)
    drone = zeros(n)
    for f, g in ((41.2, 1.0), (41.6, 0.8), (61.7, 0.5), (82.4, 0.3), (123.5, 0.12)):
        drone = mix(drone, gain(osc(f, n, SR_LOOP), g))
    wind = bandpass(white(n), lambda t: 350 + 200 * math.sin(TAU * t / 6.0), 0.8, SR_LOOP)
    wind = mul(wind, [0.5 + 0.5 * math.sin(TAU * i / SR_LOOP / 6.0 + 1.0) for i in range(n)])
    amb = mix(gain(drone, 0.5), gain(wind, 0.8))
    write("ambiente_loop", loopify(amb + zeros(N(1.0, SR_LOOP)), 1.0), SR_LOOP, 0.6)

    n = N(8.0, SR_LOOP)
    chase = zeros(n)
    for k in range(16):
        place(chase, lowpass(thud(48, 0.3, 0.08, SR_LOOP), 400, SR_LOOP), k * 0.5, 1.0, SR_LOOP)
        place(chase, lowpass(thud(44, 0.25, 0.06, SR_LOOP), 400, SR_LOOP), k * 0.5 + 0.18, 0.6, SR_LOOP)
    trem = [0.6 + 0.4 * math.sin(TAU * 7 * i / SR_LOOP) for i in range(n)]
    strings = mix(osc(880, n, SR_LOOP, "saw"), osc(932.3, n, SR_LOOP, "saw"), gain(osc(55, n, SR_LOOP, "saw"), 2.0))
    strings = mul(lowpass(strings, 1800, SR_LOOP), trem)
    write("perseguicao_loop", loopify(mix(gain(chase, 1.0), gain(strings, 0.18)) + zeros(N(0.25, SR_LOOP)), 0.25), SR_LOOP, 0.75)

    n = N(4.0, SR_LOOP)
    siren = lowpass(osc(lambda t: 620 if (t % 2.0) < 1.0 else 830, n, SR_LOOP, "square"), 1800, SR_LOOP)
    write("alarme_loop", loopify(reverb(siren, 0.5, 1.4, 0.5, SR_LOOP), 0.5), SR_LOOP, 0.6)

    print("Pronto: %d sons." % len([f for f in os.listdir(OUT) if f.endswith(".wav")]))


if __name__ == "__main__":
    if len(sys.argv) > 1:
        OUT = sys.argv[1]
    build_all()
