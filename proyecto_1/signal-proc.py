import numpy as np
import matplotlib.pyplot as plt
import time
import ft as ft

'''
Archivo de procesamiento de señales
Contiene:
Función de compresión (usa FFT)
Función de reconstrucción (usa IFFT)
Función para graficar la señal original vs. la reconstruida
Cálcula e imprime MSE y energía preservada
.
'''


def _is_power_of_two(n: int) -> bool:
	return n > 0 and (n & (n - 1) == 0)


def _to_1d_real_array(x) -> np.ndarray:
	arr = np.asarray(x, dtype=float).reshape(-1)
	if arr.size == 0:
		raise ValueError("La señal no puede ser vacía")
	return arr


def _fft(x: np.ndarray, use_custom_fft: bool = True) -> tuple[np.ndarray, float]:
	"""Retorna (X, tiempo_seg)."""
	N = int(x.size)
	if use_custom_fft and _is_power_of_two(N):
		X_list, tiempo = ft.fft(x.tolist())
		return np.asarray(X_list, dtype=complex), float(tiempo)

	# Fallback a NumPy
	inicio = time.time()
	X = np.fft.fft(x)
	return X.astype(complex), float(time.time() - inicio)


def ifft_from_fft(X: np.ndarray, use_custom_fft: bool = True) -> tuple[np.ndarray, float]:
	"""IFFT usando la propiedad: ifft(X) = conj(fft(conj(X))) / N.

	Si `use_custom_fft=True` y N es potencia de 2, usa `ft.fft`.
	"""
	X = np.asarray(X, dtype=complex).reshape(-1)
	N = int(X.size)
	if N == 0:
		raise ValueError("El espectro no puede ser vacío")

	inicio = time.time()
	X_conj = np.conjugate(X)
	Y, _ = _fft(X_conj, use_custom_fft=use_custom_fft)
	x_rec = np.conjugate(Y) / N
	return x_rec, float(time.time() - inicio)


def compress_signal(
	x,
	energy_threshold: float = 0.95,
	use_custom_fft: bool = True,
	preserve_real: bool | None = None,
) -> dict:
	"""Comprime una señal en base a energía espectral acumulada.

	Ordena coeficientes por magnitud (descendente) y elige los necesarios
	para preservar `energy_threshold` de la energía total.

	Retorna un dict con:
	- N
	- X_full (np.ndarray)
	- compressed: lista [(k, X[k]), ...]
	- energy_total, energy_kept, energy_ratio
	- k_count
	"""
	x = _to_1d_real_array(x)
	if not (0 < energy_threshold <= 1.0):
		raise ValueError("energy_threshold debe estar en (0, 1]")

	X, fft_time = _fft(x, use_custom_fft=use_custom_fft)
	N = int(X.size)

	# Energía total en frecuencia (Parseval, ignorando escala 1/N ya que cancela en la razón)
	energia_total = float(np.sum(np.abs(X) ** 2))
	limite = energy_threshold * energia_total

	# En señales reales, mantener pares conjugados ayuda a reconstruir real.
	if preserve_real is None:
		preserve_real = np.isrealobj(x)

	# Índices ordenados por magnitud descendente
	idx_sorted = np.argsort(np.abs(X))[::-1]
	seleccionados: set[int] = set()
	compressed_ordered: list[tuple[int, complex]] = []
	energia_acum = 0.0

	def add_index(k: int):
		nonlocal energia_acum
		if k in seleccionados:
			return
		seleccionados.add(k)
		compressed_ordered.append((k, X[k]))
		energia_acum += float(np.abs(X[k]) ** 2)

	for k in idx_sorted:
		k = int(k)
		add_index(k)

		if preserve_real:
			# Agregar el par conjugado (excepto DC y Nyquist)
			k_pair = (-k) % N
			if k_pair != k:
				add_index(int(k_pair))

		if energia_acum >= limite:
			break

	compressed = compressed_ordered
	energy_ratio = 0.0 if energia_total == 0 else float(energia_acum / energia_total)

	return {
		"N": N,
		"fft_time_s": float(fft_time),
		"X_full": X,
		"compressed": compressed,
		"energy_total": energia_total,
		"energy_kept": float(energia_acum),
		"energy_ratio": energy_ratio,
		"k_count": int(len(compressed)),
		"energy_threshold": float(energy_threshold),
		"use_custom_fft": bool(use_custom_fft and _is_power_of_two(N)),
		"preserve_real": bool(preserve_real),
	}


# Wrappers en español
def comprimir(x, porcentaje_energia: float = 0.95, use_custom_fft: bool = True) -> list[tuple[int, complex]]:
	"""Compresión por energía en el dominio de la frecuencia.

	- Calcula FFT
	- Calcula energía total: sum(|X[k]|^2)
	- Ordena coeficientes por |X[k]| (descendente)
	- Selecciona los primeros hasta alcanzar el porcentaje indicado

	Salida: lista de tuplas (k, X[k])
	"""
	x = _to_1d_real_array(x)
	if not (0 < porcentaje_energia <= 1.0):
		raise ValueError("porcentaje_energia debe estar en (0, 1]")

	X, _ = _fft(x, use_custom_fft=use_custom_fft)
	N = int(X.size)

	# Energía total en frecuencia
	energia_total = float(np.sum(np.abs(X) ** 2))
	limite = float(porcentaje_energia) * energia_total

	# Crear lista de (k, X[k])
	lista = [(k, X[k]) for k in range(N)]

	# Ordenar por magnitud descendente
	lista = sorted(lista, key=lambda par: abs(par[1]), reverse=True)

	energia_acumulada = 0.0
	X_compr: list[tuple[int, complex]] = []

	for (k, valor) in lista:
		energia_acumulada += float(abs(valor) ** 2)
		X_compr.append((int(k), complex(valor)))
		if energia_acumulada >= limite:
			break

	return X_compr


def reconstruir(X_comprimida: list[tuple[int, complex]], N: int, use_custom_ifft: bool = True) -> np.ndarray:
	"""Reconstruye la señal en el tiempo a partir de coeficientes comprimidos."""
	res = reconstruct_signal(X_comprimida, N=N, use_custom_ifft=use_custom_ifft)
	return res["x_rec"]


def reconstruct_signal(
	compressed: list[tuple[int, complex]],
	N: int,
	use_custom_ifft: bool = True,
) -> dict:
	"""Reconstruye señal desde coeficientes comprimidos."""
	if N <= 0:
		raise ValueError("N debe ser > 0")
	X_hat = np.zeros(int(N), dtype=complex)
	for k, val in compressed:
		X_hat[int(k)] = complex(val)

	x_rec, ifft_time = ifft_from_fft(X_hat, use_custom_fft=use_custom_ifft)
	# Idealmente real; si queda imag numérica pequeña, se descarta
	x_rec_real = np.real_if_close(x_rec, tol=1000)
	return {
		"X_hat": X_hat,
		"x_rec": np.asarray(x_rec_real, dtype=float),
		"ifft_time_s": float(ifft_time),
		"use_custom_ifft": bool(use_custom_ifft and _is_power_of_two(int(N))),
	}


def mse(x: np.ndarray, x_rec: np.ndarray) -> float:
	x = _to_1d_real_array(x)
	x_rec = _to_1d_real_array(x_rec)
	if x.size != x_rec.size:
		raise ValueError("x y x_rec deben tener el mismo tamaño")
	return float(np.mean((x - x_rec) ** 2))


def energy_time(x: np.ndarray) -> float:
	x = _to_1d_real_array(x)
	return float(np.sum(x**2))


def energy_preserved(x: np.ndarray, x_rec: np.ndarray) -> float:
	E = energy_time(x)
	if E == 0:
		return 0.0
	return float(energy_time(x_rec) / E)


def plot_original_vs_reconstructed(
	x: np.ndarray,
	x_rec: np.ndarray,
	t: np.ndarray | None = None,
	title: str | None = None,
):
	x = _to_1d_real_array(x)
	x_rec = _to_1d_real_array(x_rec)
	if x.size != x_rec.size:
		raise ValueError("x y x_rec deben tener el mismo tamaño")
	if t is None:
		t = np.arange(x.size)
	else:
		t = np.asarray(t).reshape(-1)
		if t.size != x.size:
			raise ValueError("t debe tener el mismo tamaño que x")

	plt.figure(figsize=(12, 6))
	plt.plot(t, x, label="Original", linewidth=2)
	plt.plot(t, x_rec, label="Reconstruida", linewidth=2, linestyle="--")
	plt.xlabel("Tiempo" if t is not None else "n")
	plt.ylabel("Amplitud")
	plt.title(title or "Señal original vs reconstruida")
	plt.grid(True)
	plt.legend()
	plt.tight_layout()
	plt.show()


def _demo_signal(kind: str) -> tuple[np.ndarray, np.ndarray, int]:
	kind = kind.lower().strip()
	if kind == "simple":
		fs = 128
		t = np.linspace(0, 1, fs)
		x = np.sin(2 * np.pi * 5 * t)
		return x, t, fs
	if kind == "compleja":
		fs = 1024
		t = np.linspace(0, 1, fs)
		x = (
			0.7 * np.sin(2 * np.pi * 5 * t)
			+ 0.5 * np.sin(2 * np.pi * 20 * t)
			+ 0.3 * np.sin(2 * np.pi * 60 * t)
			+ 0.2 * np.random.randn(fs)
		)
		return x, t, fs
	if kind == "voz":
		fs = 2048
		t = np.linspace(0, 1, fs)
		voz_env = np.exp(-3 * t)
		x = voz_env * (
			np.sin(2 * np.pi * 120 * t)
			+ 0.5 * np.sin(2 * np.pi * 250 * t)
			+ 0.3 * np.sin(2 * np.pi * 400 * t)
		) + 0.05 * np.random.randn(fs)
		return x, t, fs
	raise ValueError("kind debe ser: simple, compleja, voz")


def main():
	print("=" * 60)
	print("COMPRESIÓN ESPECTRAL (FFT) + RECONSTRUCCIÓN (IFFT)")
	print("=" * 60)
	print("Tests disponibles: simple | compleja | voz")
	kind = input("Digite el test a ejecutar: ").strip() or "compleja"

	x, t, fs = _demo_signal(kind)

	res_comp = compress_signal(x, energy_threshold=0.95, use_custom_fft=True)
	res_rec = reconstruct_signal(res_comp["compressed"], res_comp["N"], use_custom_ifft=True)

	mse_val = mse(x, res_rec["x_rec"])
	E_pres = energy_preserved(x, res_rec["x_rec"])

	print("-" * 60)
	print(f"N = {res_comp['N']}")
	print(f"Coeficientes retenidos = {res_comp['k_count']} (de {res_comp['N']})")
	print(
		f"Energía espectral preservada = {res_comp['energy_ratio']*100:.2f}% "
		f"(objetivo {res_comp['energy_threshold']*100:.2f}%)"
	)
	print(f"Energía (tiempo) preservada = {E_pres*100:.2f}%")
	print(f"MSE = {mse_val:.6f}")
	print(
		f"FFT custom: {res_comp['use_custom_fft']} (t={res_comp['fft_time_s']:.6f}s) | "
		f"IFFT custom: {res_rec['use_custom_ifft']} (t={res_rec['ifft_time_s']:.6f}s)"
	)
	print("-" * 60)

	plot_original_vs_reconstructed(
		x,
		res_rec["x_rec"],
		t=t,
		title=f"Original vs reconstruida ({kind}, 95% energía)",
	)


if __name__ == "__main__":
	main()