# Copyright (c) 2025 Preston Brown <pbrown@brown-house.net>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# HelixScreen UI Prototype - Code Formatting Module
# Handles automatic code and XML formatting

# Python venv for XML formatter
VENV_PYTHON := .venv/bin/python

# Format all C/C++ and XML files
format:
	$(ECHO) "$(CYAN)$(BOLD)Formatting code and XML files...$(RESET)"
	@FORMATTED_COUNT=0; \
	if ! command -v clang-format >/dev/null 2>&1; then \
		echo "$(RED)✗ clang-format not found$(RESET)"; \
		echo "  Install: $(YELLOW)brew install clang-format$(RESET) (macOS)"; \
		echo "         $(YELLOW)sudo apt install clang-format$(RESET) (Debian/Ubuntu)"; \
		echo "         $(YELLOW)sudo dnf install clang-tools-extra$(RESET) (Fedora/RHEL)"; \
		exit 1; \
	fi; \
	echo "$(CYAN)Formatting C/C++ files...$(RESET)"; \
	C_FILES=$$(find src include -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.mm" \) 2>/dev/null | grep -v '/\.' || true); \
	if [ -n "$$C_FILES" ]; then \
		for file in $$C_FILES; do \
			if [ -f "$$file" ]; then \
				clang-format -i "$$file" && FORMATTED_COUNT=$$((FORMATTED_COUNT + 1)); \
			fi; \
		done; \
		echo "$(GREEN)✓ Formatted $$FORMATTED_COUNT C/C++ files$(RESET)"; \
	else \
		echo "$(YELLOW)⚠ No C/C++ files found$(RESET)"; \
	fi; \
	echo "$(CYAN)Formatting XML files...$(RESET)"; \
	XML_COUNT=0; \
	XML_FILES=$$(find ui_xml -type f -name "*.xml" 2>/dev/null || true); \
	if [ -n "$$XML_FILES" ]; then \
		if [ -x "$(VENV_PYTHON)" ] && $(VENV_PYTHON) -c "import lxml" 2>/dev/null; then \
			$(VENV_PYTHON) scripts/format-xml.py $$XML_FILES && \
			XML_COUNT=$$(echo "$$XML_FILES" | wc -w | tr -d ' '); \
			echo "$(GREEN)✓ Formatted $$XML_COUNT XML files$(RESET)"; \
		else \
			echo "$(YELLOW)⚠ Python venv or lxml not available - run 'make venv-setup'$(RESET)"; \
			echo "  Falling back to xmllint (basic indentation only)"; \
			for file in $$XML_FILES; do \
				if [ -f "$$file" ]; then \
					if xmllint --format "$$file" > "$$file.tmp" 2>/dev/null; then \
						mv "$$file.tmp" "$$file" && XML_COUNT=$$((XML_COUNT + 1)); \
					else \
						echo "$(RED)✗ Failed to format $$file$(RESET)"; \
						rm -f "$$file.tmp"; \
					fi; \
				fi; \
			done; \
			echo "$(GREEN)✓ Formatted $$XML_COUNT XML files (basic)$(RESET)"; \
		fi; \
	else \
		echo "$(YELLOW)⚠ No XML files found$(RESET)"; \
	fi; \
	echo ""; \
	echo "$(GREEN)$(BOLD)✓ Formatting complete!$(RESET)"; \
	echo "$(CYAN)Total files formatted:$(RESET) $$((FORMATTED_COUNT + XML_COUNT))"

# Format only staged files (useful before committing)
format-staged:
	$(ECHO) "$(CYAN)$(BOLD)Formatting staged files...$(RESET)"
	@FORMATTED_COUNT=0; \
	if ! command -v clang-format >/dev/null 2>&1; then \
		echo "$(RED)✗ clang-format not found$(RESET)"; \
		exit 1; \
	fi; \
	STAGED_C_FILES=$$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(c|cpp|h|mm)$$' | grep -v '^lib/' || true); \
	if [ -n "$$STAGED_C_FILES" ]; then \
		echo "$(CYAN)Formatting staged C/C++ files...$(RESET)"; \
		for file in $$STAGED_C_FILES; do \
			if [ -f "$$file" ]; then \
				clang-format -i "$$file" && git add "$$file" && FORMATTED_COUNT=$$((FORMATTED_COUNT + 1)); \
				echo "  ✓ $$file"; \
			fi; \
		done; \
	fi; \
	STAGED_XML_FILES=$$(git diff --cached --name-only --diff-filter=ACM | grep '\.xml$$' || true); \
	if [ -n "$$STAGED_XML_FILES" ]; then \
		echo "$(CYAN)Formatting staged XML files...$(RESET)"; \
		if [ -x "$(VENV_PYTHON)" ] && $(VENV_PYTHON) -c "import lxml" 2>/dev/null; then \
			for file in $$STAGED_XML_FILES; do \
				if [ -f "$$file" ]; then \
					$(VENV_PYTHON) scripts/format-xml.py "$$file" && git add "$$file" && FORMATTED_COUNT=$$((FORMATTED_COUNT + 1)); \
					echo "  ✓ $$file"; \
				fi; \
			done; \
		else \
			echo "$(YELLOW)⚠ Python venv or lxml not available - run 'make venv-setup'$(RESET)"; \
			for file in $$STAGED_XML_FILES; do \
				if [ -f "$$file" ]; then \
					if xmllint --format "$$file" > "$$file.tmp" 2>/dev/null; then \
						mv "$$file.tmp" "$$file" && git add "$$file" && FORMATTED_COUNT=$$((FORMATTED_COUNT + 1)); \
						echo "  ✓ $$file"; \
					else \
						rm -f "$$file.tmp"; \
					fi; \
				fi; \
			done; \
		fi; \
	fi; \
	if [ $$FORMATTED_COUNT -eq 0 ]; then \
		echo "$(GREEN)ℹ️  No staged files need formatting$(RESET)"; \
	else \
		echo ""; \
		echo "$(GREEN)$(BOLD)✓ Formatted $$FORMATTED_COUNT staged files$(RESET)"; \
	fi
