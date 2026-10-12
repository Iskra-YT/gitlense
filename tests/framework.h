// Minimalna ramka testow - bez zaleznosci zewnetrznych.
#pragma once

#include <cstdlib>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace testing {

struct TestCase {
    std::string name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry()
{
    static std::vector<TestCase> cases;
    return cases;
}

struct Registrar {
    Registrar(const std::string& name, std::function<void()> body)
    {
        registry().push_back({name, std::move(body)});
    }
};

class Failure : public std::exception {
public:
    explicit Failure(std::string message) : message_(std::move(message)) {}
    [[nodiscard]] const char* what() const noexcept override { return message_.c_str(); }

private:
    std::string message_;
};

template <typename T>
std::string describe(const T& value)
{
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

inline std::string describe(bool value)
{
    return value ? "true" : "false";
}

inline int run()
{
    std::size_t passed = 0;
    std::vector<std::string> failures;
    for (const TestCase& test : registry()) {
        try {
            test.body();
            ++passed;
        } catch (const std::exception& error) {
            failures.push_back(test.name + "\n    " + error.what());
        }
    }
    for (const std::string& failure : failures) {
        std::cerr << "FAIL: " << failure << "\n";
    }
    std::cout << passed << "/" << registry().size() << " testow zaliczonych\n";
    return failures.empty() ? EXIT_SUCCESS : EXIT_FAILURE;
}

} // namespace testing

#define TEST(nazwa)                                                                     \
    static void nazwa();                                                                \
    static const ::testing::Registrar registrar_##nazwa(#nazwa, [] { nazwa(); });       \
    static void nazwa()

#define CHECK(warunek)                                                                 \
    do {                                                                               \
        if (!(warunek)) {                                                              \
            throw ::testing::Failure(std::string("nie spełnione: ") + #warunek          \
                                     + " (linia " + std::to_string(__LINE__) + ")");  \
        }                                                                              \
    } while (false)

#define CHECK_EQ(a, b)                                                                 \
    do {                                                                               \
        const auto left_ = (a);                                                        \
        const auto right_ = (b);                                                       \
        if (!(left_ == right_)) {                                                      \
            throw ::testing::Failure(std::string("oczekiwano ") + #a + " == " + #b      \
                                     + "\n    lewa:  " + ::testing::describe(left_)      \
                                     + "\n    prawa: " + ::testing::describe(right_));  \
        }                                                                              \
    } while (false)

#define CHECK_NE(a, b)                                                                 \
    do {                                                                               \
        if ((a) == (b)) {                                                              \
            throw ::testing::Failure(std::string("oczekiwano różne wartości: ") + #a   \
                                     + " i " + #b);                                    \
        }                                                                              \
    } while (false)
