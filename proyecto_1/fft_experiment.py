# Archivo main para aspectos del punto 2 fft-experimental

import dft # implementación propia
import matplotlib.pyplot as plt
import numpy as np

# =====================
# SEÑAL A PROBAR
# =====================

# Eje de TIEMPO muestreado a 128
fs = 128 # frecuencia de muestreo
t = np.linspace(0, 1, fs)

# señal senoidal en el tiempo
# frecuencia 5 Hz
x = np.sin(2*np.pi*5*t)  

# =====================
# Función principal
# =====================

if __name__ == '__main__':

    print("="*40)
    print("DFT: Transformada Discreta de Fourier")
    print("="*40)

    # Calcular la dft: transforma al dominio de frecuencia 
    X, tiempo_dft = dft.dft(x)

    print(f"DFT ejecutado exitosamente \nDuración: {tiempo_dft}")

    # Calcular la fft con numpy para verificar
    X_np = np.fft.fft(x)

    print("Error obtenido:", np.linalg.norm(np.array(X) - X_np))
    print("="*40)

    frecuencias = np.fft.fftfreq(len(x), d=1/fs)
    
    # Datos obtenibles de salida
    magnitud = np.abs(X)
    fase = np.angle(X)
    energia = np.sum(np.abs(x)**2)
    print(f"Energía de la señal: {energia:.4f}")

    # Centrar
    frecuencias = np.fft.fftshift(frecuencias)
    magnitud = np.fft.fftshift(magnitud)
    fase = np.fft.fftshift(fase)

    # ==============================
    # GRAFICAS
    # ==============================

    plt.figure(figsize=(12, 8))

    # 1. Señal original
    plt.subplot(3, 1, 1)
    plt.plot(t, x, label="x(t) = sin(2π·5t)")
    plt.title("Señal en el tiempo")
    plt.xlabel("Tiempo (s)")
    plt.ylabel("Amplitud")
    plt.legend()
    plt.grid()

    # 2. Magnitud
    plt.subplot(3, 1, 2)
    plt.plot(frecuencias, magnitud, label="|X[k]|")
    plt.title("Espectro de magnitud")
    plt.xlabel("Frecuencia (Hz)")
    plt.ylabel("Magnitud")
    plt.xlim(-15, 15) # zona de relevancia (picos en -5 y 5)
    plt.legend()
    plt.grid()

    # 3. Fase
    plt.subplot(3, 1, 3)
    plt.plot(frecuencias, fase, label="Fase ∠X[k]")
    plt.title("Espectro de fase")
    plt.xlabel("Frecuencia (Hz)")
    plt.ylabel("Fase (rad)")
    plt.xlim(-15, 15) # zona de relevancia (picos en -5 y 5)
    plt.legend()
    plt.grid()

    plt.tight_layout()
    plt.show()