#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>
#include <stack>
#include <queue>

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("SetCell for invalid position");
    }
    if (static_cast<size_t>(pos.row) >= values_.size()) {
        values_.resize(pos.row + 1);
    }
    auto& row = values_[pos.row];
    if (static_cast<size_t>(pos.col) >= row.size()) {
        row.resize(pos.col + 1);
        if (static_cast<size_t>(pos.col + 1) == width_) {
            ++max_width_rows_;
        }
        if (static_cast<size_t>(pos.col + 1) > width_) {
            width_ = pos.col + 1;
            max_width_rows_ = 1;
        }
    }
    auto& cell_into = values_[pos.row][pos.col];
    if (!cell_into.cell) {
        cell_into = {std::make_unique<Cell>(*this), {}};
    }
    // Мы не можем удалить ссылки на ячейку до вызова "Set", потому что если 
    // будет CircularDependencyException (или другая ошибка), то мы нарушим
    // состояние таблицы. 
    std::vector<Position> old_refs = cell_into.cell->GetReferencedCells();
    cell_into.cell->Set(text, [this, pos](const FormulaInterface& formula) {
        const std::vector<Position> referenced_by = formula.GetReferencedCells();
        if (!referenced_by.empty()) {
            std::unordered_set<Position> referenced{
                begin(referenced_by), end(referenced_by)
            };
            std::unordered_set<Position> visited;
            std::stack<Position> to_visit;
            to_visit.push(pos);
            while (!to_visit.empty()) {
                Position current = to_visit.top();
                to_visit.pop();
                visited.insert(current);

                if (referenced.find(current) != end(referenced)) {
                    throw CircularDependencyException{
                        "No circular dependencies allowed"
                    };
                }

                for (Position outgoing : GetBackReferences(current)) {
                    if (visited.find(outgoing) == end(visited)) {
                        to_visit.push(outgoing);
                    }
                }
            }
        }
    });
    for (auto pоs : old_refs) {
        values_[pоs.row][pоs.col].referenced_by->erase(pos);
        if (!values_[pоs.row][pоs.col].referenced_by->size()) {
            values_[pоs.row][pоs.col].referenced_by.reset();
        }
    }    
    for (auto pоs : GetCell(pos)->GetReferencedCells()) {
        if (pоs.IsValid() && !GetCell(pоs)) {
            SetCell(pоs, std::string());
        }
        if (!values_[pоs.row][pоs.col].referenced_by) {
            values_[pоs.row][pоs.col].referenced_by =
                std::make_unique<std::set<Position>>();
        }
        values_[pоs.row][pоs.col].referenced_by->insert(pos);
    }
    std::queue<Position> back_references;
    back_references.push(pos);
    while (!back_references.empty()) {
        Position pos = back_references.front();
        back_references.pop();
        if (values_[pos.row][pos.col].cell->ClearCache()) {
            for (const Position back_reference : GetBackReferences(pos)) {
                back_references.push(back_reference);
            }
        }
    }
}

const CellInterface* Sheet::GetCell(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("GetCell for invalid position");
    }
    if (static_cast<size_t>(pos.row) >= values_.size() ||
        static_cast<size_t>(pos.col) >= values_[pos.row].size()) {
        return nullptr;
    }
    return values_[pos.row][pos.col].cell.get();
}

CellInterface* Sheet::GetCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("GetCell for invalid position");
    }
    static const std::string empty_string;
    if (static_cast<size_t>(pos.row) >= values_.size() ||
        static_cast<size_t>(pos.col) >= values_[pos.row].size()) {
        return nullptr;
    }
    return values_[pos.row][pos.col].cell.get();
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("ClearCell for invalid position");
    }
    if (static_cast<size_t>(pos.row) >= values_.size()) {
        return;
    }
    auto& row = values_[pos.row];
    if (static_cast<size_t>(pos.col) >= row.size()) {
        return;
    }
    if (row[pos.col].referenced_by && row[pos.col].referenced_by->size()) {
        row[pos.col].cell->Set(std::string());
        return;
    }
    if (static_cast<size_t>(pos.col + 1) < row.size()) {
        row[pos.col].cell.reset();
        return;
    }
    row[pos.col].cell.reset();
    size_t old_col_length = row.size();
    size_t new_col_length = old_col_length - 1;
    while (new_col_length > 0 && !row[new_col_length - 1].cell) {
        --new_col_length;
    }
    row.resize(new_col_length);
    if (!new_col_length) {
        if (static_cast<size_t>(pos.row + 1) == values_.size()) {
            size_t new_row_length = values_.size() - 1;
            while (new_row_length > 0 && !values_[new_row_length - 1].size()) {
                --new_row_length;
            }
            values_.resize(new_row_length);
        }
    }
    if (old_col_length != width_) {
        return;
    }
    if (--max_width_rows_) {
        return;
    }
    size_t new_width = 0, new_max_width_rows = 0;
    for (auto& row : values_) {
        if (row.size() < new_width) {
            continue;
        }
        if (row.size() == new_width) {
            ++new_max_width_rows;
            continue;
        }
        new_width = row.size();
        new_max_width_rows = 1;
    }
    width_ = new_width;
    max_width_rows_ = new_max_width_rows;
}

Size Sheet::GetPrintableSize() const {
    return {static_cast<int>(values_.size()), static_cast<int>(width_)};
}

void Sheet::PrintValues(std::ostream& output) const {
    for (auto& row : values_) {
        for (size_t col = 0; col < width_; ++col) {
            if (col != 0) {
                output << '\t';
            }
            if (col >= row.size()) {
                continue;
            }
            if (auto& cell_info = row[col]; cell_info.cell) {
                std::visit(
                    [&output](const auto& x) {
                        output << x;
                    },
                    cell_info.cell->GetValue());
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    for (auto& row : values_) {
        for (size_t col = 0; col < width_; ++col) {
            if (col != 0) {
                output << '\t';
            }
            if (col >= row.size()) {
                continue;
            }
            if (auto& cell_info = row[col]; cell_info.cell) {
                output << cell_info.cell->GetText();
            }
        }
        output << '\n';
    }
}

FormulaInterface::Value Sheet::GetNewValue(
        Position pos,
        const FormulaInterface& formula,
        std::vector<std::vector<int>>& ref_count) {
    ref_count.clear();
    ref_count.resize(values_.size());
    std::queue<Position> back_references;
    for (const Position back_reference : GetBackReferences(pos)) {
        back_references.push(back_reference);
    }
    while (!back_references.empty()) {
        Position pos = back_references.front();
        back_references.pop();
        if (!ref_count[pos.row].size()) {
            ref_count[pos.row].resize(values_[pos.row].size());
        }
        ref_count[pos.row][pos.col]++;
        for (const Position back_reference : GetBackReferences(pos)) {
            back_references.push(back_reference);
        }
    }
    for (const Position pos : formula.GetReferencedCells()) {
        if (static_cast<size_t>(pos.row) < ref_count.size() &&
            static_cast<size_t>(pos.col) < ref_count[pos.row].size() &&
            ref_count[pos.row][pos.col]) {
            throw CircularDependencyException("Circular reference detected");
        }
    }
    return formula.Evaluate(*this);
}

const std::set<Position>& Sheet::GetBackReferences(Position pos) const {
    static const std::set<Position> empty;
    if (static_cast<size_t>(pos.row) >= values_.size() ||
        static_cast<size_t>(pos.col) >= values_[pos.row].size()) {
        return empty;
    }
    if (auto ptr = values_[pos.row][pos.col].referenced_by.get();
        ptr != nullptr) {
        return *ptr;
    } else {
        return empty;
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}
