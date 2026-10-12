# gitlense

C++20 terminalowy przeglądarka historii Git z wykorzystaniem biblioteki [libgit2](https://libgit2.org/) (bez wywoływania zewnętrznego polecenia `git`). Interfejs jest wyłącznie tekstowy (CLI), wyjście czytelne dla człowieka (tabele, wykresy ASCII).

## Cechy

- Dostęp do repozytorium wyłącznie przez libgit2 (wersja 1.9.7, nie shelluje do `git`).
- 12 podkomend: `log`, `show`, `file`, `branches`, `diff`, `contributors`, `languages`, `hotspots`, `activity`, `shortlog`, `search`, `graph`.
- Wykrywanie języków: wbudowana tabela rozszerzeń + heurystyki + wsparcie `.gitattributes` (`linguist-language`).
- Formatowanie dat (również względne), tabele z poprawnym liczeniem szerokości znaków Unicode (polskie znaki), wykresy słupkowe ASCII.
- Całość interfejsu i komentarzy w języku polskim.

## Budowa (bez root-a)

Projekt zakłada, że `cmake` i `libgit2` nie są zainstalowane systemowo. W repo znajdują się skrypty pomocnicze do budowy zależności lokalnie.

```bash
# CMake 4.1.0 (lokalnie w ~/.local/cmake)
PATH="$HOME/.local/cmake/bin:$PATH"

# libgit2 1.9.7 (lokalnie w ~/.local/gitlense-deps)
scripts/build-libgit2.sh

# konfiguracja i budowa
cmake -S /home/iskra/gitlense -B /home/iskra/gitlense/build \
  -DCMAKE_PREFIX_PATH="$HOME/.local/gitlense-deps" \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j 16
```

Po udanej kompilacji binaria znajdą się w katalogu `build/`:
- `build/gitlense` – główne narzędzie CLI
- `build/tests/gitlense_tests` – testy jednostkowe

## Uruchamianie

```bash
cd <repozytorium_git>
gitlense log
gitlense show HEAD
gitlense file src/main.cpp
gitlense branches
gitlense diff --name-status HEAD~1 HEAD
gitlense contributors
gitlense languages
gitlense hotspots
gitlense activity
gitlense shortlog
gitlense search "fix"
gitlense graph -n 20
```

## Opcje globalne

```
--repo DIR       Ścieżka do repozytorium (domyślnie .)
--threads N      Liczba wątków (domyślnie 0 = automatycznie)
--date STYL      Styl daty (iso|rfc|short|relative|raw)
--path-width N   Maksymalna szerokość ścieżek w tabelach
--no-color       Wyłącz kolory
--color          Wymuś kolory
--quiet          Minimalne wyjście
--no-header      Pomija nagłówek
--help           Pomoc
```

Szczegółowa pomoc dla każdej podkomendy: `gitlense help <komenda>`.

## Testy

```bash
ctest --test-dir build --output-on-failure
```

Lub bezpośrednio: `./build/tests/gitlense_tests`.

## Uwagi

- Wszystkie napisy użytkownika oraz komentarze kodu są w języku polskim.
- Nie używa się wywołań `git` (tylko libgit2). Wyjście jest czytelne dla człowieka (brak JSON/CSV).