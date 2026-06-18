#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace json_body {

// Перечисление типов JSON значений
enum class ValueType {
    String,
    Number,
    Boolean,
    Null
};

// Структура для хранения JSON значения вместе с его типом
struct Value {
    ValueType type = ValueType::Null;
    std::string stringValue;
    long long numberValue = 0;
    bool boolValue = false;
};

// Класс для хранения и доступа к JSON объекту с методами получения значений по ключам
class Object {
public:
    // Проверяет наличие ключа в объекте
    bool has(const std::string& key) const {
        return values_.find(key) != values_.end();
    }

    // Находит значение по ключу, возвращает указатель или nullptr
    const Value* find(const std::string& key) const {
        const auto it = values_.find(key);
        return it == values_.end() ? nullptr : &it->second;
    }

    // Получает строковое значение по ключу
    bool getString(const std::string& key, std::string& out) const {
        const Value* value = find(key);
        if (!value || value->type != ValueType::String) {
            return false;
        }
        out = value->stringValue;
        return true;
    }

    // Получает целочисленное значение по ключу (может преобразовать строку в число)
    bool getInt(const std::string& key, int& out) const {
        const Value* value = find(key);
        if (!value) {
            return false;
        }
        if (value->type == ValueType::Number) {
            out = static_cast<int>(value->numberValue);
            return true;
        }
        if (value->type == ValueType::String) {
            try {
                out = std::stoi(value->stringValue);
                return true;
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    // Получает булево значение по ключу
    bool getBool(const std::string& key, bool& out) const {
        const Value* value = find(key);
        if (!value || value->type != ValueType::Boolean) {
            return false;
        }
        out = value->boolValue;
        return true;
    }

    // Устанавливает значение для ключа
    void set(const std::string& key, Value value) {
        values_[key] = std::move(value);
    }

private:
    std::unordered_map<std::string, Value> values_;
};

// Класс для парсинга JSON текста в объекты
class Parser {
public:
    // Инициализирует парсер с текстом JSON
    explicit Parser(const std::string& text) : text_(text) {}

    // Парсит JSON из текста и заполняет объект Object, возвращает false с ошибкой при неудаче
    bool parse(Object& out, std::string& error) {
        error.clear();
        skipWs();
        if (!consume('{')) {
            error = "Ожидался JSON-объект";
            return false;
        }

        skipWs();
        if (consume('}')) {
            return true;
        }

        while (true) {
            std::string key;
            if (!parseString(key, error)) {
                return false;
            }
            skipWs();
            if (!consume(':')) {
                error = "Ожидался символ ':'";
                return false;
            }
            skipWs();

            Value value;
            if (!parseValue(value, error)) {
                return false;
            }
            out.set(key, std::move(value));

            skipWs();
            if (consume('}')) {
                break;
            }
            if (!consume(',')) {
                error = "Ожидалась запятая или конец объекта";
                return false;
            }
            skipWs();
        }

        skipWs();
        if (pos_ != text_.size()) {
            error = "Лишние символы после JSON-объекта";
            return false;
        }
        return true;
    }

private:
    const std::string& text_;
    size_t pos_ = 0;

    void skipWs() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    bool consume(char expected) {
        if (pos_ >= text_.size() || text_[pos_] != expected) {
            return false;
        }
        ++pos_;
        return true;
    }

    bool parseString(std::string& out, std::string& error) {
        if (!consume('"')) {
            error = "Ожидалась строка JSON";
            return false;
        }

        out.clear();
        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return true;
            }
            if (ch == '\\') {
                if (pos_ >= text_.size()) {
                    error = "Некорректный escape-последовательность";
                    return false;
                }
                const char esc = text_[pos_++];
                switch (esc) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) {
                            error = "Некорректный unicode escape";
                            return false;
                        }
                        unsigned int codePoint = 0;
                        for (size_t i = 0; i < 4; ++i) {
                            const char hex = text_[pos_++];
                            codePoint <<= 4;
                            if (hex >= '0' && hex <= '9') codePoint += static_cast<unsigned int>(hex - '0');
                            else if (hex >= 'a' && hex <= 'f') codePoint += static_cast<unsigned int>(hex - 'a' + 10);
                            else if (hex >= 'A' && hex <= 'F') codePoint += static_cast<unsigned int>(hex - 'A' + 10);
                            else {
                                error = "Некорректный unicode escape";
                                return false;
                            }
                        }
                        if (codePoint <= 0x7F) {
                            out += static_cast<char>(codePoint);
                        } else {
                            out += '?';
                        }
                        break;
                    }
                    default:
                        error = "Неизвестный escape-символ";
                        return false;
                }
            } else {
                out += ch;
            }
        }

        error = "Строка JSON не закрыта";
        return false;
    }

    bool parseNumber(Value& out, std::string& error) {
        const size_t start = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        if (pos_ >= text_.size() || !std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            error = "Некорректное число";
            return false;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
        try {
            out.type = ValueType::Number;
            out.numberValue = std::stoll(text_.substr(start, pos_ - start));
            return true;
        } catch (...) {
            error = "Число вне диапазона";
            return false;
        }
    }

    bool parseLiteral(const std::string& literal, ValueType type, Value& out) {
        if (text_.compare(pos_, literal.size(), literal) != 0) {
            return false;
        }
        pos_ += literal.size();
        out.type = type;
        if (type == ValueType::Boolean) {
            out.boolValue = (literal == "true");
        }
        return true;
    }

    bool parseValue(Value& out, std::string& error) {
        if (pos_ >= text_.size()) {
            error = "Ожидалось значение";
            return false;
        }

        const char ch = text_[pos_];
        if (ch == '"') {
            out.type = ValueType::String;
            return parseString(out.stringValue, error);
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            return parseNumber(out, error);
        }
        if (text_.compare(pos_, 4, "true") == 0) {
            return parseLiteral("true", ValueType::Boolean, out);
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            return parseLiteral("false", ValueType::Boolean, out);
        }
        if (text_.compare(pos_, 4, "null") == 0) {
            return parseLiteral("null", ValueType::Null, out);
        }

        error = "Неподдерживаемый тип значения";
        return false;
    }
};

// Вспомогательная функция для парсинга JSON текста
inline bool parseObject(const std::string& text, Object& out, std::string& error) {
    Parser parser(text);
    return parser.parse(out, error);
}

} // namespace json_body
