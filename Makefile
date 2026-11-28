CXX        := g++
CXXFLAGS   := -std=c++23
TARGET     := main
SRCS       := main.cpp demos.cpp
PR_OUT     := tmp/pr_comment.md
PR_SCRIPT  := scripts/gen_pr_comment.sh
DOCS_PORT  ?= 8000

.PHONY: all run pr docs clean

all: clean $(TARGET) run

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

pr:
	@bash $(PR_SCRIPT) README.md $(PR_OUT)

docs:
	@mkdir -p build/docs
	doxygen Doxyfile
	@echo ""
	@echo "Documentacion generada. Abre: http://localhost:$(DOCS_PORT)"
	@echo "Presiona Ctrl+C para detener el servidor."
	@python3 -m http.server $(DOCS_PORT) -d build/docs/html

# Pruebas unitarias: compila y ejecuta cada tests/*.cpp con sanitizadores; falla al primer error.
TEST_DIR   := tests
TEST_BIN   := build/tests
TEST_SRCS  := $(wildcard $(TEST_DIR)/*.cpp)
TEST_FLAGS := $(CXXFLAGS) -Wall -Wextra -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
# Concurrencia: binario aparte con ThreadSanitizer (incompatible con ASan); falla si TSan reporta algo.
TSAN_SRCS  := $(TEST_DIR)/btree_concurrency_test.cpp
TSAN_FLAGS := $(CXXFLAGS) -Wall -Wextra -g -O1 -fsanitize=thread -fno-omit-frame-pointer

# Cada caso (compilar + ejecutar) es un objetivo propio y `make test` los corre en paralelo
# (TEST_JOBS, por defecto la cantidad de núcleos). La salida de cada caso se guarda en
# build/tests/<caso>.log y se muestra completa al terminar, sin mezclarse con las demás.
TEST_JOBS  ?= $(shell nproc 2>/dev/null || echo 4)
ASAN_RUNS  := $(patsubst $(TEST_DIR)/%.cpp,$(TEST_BIN)/%.run,$(TEST_SRCS))
TSAN_RUNS  := $(patsubst $(TEST_DIR)/%.cpp,$(TEST_BIN)/%_tsan.run,$(TSAN_SRCS))

.PHONY: test test-run FORCE

test:
	@mkdir -p $(TEST_BIN)
	@$(MAKE) --no-print-directory -j$(TEST_JOBS) test-run
	@echo "==> Todas las pruebas pasaron"

test-run: $(ASAN_RUNS) $(TSAN_RUNS)

$(TEST_BIN)/%.run: $(TEST_DIR)/%.cpp FORCE
	@$(CXX) $(TEST_FLAGS) $< -o $(TEST_BIN)/$* > $(TEST_BIN)/$*.log 2>&1 && ./$(TEST_BIN)/$* >> $(TEST_BIN)/$*.log 2>&1; \
	status=$$?; echo "==> $< (ASan+UBSan)"; cat $(TEST_BIN)/$*.log; exit $$status

$(TEST_BIN)/%_tsan.run: $(TEST_DIR)/%.cpp FORCE
	@$(CXX) $(TSAN_FLAGS) $< -o $(TEST_BIN)/$*_tsan > $(TEST_BIN)/$*_tsan.log 2>&1 && \
	TSAN_OPTIONS="halt_on_error=1 exitcode=66" ./$(TEST_BIN)/$*_tsan >> $(TEST_BIN)/$*_tsan.log 2>&1; \
	status=$$?; echo "==> $< (TSan)"; cat $(TEST_BIN)/$*_tsan.log; exit $$status

FORCE:

clean:
	rm -rf $(TARGET) tmp build/docs
	@clear 2>/dev/null || printf "\033c"
