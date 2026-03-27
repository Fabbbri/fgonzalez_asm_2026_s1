# Proyecto Grupal 1: Diseño de un sistema de compresión espectral adaptativa en tiempo real

Mediante el desarrollo de este proyecto, el estudiante aplicará los conceptos de análisis de señales mixtas, variable compleja, ortogonalidad, series de Fourier, transformada discreta de Fourier (DFT), transformada rápida de Fourier (FFT), sistemas lineales e invariantes en el tiempo (LTI) y convolución. Atributos relacionados: Análisis de Problemas (AP) y Herramientas de Ingeniería (HI).

## 1. Objetivo General

Generar un sistema de compresión y reconstrucción espectral en tiempo real utilizando la `Transformada Rápida de Fourier (FFT)`. Esto se realizará con 3 microcontroladores.

1. `Transmisor`: Captura una señal analógica (tono seno o señal compuesta definida), calcula su `FFT` y realiza `compresión espectral adaptativa` conservando únicamente las `componentes de mayor energía`. Posteriormente transmite únicamente los `coeficientes espectrales` seleccionados a los otros microcontroladores.

2. `Receptor 1`: Reconstruye la señal en el `dominio del tiempo` utilizando la transformada inversa (`IFFT`) y la reproduce en un parlante mediante `DAC` o `PWM filtrado`. Se busca que la señal sea muy similar a la original.

3. `Receptor 2`: Reconstruye la señal y calcula métricas cuantitativas como `Error Cuadrático Medio (MSE)`, `energìa preservada` y `relación señal a error`. Los resultados se muestran en una pantalla LCD conectada al microcontrolador.

Cada una de estas etapas requiere de su implementación programada y en hardware. 

## 2. Experimentos con FFT

El objetivo de esta sección en explicar como se implementan en Python los algoritmos `DFT` y `FFT`, la representación compleja de señales además de analizar magnitud y fase.

### 2.1. DFT



