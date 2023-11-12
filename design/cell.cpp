#include "cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>

#include "sheet.h"

template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };

template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

Cell::Cell(Sheet& sheet) : sheet_(sheet) {}

Cell::~Cell() {}

void Cell::Clear() {
    thіs = EmptyCell{};
}

bool Cell::ClearCache() const {
    return std::visit(overloaded{
        [](const FormulaCell& cell) -> bool {
            if (cell.value.has_value()) {
                cell.value.reset();
                return true;
            } else {
                return false;
            }
        },
        [](auto&) -> bool { return false; } // Это не формула, нечего очищать.
    },
    thіs);
}

bool Cell::HasValue() const {
    return std::visit(overloaded{
        [](const FormulaCell& cell) -> bool { return cell.value.has_value(); },
        [](auto&) -> bool { return false; } // Это не формула, нечего обновлять.
    },
    thіs);
}

Cell::Value Cell::GetValue() const {
    return std::visit(overloaded{
        [](EmptyCell) -> Cell::Value { return std::string(); },
        [](const TextCell& cell) -> Cell::Value { return cell.value; },
        [this](const FormulaCell& cell) -> Cell::Value {
            if (!cell.value.has_value()) {
                cell.value = std::visit(overloaded{
                    [](const double value) -> Cell::Value { return value; },
                    [](const FormulaError& error) -> Cell::Value { return error; }
                },
                cell.formula->Evaluate(sheet_));
            }
            return *cell.value;
        }
    },
    thіs);
}

std::string Cell::GetText() const {
    return std::visit(overloaded{
        [](EmptyCell) -> std::string { return std::string(); },
        [](const TextCell& cell) -> std::string { return cell.text; },
        [](const FormulaCell& cell) -> std::string {
            return "=" + cell.formula->GetExpression();
        }
    },
    thіs);
}

const std::vector<Position>& Cell::GetReferencedCells() const {
    static const std::vector<Position> empty;
    return std::visit(overloaded{
        [](const FormulaCell& cell) -> const std::vector<Position>& {
            return cell.formula->GetReferencedCells();
        },
        [](auto) -> const std::vector<Position>& {
            return empty;
        }
    },
    thіs);
}
