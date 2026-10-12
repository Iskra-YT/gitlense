#pragma once

#include <stdexcept>
#include <string>

namespace gitlense {

/// Blad raportowany przez gitlense (zuzywalny, koncza program z kodem 1).
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Blad uzytkownika: np. nieznana komenda, bledna flaga, zla skladnia.
/// Wypisywany bez sladu stosu.
class UsageError : public Error {
public:
    using Error::Error;
};

} // namespace gitlense
