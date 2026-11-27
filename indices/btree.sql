-- Índice B-tree: igualdad, rangos, ORDER BY, prefijos (LIKE 'abc%') e índices compuestos.
-- Compara SIN índice vs CON índice (plan, tiempo de ejecución con EXPLAIN ANALYZE y mismos resultados).
-- Ejecutar: psql -d <base> -f indices/btree.sql   (todo se deshace con ROLLBACK)
\timing on
BEGIN;

CREATE TABLE alumnos (
    id        serial,
    codigo    text          NOT NULL,
    apellido  text          NOT NULL,
    ciclo     int           NOT NULL,
    nota      int           NOT NULL,
    promedio  numeric(4, 2) NOT NULL
);

-- 500 000 registros para que la diferencia entre Seq Scan e Index Scan sea clara.
\echo ''
\echo '>>> [B-TREE] Creando tabla alumnos e insertando 500 000 filas...'
INSERT INTO alumnos (codigo, apellido, ciclo, nota, promedio)
SELECT '2026' || lpad(i::text, 6, '0'),
       (ARRAY['Perez','Quispe','Rojas','Flores','Huaman'])[1 + i % 5] || i,
       1 + (i % 10),
       i % 21,
       round(((i % 2001) / 100.0)::numeric, 2)
FROM generate_series(1, 500000) AS i;

ANALYZE alumnos;

-- ============================================================================
-- PARTE 1: SIN ÍNDICES (todas las consultas hacen Seq Scan sobre 500 000 filas)
-- ============================================================================
\echo ''
\echo '================================================================'
\echo 'PARTE 1: SIN INDICES (Seq Scan sobre 500 000 filas)'
\echo '================================================================'

-- 1) Igualdad exacta
\echo ''
\echo '--- 1) Igualdad exacta (sin indice) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, codigo, apellido, promedio
FROM alumnos
WHERE codigo = '2026250000';

SELECT id, codigo, apellido, promedio
FROM alumnos
WHERE codigo = '2026250000';

-- 2) Rango selectivo (~0.5% de la tabla)
\echo ''
\echo '--- 2) Rango selectivo BETWEEN (sin indice) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS total_sobresalientes
FROM alumnos
WHERE promedio BETWEEN 19.90 AND 20.00;

SELECT count(*) AS total_sobresalientes
FROM alumnos
WHERE promedio BETWEEN 19.90 AND 20.00;

-- 3) Orden + límite (sin índice obliga a ordenar las 500 000 filas con Sort)
\echo ''
\echo '--- 3) ORDER BY + LIMIT (sin indice -> Sort) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT codigo, apellido, promedio
FROM alumnos
ORDER BY promedio DESC, codigo ASC
LIMIT 5;

SELECT codigo, apellido, promedio
FROM alumnos
ORDER BY promedio DESC, codigo ASC
LIMIT 5;

-- 4) Búsqueda por prefijo con LIKE
\echo ''
\echo '--- 4) Prefijo con LIKE (sin indice) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT codigo, apellido, nota
FROM alumnos
WHERE apellido LIKE 'Rojas2500%'
ORDER BY apellido
LIMIT 5;

SELECT codigo, apellido, nota
FROM alumnos
WHERE apellido LIKE 'Rojas2500%'
ORDER BY apellido
LIMIT 5;

-- 5) Filtro por dos columnas (ciclo y nota)
\echo ''
\echo '--- 5) Filtro compuesto ciclo + nota (sin indice) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS total_ciclo3_nota20
FROM alumnos
WHERE ciclo = 3 AND nota = 20;

SELECT count(*) AS total_ciclo3_nota20
FROM alumnos
WHERE ciclo = 3 AND nota = 20;

-- ============================================================================
-- PARTE 2: CREAR ÍNDICES B-TREE
-- ============================================================================
\echo ''
\echo '================================================================'
\echo 'PARTE 2: Creando indices B-tree...'
\echo '================================================================'
CREATE UNIQUE INDEX idx_alumnos_codigo ON alumnos USING btree (codigo);
CREATE INDEX idx_alumnos_promedio ON alumnos USING btree (promedio DESC, codigo ASC);
CREATE INDEX idx_alumnos_apellido ON alumnos (apellido text_pattern_ops);
CREATE INDEX idx_alumnos_ciclo_nota ON alumnos (ciclo, nota);
ANALYZE alumnos;

-- ============================================================================
-- PARTE 3: CON ÍNDICES (mismas consultas -> mismos resultados, mucho menor tiempo)
-- ============================================================================
\echo ''
\echo '================================================================'
\echo 'PARTE 3: CON INDICES B-TREE (Index Scan / Index Only Scan)'
\echo '================================================================'

-- 1) Igualdad exacta -> Index Scan (~0.05 ms vs decenas de ms)
\echo ''
\echo '--- 1) Igualdad exacta (con B-tree) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT id, codigo, apellido, promedio
FROM alumnos
WHERE codigo = '2026250000';

SELECT id, codigo, apellido, promedio
FROM alumnos
WHERE codigo = '2026250000';

-- 2) Rango selectivo -> Index Only Scan / Bitmap Index Scan
\echo ''
\echo '--- 2) Rango selectivo BETWEEN (con B-tree) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS total_sobresalientes
FROM alumnos
WHERE promedio BETWEEN 19.90 AND 20.00;

SELECT count(*) AS total_sobresalientes
FROM alumnos
WHERE promedio BETWEEN 19.90 AND 20.00;

-- 3) Orden + límite -> Index Only Scan sin Sort (lee directo las primeras 5 entradas)
\echo ''
\echo '--- 3) ORDER BY + LIMIT (con B-tree -> sin Sort) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT codigo, apellido, promedio
FROM alumnos
ORDER BY promedio DESC, codigo ASC
LIMIT 5;

SELECT codigo, apellido, promedio
FROM alumnos
ORDER BY promedio DESC, codigo ASC
LIMIT 5;

-- 4) Prefijo con LIKE -> Index Scan usando rango ['Rojas2500', 'Rojas2501')
\echo ''
\echo '--- 4) Prefijo con LIKE (con B-tree text_pattern_ops) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT codigo, apellido, nota
FROM alumnos
WHERE apellido LIKE 'Rojas2500%'
ORDER BY apellido
LIMIT 5;

SELECT codigo, apellido, nota
FROM alumnos
WHERE apellido LIKE 'Rojas2500%'
ORDER BY apellido
LIMIT 5;

-- 5) Filtro compuesto (ciclo, nota) -> Bitmap Index Scan / Index Only Scan
\echo ''
\echo '--- 5) Filtro compuesto ciclo + nota (con B-tree) ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) AS total_ciclo3_nota20
FROM alumnos
WHERE ciclo = 3 AND nota = 20;

SELECT count(*) AS total_ciclo3_nota20
FROM alumnos
WHERE ciclo = 3 AND nota = 20;

-- ============================================================================
-- PARTE 4: CASOS USUALES DONDE EL B-TREE *NO* SE USA (vuelve a Seq Scan)
-- ============================================================================
\echo ''
\echo '================================================================'
\echo 'PARTE 4: CASOS DONDE B-TREE NO SE USA (vuelve a Seq Scan)'
\echo '================================================================'

-- a) Comodín al inicio (%): el orden alfabético no sirve para buscar sufijos.
\echo ''
\echo '--- a) LIKE con comodin al inicio (%25000) -> Seq Scan ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM alumnos WHERE apellido LIKE '%25000';

-- b) Expresión sobre la columna indexada: rompe el uso directo de idx_alumnos_ciclo_nota.
\echo ''
\echo '--- b) Expresion sobre columna indexada (ciclo + 0 = 3) -> Seq Scan ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM alumnos WHERE ciclo + 0 = 3 AND nota = 20;

-- c) Segunda columna de un índice compuesto sin filtrar por la primera (falta "ciclo").
\echo ''
\echo '--- c) Solo segunda columna del indice compuesto (nota = 20) -> Seq Scan ---'
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM alumnos WHERE nota = 20;

ROLLBACK;
