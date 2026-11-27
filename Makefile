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

.PHONY: test

test:
	@mkdir -p $(TEST_BIN)
	@set -e; for src in $(TEST_SRCS); do \
		bin=$(TEST_BIN)/$$(basename $$src .cpp); \
		echo "==> Compilando $$src"; \
		$(CXX) $(TEST_FLAGS) $$src -o $$bin; \
		echo "==> Ejecutando $$bin"; \
		./$$bin; \
	done; \
	for src in $(TSAN_SRCS); do \
		bin=$(TEST_BIN)/$$(basename $$src .cpp)_tsan; \
		echo "==> Compilando $$src (TSan)"; \
		$(CXX) $(TSAN_FLAGS) $$src -o $$bin; \
		echo "==> Ejecutando $$bin"; \
		TSAN_OPTIONS="halt_on_error=1 exitcode=66" ./$$bin; \
	done; \
	echo "==> Todas las pruebas pasaron"

clean:
	rm -rf $(TARGET) tmp build/docs
	@clear 2>/dev/null || printf "\033c"
