-- Índice Hash: solo igualdad (=). No sirve para rangos, orden ni LIKE.
-- Ejecutar: psql -d <base> -f indices/hash.sql   (todo se deshace con ROLLBACK)
BEGIN;

CREATE TABLE sesiones (
    token    text PRIMARY KEY,
    usuario  int  NOT NULL,
    creada   timestamptz NOT NULL DEFAULT now()
);

INSERT INTO sesiones (token, usuario)
SELECT md5(i::text), i % 1000
FROM generate_series(1, 100000) AS i;

-- El token es largo y solo se busca por igualdad: el hash guarda 4 bytes por clave en vez del texto completo.
CREATE INDEX idx_sesiones_token_hash ON sesiones USING hash (token);
ANALYZE sesiones;

-- Igualdad: calcula hash(token), va al bucket y compara solo esas filas.
EXPLAIN (COSTS OFF) SELECT usuario FROM sesiones WHERE token = md5('4242');

-- Rango: el hash no conserva orden, así que no se usa. Se quita la PK (que es un B-tree) para que solo quede el hash.
DROP INDEX idx_sesiones_token_hash;
ALTER TABLE sesiones DROP CONSTRAINT sesiones_pkey;
CREATE INDEX idx_sesiones_token_hash ON sesiones USING hash (token);
EXPLAIN (COSTS OFF) SELECT count(*) FROM sesiones WHERE token > 'f';

-- El hash no admite UNIQUE: esto falla.
SAVEPOINT antes_unique;
CREATE UNIQUE INDEX idx_unico ON sesiones USING hash (token);
ROLLBACK TO antes_unique;

-- Tamaño comparado con un B-tree sobre la misma columna.
CREATE INDEX idx_sesiones_token_btree ON sesiones USING btree (token);
SELECT relname AS indice, pg_size_pretty(pg_relation_size(oid)) AS tamano
FROM pg_class WHERE relname IN ('idx_sesiones_token_hash', 'idx_sesiones_token_btree');

ROLLBACK;
