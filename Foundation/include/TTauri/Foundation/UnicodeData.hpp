// Copyright 2019 Pokitec
// All rights reserved.

#pragma once

#include "TTauri/Foundation/ResourceView.hpp"
#include "TTauri/Foundation/placement.hpp"
#include "TTauri/Foundation/required.hpp"
#include <gsl/gsl>

namespace TTauri {
struct UnicodeData_GraphemeBreakState;
struct UnicodeData_Description;

enum class GraphemeUnitType : uint8_t {
    Other = 0,
    CR = 1,
    LF = 2,
    Control = 3,
    Extend = 4,
    ZWJ = 5,
    Regional_Indicator = 6,
    Prepend = 7,
    SpacingMark = 8,
    L = 9,
    V = 10,
    T = 11,
    LV = 12,
    LVT = 13,
    Extended_Pictographic = 14
};

class UnicodeData {
private:
    gsl::span<std::byte const> bytes;
    std::unique_ptr<ResourceView> view;

    size_t descriptions_offset;
    size_t descriptions_count;

    size_t compositions_offset;
    size_t compositions_count;
public:
    /*! Load a true type font.
    * The methods in this class will parse the true-type font at run time.
    * This also means that the bytes passed into this constructor will need to
    * remain available.
    */
    UnicodeData(gsl::span<std::byte const> bytes);
    UnicodeData(std::unique_ptr<ResourceView> view);
    UnicodeData() = delete;
    UnicodeData(UnicodeData const &other) = delete;
    UnicodeData &operator=(UnicodeData const &other) = delete;
    UnicodeData(UnicodeData &&other) = delete;
    UnicodeData &operator=(UnicodeData &&other) = delete;
    ~UnicodeData() = default;

    /*! This function will canonically decompose the text.
     * Ligatures will be decomposed.
     *
     * \param text to decompose, must be passed to checkCanonicalComposition() first.
     * \return The text after canonical decomposition.
     */
    std::u32string canonicalDecompose(std::u32string_view text, bool decomposeLigatures=false) const noexcept;

    /*! This function will compatible decompose the text.
    * This function should be used before comparing two texts.
    *
    * \param text to decompose, must be passed to checkCanonicalComposition() first.
    * \return The text after canonical decomposition.
    */
    std::u32string compatibleDecompose(std::u32string_view text) const noexcept;

    /*! This function will compose the text.
     *
     * \param text to compose, must be passed to checkCanonicalComposition() or canonicalDecompose() first.
     * \return The text after composition.
     */
    void compose(std::u32string &text) const noexcept;


private:
    UnicodeData_Description const *getDescription(char32_t c) const noexcept;
    GraphemeUnitType UnicodeData::getGraphemeUnitType(char32_t c) const noexcept;

    void initialize();
    bool checkGraphemeBreak(char32_t c, UnicodeData_GraphemeBreakState &state) const noexcept;
    char32_t compose(char32_t startCharacter, char32_t composingCharacter) const noexcept;
    void decomposeCodePoint(std::u32string &result, char32_t c, bool decomposeCompatible, bool decomposeLigatures) const noexcept;
    std::u32string decompose(std::u32string_view text, bool decomposeCompatible, bool decomposeLigatures=false) const noexcept;
    void normalizeDecompositionOrder(std::u32string &result) const noexcept;

};

template<>
std::unique_ptr<UnicodeData> parseResource(URL const &location);

}