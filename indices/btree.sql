-- Índice B-tree: igualdad, rangos, ORDER BY y prefijos con LIKE 'abc%'.
-- Ejecutar: psql -d <base> -f indices/btree.sql   (todo se deshace con ROLLBACK)
BEGIN;

CREATE TABLE alumnos (
    id        serial PRIMARY KEY,          -- la PK ya crea un B-tree
    codigo    text NOT NULL,
    apellido  text NOT NULL,
    nota      int  NOT NULL
);

INSERT INTO alumnos (codigo, apellido, nota)
SELECT '2026' || lpad(i::text, 6, '0'),
       (ARRAY['Perez','Quispe','Rojas','Flores','Huaman'])[1 + i % 5] || i,
       i % 21
FROM generate_series(1, 100000) AS i;

-- B-tree es el tipo por defecto: USING btree es opcional.
CREATE INDEX idx_alumnos_nota ON alumnos USING btree (nota);
CREATE INDEX idx_alumnos_apellido ON alumnos (apellido text_pattern_ops);
ANALYZE alumnos;

-- Rango: el árbol baja hasta nota = 18 y recorre las hojas enlazadas hacia la derecha.
EXPLAIN (COSTS OFF) SELECT count(*) FROM alumnos WHERE nota BETWEEN 18 AND 20;

-- Orden: las hojas ya están ordenadas, no hace falta un Sort.
EXPLAIN (COSTS OFF) SELECT apellido, nota FROM alumnos ORDER BY nota DESC LIMIT 5;

-- Prefijo: 'Rojas1%' es el rango ['Rojas1', 'Rojas2').
EXPLAIN (COSTS OFF) SELECT * FROM alumnos WHERE apellido LIKE 'Rojas123%';

SELECT apellido, nota FROM alumnos WHERE apellido LIKE 'Rojas123%' ORDER BY apellido LIMIT 3;

ROLLBACK;
