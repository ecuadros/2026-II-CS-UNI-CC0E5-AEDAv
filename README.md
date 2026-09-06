# 2026-II-CS-UNI-CC0E5-AEDAv

**Actualización de nuevas ramas**

```bash
git remote add upstream https://github.com/ecuadros/2026-II-CS-UNI-CC0E5-AEDAv.git
git fetch --all
git branch --all # Solo para verificar
```

**Trabajar sobre la rama indicada**

```bash
git checkout <rama-indicada>
```

**Subir el trabajo**

```bash
git add <documento>
git commit -m "mensaje"
git push origin <rama-indicada> # git push solo no basta
```

Desde la rama indicada en el fork se crea un PR hacia la rama del mismo nombre en el repositorio del profesor.