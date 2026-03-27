# Archivo main para aspectos del punto 2 fft-experimental

import proyecto_1.ft as ft # implementación propia de Transformadas de Fourier
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

    ft.graficar_dft(x, t, fs)