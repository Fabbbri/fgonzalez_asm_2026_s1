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


def compress(x, energy_threshold: float = 0.95, use_custom_fft: bool = True) -> list[tuple[int, complex]]:
	"""Compresión por energía en el dominio de la frecuencia.

	- Calcula FFT
	- Calcula energía total: sum(|X[k]|^2)
	- Ordena coeficientes por |X[k]| (descendente)
	- Selecciona los primeros hasta alcanzar el porcentaje indicado

	Salida: lista de tuplas (k, X[k])
	"""
	x = _to_1d_real_array(x)
	if not (0 < energy_threshold <= 1.0):
		raise ValueError("energy_threshold debe estar en (0, 1]")

	X, _ = _fft(x, use_custom_fft=use_custom_fft)
	N = int(X.size)

	# Energía total en frecuencia
	energia_total = float(np.sum(np.abs(X) ** 2))
	limite = float(energy_threshold) * energia_total

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