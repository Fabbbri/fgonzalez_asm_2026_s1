# Archivo main para aspectos del punto 2 fft-experimental

import ft as ft # implementación propia de Transformadas de Fourier
import matplotlib.pyplot as plt
import numpy as np

# =====================
# SEÑALES A PROBAR
# =====================

def elegir_test():

    test = -1

    while test != 0:
        print("="*50)
        test = int(input("Digite el número de test a ejecutar\n[Simple(1), Complejo(2), Voz(3) o Salir del simulador (0)]: "))

        if test == 0:
            print("Se ha elegido salir del simulador")

        elif test==1:
            # Prueba 1
            print("Test 1 elegido: para salir del test y elegir otro test o salir")
            print("cierre la ventana de la gráfica generada")
            # Eje de TIEMPO muestreado a 128
            fs = 128 # frecuencia de muestreo
            t = np.linspace(0, 1, fs)

            # señal senoidal en el tiempo
            # frecuencia 5 Hz
            x = np.sin(2*np.pi*5*t)  

            ft.graficar(x, t, fs)

        elif test == 2:
            # Prueba 2
            print("Test 2 elegido: para salir del test y elegir otro test o salir")
            print("cierre la ventana de la gráfica generada")
            fs = 1024  # más resolución
            t = np.linspace(0, 1, fs)

            # señal con varias frecuencias + ruido
            x = (
                0.7*np.sin(2*np.pi*5*t) +     # componente baja
                0.5*np.sin(2*np.pi*20*t) +    # media
                0.3*np.sin(2*np.pi*60*t) +    # alta
                0.2*np.random.randn(fs)       # ruido
            )

            ft.graficar(x, t, fs)

        elif test ==3:
            # Prueba 3
            print("Test 3 elegido: para salir del test y elegir otro test o salir")
            print("cierre la ventana de la gráfica generada")
            fs = 2048
            t = np.linspace(0, 1, fs)

            # exponencial para emular voz
            voz_emulate = np.exp(-3*t)

            x = voz_emulate * (
                np.sin(2*np.pi*120*t) +
                0.5*np.sin(2*np.pi*250*t) +
                0.3*np.sin(2*np.pi*400*t)
            ) + 0.05*np.random.randn(fs)

            ft.graficar(x, t, fs)

        else:
            print("Error, debe digitar un número entre 0 y 3")

# =====================
# Función principal
# =====================

if __name__ == '__main__':


    elegir_test()