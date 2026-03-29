import numpy as np
import matplotlib.pyplot as plt
import time

# Archivo que contiene la función de DFT, FFT y para graficar
# ver pseudocódigo en DOCUMENTACION.md

# TESTED
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


# TESTED
def fft(x):
    '''
    Transformada Rápida de Fourier Recursiva

    Entrada: Entrada: señal x (muestras discretas)
    Salida: Componente en frecuencia k
    '''
    inicio = time.time()

    N = len(x)

    if N <= 0:
        raise TypeError("La señal no puede ser vacía")
    elif N == 1:
        final = time.time() - inicio
        return [x[0]], final
    
    if N % 2 != 0:
        raise TypeError("FFT requiere N potencia de 2")
    

    pares = []
    impares = []

    for i in range(N): 
        if i % 2 == 0:
            pares.append(x[i])
        else:
            impares.append(x[i])
    
    # Llamadas recursivas
    x_pares, _ = fft(pares)
    x_impares, _ = fft(impares)

    X = [0j]*N
    for k in range(N//2):  
        # factor de twiddle
        angulo = -2*np.pi*k/N
        W_REAL = np.cos(angulo)
        W_IMAG = np.sin(angulo)

        W = complex(W_REAL, W_IMAG)

        t = W * x_impares[k]

        # Parte superior
        X[k] = x_pares[k] + t

        # Parte inferior
        X[k + (N//2)] = x_pares[k] - t

    final = time.time() - inicio

    return X, final  



#IMPLEMENTAR
def comprimir():
    '''
    Entrada: 
    Salida: 
    '''

#IMPLEMENTAR
def ifft():
    '''
    Entrada:
    Salida: 
    '''

# TESTED
def graficar_transformada(x, t, fs, metodo="dft"):
    '''
    Recibe una señal
    vector eje tiempo de muestreo 
    frecuencia fs de muestreo
    
    metodo:
        "dft" -> función dft(x)
        "fft" -> función fft(x)
    '''
    metodo = metodo.lower()

    # Qué método de transformada usar?
    if metodo == "dft":
        print("-" * 40)
        print("DFT: Transformada Discreta de Fourier")
        print("-" * 40)
        X, tiempo = dft(x)
        titulo_mag = "Espectro de magnitud"
        titulo_fase = "Espectro de fase"

    elif metodo == "fft":
        print("-" * 40)
        print("FFT: Transformada Rápida de Fourier")
        print("-" * 40)
        X, tiempo = fft(x)
        titulo_mag = "Espectro de magnitud (FFT)"
        titulo_fase = "Espectro de fase (FFT)"

    else:
        raise ValueError("El método debe ser 'dft' o 'fft'")

    print(f"{metodo.upper()} ejecutado exitosamente")
    print(f"Duración: {tiempo:.6f} segundos")

    # Verificación con NumPy
    X_np = np.fft.fft(x)
    print("Error obtenido:", np.linalg.norm(np.array(X) - X_np))
    print("-" * 40)

    # ==============================
    # Preparar datos
    # ==============================
    frecuencias = np.fft.fftfreq(len(x), d=1/fs)
    magnitud = np.abs(X)
    fase = np.angle(X)
    energia = np.sum(np.abs(x)**2)

    print(f"Energía de la señal: {energia:.4f}")

    # Centrar 
    frecuencias = np.fft.fftshift(frecuencias)
    magnitud = np.fft.fftshift(magnitud)
    fase = np.fft.fftshift(fase)

    # ==============================
    # Graficar
    # ==============================
    plt.figure(figsize=(12, 8))

    # 1. Señal original
    plt.subplot(3, 1, 1)
    plt.plot(t, x, label="x(t)")
    plt.title("Señal en el tiempo")
    plt.xlabel("Tiempo (s)")
    plt.ylabel("Amplitud")
    plt.legend()
    plt.grid()

    # 2. Magnitud
    plt.subplot(3, 1, 2)
    plt.plot(frecuencias, magnitud, label="|X[k]|")
    plt.title(titulo_mag)
    plt.xlabel("Frecuencia (Hz)")
    plt.ylabel("Magnitud")
    plt.legend()
    plt.grid()

    # 3. Fase
    plt.subplot(3, 1, 3)
    plt.plot(frecuencias, fase, label="Fase ∠X[k]")
    plt.title(titulo_fase)
    plt.xlabel("Frecuencia (Hz)")
    plt.ylabel("Fase (rad)")
    plt.legend()
    plt.grid()

    # Esto cambio un poco para hacer que se pueda graficar ambos metodos a la vez
    plt.tight_layout()
    plt.show()

def comparar_dft_fft(x, t, fs):
    '''
    Compara el rendimiento de DFT vs FFT
    '''
    print("-"*50)
    print("COMPARACIÓN DFT vs FFT")
    print("-"*50)
    
    # Ejecutar DFT
    print("\nEjecutando DFT...")
    X_dft, tiempo_dft = dft(x)
    print(f"DFT completado en: {tiempo_dft:.6f} segundos")
    
    # Ejecutar FFT
    print("\nEjecutando FFT...")
    X_fft, tiempo_fft = fft(x)
    print(f"FFT completado en: {tiempo_fft:.6f} segundos")
    
    # Comparar resultados
    error = np.linalg.norm(np.array(X_dft) - np.array(X_fft))
    print(f"\nError entre DFT y FFT: {error:.10f}")
    
    # Mejora de velocidad
    if tiempo_fft > 0:
        speedup = tiempo_dft / tiempo_fft
        print(f"FFT es {speedup:.2f}x más rápido que DFT")
    
    # Verificar con numpy
    X_np = np.fft.fft(x)
    error_dft_np = np.linalg.norm(np.array(X_dft) - X_np)
    error_fft_np = np.linalg.norm(np.array(X_fft) - X_np)
    
    print(f"\nError DFT vs NumPy: {error_dft_np:.10f}")
    print(f"Error FFT vs NumPy: {error_fft_np:.10f}")
    print("-"*50)


def benchmark_tiempos(tamaño=[32, 64, 128, 256, 512, 1024]):
    '''
    Realiza un benchmark comparando tiempos de DFT vs FFT
    para diferentes tamaños de señal
    '''
    print("-"*60)
    print("BENCHMARK: Comparación de Tiempos DFT vs FFT")
    print("-"*60)
    
    tiempos_dft = []
    tiempos_fft = []
    
    for N in tamaño:
        print(f"\nProbando con N = {N} muestras...")
        
        # Generar señal de prueba
        t = np.linspace(0, 1, N)
        x = np.sin(2*np.pi*5*t) + 0.5*np.sin(2*np.pi*10*t)
        
        # Medir DFT
        print(f"  Ejecutando DFT...", end=" ")
        X_dft, t_dft = dft(x)
        tiempos_dft.append(t_dft)
        print(f"{t_dft:.6f}s")
        
        # Medir FFT
        print(f"  Ejecutando FFT...", end=" ")
        X_fft, t_fft = fft(x)
        tiempos_fft.append(t_fft)
        print(f"{t_fft:.6f}s")
        
        # Speedup
        if t_fft > 0:
            speedup = t_dft / t_fft
            print(f"  Speedup: {speedup:.2f}x")
    
    # ==============================
    # GRAFICAR RESULTADOS
    # ==============================
    
    print("\n" + "-"*60)
    print("Generando gráfica...")
    print("-"*60)
    
    # Calcular speedups para el resumen
    speedups = [t_dft/t_fft if t_fft > 0 else 0 for t_dft, t_fft in zip(tiempos_dft, tiempos_fft)]
    
    # Gráfica 
    plt.figure(figsize=(12, 6))
    plt.plot(tamaño, tiempos_dft, 'o-', label='DFT', linewidth=2, markersize=10, color='#e74c3c')
    plt.plot(tamaño, tiempos_fft, 's-', label='FFT', linewidth=2, markersize=10, color='#3498db')
    plt.xlabel('Tamaño de la señal (N)', fontsize=12)
    plt.ylabel('Tiempo (segundos)', fontsize=12)
    plt.title('Comparación de Tiempo de Ejecución: DFT vs FFT', fontsize=14, fontweight='bold')
    plt.legend(fontsize=12)
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.show()
    
    # ==============================
    # RESUMEN ESTADÍSTICO
    # ==============================
    
    print("\n" + "-"*60)
    print("RESUMEN ESTADÍSTICO")
    print("-"*60)
    print(f"Speedup promedio: {np.mean(speedups):.2f}x")
    print(f"Speedup máximo: {np.max(speedups):.2f}x (N={tamaño[np.argmax(speedups)]})")
    print(f"Speedup mínimo: {np.min(speedups):.2f}x (N={tamaño[np.argmin(speedups)]})")
    print("-"*60)