#!/bin/bash

echo "📁 Moviéndose a la carpeta..."
cd proyecto_1

docker exec -it rvqemu /bin/bash -c "
cd /home/rvqemu-dev/workspace/chacha20/c-asm &&
echo '📁 En carpeta correcta' &&
echo '🐞 Ejecutando GDB...' &&
gdb-multiarch chacha20.elf
"
echo "🐍 Ejecutando archivo Python..."
echo "📈 Archivo a ejecutar: fft_experiment.py"

python3 fft_experiment.py