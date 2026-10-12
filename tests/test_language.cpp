#include "framework.h"

#include "core/language.h"

#include <string>
#include <vector>

using namespace gitlense;

TEST(jezyk_po_roszszerzeniu_cpp)
{
    const LanguageDetector detector;
    CHECK_EQ(detector.languageOf("src/main.cpp"), std::string("C++"));
    CHECK_EQ(detector.languageOf("src/main.cc"), std::string("C++"));
    CHECK_EQ(detector.languageOf("a/b/c/main.cxx"), std::string("C++"));
    CHECK_EQ(detector.languageOf("main.c"), std::string("C"));
}

TEST(jezyk_po_roszszerzeniu_jest_nieczulny_na_wielkosc_liter)
{
    const LanguageDetector detector;
    CHECK_EQ(detector.languageOf("README.MD"), detector.languageOf("README.md"));
    CHECK_EQ(detector.languageOf("A.PY"), std::string("Python"));
}

TEST(rozszerzenia_jezykow_skryptowych)
{
    const LanguageDetector detector;
    CHECK_EQ(detector.languageOf("a.py"), std::string("Python"));
    CHECK_EQ(detector.languageOf("a.rb"), std::string("Ruby"));
    CHECK_EQ(detector.languageOf("a.rs"), std::string("Rust"));
    CHECK_EQ(detector.languageOf("a.go"), std::string("Go"));
    CHECK_EQ(detector.languageOf("a.js"), std::string("JavaScript"));
    CHECK_EQ(detector.languageOf("a.ts"), std::string("TypeScript"));
    CHECK_EQ(detector.languageOf("a.sh"), std::string("Shell"));
    CHECK_EQ(detector.languageOf("a.lua"), std::string("Lua"));
    CHECK_EQ(detector.languageOf("a.php"), std::string("PHP"));
}

TEST(rozszerzenia_jezykow_markupowych_i_danych)
{
    const LanguageDetector detector;
    CHECK_EQ(detector.languageOf("a.md"), std::string("Markdown"));
    CHECK_EQ(detector.languageOf("a.html"), std::string("HTML"));
    CHECK_EQ(detector.languageOf("a.css"), std::string("CSS"));
    CHECK_EQ(detector.languageOf("a.json"), std::string("JSON"));
    CHECK_EQ(detector.languageOf("a.yaml"), std::string("YAML"));
    CHECK_EQ(detector.languageOf("a.toml"), std::string("TOML"));
    CHECK_EQ(detector.languageOf("a.sql"), std::string("SQL"));
}

TEST(jezyk_po_nazwie_pliku_bez_roszszerzenia)
{
    const LanguageDetector detector;
    CHECK_EQ(detector.languageOf("Makefile"), std::string("Makefile"));
    CHECK_EQ(detector.languageOf("CMakeLists.txt"), std::string("CMake"));
    CHECK_EQ(detector.languageOf("Dockerfile"), std::string("Dockerfile"));
    CHECK_EQ(detector.languageOf("src/Dockerfile"), std::string("Dockerfile"));
    CHECK_EQ(detector.languageOf("Gemfile"), std::string("Ruby"));
    CHECK_EQ(detector.languageOf("Rakefile"), std::string("Ruby"));
    CHECK_EQ(detector.languageOf("go.mod"), std::string("Go Module"));
    CHECK_EQ(detector.languageOf("Cargo.toml"), std::string("TOML"));
    CHECK_EQ(detector.languageOf("README.md"), std::string("Markdown"));
    CHECK_EQ(detector.languageOf("LICENSE"), std::string("Text"));
}

TEST(nieznane_rozszerzenie_daje_pusty_jezyk)
{
    const LanguageDetector detector;
    CHECK_EQ(detector.languageOf("plik.xyz"), std::string(""));
    CHECK_EQ(detector.languageOf("cos"), std::string(""));
    CHECK(!LanguageDetector::languageForExtension("xyz").has_value());
    CHECK(!LanguageDetector::languageForFilename("Cos").has_value());
}

TEST(kropka_w_katalogu_nie_jest_rozszerzeniem)
{
    const LanguageDetector detector;
    // katalog "src/v1.2" nie ma rozszerzenia - plik ma pelna nazwe
    CHECK_EQ(detector.languageOf("src/v1.2/CMakeLists.txt"), std::string("CMake"));
}

TEST(gitattributes_nadpisuje_jezyk)
{
    LanguageDetector detector;
    detector.loadAttributes("# komentarz\n*.inc linguist-language=Python\n");
    CHECK_EQ(detector.languageOf("tools/parser.inc"), std::string("Python"));
}

TEST(gitattributes_obsługuje_cytowanie)
{
    LanguageDetector detector;
    detector.loadAttributes("*.tpl linguist-language=\"C++\"\n");
    CHECK_EQ(detector.languageOf("x.tpl"), std::string("C++"));
}

TEST(gitattributes_pomija_wzorzec_gwiazdka)
{
    LanguageDetector detector;
    detector.loadAttributes("* linguist-language=Ruby\n");
    // globalne nadpisanie jest zbyt szerokie - ignorujemy
    CHECK_EQ(detector.languageOf("a.py"), std::string("Python"));
}

TEST(gitattributes_ignoruje_inne_atrybuty)
{
    LanguageDetector detector;
    detector.loadAttributes("*.dat text eol=lf\n");
    CHECK_EQ(detector.languageOf("x.dat"), std::string(""));
}

TEST(rodzaj_pliku_odkodu)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("src/main.cpp") == FileKind::Source);
    CHECK(detector.kindOf("src/main.c") == FileKind::Source);
}

TEST(rodzaj_pliku_naglowek)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("include/api.h") == FileKind::Header);
    CHECK(detector.kindOf("include/api.hpp") == FileKind::Header);
}

TEST(rodzaj_pliku_konfiguracja)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("package.json") == FileKind::Config);
    CHECK(detector.kindOf("config.yaml") == FileKind::Config);
    CHECK(detector.kindOf("settings.toml") == FileKind::Config);
    CHECK(detector.kindOf("Makefile") == FileKind::Config);
}

TEST(rodzaj_pliku_dokumentacja)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("README.md") == FileKind::Doc);
    CHECK(detector.kindOf("doc.txt") == FileKind::Doc);
    CHECK(detector.kindOf("paper.pdf") == FileKind::Doc);
}

TEST(rodzaj_pliku_dane)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("data.csv") == FileKind::Data);
    CHECK(detector.kindOf("data.tsv") == FileKind::Data);
}

TEST(rodzaj_pliku_media)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("img/logo.png") == FileKind::Image);
    CHECK(detector.kindOf("sound.mp3") == FileKind::Audio);
    CHECK(detector.kindOf("clip.mp4") == FileKind::Video);
    CHECK(detector.kindOf("arch.zip") == FileKind::Archive);
    CHECK(detector.kindOf("app.exe") == FileKind::Executable);
    CHECK(detector.kindOf("font.ttf") == FileKind::Font);
}

TEST(rodzaj_pliku_notatnik)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("analysis.ipynb") == FileKind::Notebook);
}

TEST(rodzaj_pliku_lockfile)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("Cargo.lock") == FileKind::Lock);
    CHECK(detector.kindOf("go.sum") == FileKind::Lock);
}

TEST(rodzaj_pliku_vendored)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("third_party/lib/x.cpp") == FileKind::Vendored);
    CHECK(detector.kindOf("vendor/thing.go") == FileKind::Vendored);
    CHECK(detector.kindOf("node_modules/pkg/index.js") == FileKind::Vendored);
    CHECK(LanguageDetector::isVendoredPath("external/x.c"));
    CHECK(!LanguageDetector::isVendoredPath("src/external.c"));
}

TEST(rodzaj_pliku_wygenerowany)
{
    const LanguageDetector detector;
    CHECK(detector.kindOf("static/app.min.js") == FileKind::Generated);
    CHECK(detector.kindOf("proto/msg_pb2.py") == FileKind::Generated);
    CHECK(detector.kindOf("lib/thing.g.dart") == FileKind::Generated);
    CHECK(LanguageDetector::isGeneratedPath("api/service.pb.go"));
    CHECK(!LanguageDetector::isGeneratedPath("src/app.js"));
}

TEST(tekstowe_pliki_to_nie_tylko_kod)
{
    const LanguageDetector detector;
    CHECK(detector.isTextual("README.md"));
    CHECK(detector.isTextual("src/main.cpp"));
    CHECK(detector.isTextual("package.json"));
}

TEST(pliki_binarne_sa_odfiltrowane)
{
    const LanguageDetector detector;
    CHECK(!detector.isTextual("img/logo.png"));
    CHECK(!detector.isTextual("arch.zip"));
    CHECK(!detector.isTextual("app.exe"));
    CHECK(!detector.isTextual("font.woff2"));
    CHECK(!detector.isTextual("clip.webm"));
}

TEST(known_languages_jest_posortowana_i_bez_duplikatow)
{
    const std::vector<std::string> languages = LanguageDetector::knownLanguages();
    CHECK(languages.size() > 50);
    for (std::size_t i = 1; i < languages.size(); ++i) {
        CHECK(languages[i - 1] < languages[i]);
    }
    bool hasCpp = false;
    for (const std::string& language : languages) {
        if (language == "C++") {
            hasCpp = true;
        }
    }
    CHECK(hasCpp);
}

TEST(rodzaje_plikow_maja_nazwy_po_polsku)
{
    CHECK_EQ(std::string(toString(FileKind::Source)), std::string("kod"));
    CHECK_EQ(std::string(toString(FileKind::Doc)), std::string("dokumentacja"));
    CHECK_EQ(std::string(toString(FileKind::Vendored)), std::string("vendored"));
}
