-- Índice Hash: solo igualdad (=). No sirve para rangos, orden ni LIKE.
-- Compara SIN índice vs CON índice Hash vs B-tree (plan, tiempo, tamaño y mismos resultados).
-- Ejecutar: psql -d <base> -f indices/hash.sql   (todo se deshace con ROLLBACK)
\timing on
BEGIN;

CREATE TABLE sesiones (
    token    text        NOT NULL,
    usuario  int         NOT NULL,
    creada   timestamptz NOT NULL DEFAULT now()
);

-- 500 000 sesiones con tokens largos (MD5 de 32 caracteres).
INSERT INTO sesiones (token, usuario)
SELECT md5(i::text), i % 10000
FROM generate_series(1, 500000) AS i;

ANALYZE sesiones;

-- ============================================================================
-- PARTE 1: SIN ÍNDICE (Seq Scan sobre 500 000 filas)
-- ============================================================================

-- 1) Búsqueda puntual por token
EXPLAIN (ANALYZE, BUFFERS)
SELECT usuario, token
FROM sesiones
WHERE token = md5('250000');

SELECT usuario, token
FROM sesiones
WHERE token = md5('250000');

-- 2) Búsqueda de varios tokens puntuales (IN)
EXPLAIN (ANALYZE, BUFFERS)
SELECT usuario, token
FROM sesiones
WHERE token IN (md5('10'), md5('250000'), md5('499999'))
ORDER BY usuario;

SELECT usuario, token
FROM sesiones
WHERE token IN (md5('10'), md5('250000'), md5('499999'))
ORDER BY usuario;

-- ============================================================================
-- PARTE 2: CON ÍNDICE HASH (solo guarda hash de 32 bits -> acceso O(1) promedio)
-- ============================================================================
CREATE INDEX idx_sesiones_token_hash ON sesiones USING hash (token);
ANALYZE sesiones;

-- 1) Búsqueda puntual -> Index Scan using idx_sesiones_token_hash (mismo resultado, < 1 ms)
EXPLAIN (ANALYZE, BUFFERS)
SELECT usuario, token
FROM sesiones
WHERE token = md5('250000');

SELECT usuario, token
FROM sesiones
WHERE token = md5('250000');

-- 2) Búsqueda múltiple con IN -> Bitmap Index Scan sobre varios buckets
EXPLAIN (ANALYZE, BUFFERS)
SELECT usuario, token
FROM sesiones
WHERE token IN (md5('10'), md5('250000'), md5('499999'))
ORDER BY usuario;

SELECT usuario, token
FROM sesiones
WHERE token IN (md5('10'), md5('250000'), md5('499999'))
ORDER BY usuario;

-- ============================================================================
-- PARTE 3: LIMITACIONES DE HASH Y COMPARACIÓN CON B-TREE
-- ============================================================================

-- 1) Rango (>): Hash no conserva orden -> vuelve a Seq Scan.
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM sesiones WHERE token > 'ff';

-- 2) Ordenamiento (ORDER BY): Hash no sirve -> obliga a Sort sobre las 500 000 filas.
EXPLAIN (ANALYZE, BUFFERS)
SELECT token, usuario FROM sesiones ORDER BY token LIMIT 5;

-- 3) Hash no admite UNIQUE: esto falla y se revierte con SAVEPOINT.
SAVEPOINT antes_unique;
CREATE UNIQUE INDEX idx_unico ON sesiones USING hash (token);
ROLLBACK TO antes_unique;

-- 4) Agregamos B-tree sobre la misma columna para comparar tamaño y uso en rangos/orden.
CREATE INDEX idx_sesiones_token_btree ON sesiones USING btree (token);
ANALYZE sesiones;

-- Ahora rango y ORDER BY usan el B-tree en lugar de Seq Scan / Sort:
EXPLAIN (ANALYZE, BUFFERS)
SELECT count(*) FROM sesiones WHERE token > 'ff';

EXPLAIN (ANALYZE, BUFFERS)
SELECT token, usuario FROM sesiones ORDER BY token LIMIT 5;

-- Tamaño en disco: Hash suele ser más compacto que B-tree cuando la clave es larga.
SELECT relname AS indice, pg_size_pretty(pg_relation_size(oid)) AS tamano
FROM pg_class
WHERE relname IN ('idx_sesiones_token_hash', 'idx_sesiones_token_btree')
ORDER BY relname;

ROLLBACK;
