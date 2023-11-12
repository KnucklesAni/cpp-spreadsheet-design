#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    return output << "#ARITHM!";
}

namespace {

class Formula : public FormulaInterface {
public:
    explicit Formula(std::string expression) :
        ast_(ParseFormulaAST(expression)),
        referenced_cells_(ast_.GetReferencedCells()) {}
    Value Evaluate(const SheetInterface& sheet) const override {
        try {
            return ast_.Execute(sheet);
        } catch (const FormulaError& fe) {
            return fe;
        }    
    }
    std::string GetExpression() const override {
        std::ostringstream os;
        ast_.PrintFormula(os);
        return os.str();
    }
    const std::vector<Position>& GetReferencedCells() const override {
        return referenced_cells_;
    }

private:
    FormulaAST ast_;
    std::vector<Position> referenced_cells_;
};

}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    try {
        return std::make_unique<Formula>(std::move(expression));
    } catch (const ParsingError&) {
        throw FormulaException("Couldn\'t parse formula"); 
    } catch (const antlr4::ParseCancellationException&) {
        throw FormulaException("Premature end of formula"); 
    }
}

FormulaError::FormulaError(FormulaError::Category category)
    : category_(category) {
}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
    switch (category_) {
        case FormulaError::Category::Ref: return "REF";
        case FormulaError::Category::Value: return "VALUE";
        case FormulaError::Category::Div0: return "ARITHM";
        default:
            // have to do this because VC++ has a buggy warning
            assert(false);
            return "";
    }
}
