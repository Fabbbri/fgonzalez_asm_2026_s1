# Archivo main para aspectos del punto 2 fft-experimental

import ft as ft # implementación propia de Transformadas de Fourier
import signal_proc as ps # procesamiento de señales (compresión/reconstrucción)
import matplotlib.pyplot as plt
import numpy as np

# =====================
# SEÑALES GLOBALES
# =====================

# Señal Simple
FS_SIMPLE = 128
T_SIMPLE = np.linspace(0, 1, FS_SIMPLE)
X_SIMPLE = np.sin(2*np.pi*5*T_SIMPLE)

# Señal Compleja
FS_COMPLEJO = 1024
T_COMPLEJO = np.linspace(0, 1, FS_COMPLEJO)
X_COMPLEJO = (
    0.7*np.sin(2*np.pi*5*T_COMPLEJO) +     # componente baja
    0.5*np.sin(2*np.pi*20*T_COMPLEJO) +    # media
    0.3*np.sin(2*np.pi*60*T_COMPLEJO) +    # alta
    0.2*np.random.randn(FS_COMPLEJO)       # ruido
)

# Señal Voz
FS_VOZ = 2048
T_VOZ = np.linspace(0, 1, FS_VOZ)
VOZ_EMULATE = np.exp(-3*T_VOZ)
X_VOZ = VOZ_EMULATE * (
    np.sin(2*np.pi*120*T_VOZ) +
    0.5*np.sin(2*np.pi*250*T_VOZ) +
    0.3*np.sin(2*np.pi*400*T_VOZ)
) + 0.05*np.random.randn(FS_VOZ)


# =====================
# HELPER: Compresión + Reconstrucción + Reporte
# =====================

def _ejecutar_compresion(x, t, fs, nombre):
    print("-" * 60)

    X_comprimida = ps.comprimir(x, energy_threshold=0.95)
    N = len(x)
    res_rec = ps.reconstruct_signal(X_comprimida, N=N, use_custom_ifft=True)

    mse_val = ps.mse(x, res_rec["x_rec"])
    E_pres  = ps.energy_preserved(x, res_rec["x_rec"])

    print(f"N = {N}")
    print(f"Coeficientes retenidos    = {len(X_comprimida)} (de {N})")
    print(f"Energía (tiempo) preservada  = {E_pres*100:.2f}%")
    print(f"MSE = {mse_val:.6f}")
    print("-" * 60)

    ps.plot_original_vs_reconstructed(
        x, res_rec["x_rec"], t=t,
        title=f"Original vs reconstruida ({nombre}, 95% energía)"
    )

# =====================
# MENÚ DE PRUEBAS
# =====================

def elegir_test():

    test = -1

    while test != 0:
        print("="*50)
        print("MENÚ DE PRUEBAS")
        print("="*50)
        print("DFT (Transformada Discreta de Fourier):")
        print("  1 - Test Simple con DFT")
        print("  2 - Test Complejo con DFT")
        print("  3 - Test Voz con DFT")
        print("\nFFT (Transformada Rápida de Fourier):")
        print("  4 - Test Simple con FFT")
        print("  5 - Test Complejo con FFT")
        print("  6 - Test Voz con FFT")
        print("\nComparación DFT vs FFT:")
        print("  7 - Comparar DFT vs FFT (Test Simple)")
        print("  8 - Comparar DFT vs FFT (Test Complejo)")
        print("  9 - Gráfico de Benchmark de Tiempos DFT vs FFT")
        print("\nCompresión y Reconstrucción Espectral (FFT + IFFT):")
        print(" 10 - Compresión/Reconstrucción (Señal Simple)")
        print(" 11 - Compresión/Reconstrucción (Señal Compleja)")
        print(" 12 - Compresión/Reconstrucción (Emulación de Voz)")
        print("\n  0 - Salir del simulador")
        print("="*50)

        test = int(input("Digite el número de test a ejecutar: "))

        if test == 0:
            print("Se ha elegido salir del simulador")

        elif test == 1:
            print("\nTest 1 elegido: Señal simple con DFT")
            print("Para salir del test cierre la ventana de la gráfica")
            ft.graficar_transformada(X_SIMPLE, T_SIMPLE, FS_SIMPLE, metodo="dft")

        elif test == 2:
            print("\nTest 2 elegido: Señal compleja con DFT")
            print("Para salir del test cierre la ventana de la gráfica")
            ft.graficar_transformada(X_COMPLEJO, T_COMPLEJO, FS_COMPLEJO, metodo="dft")

        elif test == 3:
            print("\nTest 3 elegido: Emulación de voz con DFT")
            print("Para salir del test cierre la ventana de la gráfica")
            ft.graficar_transformada(X_VOZ, T_VOZ, FS_VOZ, metodo="dft")

        elif test == 4:
            print("\nTest 4 elegido: Señal simple con FFT")
            print("Para salir del test cierre la ventana de la gráfica")
            ft.graficar_transformada(X_SIMPLE, T_SIMPLE, FS_SIMPLE, metodo="fft")

        elif test == 5:
            print("\nTest 5 elegido: Señal compleja con FFT")
            print("Para salir del test cierre la ventana de la gráfica")
            ft.graficar_transformada(X_COMPLEJO, T_COMPLEJO, FS_COMPLEJO, metodo="fft")

        elif test == 6:
            print("\nTest 6 elegido: Emulación de voz con FFT")
            print("Para salir del test cierre la ventana de la gráfica")
            ft.graficar_transformada(X_VOZ, T_VOZ, FS_VOZ, metodo="fft")

        elif test == 7:
            print("\nTest 7 elegido: Comparación DFT vs FFT (Señal Simple)")
            ft.comparar_dft_fft(X_SIMPLE, T_SIMPLE, FS_SIMPLE)

        elif test == 8:
            print("\nTest 8 elegido: Comparación DFT vs FFT (Señal Compleja)")
            ft.comparar_dft_fft(X_COMPLEJO, T_COMPLEJO, FS_COMPLEJO)

        elif test == 9:
            print("\nTest 9 elegido: Gráfico de Benchmark de Tiempos DFT vs FFT")
            print("="*60)
            print("Este test comparará DFT vs FFT con múltiples tamaños")
            print("Tamaños: 32, 64, 128, 256, 512, 1024")
            print("="*60)
            ft.benchmark_tiempos([32, 64, 128, 256, 512, 1024])

        elif test == 10:
            print("\nTest 10 elegido: Compresión/Reconstrucción (Señal Simple)")
            print("Para salir del test cierre la ventana de la gráfica")
            _ejecutar_compresion(X_SIMPLE, T_SIMPLE, FS_SIMPLE, "simple")

        elif test == 11:
            print("\nTest 11 elegido: Compresión/Reconstrucción (Señal Compleja)")
            print("Para salir del test cierre la ventana de la gráfica")
            _ejecutar_compresion(X_COMPLEJO, T_COMPLEJO, FS_COMPLEJO, "compleja")

        elif test == 12:
            print("\nTest 12 elegido: Compresión/Reconstrucción (Emulación de Voz)")
            print("Para salir del test cierre la ventana de la gráfica")
            _ejecutar_compresion(X_VOZ, T_VOZ, FS_VOZ, "voz")

        else:
            print("Error, debe digitar un número entre 0 y 12")


# =====================
# Función principal
# =====================

if __name__ == '__main__':
    elegir_test()