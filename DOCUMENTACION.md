# Proyecto Grupal 1: Diseño de un sistema de compresión espectral adaptativa en tiempo real

Mediante el desarrollo de este proyecto, el estudiante aplicará los conceptos de análisis de señales mixtas, variable compleja, ortogonalidad, series de Fourier, transformada discreta de Fourier (DFT), transformada rápida de Fourier (FFT), sistemas lineales e invariantes en el tiempo (LTI) y convolución. Atributos relacionados: Análisis de Problemas (AP) y Herramientas de Ingeniería (HI).

## 1. Objetivo General

Generar un sistema de compresión y reconstrucción espectral en tiempo real utilizando la `Transformada Rápida de Fourier (FFT)`. Esto se realizará con 3 microcontroladores.

1. `Transmisor`: Captura una señal analógica (tono seno o señal compuesta definida), calcula su `FFT` y realiza `compresión espectral adaptativa` conservando únicamente las `componentes de mayor energía`. Posteriormente transmite únicamente los `coeficientes espectrales` seleccionados a los otros microcontroladores.

2. `Receptor 1`: Reconstruye la señal en el `dominio del tiempo` utilizando la transformada inversa (`IFFT`) y la reproduce en un parlante mediante `DAC` o `PWM filtrado`. Se busca que la señal sea muy similar a la original.

3. `Receptor 2`: Reconstruye la señal y calcula métricas cuantitativas como `Error Cuadrático Medio (MSE)`, `energìa preservada` y `relación señal a error`. Los resultados se muestran en una pantalla LCD conectada al microcontrolador.

Cada una de estas etapas requiere de su implementación programada y en hardware. 

## 2. Experimentos con DFT

El objetivo de esta sección en explicar como se implementan en Python los algoritmos `DFT` y `FFT`, la representación compleja de señales además de analizar magnitud y fase.

### 2.1. Teoría de DFT

La `Transformada Discreta de Fourier (DFT)`:

```
Entrada: Señal en el tiempo x[n]

Salida: Componentes en frecuencia de la señal (magnitud, fases, energía por frecuencia k) X[k]
``` 

En el sistema del proyecto el transmisor captura una señal (por ejemplo, un seno o mezcla de tonos). La señal puede entenderse como una secuencia `discreta`:

```python
𝑥[0],x[1],x[2], ... , x[N-1]
```

La `DFT` transforma esa secuencia discreta en:

```python
X[0],X[1],X[2], ... , X[N-1]
```

Donde `X[k]` es un número complejo (con magnitud y fase) e indica `que tan presente` está la frecuencia `k` en la señal.

Responde a "¿cuánta de la `k` frecuencia hay en la señal?"

R/ Magnitud (qué tan fuerte es la frecuencia), fase (desplazamiento en el tiempo).

Esto es bastante útil porque con `DFT` es posible `descartar` los coeficientes `X[k]` de menor magnitud (compresión), con el fin de tener una transmisión más eficiente de los datos (menor ancho de banda). 

Se descartan los de menor magnitud porque la `energía` está relacionada con $|X[k]|^2$.

Ahora bien, como no se envía toda la señal, se depende de una aproximación de la señal para reconstruirla. Por esta razón hay que medir:

- `MSE` (error cuadrático medio)
- Energía preservada
- Relación señal-error

### 2.2. Pseudocódigo de DFT

La fórmula de la `DFT` corresponde a: 

```math
X[k] = \sum_{n=0}^{N-1} x[n]e^{-j\frac{2\pi}{N}kn}
```

- `x[n]`: señal en el tiempo
- `X[k]`: componente en frecuencia
- `N`: número de muestras
- `k`: frecuencia

Se tiene que cada coeficiente X[k] es de la forma:

```math
X[k] = a + jb
```

donde para cada X[k] se tiene una:

```math
magnitud = \sqrt{a^2 + b^2}
```

```math
fase = tan^{-1}(b/a)
```

Se tiene que la energía total es:
```math
energía = \sum|x[n]|^2
```

y la energia por frecuencia está dada por:

```python
energia_frec = |X(k)^2|
```

Se tiene el siguiente pseudocódigo:

```python
function DFT(x):
    N = len(x)
    X = array de tamaño N (complejos)

    for k desde 0 hasta N-1:
        suma = 0 + 0j

        for n desde 0 hasta N-1:
            angulo = -2 * pi * k * n / N
            suma += x[n] * (cos(angulo) + j*sin(angulo))

        X[k] = suma

    return X
```

### 2.3. Teoría de FFT

La `Transformada Rápida de Fourier (FFT)` es una forma alternativa, un `algoritmo eficiente y rápido` para calcular el mismo resultado que calcula la `DFT`.

La idea general es entender el `DFT` para implementar la `FFT` y obtener obtener los `coeficientes espectrales en tiempo real` de forma más eficiente.

#### Complejidad

- `DFT:` $O(N^2)$
- `FFT:` $O(N log N)$

Note que la complejidad va a marcar una gran diferencia en tiempo de ejecución (razón por la cuál se hace la comparativa de tiempos de ejecución).

### 2.4 Pseudocódigo de FFT

El algoritmo de FFT (Cooley-Tukey) aplica el concepto de `Divide y Vencerás` (`Divide and Conquer`):

1. Divide la señal en partes más pequeñas (índices `n` pares e impares, las cuales son muestras en el tiempo de la señal)

2. Aplica DFTs para ambos.
3. Combina los resultados usando factores complejos.

```math
X[k]=DFT_{pares} +W_{N,k}​⋅DFT_{impares}
```

donde el `twiddle factor` $W_{N,k} = e^{-j\frac{2\pi}{N}kn}$

Se tiene el siguiente pseudocódigo:

```python
function FFT(x):
    N = len(x)

    # 1. Validación
    if N <= 0:
        error("Señal vacía")

    if N == 1:
        return [x[0]]

    if N no es potencia de 2:
        error("FFT requiere N potencia de 2")

    # 2. Separar en pares e impares
    pares = []
    impares = []

    for i desde 0 hasta N-1:
        if i % 2 == 0:
            pares.append(x[i])
        else:
            impares.append(x[i])

    # 3. Llamadas recursivas
    X_pares = FFT(pares)
    X_impares = FFT(impares)

    # 4. Preparar salida
    X = array de tamaño N (complejos)

    # 5. Combinar resultados
    for k desde 0 hasta (N/2 - 1):

        # Factor twiddle
        angulo = -2 * pi * k / N
        W_real = cos(angulo)
        W_imag = sin(angulo)
        W = complejo(W_real, W_imag)

        t = W * X_impares[k]

        # Parte superior
        X[k] = X_pares[k] + t

        # Parte inferior
        X[k + N/2] = X_pares[k] - t

    return X
```

## Bitácora de Implementación

### DFT - 26 de marzo del 2026

