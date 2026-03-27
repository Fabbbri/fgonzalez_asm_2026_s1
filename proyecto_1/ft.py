import numpy as np
import matplotlib.pyplot as plt
import time

# Archivo que contiene la función de DFT, FFT y para graficar
# ver pseudocódigo en DOCUMENTACION.md

def dft(x):
    '''
    Entrada: señal x (muestras discretas)
    Salida: Componente en frecuencia k
    '''
    inicio = time.time()
    N = len(x) # número de muestras
    X = [0j]*N # componente en frecuencia compleja de tamaño N

    for k in range (0, N): # 0, 1, ... N-1
        suma = 0 + 0j

        for n in range (0, N):
            # angulo del exponencial
            angulo = -2*np.pi*k*n/N
            # forma polar del exponencial
            polar = np.cos(angulo) + 1j*np.sin(angulo)

            suma += x[n] * polar

        X[k] = suma

    final = time.time() - inicio
    
    return X, final


def graficar_dft(x, t, fs):
    '''
    Recibe una señal
    vector eje tiempo de muestreo 
    frecuencia fs de muestreo
    '''
    print("="*40)
    print("DFT: Transformada Discreta de Fourier")
    print("="*40)

    # Calcular la dft: transforma al dominio de frecuencia 
    X, tiempo_dft = dft(x)

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
    # GRAFICAR
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