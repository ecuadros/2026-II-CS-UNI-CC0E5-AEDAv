DROP TABLE IF EXISTS tiendas_btree;
CREATE TABLE tiendas_btree (id integer PRIMARY KEY, codigo text, ventas integer, ubicacion point);

SELECT setseed(0.42);
INSERT INTO tiendas_btree
SELECT i, 'T' || lpad(i::text, 6, '0'), (random() * 100000)::integer,
       point(round((random() * 1000)::numeric, 2), round((random() * 1000)::numeric, 2))
FROM generate_series(1, 100000) AS i;
ANALYZE tiendas_btree;

EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_btree WHERE ventas = 74294;

CREATE INDEX ventas_btree ON tiendas_btree USING btree (ventas);

EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_btree WHERE ventas = 74294;
EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_btree WHERE ventas BETWEEN 50000 AND 50010;
EXPLAIN (ANALYZE, BUFFERS, COSTS OFF, TIMING OFF) SELECT * FROM tiendas_btree ORDER BY ventas LIMIT 3;
