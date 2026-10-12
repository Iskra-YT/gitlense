#!/usr/bin/env bash
# ============================================================
#  build.sh - skrypt budowy gitlense dla Linux/macOS
# ============================================================
set -euo pipefail

SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SOURCE_DIR}/build"
CONFIG="Release"
CLEAN=0
RUN_TESTS=0
FETCH_LIBGIT2=1

# --- Parsowanie argumentów -------------------------------------
for arg in "$@"; do
    case "${arg}" in
        --debug)              CONFIG="Debug" ;;
        --release)            CONFIG="Release" ;;
        --clean)              CLEAN=1 ;;
        --test)               RUN_TESTS=1 ;;
        --no-fetch-libgit2)   FETCH_LIBGIT2=0 ;;
        *)
            echo "[BLAD] Nieznany argument: ${arg}" >&2
            echo "       Uzycie: $0 [--debug|--release] [--clean] [--test] [--no-fetch-libgit2]" >&2
            exit 1
            ;;
    esac
done

# --- Sprawdz czy cmake jest dostepny ---------------------------
if ! command -v cmake >/dev/null 2>&1; then
    echo "[BLAD] cmake nie zostal znaleziony w PATH." >&2
    echo "       Zainstaluj CMake 3.20+ i dodaj go do PATH." >&2
    exit 1
fi

# --- Opcjonalny czysty build -----------------------------------
if [[ "${CLEAN}" -eq 1 ]]; then
    echo "[INFO] Czyszczenie katalogu build ..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

# --- Konfiguracja ----------------------------------------------
echo "[INFO] Konfiguracja CMake (${CONFIG}) ..."
cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${CONFIG}" \
    -DGITLENSE_FETCH_LIBGIT2="${FETCH_LIBGIT2}"

# --- Budowa -----------------------------------------------------
echo "[INFO] Kompilacja ..."
cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j "$(nproc)"

echo "[OK] Zakonczono pomyslnie."
echo "     Binary: ${BUILD_DIR}/gitlense"

# --- Opcjonalne testy -----------------------------------------
if [[ "${RUN_TESTS}" -eq 1 ]]; then
    echo "[INFO] Uruchamianie testow ..."
    ctest --test-dir "${BUILD_DIR}" --output-on-failure
fi
