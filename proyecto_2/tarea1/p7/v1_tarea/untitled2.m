clc; clear; close all;

data = readmatrix("datos_escalon.csv");

t = data(:,1);
v = data(:,2);

figure;
plot(t, v, "LineWidth", 1.8);
grid on;
xlabel("Tiempo (s)");
ylabel("Tension del sensor (V)");
title("Respuesta al escalon de la planta");

v0 = v(1);
vss = mean(v(max(1,end-10):end));

fprintf("Tension inicial: %.3f V\n", v0);
fprintf("Tension final aproximada: %.3f V\n", vss);
fprintf("Cambio de tension: %.3f V\n", vss - v0);