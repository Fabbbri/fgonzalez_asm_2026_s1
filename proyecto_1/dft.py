import numpy as np
import time

# Archivo que contiene la función de DFT 
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