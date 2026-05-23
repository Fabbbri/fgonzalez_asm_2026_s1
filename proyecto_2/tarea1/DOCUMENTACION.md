# Proyecto Grupal 2: Diseño de un sistema de control de un ascensor

Mediante el desarrollo de este proyecto, los estudiantes deben generar un sistema ascensor de no menos de 5 pisos. La planta va a estar constituida por un motor de CD y un potenciómetro (u otro sensor pactado con el profesor) para medir el piso en el que está. 

No es necesario coloar un objeto en la cabina del ascensor. La construcción del ascensor es libre y a conveniencia del grupo de trabajo. El sistema completo sería visualizado en la siguiente figura:

![Diagrama de Bloques](images\diag_sistema.png)

Mediante el desarrollo de este proyecto, el estudiante aplicará los conceptos de análisis de señales, 

Atributos relacionados: Análisis de Problemas (AP).

## 1. Estructura General Propuesta

1. `Botones para ascensor:` Son los botones y una cantidad discreta de valores que es relativo a la cantidad de pisos del ascensor.

2. `Comparador:` Se encarga de extraer el error a partir de la diferencia entre el potenciometro (o sensor de medición) y la medición del piso deseado.

3. `Controlador:` Es un controlador PID desarrollado por el grupo de trabajo. Este será implementado en un microcontrolador.

4. `Motor DC:` Motor en corriente directa que sube o baja el ascensor.

5. `Cabina:` Elemento que sube o baja relativo al movimiento del motor DC. La cabina debe ser de al menos un cubo de 10 cm × 10 cm × 10 cm. Procure que cada piso sea de más de 10 cm. Estas medidas pueden ser modificadas con acuerdo previo del profesor.

6. `Pot. 2:` Es el potenciómetro que da la altura del ascensor. Puede ser algún otro pactado anteriormente con el profesor.

El sistema sin controlador muestra un movimiento impreciso que genera movimientos erráticos, el controlador ayudará para que el movimiento sea preciso. El sistema se debe mover rápidamente al punto y no tener errores desde el movimiento actual a la posición deseada.



## Bitácora de Implementación
