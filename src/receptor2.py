"""Receptor 2 (Raspberry Pi Pico W) — MicroPython

UART (desde ESP32 transmisor):
  - ESP32 Serial1 TX (ver emisor.cpp: actualmente GPIO22) -> Pico UART1 RX = GP9 (pin físico 12)
  - ESP32 GND -> Pico GND (pin físico 3 u 8)

LCD I2C (PCF8574T típico, addr 0x27):
  - SDA -> GP0 (pin físico 1)
  - SCL -> GP1 (pin físico 2)
  - VCC -> 3V3(OUT) (pin físico 36)
  - GND -> GND (pin físico 3)
"""

from machine import UART, Pin, I2C
import struct
import math
import time


FRAME_HEADER = 0xAA
UART_BAUD = 115200
N = 128

# Debug opcional: imprime algunas muestras para comparar reconstrucción
DEBUG_SAMPLES = True
DEBUG_SAMPLES_N = 3

UART_ID = 1

# UART1 

UART_RX_PIN = 5
UART_TX_PIN = 8  # opcional (no es necesario conectar al ESP32)

I2C_ID = 0
I2C_SDA_PIN = 0
I2C_SCL_PIN = 1

# Si ya sabes la dirección I2C del backpack, ponla aquí (por ejemplo 0x27 o 0x3F).
# Si es None, se auto-detecta con i2c.scan().
LCD_FORCE_ADDR = None

LCD_MAPS_TO_TRY = ("A", "B", "C")


def clamp_u8(x):
    if x < 0:
        return 0
    if x > 255:
        return 255
    return x


def read_exact(uart, nbytes, timeout_ms):
    buf = bytearray()
    start = time.ticks_ms()
    while len(buf) < nbytes:
        if time.ticks_diff(time.ticks_ms(), start) > timeout_ms:
            return None
        chunk = uart.read(nbytes - len(buf))
        if chunk:
            buf.extend(chunk)
        else:
            time.sleep_ms(1)
    return buf


class Pcf8574Lcd:
    # Mapeos comunes (varían entre backpacks). Datos siempre en P4..P7.
    # 'A' (muy común): P0=RS, P1=RW, P2=E, P3=BL
    # 'B' (variante):  P0=RS, P1=RW, P3=E, P2=BL
    # 'C' (variante):  P2=RS, P1=RW, P0=E, P3=BL
    PINMAPS = {
        "A": {"RS": 0x01, "RW": 0x02, "E": 0x04, "BL": 0x08, "BL_INV": False},
        "B": {"RS": 0x01, "RW": 0x02, "E": 0x08, "BL": 0x04, "BL_INV": False},
        "C": {"RS": 0x04, "RW": 0x02, "E": 0x01, "BL": 0x08, "BL_INV": False},
    }

    def __init__(self, i2c, addr, cols=16, rows=2, pinmap="A"):
        self.i2c = i2c
        self.addr = addr
        self.cols = cols
        self.rows = rows
        self._pinmap = self.PINMAPS.get(pinmap, self.PINMAPS["A"])
        self._backlight = True
        time.sleep_ms(50)
        self._init_lcd()

    def _write_pcf(self, data):
        bl_bit = self._pinmap["BL"]
        if self._pinmap.get("BL_INV", False):
            if self._backlight:
                data &= ~bl_bit
            else:
                data |= bl_bit
        else:
            if self._backlight:
                data |= bl_bit
            else:
                data &= ~bl_bit
        self.i2c.writeto(self.addr, bytes([data]))

    def _pulse_enable(self, data):
        e_bit = self._pinmap["E"]
        self._write_pcf(data | e_bit)
        time.sleep_us(2)
        self._write_pcf(data & ~e_bit)
        time.sleep_us(50)

    def _write_nibble(self, nibble4, rs):
        data = ((nibble4 & 0x0F) << 4) & 0xF0
        if rs:
            data |= self._pinmap["RS"]
        self._write_pcf(data)
        self._pulse_enable(data)

    def _cmd(self, cmd):
        self._write_nibble((cmd >> 4) & 0x0F, rs=False)
        self._write_nibble(cmd & 0x0F, rs=False)

    def _data(self, val):
        self._write_nibble((val >> 4) & 0x0F, rs=True)
        self._write_nibble(val & 0x0F, rs=True)

    def _init_lcd(self):
        for _ in range(3):
            self._write_nibble(0x3, rs=False)
            time.sleep_ms(5)
        self._write_nibble(0x2, rs=False)
        time.sleep_ms(5)

        self._cmd(0x28)
        self._cmd(0x0C)
        self._cmd(0x06)
        self.clear()

    def clear(self):
        self._cmd(0x01)
        time.sleep_ms(2)

    def move_to(self, col, row):
        row_offsets = [0x00, 0x40, 0x14, 0x54]
        if row >= self.rows:
            row = self.rows - 1
        self._cmd(0x80 | (row_offsets[row] + col))

    def putstr(self, s):
        for ch in s:
            self._data(ord(ch))


def reconstruct_u8(coeffs):
    # Reconstrucción equivalente al receptor 1 (ESP32):
    # 1) Armar espectro complejo X[k] desde (mag, phase)
    # 2) Aplicar simetría hermítica: X[N-k] = conj(X[k])
    # 3) IFFT y escalamiento a u8 (0..255) centrado en 128

    X = [0j] * N
    for (k, mag, ph) in coeffs:
        if k <= 0 or k >= (N // 2):
            continue

        re = float(mag) * math.cos(float(ph))
        im = float(mag) * math.sin(float(ph))
        X[int(k)] = complex(re, im)

        k2 = int(N - int(k))
        X[k2] = complex(re, -im)

    x_cplx = ifft(X)
    # Idealmente real (la parte imag es numérica)
    x = [float(v.real) for v in x_cplx]

    max_abs = 1e-9
    for n in range(N):
        a = abs(x[n])
        if a > max_abs:
            max_abs = a

    gain = 120.0 / max_abs
    out = bytearray(N)
    for n in range(N):
        s = int(round(128.0 + x[n] * gain))
        out[n] = clamp_u8(s)
    return out


def _is_power_of_two(n):
    return n > 0 and (n & (n - 1)) == 0


def _bit_reverse(x, bits):
    y = 0
    for _ in range(bits):
        y = (y << 1) | (x & 1)
        x >>= 1
    return y


_twiddles = None


def _get_twiddles(n):
    # Precalcula twiddles para FFT de longitud n (potencia de 2)
    # tw[size][j] = exp(-j*2*pi*j/size)
    global _twiddles
    if _twiddles is not None and _twiddles.get("n") == n:
        return _twiddles

    if not _is_power_of_two(n):
        raise ValueError("FFT/IFFT requiere N potencia de 2")

    levels = 0
    t = n
    while t > 1:
        levels += 1
        t >>= 1

    tw = {}
    size = 2
    while size <= n:
        half = size >> 1
        theta = -2.0 * math.pi / float(size)
        wlist = [0j] * half
        for j in range(half):
            ang = theta * float(j)
            wlist[j] = complex(math.cos(ang), math.sin(ang))
        tw[size] = wlist
        size <<= 1

    rev = [0] * n
    for i in range(n):
        rev[i] = _bit_reverse(i, levels)

    _twiddles = {"n": n, "rev": rev, "tw": tw}
    return _twiddles


def fft(x):
    # FFT radix-2 iterativa in-place sobre copia
    n = len(x)
    cfg = _get_twiddles(n)
    a = list(x)

    # Permutación bit-reversal
    rev = cfg["rev"]
    for i in range(n):
        j = rev[i]
        if j > i:
            a[i], a[j] = a[j], a[i]

    size = 2
    tw = cfg["tw"]
    while size <= n:
        half = size >> 1
        wlist = tw[size]
        for start in range(0, n, size):
            for j in range(half):
                u = a[start + j]
                v = wlist[j] * a[start + j + half]
                a[start + j] = u + v
                a[start + j + half] = u - v
        size <<= 1
    return a


def ifft(X):
    # IFFT usando la propiedad: ifft(X) = conj(fft(conj(X))) / N
    n = len(X)
    Xc = [complex(v.real, -v.imag) for v in X]
    y = fft(Xc)
    inv_n = 1.0 / float(n)
    return [complex(v.real, -v.imag) * inv_n for v in y]


def compute_metrics(orig_u8, recon_u8, total_energy, coeffs):
    # Trabajamos en dominio centrado (quitar offset 128)
    # y ajustamos una ganancia 'a' para que la comparación no castigue
    # diferencias de escala entre original y reconstruida.
    # a = argmin ||o - a*r||^2  => a = (o·r)/(r·r)
    dot_or = 0.0
    dot_rr = 0.0
    sig2 = 0.0
    for i in range(N):
        o = float(int(orig_u8[i]) - 128)
        r = float(int(recon_u8[i]) - 128)
        dot_or += o * r
        dot_rr += r * r
        sig2 += o * o

    if dot_rr > 1e-12:
        a = dot_or / dot_rr
    else:
        a = 0.0

    err2 = 0.0
    for i in range(N):
        o = float(int(orig_u8[i]) - 128)
        r = float(int(recon_u8[i]) - 128)
        e = o - a * r
        err2 += e * e

    mse = err2 / float(N)

    sel_energy = 0.0
    for (_, mag, _) in coeffs:
        sel_energy += float(mag) * float(mag)
    energy_ratio = (sel_energy / float(total_energy)) if total_energy > 1e-12 else 0.0

    if err2 <= 1e-12:
        snr_db = float('inf')
    elif sig2 <= 1e-12:
        snr_db = float('-inf')
    else:
        snr_db = 10.0 * math.log10(sig2 / err2)

    return mse, energy_ratio, snr_db


def main():
    uart = UART(
        UART_ID,
        baudrate=UART_BAUD,
        bits=8,
        parity=None,
        stop=1,
        rx=Pin(UART_RX_PIN),
        tx=Pin(UART_TX_PIN),
        timeout=0,
    )

    i2c = I2C(I2C_ID, sda=Pin(I2C_SDA_PIN), scl=Pin(I2C_SCL_PIN), freq=100_000)
    lcd = None
    lcd_addr = None

    addrs = i2c.scan()
    print("I2C scan:", [hex(a) for a in addrs])
    if addrs:
        lcd_addr = LCD_FORCE_ADDR if LCD_FORCE_ADDR is not None else addrs[0]

    if lcd_addr is None:
        print("[ERR] No se detecta LCD por I2C.")
    else:
        for name in LCD_MAPS_TO_TRY:
            try:
                lcd = Pcf8574Lcd(i2c, lcd_addr, pinmap=name)
                for _ in range(2):
                    lcd._backlight = False
                    lcd._write_pcf(0x00)
                    time.sleep_ms(150)
                    lcd._backlight = True
                    lcd._write_pcf(0x00)
                    time.sleep_ms(150)
                lcd.clear()
                lcd.move_to(0, 0)
                lcd.putstr("LCD OK")
                lcd.move_to(0, 1)
                lcd.putstr("Esperando UART")
                print("Probando pinmap:", name)
                break
            except Exception as e:
                print("[WARN] Pinmap", name, "fallo:", e)
                lcd = None

    last_ui = time.ticks_ms()
    frames = 0
    uart_ok = False
    rx_bytes = 0

    while True:
        pending = uart.any()
        if pending:
            rx_bytes += pending

        b = uart.read(1)
        if not b:
            now = time.ticks_ms()
            if lcd and (not uart_ok) and time.ticks_diff(now, last_ui) >= 500:
                last_ui = now
                lcd.move_to(0, 0)
                lcd.putstr(("RX {:6d}".format(rx_bytes % 1000000) + " " * 16)[:16])
                lcd.move_to(0, 1)
                lcd.putstr(("Esperando UART" + " " * 16)[:16])
            continue
        if b[0] != FRAME_HEADER:
            continue

        k_bytes = read_exact(uart, 1, timeout_ms=50)
        if not k_bytes:
            continue
        K = int(k_bytes[0])
        if K <= 0 or K > 63:
            continue

        payload_len = 4 + (K * 10) + N + 1
        payload = read_exact(uart, payload_len, timeout_ms=150)
        if not payload:
            continue

        cs = FRAME_HEADER ^ K
        for bb in payload[:-1]:
            cs ^= bb
        if cs != payload[-1]:
            continue

        total_energy = struct.unpack_from('<f', payload, 0)[0]

        coeffs = []
        for i in range(K):
            base = 4 + (i * 10)
            idx = (payload[base] << 8) | payload[base + 1]
            mag = struct.unpack_from('<f', payload, base + 2)[0]
            ph = struct.unpack_from('<f', payload, base + 6)[0]
            coeffs.append((idx, mag, ph))

        orig_start = 4 + (K * 10)
        orig_u8 = memoryview(payload)[orig_start:orig_start + N]

        recon_u8 = reconstruct_u8(coeffs)
        mse, e_ratio, snr_db = compute_metrics(orig_u8, recon_u8, total_energy, coeffs)
        frames += 1
        uart_ok = True

        now = time.ticks_ms()
        if time.ticks_diff(now, last_ui) >= 500:
            last_ui = now

            epct_f = e_ratio * 100.0
            if snr_db == float('inf'):
                snr_txt = "inf"
            elif snr_db == float('-inf'):
                snr_txt = "-inf"
            else:
                snr_txt = "{:.1f}".format(snr_db)

            print("K={}, MSE={:.2f}, E={:.1f}%, SNR={} dB".format(K, mse, e_ratio * 100.0, snr_txt))

            if DEBUG_SAMPLES:
                m = DEBUG_SAMPLES_N
                # orig_u8 es un memoryview; convertir a int para imprimir
                o = [int(orig_u8[i]) for i in range(m)]
                r = [int(recon_u8[i]) for i in range(m)]
                print("orig[0:{}]={}  recon[0:{}]={}".format(m - 1, o, m - 1, r))

            if lcd:
                # LCD 16x2: mostrar fijo las 3 métricas solicitadas.
                # Línea 1: MSE con 1 decimal
                # Línea 2: Energía preservada + SNR
                # MSE con 2 decimales (compacto para 16 columnas)
                line1 = "MSE={:.2f}".format(mse)
                # E puede llegar a 100.0%, así que reservamos 5 caracteres.
                line2 = "E{:5.1f}% S{:>4}dB".format(epct_f, snr_txt)
                lcd.move_to(0, 0)
                lcd.putstr((line1 + " " * 16)[:16])
                lcd.move_to(0, 1)
                lcd.putstr((line2 + " " * 16)[:16])


main()