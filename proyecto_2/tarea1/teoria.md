# Investigación Teórica

## 1. ¿Por qué es importante la respuesta al impulso y al escalón para modelar sistemas?

### 1.1. Respuesta el impulso

Los `sistemas LTI (Linealmente Independientes en el Tiempo)` son accesibles al análisis por varias razones. Una de ellas es que poseen la priopiedad de `superposición`, permitiendo representar al sistema en términos de una combinación lineal de un conjunto de señales básicas y calcular la salida en términos de sus respuestas a señales básicas. 

Una de las señales básicas más utilizadas es el `Impulso Delta Dirac (impulso unitario)`, tanto discreto como continuo, puesto que las señales en general se pueden representar como la combinación lineal de impulsos retardados. 

Así, es posible realizar una `caracterización completa de cualquier sistema LTI en términos de su respuesta a un impulso unitario`. Esto es llamado la convolución (sumatoria en discreto, integral en continuo)

Esto puede verse a través de la siguiente ecuación llamada `propiedad de selección del impulso unitario discreto`:

```math
x[n] = \sum_{k= - \inf}^{+\inf} x[k] \delta[n-k]            
```

Note que esta ecuación permite representar cualquier función como la suma de impulsos unitarios desplazados donde los pesos o coeficientes de cada impulso unitario es `x[k]`. Si `x[n] = u[n]` se vería de esta forma: 

```math
u[n] = \sum_{k= - \inf}^{+\inf} \delta[n-k]
```

Por ejemplo, observe la siguiente figura que representa una función x[n]. Note que puede representarse en base a impulsos.

![Descomposición de una señal discreta en una suma ponderada de impulsos desplazados](images/conv_imp_ut.png)

Ahora bien, considere una entrada arbitraria `x[n]`, la cuál puede representarse como suma de impulsos (ecuación de propiedad de selección). 

De esta forma, para sistemas se puede definir que la respuesta/salida (`y[n]`) de un sistema a una entrada arbitraria `x[n]` será la superposición de las respuestas escaladas al sistema a cada uno de estos impulsos desplazados, o sea: 

1. La entrada descompuesta en una combinación lineal de impulsos desplazados, de forma que cada impulso sea un `x[k]`
2. La respuesta ya conocida del sistema lineal al impulso unitario desplazado, o sea, h[n] es la respuesta al sistema LTI cuando la entrada es delta dirac:

```math
h_k[n] --> \delta[n-k]
```

de forma que se puede resumir la salida de un LTI en la siguiente ecuación:

```math
y[n] = \sum_{k= - \inf}^{+\inf} x[k] h_k[n]
```

De esta forma, si ya conocemos la respuesta de nuestro sistema LTI al conjunto de impulsos unitarios desplazados (delta dirac), puede decirse que un sistema LTI se caracteriza completamente por su respuesta a esa señal, por lo que, podemos `construir` la respuesta a cualquier entrada arbitraria.

Si el sistema no es invariante en el tiempo, las respuestas `h_k[n]` no tienen que estar relacionadas entre sí, pero si el sistema es LTI, entonces cada elemento es simplemente el mismo pero desplazado en el tiempo, de forma que se obtiene la ecuación llamada `suma de convolución` o `suma de superposición`:

```math
y[n] = \sum_{k= - \inf}^{+\inf} x[k] h[n-k]
```

Y la siguiente la `convolución de secuencias`

```math
y[n] =  x[k] * h[n]
```

![Respuesta y[n] vista como la combinación lineal de 2h[n-1] y 0.5h[n]](/proyecto_2/images/yn_ej.png)

### 1.1.2. Propiedades en sistemas LTI con respuesta al impulso.

Otro aspecto importante de los sistemas LTI caracterizados en base a la respuesta al impulso es la propiedad de causalidad y estabilidad. 

La causalidad implica que: 

"la salida de un sistema causal
depende sólo de los valores presentes y pasados de la entrada al mismo."

Esto puede verse en la siguiente relación:

```math
y[n] \text{ no depende de } x[k] \text{ para k>n }
```

```math
h[n] = 0 \text{ , para n<0}
```

Para que esta condición se cumpla, se tiene que todos los coaeficientes `h[n-k]` (para k > n) deben ser iguales a cero.

En otras palabras, la respuesta al impulso de un sistema LTI causal DEBE ser cero antes de que ocurra el impulso. Esto (específicamente en sistmeas LTI) es equivalente a la condición de `reposo inicial`, es decir, si la entrada es 0 hasta algún punto en el tiempo, entonces la salida debe ser 0 hasta ese tiempo.

Si el sistema es causal, entonces la ecuación `suma de convolución` se convierte en:

```math
y[n] = \sum_{k= - \inf}^{n} x[k] h_k[n]
```

Por otro lado, un sistema es estable si cada entrada limitada produce una salida limitada.

En sistemas LTI con respuesta al impulso, se dice que la salida `y[n]` está limitada en magnitud (y por consiguiente, es estable) si se cumple la siguiente inecuación:

```math
\sum_{k= - \inf}^{+\inf} | h_k[n] | < \inf
```

### 1.2. Respuesta al escalón unitario

Se ha visto que `h[n]` determina por completo el comportamiento de un sistema LTI, es posible relacionar las propiedades de estabilidad y causalidad de un sistema con las propiedades de la respuesta al impulso.

Sin embargo, hay otra forma de caracterizar el comportamiento de un sistema LTI y esa es utilizando la respuesta al escalón unitario `s[n]`, el cual corresponde a la salida del sistema cuadno `x[n] = u[n]`. 

Lógicamente, la respuesta al escalón de un sistema LTI está dado por la convolución del escalón con la respuesta al impulso:

```math
s[n] = u[n] * h[n]
```

Por conmutatividad, `s[n]` puede verse como la respuesta a la entrada (en este caso, `h[n]`) de un sistema LTI con respuesta al impulso unitario `u[n]`:

```math
s[n] = h[n] * u[n]
```

De esta forma, la respuesta al escalón de un sistema LTI es la sumatoria consecutiva de su respuesta al impulso. 

```math
s[n] = \sum_{k= - \inf}^{n} h[k]
```

La ecuación de sumatoria de convolución para un sistema LTI con respuesta al escalón unitario es:

```math
x[n] = \sum_{k= - \inf}^{+\inf} x[n]s[n]
```


## Pregunta 2