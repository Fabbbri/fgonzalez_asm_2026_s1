clc; clear; close all;

%% ================================
%  CONFIGURACION SERIAL
%  ================================

% Cambiar este puerto segun el Arduino
puerto = "COM7";
baud = 115200;

% Crear conexion serial
arduinoSerial = serialport(puerto, baud);
configureTerminator(arduinoSerial, "LF");
arduinoSerial.Timeout = 10;

% Limpiar datos viejos
flush(arduinoSerial);

% Esperar a que Arduino reinicie al abrir el puerto serial
pause(2);

%% ================================
%  PARAMETROS DEL ESCALON
%  ================================

% Valor PWM aplicado al motor
pwmValue = 150;

% Duracion de la prueba en segundos
durationSeconds = 3.0;

% Enviar comando al Arduino en formato: PWM,duracion
comando = sprintf("%d,%.2f", pwmValue, durationSeconds);
writeline(arduinoSerial, comando);

disp("Prueba iniciada...");
disp("Esperando datos del Arduino...");

%% ================================
%  LECTURA DE DATOS DESDE ARDUINO
%  ================================

data = [];

while true
    linea = strtrim(readline(arduinoSerial));

    if linea == "END"
        disp("Prueba finalizada.");
        break;
    end

    valores = sscanf(linea, "%f,%f");

    if numel(valores) == 2
        data = [data; valores'];
    end
end

% Cerrar conexion serial
clear arduinoSerial;

%% ================================
%  VALIDACION Y GUARDADO DE DATOS
%  ================================

if isempty(data)
    error("No se recibieron datos desde Arduino.");
end

% Guardar datos en archivo CSV
writematrix(data, "datos_escalon.csv");

disp("Datos guardados en datos_escalon.csv");

% Separar columnas
t = data(:,1);
v = data(:,2);

%% ================================
%  RESPUESTA AL ESCALON EXPERIMENTAL
%  ================================

figure;
plot(t, v, "LineWidth", 1.8);
grid on;
xlabel("Tiempo (s)");
ylabel("Tension del sensor (V)");
title("Respuesta al escalon experimental de la planta");

%% ================================
%  VALORES CARACTERISTICOS
%  ================================

v0 = v(1);
vss = mean(v(max(1,end-10):end));
deltaV = vss - v0;

fprintf("\n--- Resultados experimentales ---\n");
fprintf("PWM aplicado: %d\n", pwmValue);
fprintf("Duracion de la prueba: %.2f s\n", durationSeconds);
fprintf("Tension inicial: %.3f V\n", v0);
fprintf("Tension final aproximada: %.3f V\n", vss);
fprintf("Cambio de tension: %.3f V\n", deltaV);

if abs(deltaV) < 1e-6
    error("El cambio de tension es casi cero. No se puede estimar el modelo.");
end

%% ================================
%  ESTIMACION DE MODELO DE PRIMER ORDEN
%  ================================

% Normalizar la respuesta experimental
v_norm = (v - v0) / deltaV;

% Buscar el tiempo tau: cuando la respuesta alcanza el 63.2 %
target = 0.632;
[~, idx_tau] = min(abs(v_norm - target));
tau = t(idx_tau);

fprintf("Tiempo tau aproximado: %.3f s\n", tau);

if tau <= 0
    error("Tau no puede ser cero o negativo. Revise los datos experimentales.");
end

% Amplitud del escalon aplicado
A = pwmValue;

% Ganancia aproximada de la planta:
% K = cambio final de salida / amplitud del escalon
K = deltaV / A;

% Modelo aproximado de primer orden:
% G(s) = K / (tau*s + 1)
s = tf('s');
G = K / (tau*s + 1);

fprintf("\nModelo aproximado de primer orden:\n");
G

fprintf("Ganancia K aproximada: %.5f V/PWM\n", K);

%% ================================
%  COMPARACION ESCALON EXPERIMENTAL VS MODELO
%  ================================

% Crear un vector de tiempo uniforme para que step() funcione
t_model = linspace(0, max(t), length(t));

% Respuesta al escalon del modelo ante una entrada de amplitud A
[y_model, t_model] = step(A*G, t_model);

% Ajustar el modelo para que inicie en la tension inicial experimental
y_model = squeeze(y_model) + v0;

figure;
plot(t, v, "LineWidth", 1.8);
hold on;
plot(t_model, y_model, "--", "LineWidth", 1.8);
grid on;
xlabel("Tiempo (s)");
ylabel("Tension del sensor (V)");
title("Comparacion entre respuesta experimental y modelo aproximado");
legend("Datos experimentales", "Modelo de primer orden", "Location", "best");

%% ================================
%  RESPUESTA AL IMPULSO EN MATLAB
%  ================================

t_imp = linspace(0, max(t), 500);

figure;
impulse(G, t_imp);
grid on;
xlabel("Tiempo (s)");
ylabel("Amplitud");
title("Respuesta al impulso del modelo aproximado de la planta");

%% ================================
%  RESUMEN FINAL
%  ================================

fprintf("\n--- Resumen ---\n");
fprintf("Se aplico un escalon de PWM = 0 a PWM = %d.\n", pwmValue);
fprintf("La salida medida fue la tension del sensor en funcion del tiempo.\n");
fprintf("Se estimo un modelo de primer orden con K = %.5f V/PWM y tau = %.3f s.\n", K, tau);
fprintf("El modelo obtenido fue G(s) = %.5f / (%.3fs + 1).\n", K, tau);
fprintf("Se genero la respuesta al impulso del modelo usando MATLAB.\n");