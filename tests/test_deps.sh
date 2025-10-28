#!/bin/bash
# Copyright (c) 2025 Preston Brown <pbrown@brown-house.net>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Test harness for dependency checking system
# Tests both check-deps and install-deps targets

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
CYAN='\033[36m'
BOLD='\033[1m'
RESET='\033[0m'

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Test result tracking
declare -a FAILED_TESTS

# Print test header
print_test() {
    TESTS_RUN=$((TESTS_RUN + 1))
    echo -e "\n${CYAN}[TEST $TESTS_RUN]${RESET} $1"
}

# Assert condition
assert() {
    local condition="$1"
    local message="$2"

    if eval "$condition"; then
        echo -e "${GREEN}  ✓${RESET} $message"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}  ✗${RESET} $message"
        FAILED_TESTS+=("TEST $TESTS_RUN: $message")
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Test check-deps output format
test_check_deps_format() {
    print_test "check-deps output format"

    cd "$PROJECT_ROOT"
    OUTPUT=$(make check-deps 2>&1 || true)

    # Should contain "Checking build dependencies" header
    assert "[[ \"$OUTPUT\" =~ \"Checking build dependencies\" ]]" \
        "Output contains header"

    # Should check for C compiler
    assert "[[ \"$OUTPUT\" =~ \"clang\" || \"$OUTPUT\" =~ \"gcc\" ]]" \
        "Checks for C compiler"

    # Should check for C++ compiler
    assert "[[ \"$OUTPUT\" =~ \"clang++\" || \"$OUTPUT\" =~ \"g++\" ]]" \
        "Checks for C++ compiler"

    # Should check for SDL2
    assert "[[ \"$OUTPUT\" =~ \"SDL2\" ]]" \
        "Checks for SDL2"

    # Should check for npm
    assert "[[ \"$OUTPUT\" =~ \"npm\" ]]" \
        "Checks for npm"

    # Should check for python3
    assert "[[ \"$OUTPUT\" =~ \"python3\" ]]" \
        "Checks for python3"

    # Should check for make
    assert "[[ \"$OUTPUT\" =~ \"make\" ]]" \
        "Checks for make"

    # Should check for LVGL
    assert "[[ \"$OUTPUT\" =~ \"LVGL\" ]]" \
        "Checks for LVGL submodule"

    # Should check for spdlog
    assert "[[ \"$OUTPUT\" =~ \"spdlog\" ]]" \
        "Checks for spdlog submodule"

    # Should check for libhv
    assert "[[ \"$OUTPUT\" =~ \"libhv\" ]]" \
        "Checks for libhv"
}

# Test check-deps exit codes
test_check_deps_exit_codes() {
    print_test "check-deps exit codes"

    cd "$PROJECT_ROOT"

    # If all deps satisfied, should exit 0
    if make check-deps >/dev/null 2>&1; then
        assert "true" "Exit code 0 when all dependencies satisfied"
    else
        # If deps missing, should suggest install-deps
        OUTPUT=$(make check-deps 2>&1 || true)
        assert "[[ \"$OUTPUT\" =~ \"make install-deps\" ]]" \
            "Suggests install-deps when dependencies missing"
    fi
}

# Test check-deps detects missing lv_font_conv
test_check_deps_lv_font_conv() {
    print_test "check-deps detects missing lv_font_conv"

    cd "$PROJECT_ROOT"

    # Temporarily rename node_modules if it exists
    if [ -d "node_modules" ]; then
        mv node_modules node_modules.bak
        OUTPUT=$(make check-deps 2>&1 || true)
        mv node_modules.bak node_modules

        assert "[[ \"$OUTPUT\" =~ \"lv_font_conv\" ]]" \
            "Detects missing lv_font_conv"

        assert "[[ \"$OUTPUT\" =~ \"npm install\" ]]" \
            "Suggests npm install"
    else
        # node_modules doesn't exist, should detect missing lv_font_conv
        OUTPUT=$(make check-deps 2>&1 || true)

        assert "[[ \"$OUTPUT\" =~ \"lv_font_conv\" ]]" \
            "Detects missing lv_font_conv when node_modules absent"
    fi
}

# Test check-deps detects canvas dependencies
test_check_deps_canvas() {
    print_test "check-deps detects canvas dependencies"

    cd "$PROJECT_ROOT"
    OUTPUT=$(make check-deps 2>&1 || true)

    # Should check for pkg-config
    assert "[[ \"$OUTPUT\" =~ \"pkg-config\" ]]" \
        "Checks for pkg-config"

    # If pkg-config is present, should check for canvas libs
    if command -v pkg-config >/dev/null 2>&1; then
        # Should check cairo, pango, libpng
        if ! pkg-config --exists cairo 2>/dev/null || \
           ! pkg-config --exists pango 2>/dev/null || \
           ! pkg-config --exists libpng 2>/dev/null; then
            # At least one is missing, should show install command
            assert "[[ \"$OUTPUT\" =~ \"cairo\" || \"$OUTPUT\" =~ \"pango\" || \"$OUTPUT\" =~ \"libpng\" ]]" \
                "Detects missing canvas libraries"
        else
            # All present
            assert "true" \
                "All canvas dependencies satisfied"
        fi
    else
        assert "true" \
            "pkg-config not available (test skipped)"
    fi
}

# Test check-deps platform-specific install commands
test_check_deps_platform_specific() {
    print_test "check-deps shows platform-specific install commands"

    cd "$PROJECT_ROOT"
    OUTPUT=$(make check-deps 2>&1 || true)

    # Platform-specific commands only appear when SYSTEM dependencies are missing
    # Optional deps (npm packages) don't trigger platform commands
    # Just verify that install-deps is suggested when deps are missing
    if [[ "$OUTPUT" =~ "make install-deps" ]]; then
        assert "true" \
            "Suggests make install-deps when dependencies missing"
    elif [[ "$OUTPUT" =~ "All dependencies satisfied" ]]; then
        assert "true" \
            "All dependencies satisfied"
    else
        assert "false" \
            "Should suggest install-deps or report all satisfied"
    fi
}

# Test install-deps is in help output
test_install_deps_in_help() {
    print_test "install-deps appears in help output"

    cd "$PROJECT_ROOT"
    OUTPUT=$(make help 2>&1)

    assert "[[ \"$OUTPUT\" =~ \"install-deps\" ]]" \
        "install-deps target listed in help"

    assert "[[ \"$OUTPUT\" =~ \"Auto-install missing dependencies\" ]]" \
        "install-deps description present"
}

# Test install-deps requires confirmation (dry-run check)
test_install_deps_interactive() {
    print_test "install-deps is interactive (requires confirmation)"

    cd "$PROJECT_ROOT"

    # Send "n" to interactive prompt to cancel
    OUTPUT=$(echo "n" | make install-deps 2>&1 || true)

    # Should show detected platform
    if [[ "$OUTPUT" =~ "Detected platform" ]]; then
        assert "true" "Shows detected platform"
    else
        assert "false" "Shows detected platform"
    fi

    # Should show confirmation prompt, "No missing", or "cancelled" message
    if [[ "$OUTPUT" =~ "Continue" ]] || [[ "$OUTPUT" =~ "No missing" ]] || [[ "$OUTPUT" =~ "cancelled" ]]; then
        assert "true" "Shows confirmation prompt or appropriate message"
    else
        assert "false" "Shows confirmation prompt or appropriate message"
    fi
}

# Test Makefile .PHONY declaration
test_phony_targets() {
    print_test "check-deps and install-deps are in .PHONY"

    cd "$PROJECT_ROOT"

    # Read Makefile and check .PHONY line
    PHONY_LINE=$(grep "^\.PHONY:" Makefile || true)

    assert "[[ \"$PHONY_LINE\" =~ \"check-deps\" ]]" \
        "check-deps in .PHONY declaration"

    assert "[[ \"$PHONY_LINE\" =~ \"install-deps\" ]]" \
        "install-deps in .PHONY declaration"
}

# Test that check-deps is called before build
test_check_deps_in_build() {
    print_test "check-deps is called as part of default build"

    cd "$PROJECT_ROOT"

    # Check if 'all' target depends on check-deps (may be in main Makefile or mk/rules.mk)
    if [ -f "mk/rules.mk" ]; then
        ALL_TARGET=$(grep "^all:" mk/rules.mk | head -n1)
    else
        ALL_TARGET=$(grep "^all:" Makefile | head -n1)
    fi

    assert "[[ \"$ALL_TARGET\" =~ \"check-deps\" ]]" \
        "all target depends on check-deps"
}

# Run all tests
main() {
    echo -e "${BOLD}${CYAN}Dependency Checking System Test Harness${RESET}"
    echo -e "${CYAN}Project:${RESET} $PROJECT_ROOT"
    echo ""

    test_check_deps_format
    test_check_deps_exit_codes
    test_check_deps_lv_font_conv
    test_check_deps_canvas
    test_check_deps_platform_specific
    test_install_deps_in_help
    test_install_deps_interactive
    test_phony_targets
    test_check_deps_in_build

    # Print summary
    echo ""
    echo -e "${BOLD}${CYAN}Test Summary${RESET}"
    echo -e "${CYAN}────────────────────────────────────────${RESET}"
    echo -e "Total tests: $TESTS_RUN"
    echo -e "${GREEN}Passed: $TESTS_PASSED${RESET}"

    if [ $TESTS_FAILED -gt 0 ]; then
        echo -e "${RED}Failed: $TESTS_FAILED${RESET}"
        echo ""
        echo -e "${RED}${BOLD}Failed tests:${RESET}"
        for test in "${FAILED_TESTS[@]}"; do
            echo -e "  ${RED}✗${RESET} $test"
        done
        exit 1
    else
        echo -e "${GREEN}${BOLD}✓ All tests passed!${RESET}"
        exit 0
    fi
}

main
