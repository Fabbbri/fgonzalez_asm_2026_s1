# fgonzalez_asm_2026_s1

## fft_experimental

```bash
# Para ejecutar fft_experimental

# (una única vez)
chmod +x run.sh

./run.sh
```

## Comandos útiles de Git

Las ramas son de tipo feature, fix o docs. Los commits son de tipo feat, fix, test, doc, refacture

```
git checkout <rama_existente>

git checkout -b <tipo>/<nombre_nueva_rama>

git commit -m "<tipo>(): <descripcion>"

# Desde la rama develop/main por ejemplo:
git merge --no-ff <nombre_rama>

# Despues de editar el mensaje:
# Presionar ESC
# Escribir 
:wq # significa write y quit

# Eliminar rama local
git push origin --delete <nombre_rama>

# Eliminar rama remota
git branch -d <nombre_rama>
```