#pragma once

#include "cell.h"
#include "common.h"

#include <functional>
#include <set>

class Sheet : public SheetInterface {
public:
    ~Sheet();

    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;

    // Возвращает новое значение, обновляет значения формул при необходимости.
    FormulaInterface::Value GetNewValue(
        Position pos,
        const FormulaInterface& formula,
        std::vector<std::vector<int>>& ref_count);
    const std::set<Position>& GetBackReferences(Position pos) const;

private:
    struct CellInfo {
        std::unique_ptr<Cell> cell;
        std::unique_ptr<std::set<Position>> referenced_by;
    };
    std::vector<std::vector<CellInfo>> values_;
    size_t width_ = 0;
    size_t max_width_rows_ = 0;
};