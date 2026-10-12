#include "core/language.h"

#include "core/error.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace gitlense {
namespace {

using ExtensionTable = std::unordered_map<std::string, std::string>;
using FilenameTable = std::unordered_map<std::string, std::string>;

const ExtensionTable& extensionTable()
{
    // Rozszerzenie (bez kropki, male) -> jezyk w stylu Linguist.
    static const ExtensionTable table = {
        // --- C / C++ ------------------------------------------------------
        {"c", "C"},
        {"h", "C"},
        {"cc", "C++"},
        {"cpp", "C++"},
        {"cxx", "C++"},
        {"c++", "C++"},
        {"hpp", "C++"},
        {"hh", "C++"},
        {"hxx", "C++"},
        {"h++", "C++"},
        {"ipp", "C++"},
        {"inl", "C++"},
        {"tcc", "C++"},
        {"mm", "Objective-C++"},
        {"m", "Objective-C"},

        // --- systemowe / niskopoziomowe ------------------------------------
        {"asm", "Assembly"},
        {"s", "Assembly"},
        {"S", "Assembly"},
        {"nasm", "Assembly"},
        {"ld", "Linker Script"},
        {"lds", "Linker Script"},
        {"f", "Fortran"},
        {"f90", "Fortran"},
        {"f95", "Fortran"},
        {"f03", "Fortran"},
        {"for", "Fortran"},
        {"rs", "Rust"},
        {"zig", "Zig"},
        {"nim", "Nim"},
        {"d", "D"},
        {"cr", "Crystal"},
        {"vala", "Vala"},
        {"pas", "Pascal"},
        {"pp", "Puppet"},
        {"p6", "Raku"},

        // --- skryptowe ------------------------------------------------------
        {"py", "Python"},
        {"pyi", "Python"},
        {"pyw", "Python"},
        {"pyx", "Cython"},
        {"pxd", "Cython"},
        {"ipynb", "Jupyter Notebook"},
        {"rb", "Ruby"},
        {"rake", "Ruby"},
        {"gemspec", "Ruby"},
        {"erb", "ERB"},
        {"pl", "Perl"},
        {"pm", "Perl"},
        {"t", "Perl"},
        {"php", "PHP"},
        {"php3", "PHP"},
        {"php4", "PHP"},
        {"php5", "PHP"},
        {"phtml", "PHP"},
        {"lua", "Lua"},
        {"tcl", "Tcl"},
        {"r", "R"},
        {"jl", "Julia"},
        {"dart", "Dart"},
        {"swift", "Swift"},
        {"kt", "Kotlin"},
        {"kts", "Kotlin"},
        {"scala", "Scala"},
        {"sc", "Scala"},
        {"groovy", "Groovy"},
        {"gradle", "Groovy"},
        {"clj", "Clojure"},
        {"cljs", "Clojure"},
        {"cljc", "Clojure"},
        {"edn", "Clojure"},
        {"ex", "Elixir"},
        {"exs", "Elixir"},
        {"erl", "Erlang"},
        {"hrl", "Erlang"},
        {"go", "Go"},
        {"sol", "Solidity"},
        {"hs", "Haskell"},
        {"lhs", "Haskell"},
        {"ml", "OCaml"},
        {"mli", "OCaml"},
        {"fs", "F#"},
        {"fsi", "F#"},
        {"fsx", "F#"},
        {"vb", "Visual Basic"},
        {"vbs", "VBScript"},
        {"ps1", "PowerShell"},
        {"psm1", "PowerShell"},
        {"psd1", "PowerShell"},
        {"ahk", "AutoHotkey"},
        {"au3", "AutoIt"},
        {"sh", "Shell"},
        {"bash", "Shell"},
        {"zsh", "Shell"},
        {"ksh", "Shell"},
        {"fish", "Fish"},
        {"psh", "PowerShell"},
        {"bat", "Batchfile"},
        {"cmd", "Batchfile"},
        {"nu", "NuShell"},
        {"applescript", "AppleScript"},
        {"scpt", "AppleScript"},

        // --- JS / TS / web --------------------------------------------------
        {"js", "JavaScript"},
        {"mjs", "JavaScript"},
        {"cjs", "JavaScript"},
        {"jsx", "JavaScript"},
        {"mjsx", "JavaScript"},
        {"ts", "TypeScript"},
        {"mts", "TypeScript"},
        {"cts", "TypeScript"},
        {"tsx", "TypeScript"},
        {"vue", "Vue"},
        {"svelte", "Svelte"},
        {"astro", "Astro"},
        {"css", "CSS"},
        {"scss", "SCSS"},
        {"sass", "Sass"},
        {"less", "Less"},
        {"styl", "Stylus"},
        {"html", "HTML"},
        {"htm", "HTML"},
        {"xhtml", "HTML"},
        {"jinja", "Jinja"},
        {"jinja2", "Jinja"},
        {"j2", "Jinja"},
        {"twig", "Twig"},
        {"hbs", "Handlebars"},
        {"mustache", "Mustache"},
        {"ejs", "EJS"},
        {"haml", "Haml"},
        {"slim", "Slim"},
        {"pug", "Pug"},
        {"jade", "Pug"},
        {"xml", "XML"},
        {"xsd", "XML"},
        {"xsl", "XSLT"},
        {"xslt", "XSLT"},
        {"svg", "SVG"},
        {"plist", "XML"},
        {"csproj", "XML"},
        {"targets", "XML"},
        {"props", "XML"},
        {"resx", "XML"},
        {"rss", "XML"},
        {"wsdl", "Web Services Description Language"},
        {"proto", "Protocol Buffer"},
        {"graphql", "GraphQL"},
        {"gql", "GraphQL"},
        {"thrift", "Thrift"},
        {"wasm", "WebAssembly"},
        {"wat", "WebAssembly"},

        // --- szablony / dokumentacja / dane ----------------------------------
        {"md", "Markdown"},
        {"markdown", "Markdown"},
        {"mdx", "MDX"},
        {"rst", "reStructuredText"},
        {"adoc", "AsciiDoc"},
        {"asciidoc", "AsciiDoc"},
        {"org", "Org"},
        {"tex", "TeX"},
        {"latex", "TeX"},
        {"bib", "BibTeX"},
        {"txt", "Text"},
        {"text", "Text"},
        {"csv", "CSV"},
        {"tsv", "TSV"},
        {"json", "JSON"},
        {"json5", "JSON"},
        {"jsonc", "JSON"},
        {"geojson", "JSON"},
        {"jsonl", "JSON Lines"},
        {"ndjson", "JSON Lines"},
        {"yaml", "YAML"},
        {"yml", "YAML"},
        {"toml", "TOML"},
        {"ini", "INI"},
        {"cfg", "INI"},
        {"conf", "Config"},
        {"properties", "Properties"},
        {"env", "Dotenv"},
        {"editorconfig", "EditorConfig"},
        {"diff", "Diff"},
        {"patch", "Diff"},
        {"po", "Gettext Catalog"},
        {"pot", "Gettext Catalog"},
        {"mo", "Gettext Catalog"},

        // --- zapytania SQL i bazy -------------------------------------------
        {"sql", "SQL"},
        {"psql", "SQL"},
        {"ddl", "SQL"},
        {"dml", "SQL"},
        {"sqlite", "SQL"},
        {"db", "SQLite"},
        {"sqlite3", "SQLite"},
        {"prisma", "Prisma"},
        {"hql", "HiveQL"},
        {"cypher", "Cypher"},

        // --- style / shader / modele -----------------------------------------
        {"glsl", "GLSL"},
        {"vert", "GLSL"},
        {"frag", "GLSL"},
        {"geom", "GLSL"},
        {"comp", "GLSL"},
        {"hlsl", "HLSL"},
        {"shader", "ShaderLab"},
        {"wgsl", "WGSL"},
        {"scad", "OpenSCAD"},
        {"obj", "Compiled Object"},
        {"stl", "STL"},
        {"ply", "PLY"},
        {"blend", "Blender"},
        {"gltf", "glTF"},
        {"glb", "glTF"},
        {"usdz", "USDZ"},

        // --- slowniki i formaty wymiany ----------------------------------------
        {"strings", "Strings File"},
        {"capnp", "Cap'n Proto"},
        {"fbs", "FlatBuffers"},
        {"avsc", "Avro"},
        {"ico", "Icon"},

        // --- klawiatury, pluginy, zasoby -----------------------------------------
        {"vim", "Vim Script"},
        {"vimrc", "Vim Script"},
        {"el", "Emacs Lisp"},
        {"lisp", "Common Lisp"},
        {"scm", "Scheme"},
        {"ss", "Scheme"},
        {"rkt", "Racket"},
        {"st", "Smalltalk"},
        {"itermcolors", "XML"},
        {"sublime-syntax", "YAML"},
        {"desktop", "desktop"},
        {"service", "systemd"},
        {"socket", "systemd"},
        {"timer", "systemd"},
        {"mount", "systemd"},
        {"udev", "systemd"},
        {"rules", "Makefile"},
        {"spec", "RPM Spec"},
        {"nsi", "NSIS"},
        {"iss", "Inno Setup"},
        {"app", "Rebol"},
        {"rc", "Windows Resource File"},
        {"def", "Modula-2"},
        {"4th", "Forth"},
        {"fth", "Forth"},
        {"f83", "Forth"},
        {"forth", "Forth"},
        {"rmd", "RMarkdown"},
        {"qmd", "Quarto"},
        {"rmarkdown", "RMarkdown"},
        {"sas", "SAS"},
        {"sas7bdat", "SAS"},
        {"ado", "Stata"},
        {"do", "Stata"},
        {"dof", "Stata"},
        {"sps", "SPSS"},
        {"mata", "Mata"},
        {"matlab", "MATLAB"},
        {"gnuplot", "Gnuplot"},
        {"gp", "Gnuplot"},
        {"tikz", "TeX"},
        {"lilypond", "LilyPond"},
        {"ly", "LilyPond"},
        {"abc", "ABC"},
        {"sol", "Solidity"},
        {"vy", "Vyper"},
        {"move", "Move"},
        {"cairo", "Cairo"},
        {"ard", "Arduino"},
        {"ino", "C++"},
        {"pde", "Processing"},
        {"nb", "Wolfram Language"},
        {"wl", "Wolfram Language"},
        {"wol", "Wolfram Language"},
        {"sub", "SubRip Text"},
        {"srt", "SubRip Text"},
        {"vtt", "WebVTT"},
        {"ass", "Advanced SubStation Alpha"},
        {"nfo", "Scene Information File"},
        {"cue", "Cue"},
        {"amr", "Audio"},
        {"mp3", "MP3"},
        {"wav", "Waveform Audio"},
        {"wma", "Windows Media Audio"},
        {"aac", "AAC"},
        {"m4a", "Apple MPEG-4 Audio"},
        {"ogg", "Ogg Vorbis"},
        {"oga", "Ogg Vorbis"},
        {"opus", "Opus"},
        {"flac", "FLAC"},
        {"aiff", "Audio Interchange File Format"},
        {"mid", "MIDI"},
        {"midi", "MIDI"},
        {"ape", "Monkey's Audio"},
        {"m3u", "M3U"},
        {"pls", "M3U"},
        {"au", "NeXT"},
        {"ra", "RealAudio"},
        {"weba", "WebM"},
        {"bmp", "BMP"},
        {"gif", "GIF"},
        {"jpeg", "JPEG"},
        {"jpg", "JPEG"},
        {"png", "PNG"},
        {"apng", "APNG"},
        {"tiff", "TIFF"},
        {"tif", "TIFF"},
        {"webp", "WebP"},
        {"avif", "AVIF"},
        {"heic", "HEIF"},
        {"ppm", "Netpbm"},
        {"pgm", "Netpbm"},
        {"pbm", "Netpbm"},
        {"tga", "Truevision TGA"},
        {"psd", "Photoshop"},
        {"ai", "Adobe Illustrator"},
        {"eps", "EPS"},
        {"xcf", "GIMP"},
        {"flif", "FLIF"},
        {"cr2", "Raw Photo"},
        {"nef", "Raw Photo"},
        {"dng", "Raw Photo"},
        {"arw", "Raw Photo"},
        {"heif", "HEIF"},
        {"avifs", "AVIF"},
        {"jxl", "JPEG XL"},
        {"svgz", "SVG"},
        {"mov", "QuickTime"},
        {"mp4", "MPEG-4"},
        {"m4v", "Apple MPEG-4 Video"},
        {"avi", "AVI"},
        {"mkv", "Matroska"},
        {"webm", "WebM"},
        {"wmv", "Windows Media Video"},
        {"mpg", "MPEG"},
        {"mpeg", "MPEG"},
        {"m2ts", "MPEG-TS"},
        {"flv", "FLV"},
        {"3gp", "3GPP"},
        {"ogv", "Ogg Video"},
        {"rm", "RealMedia"},
        {"rmvb", "RealMedia"},
        {"vob", "MPEG-TS"},
        {"7z", "7-Zip"},
        {"bz2", "BZip2"},
        {"gz", "Gzip"},
        {"lz", "LZ4"},
        {"lz4", "LZ4"},
        {"lzma", "LZMA"},
        {"lzo", "LZO"},
        {"rar", "RAR"},
        {"tar", "Tar"},
        {"tgz", "Gzip"},
        {"txz", "XZ"},
        {"xz", "XZ"},
        {"z", "Zlib"},
        {"zip", "Zip"},
        {"zst", "Zstandard"},
        {"jar", "Java Archive"},
        {"war", "Java Archive"},
        {"ear", "Java Archive"},
        {"whl", "Zip"},
        {"egg", "Zip"},
        {"aar", "Java Archive"},
        {"apk", "Zip"},
        {"deb", "Debian Package"},
        {"rpm", "RPM"},
        {"msi", "Windows Installer"},
        {"iso", "ISO Image"},
        {"dmg", "Apple Disk Image"},
        {"img", "DOS Image"},
        {"pak", "Quake"},
        {"so", "Shared Object"},
        {"dll", "Dynamic Library"},
        {"dylib", "Dynamic Library"},
        {"a", "Static Library"},
        {"lib", "Static Library"},
        {"o", "Compiled Object"},
        {"obj", "Compiled Object"},
        {"class", "Compiled Object"},
        {"elf", "ELF"},
        {"exe", "Executable"},
        {"bin", "Binary"},
        {"eot", "Embedded OpenType"},
        {"otf", "OpenType"},
        {"ttf", "TrueType"},
        {"ttc", "TrueType"},
        {"woff", "WOFF"},
        {"woff2", "WOFF"},
        {"pdf", "PDF"},
        {"djvu", "DjVu"},
        {"ps", "PostScript"},
        {"epub", "EPUB"},
        {"mobi", "Mobipocket"},
        {"azw", "Kindle"},
        {"ttcn", "TTCN"},
        {"rdc", "RenderDoc"},
        {"spv", "SPIR-V"},
        {"pdb", "PDB"},
        {"cppm", "C++ Modules"},
        {"ixx", "C++ Modules"},
        {"mpp", "C++ Modules"},
        {"cu", "Cuda"},
        {"cuh", "Cuda"},
        {"metal", "Metal"},
        {"vbproj", "XML"},
        {"fsproj", "XML"},
        {"vbxproj", "XML"},
        {"sln", "Microsoft Visual Studio"},
        {"csproj.user", "XML"},
        {"g4", "ANTLR"},
        {"peg", "PEG"},
        {"pegjs", "PEG.js"},
        {"jison", "Jison"},
        {"ne", "NEON"},
        {"ron", "RON"},
        {"dockerfile", "Dockerfile"},
        {"containerfile", "Dockerfile"},
        {"gemfile", "Ruby"},
        {"rakefile", "Ruby"},
        {"podfile", "CocoaPods"},
        {"cartfile", "Swift Package Manager"},
        {"cs", "C#"},
        {"csx", "C#"},
        {"cake", "C#"},
        {"vbhtml", "Visual Basic"},
        {"razor", "Razor"},
        {"cshtml", "HTML"},
        {"aspx", "HTML"},
        {"ashx", "XML"},
        {"master", "HTML"},
        {"asc", "AsciiDoc"},
        {"desktop.in", "desktop"},
        {"mocharc", "JSON"},
        {"babelrc", "JSON"},
        {"eslintrc", "JSON"},
        {"prettierrc", "JSON"},
        {"tsconfig", "JSON"},
        {"lock", "Lockfile"},
        {"mk", "Makefile"},
        {"mak", "Makefile"},
        {"cmake.in", "CMake"},
        {"ac", "Autoconf"},
        {"am", "Autoconf"},
        {"m4", "M4"},
        {"in", "Autoconf"},
        {"e", "Eiffel"},
        {"ep", "Eiffel"},
        {"chef", "Chef"},
        {"puppet", "Puppet"},
        {"rbw", "Ruby"},
        {"sbt", "Scala"},
        {"mill", "Scala"},
        {"cabal", "Haskell"},
        {"bzl", "Starlark"},
        {"bazel", "Starlark"},
        {"bzlmod", "Starlark"},
        {"star", "Starlark"},
        {"dhall", "Dhall"},
        {"jsonnet", "Jsonnet"},
        {"libsonnet", "Jsonnet"},
        {"hcl", "HCL"},
        {"tf", "Terraform"},
        {"tfvars", "Terraform"},
        {"nomad", "HCL"},
        {"pkl", "Pkl"},
        {"kdl", "KDL"},
        {"ronn", "Markdown"},
        {"sage", "Sage"},
        {"frg", "GLSL"},
        {"glslv", "GLSL"},
        {"fp", "GLSL"},
        {"vs", "GLSL"},
        {"dib", "BMP"},
        {"art", "ART"},
        {"bdf", "PCF bitmap"},
        {"pcf", "PCF bitmap"},
        {"snapshot", "JavaScript"},
        {"map", "JavaScript"},
        {"es6", "JavaScript"},
        {"es", "JavaScript"},
        {"pac", "PAC Script"},

        // --- dodatkowe jezyki --------------------------------------------------
        {"java", "Java"},
        {"ada", "Ada"},
        {"adb", "Ada"},
        {"agda", "Agda"},
        {"apex", "Apex"},
        {"cls", "Apex"},
        {"arc", "Arc"},
        {"awk", "Awk"},
        {"bal", "Ballerina"},
        {"basm", "BASM"},
        {"bmx", "BlitzMax"},
        {"bsv", "Bluespec"},
        {"boo", "Boo"},
        {"bf", "Brainfuck"},
        {"chpl", "Chapel"},
        {"cl", "OpenCL"},
        {"icl", "Clean"},
        {"click", "Click"},
        {"clp", "CLIPS"},
        {"cbl", "COBOL"},
        {"cob", "COBOL"},
        {"cfm", "ColdFusion"},
        {"cfc", "ColdFusion"},
        {"coq", "Coq"},
        {"dylan", "Dylan"},
        {"ecl", "ECL"},
        {"elm", "Elm"},
        {"fy", "Fancy"},
        {"fan", "Fantom"},
        {"fst", "Faust"},
        {"ftl", "FreeMarker"},
        {"gd", "GDScript"},
        {"feature", "Gherkin"},
        {"golo", "Golo"},
        {"gs", "Gosu"},
        {"hack", "Hack"},
        {"hx", "Haxe"},
        {"idl", "IDL"},
        {"idr", "Idris"},
        {"io", "Io"},
        {"ik", "Ioke"},
        {"thy", "Isabelle"},
        {"ijs", "J"},
        {"janet", "Janet"},
        {"krl", "KRL"},
        {"lfe", "LFE"},
        {"b", "Limbo"},
        {"liquid", "Liquid"},
        {"ls", "LiveScript"},
        {"ll", "LLVM"},
        {"lgt", "Logtalk"},
        {"lol", "LOLCODE"},
        {"lookml", "LookML"},
        {"maxpat", "Max"},
        {"mirah", "Mirah"},
        {"monkey", "Monkey"},
        {"moon", "Moonscript"},
        {"ncl", "NCL"},
        {"nemerle", "Nemerle"},
        {"nlogo", "NetLogo"},
        {"lsp", "NewLisp"},
        {"nl", "NewLisp"},
        {"nf", "Nextflow"},
        {"nit", "Nit"},
        {"nix", "Nix"},
        {"nut", "Nut"},
        {"j", "Objective-J"},
        {"odin", "Odin"},
        {"ooc", "Ooc"},
        {"opa", "Opa"},
        {"p", "OpenEdge ABL"},
        {"qasm", "OpenQASM"},
        {"oxygene", "Oxygene"},
        {"oz", "Oz"},
        {"p4", "P4"},
        {"pan", "Pan"},
        {"psc", "Papyrus"},
        {"pir", "Parrot"},
        {"pwn", "PAWN"},
        {"pep", "Pep8"},
        {"pig", "PigLatin"},
        {"pike", "Pike"},
        {"puml", "PlantUML"},
        {"pro", "Prolog"},
        {"pml", "Promela"},
        {"purs", "PureScript"},
        {"qs", "Q#"},
        {"qml", "QML"},
        {"ragel", "Ragel"},
        {"rsc", "Rascal"},
        {"red", "Red"},
        {"res", "ReScript"},
        {"rpy", "Ren'Py"},
        {"rexx", "REXX"},
        {"ring", "Ring"},
        {"robot", "RobotFramework"},
        {"sls", "SaltStack"},
        {"sci", "Scilab"},
        {"sed", "Sed"},
        {"shen", "Shen"},
        {"sl", "Slash"},
        {"smali", "Smali"},
        {"smithy", "Smithy"},
        {"smt", "SMT"},
        {"sp", "SourcePawn"},
        {"rq", "SPARQL"},
        {"sqf", "SQF"},
        {"stan", "Stan"},
        {"stg", "StringTemplate"},
        {"sss", "SugarSS"},
        {"sc", "SuperCollider"},
        {"sv", "SystemVerilog"},
        {"tt", "T4"},
        {"tla", "TLA"},
        {"uc", "UnrealScript"},
        {"v", "Verilog"},
        {"vhd", "VHDL"},
        {"vhdl", "VHDL"},
        {"volt", "Volt"},
        {"webidl", "WebIDL"},
        {"reg", "Windows Registry Entries"},
        {"ws", "Witcher Script"},
        {"wren", "Wren"},
        {"x10", "X10"},
        {"prg", "xBase"},
        {"xc", "XC"},
        {"xpl", "XProc"},
        {"xq", "XQuery"},
        {"xs", "XS"},
        {"xtend", "Xtend"},
        {"yang", "YANG"},
        {"yar", "YARA"},
        {"zil", "ZIL"},
    };
    return table;
}

const FilenameTable& filenameTable()
{
    static const FilenameTable table = {
        {"Makefile", "Makefile"},
        {"makefile", "Makefile"},
        {"GNUmakefile", "Makefile"},
        {"Makefile.am", "Automake"},
        {"Makefile.in", "Makefile"},
        {"CMakeLists.txt", "CMake"},
        {"CMakeCache.txt", "CMake"},
        {"configure", "Shell"},
        {"configure.ac", "Autoconf"},
        {"Dockerfile", "Dockerfile"},
        {"Containerfile", "Dockerfile"},
        {"docker-compose.yml", "YAML"},
        {"docker-compose.yaml", "YAML"},
        {"Rakefile", "Ruby"},
        {"Gemfile", "Ruby"},
        {"Guardfile", "Ruby"},
        {"Capfile", "Ruby"},
        {"Podfile", "CocoaPods"},
        {"Package.swift", "Swift"},
        {"build.gradle", "Gradle"},
        {"build.gradle.kts", "Kotlin"},
        {"settings.gradle", "Gradle"},
        {"settings.gradle.kts", "Kotlin"},
        {"pom.xml", "Maven"},
        {"build.xml", "Ant Build System"},
        {"build.sbt", "sbt"},
        {"mix.exs", "Elixir"},
        {"mix.lock", "Lockfile"},
        {"stack.yaml", "Haskell"},
        {"cabal.project", "Haskell"},
        {"composer.json", "JSON"},
        {"composer.lock", "Lockfile"},
        {"package.json", "JSON"},
        {"package-lock.json", "Lockfile"},
        {"yarn.lock", "Lockfile"},
        {"pnpm-lock.yaml", "Lockfile"},
        {"bun.lockb", "Lockfile"},
        {"requirements.txt", "Python"},
        {"requirements-dev.txt", "Python"},
        {"Pipfile", "TOML"},
        {"Pipfile.lock", "Lockfile"},
        {"pyproject.toml", "TOML"},
        {"poetry.lock", "Lockfile"},
        {"setup.py", "Python"},
        {"setup.cfg", "INI"},
        {"tox.ini", "INI"},
        {"conda.yaml", "YAML"},
        {"environment.yml", "YAML"},
        {"Cargo.toml", "TOML"},
        {"Cargo.lock", "Lockfile"},
        {"go.mod", "Go Module"},
        {"go.sum", "Lockfile"},
        {"Gopkg.toml", "TOML"},
        {"Glide.yaml", "YAML"},
        {"go", "Go"},
        {"Cargo", "Rust"},
        {"main.rs", "Rust"},
        {"lib.rs", "Rust"},
        {"Vagrantfile", "Ruby"},
        {"Jenkinsfile", "Groovy"},
        {"Brewfile", "Ruby"},
        {"Procfile", "Procfile"},
        {".gitignore", "gitignore"},
        {".gitattributes", "gitattributes"},
        {".gitmodules", "INI"},
        {".editorconfig", "EditorConfig"},
        {".env", "Dotenv"},
        {".bashrc", "Shell"},
        {".bash_profile", "Shell"},
        {".zshrc", "Shell"},
        {".vimrc", "Vim Script"},
        {".profile", "Shell"},
        {"CODEOWNERS", "CODEOWNERS"},
        {"LICENSE", "Text"},
        {"LICENSE.md", "Markdown"},
        {"LICENCE", "Text"},
        {"NOTICE", "Text"},
        {"AUTHORS", "Text"},
        {"CONTRIBUTING", "Text"},
        {"CHANGELOG", "Text"},
        {"CHANGELOG.md", "Markdown"},
        {"README", "Text"},
        {"README.md", "Markdown"},
        {"README.rst", "reStructuredText"},
        {"Kconfig", "Kconfig"},
        {"Kbuild", "Makefile"},
        {"config.guess", "Shell"},
        {"config.sub", "Shell"},
        {"gtkwave", "Verilog"},
        {"ld.script", "Linker Script"},
        {"Makefile.PL", "Perl"},
        {"MANIFEST", "Perl"},
        {"cpanfile", "Perl"},
        {"Doxyfile", "Doxyfile"},
        {"meson.build", "Meson"},
        {"meson_options.txt", "Meson"},
        {"build.shar", "XML"},
        {"hostapd.conf", "INI"},
        {"opentype.css", "CSS"},
    };
    return table;
}

const std::unordered_set<std::string>& binaryExtensions()
{
    static const std::unordered_set<std::string> set = {
        "png", "jpg", "jpeg", "gif", "bmp", "tiff", "tif", "webp", "avif", "heic", "heif", "ico",
        "icns", "psd", "ai", "eps", "xcf", "sketch", "fig", "jxl", "jp2", "j2k", "dds", "ktx",
        "mp3", "wav", "flac", "ogg", "oga", "opus", "aac", "m4a", "wma", "aiff", "aif", "au",
        "mid", "midi", "ape", "ra", "amr", "weba",
        "mp4", "mkv", "avi", "mov", "wmv", "flv", "webm", "m4v", "mpg", "mpeg", "m2ts", "ts",
        "3gp", "ogv", "rm", "rmvb", "vob", "m2v", "divx",
        "zip", "gz", "bz2", "xz", "lz", "lz4", "lzma", "lzo", "zst", "7z", "rar", "tar",
        "tgz", "txz", "jar", "war", "ear", "whl", "egg", "aar", "apk", "deb", "rpm", "msi",
        "iso", "dmg", "img", "pak", "bin", "dat", "db", "sqlite", "sqlite3", "mdb", "idx",
        "exe", "dll", "so", "dylib", "a", "lib", "o", "obj", "class", "pyc", "pyo", "elf",
        "woff", "woff2", "ttf", "otf", "eot", "ttc", "pfb", "pfa",
        "lock", "pdf", "djvu", "ps", "mp3", "m3u", "pls",
    };
    return set;
}

const std::unordered_set<std::string>& configExtensions()
{
    static const std::unordered_set<std::string> set = {
        "json", "yaml", "yml", "toml", "ini", "cfg", "conf", "properties", "env", "xml", "editorconfig",
    };
    return set;
}

const std::unordered_set<std::string>& docExtensions()
{
    static const std::unordered_set<std::string> set = {
        "md", "markdown", "mdx", "rst", "adoc", "asciidoc", "org", "tex", "bib", "txt", "text",
    };
    return set;
}

std::string toLowerCopy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string extensionOf(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    const std::size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return {};
    }
    return toLowerCopy(path.substr(dot + 1));
}

std::string filenameOf(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string trim(std::string value)
{
    const auto notSpace = [](unsigned char c) { return std::isspace(c) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string unquote(std::string value)
{
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"')
                              || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

/// `*` = dowolny ciag, `?` = jeden znak. Wzorzec nie zawiera `/`,
/// wiec dopasowanie odbywa sie w obrebie jednego elementu sciezki.
bool globMatch(const std::string& pattern, const std::string& text)
{
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string::npos; // ostatnie `*` we wzorcu
    std::size_t from = 0;                 // gdzie ostatnie `*` sie zaczyna
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            from = t;
        } else if (star != std::string::npos) {
            // `*` nie mialo jeszcze gdzie siegnac - wydluzamy je o znak
            p = star + 1;
            t = ++from;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

std::vector<std::string> pathSegments(const std::string& path)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            if (i > start) {
                parts.push_back(path.substr(start, i - start));
            }
            start = i + 1;
        }
    }
    return parts;
}

/// `**` dopasuje dowolna liczbe elementow sciezki, w tym zero.
bool matchSegments(const std::vector<std::string>& pattern, std::size_t pi,
                   const std::vector<std::string>& path, std::size_t si)
{
    while (pi < pattern.size() && pattern[pi] == "**") {
        // `**` moze zjadać kolejne elementy albo zniknac w ogole
        for (std::size_t skip = si; skip <= path.size(); ++skip) {
            if (matchSegments(pattern, pi + 1, path, skip)) {
                return true;
            }
        }
        return false;
    }
    if (pi == pattern.size()) {
        return si == path.size();
    }
    return si < path.size() && globMatch(pattern[pi], path[si])
        && matchSegments(pattern, pi + 1, path, si + 1);
}

/// Czy `path` pasuje do wzorca z .gitattributes.
bool matchesGlob(const std::string& pattern, const std::string& path)
{
    // katalog wiodacy "/" w .gitattributes oznacza tylko katalogi
    std::string clean = pattern;
    if (!clean.empty() && clean.front() == '/') {
        clean = clean.substr(1);
    }
    if (clean.empty()) {
        return false;
    }
    if (clean.find('/') == std::string::npos) {
        // wzorzec bez separatorow pasuje do nazwy pliku w kazdym katalogu
        return globMatch(clean, filenameOf(path));
    }
    const std::vector<std::string> parts = pathSegments(clean);
    return matchSegments(parts, 0, pathSegments(path), 0);
}

} // namespace

const char* toString(FileKind kind) noexcept
{
    switch (kind) {
    case FileKind::Source:
        return "kod";
    case FileKind::Header:
        return "naglowek";
    case FileKind::Config:
        return "konfiguracja";
    case FileKind::Doc:
        return "dokumentacja";
    case FileKind::Data:
        return "dane";
    case FileKind::Image:
        return "obraz";
    case FileKind::Audio:
        return "audio";
    case FileKind::Video:
        return "wideo";
    case FileKind::Archive:
        return "archiwum";
    case FileKind::Executable:
        return "binarka";
    case FileKind::Font:
        return "czcionka";
    case FileKind::Notebook:
        return "notatnik";
    case FileKind::Lock:
        return "lockfile";
    case FileKind::Generated:
        return "wygenerowany";
    case FileKind::Vendored:
        return "vendored";
    case FileKind::Other:
        return "inne";
    }
    return "inne";
}

std::optional<std::string> LanguageDetector::languageForExtension(const std::string& ext)
{
    const std::string key = toLowerCopy(ext);
    if (key.empty()) {
        return std::nullopt;
    }
    const auto it = extensionTable().find(key);
    if (it == extensionTable().end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::string> LanguageDetector::languageForFilename(const std::string& filename)
{
    const auto it = filenameTable().find(filename);
    if (it == filenameTable().end()) {
        return std::nullopt;
    }
    return it->second;
}

std::vector<std::string> LanguageDetector::knownLanguages()
{
    std::vector<std::string> result;
    for (const auto& [ext, language] : extensionTable()) {
        result.push_back(language);
    }
    for (const auto& [name, language] : filenameTable()) {
        result.push_back(language);
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

void LanguageDetector::loadAttributes(const std::string& attributesText)
{
    attributeOverrides_.clear();
    std::istringstream stream(attributesText);
    std::string line;
    while (std::getline(stream, line)) {
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) {
            line = line.substr(0, hash);
        }
        line = trim(std::move(line));
        if (line.empty()) {
            continue;
        }
        // format: <wzorzec> attr1 attr2=wartosc
        std::istringstream tokens(line);
        std::string pattern;
        tokens >> pattern;
        std::string attribute;
        std::string key;
        std::string value;
        while (tokens >> attribute) {
            const std::size_t equals = attribute.find('=');
            if (equals == std::string::npos) {
                key = attribute;
                value.clear();
            } else {
                key = attribute.substr(0, equals);
                value = attribute.substr(equals + 1);
            }
            if (key == "linguist-language") {
                std::string language = unquote(trim(value));
                if (pattern == "*") {
                    continue; // globalne nadpisanie - zbyt szerokie, ignorujemy
                }
                // gitattributes podaje gotowa nazwe jezyka ("Python", "C++")
                attributeOverrides_.emplace_back(pattern, language);
            }
        }
    }
}

std::string LanguageDetector::languageOf(const std::string& path) const
{
    // 1. .gitattributes ma najwyzszy priorytet (ostatni pasujacy wzorzec wygrywa)
    std::string found;
    for (const auto& [pattern, language] : attributeOverrides_) {
        if (matchesGlob(pattern, path)) {
            found = language;
        }
    }
    if (!found.empty()) {
        return found;
    }
    // 2. pliki bez rozszerzenia / o znanych nazwach
    const std::string filename = filenameOf(path);
    if (const auto byName = languageForFilename(filename)) {
        return *byName;
    }
    // 3. pelna nazwa jako klucz (np. "Podfile.spec" w tabeli nazw)
    if (const auto byFullName = languageForFilename(path)) {
        return *byFullName;
    }
    // 4. rozszerzenie
    if (const auto byExt = languageForExtension(extensionOf(path))) {
        return *byExt;
    }
    // 5. heurystyka dla nieznanych rozszerzen
    if (filename.rfind("Dockerfile", 0) == 0) {
        return "Dockerfile";
    }
    if (path.find("test") != std::string::npos && extensionOf(path) == "sh") {
        return "Shell";
    }
    return {};
}

FileKind LanguageDetector::kindOf(const std::string& path) const
{
    if (isVendoredPath(path)) {
        return FileKind::Vendored;
    }
    if (isGeneratedPath(path)) {
        return FileKind::Generated;
    }
    const std::string ext = extensionOf(path);
    if (binaryExtensions().count(ext) != 0) {
        if (ext == "pdf" || ext == "ps") {
            return FileKind::Doc;
        }
        if (ext == "ttf" || ext == "otf" || ext == "woff" || ext == "woff2" || ext == "eot"
            || ext == "ttc") {
            return FileKind::Font;
        }
        if (ext == "mp3" || ext == "wav" || ext == "flac" || ext == "ogg" || ext == "oga"
            || ext == "opus" || ext == "aac" || ext == "m4a" || ext == "wma" || ext == "aiff"
            || ext == "mid" || ext == "midi" || ext == "ape" || ext == "amr") {
            return FileKind::Audio;
        }
        if (ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" || ext == "wmv"
            || ext == "flv" || ext == "webm" || ext == "m4v" || ext == "mpg" || ext == "mpeg"
            || ext == "m2ts" || ext == "3gp" || ext == "ogv" || ext == "rm" || ext == "rmvb"
            || ext == "vob") {
            return FileKind::Video;
        }
        if (ext == "zip" || ext == "gz" || ext == "bz2" || ext == "xz" || ext == "lz" || ext == "lz4"
            || ext == "lzma" || ext == "lzo" || ext == "zst" || ext == "7z" || ext == "rar"
            || ext == "tar" || ext == "tgz" || ext == "jar" || ext == "war" || ext == "apk"
            || ext == "deb" || ext == "rpm" || ext == "msi" || ext == "iso" || ext == "dmg") {
            return FileKind::Archive;
        }
        if (ext == "exe" || ext == "dll" || ext == "so" || ext == "dylib" || ext == "a"
            || ext == "lib" || ext == "o" || ext == "obj" || ext == "class" || ext == "pyc"
            || ext == "elf") {
            return FileKind::Executable;
        }
        if (ext == "lock") {
            return FileKind::Lock;
        }
        return FileKind::Image;
    }
    if (ext == "ipynb") {
        return FileKind::Notebook;
    }
    if (configExtensions().count(ext) != 0) {
        return FileKind::Config;
    }
    if (docExtensions().count(ext) != 0) {
        return FileKind::Doc;
    }
    // pliki o rozpoznawalnej nazwie: build systemy i dokumenty korzenia
    const std::string filename = filenameOf(path);
    if (filename == "Makefile" || filename == "Dockerfile" || filename == "CMakeLists.txt"
        || filename == "configure" || filename == "Jenkinsfile" || filename == "Gemfile"
        || filename == "Rakefile") {
        return FileKind::Config;
    }
    if (filename.rfind("LICENSE", 0) == 0 || filename.rfind("README", 0) == 0
        || filename == "COPYING" || filename == "AUTHORS" || filename == "CHANGELOG"
        || filename == "NOTICE" || filename == "CONTRIBUTING") {
        return FileKind::Doc;
    }
    const std::string lang = languageOf(path);
    if (!lang.empty()) {
        if (lang == "Lockfile") {
            return FileKind::Lock;
        }
        if (lang == "Text" || lang == "CSV" || lang == "TSV" || lang == "JSON"
            || lang == "JSON Lines") {
            return FileKind::Data;
        }
        if (extensionOf(path) == "h" || extensionOf(path) == "hpp" || extensionOf(path) == "hh"
            || extensionOf(path) == "hxx" || lang.find("Header") != std::string::npos) {
            return FileKind::Header;
        }
        return FileKind::Source;
    }
    return FileKind::Other;
}

bool LanguageDetector::isTextual(const std::string& path) const
{
    const FileKind kind = kindOf(path);
    switch (kind) {
    case FileKind::Image:
    case FileKind::Audio:
    case FileKind::Video:
    case FileKind::Archive:
    case FileKind::Executable:
    case FileKind::Font:
        return false;
    default:
        return true;
    }
}

bool LanguageDetector::isVendoredPath(const std::string& path)
{
    static const char* kMarkers[] = {"third_party/", "thirdparty/", "vendor/", "vendored/",
                                    "node_modules/", "bower_components/", "external/",
                                    "deps/", "Godeps/", "site-packages/", ".yarn/",
                                    "packages/", "extern/"};
    for (const char* marker : kMarkers) {
        if (path.rfind(marker, 0) == 0) {
            return true;
        }
        if (path.find(std::string("/") + marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool LanguageDetector::isGeneratedPath(const std::string& path)
{
    static const char* kMarkers[] = {".min.js",  ".min.css",  "_pb2.py",   "_pb2_grpc.py",
                                     ".g.dart",  ".pb.go",     "_pb.js",     ".designer.cs",
                                     ".g.cs",    ".freezed.dart", ".gen.go",  ".designer.vb",
                                     "_generated.", ".generated.", ".lock.h",  ".d.ts"};
    for (const char* marker : kMarkers) {
        if (path.find(marker) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace gitlense
